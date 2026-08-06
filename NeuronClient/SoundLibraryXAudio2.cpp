#include "pch.h"

// xaudio2.h keeps its inline helpers behind this define — without it
// XAudio2CutoffFrequencyToRadians, which turns a cutoff in hertz into the
// units SetFilterParameters wants, is simply not declared.
#define XAUDIO2_HELPER_FUNCTIONS

#include <xaudio2.h>
#include <x3daudio.h>
#include <xaudio2fx.h>
#include <xapofx.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include "Debug.h"
#include "MathUtils.h"
#include "Profiler.h"

#include "SoundFilter.h"
#include "SoundLibraryXAudio2.h"


namespace Neuron
{
  namespace
  {
    // One block is 50ms, which is exactly the SoundSystem tick — Advance runs
    // inside the SOUNDSYSTEM_UPDATEPERIOD gate, so a block is consumed between
    // consecutive top-ups and no more. Six of them give 300ms of ring with
    // 250ms of slack for a late frame.
    //
    // The DirectSound backend this replaces used 20,000 samples per channel,
    // which is 900ms at the default 22kHz mix rate. Shorter is the point: that
    // ring was sized to survive cursor-chasing, and there is no cursor here.
    constexpr int BlocksPerChannel = 6;
    constexpr int BlocksPerSecond = 20;

    // The highest rate any sample in GameData uses. See Initialise for why a
    // nominal rate still matters when XAudio2 resamples everything anyway.
    constexpr int NominalSampleRate = 44100;

    // The music voice's width. Effect voices are mono and must stay mono —
    // X3DAudio positions them and it pans a mono source — so this is the one
    // voice in the backend that is not 1.
    constexpr int MusicChannels = 2;

    // The largest pitch multiplier the blueprints ask for is 2.17 (measured over
    // GameData/Sounds.txt), and a voice cannot exceed the ratio it was created
    // with. Four is that with room, and well inside XAUDIO2_MAX_FREQ_RATIO.
    constexpr float MaxFrequencyRatio = 4.0f;

    // The value DirectSound was given through SetDopplerFactor, kept so the
    // pitch shift on a moving sound is the one the game was tuned against
    // rather than the physically correct one.
    constexpr float DopplerScaler = 0.1f;

    // Enough for any endpoint XAudio2 will hand us; 5.1 and 7.1 both fit.
    constexpr int MaxOutputChannels = 8;

    X3DAUDIO_VECTOR ToX3DAudio(DirectX::XMFLOAT3 const& _v)
    {
      X3DAUDIO_VECTOR out;
      out.x = _v.x;
      out.y = _v.y;
      out.z = _v.z;
      return out;
    }

    // m_masterVolume and the per-channel calculation below are both in
    // hundredths of a decibel of ATTENUATION, which is DirectSound's unit and
    // the one every caller above this layer was tuned against. XAudio2 wants a
    // linear amplitude, so the conversion happens once, here.
    float HundredthsOfDecibelToAmplitude(float _hundredths)
    {
      if (_hundredths <= -10000.0f)
        return 0.0f;
      if (_hundredths >= 0.0f)
        return 1.0f;

      return std::pow(10.0f, _hundredths / 2000.0f);
    }
  } // namespace


  //*****************************************************************************
  // Class XAudio2Voice
  //*****************************************************************************

  class XAudio2Voice
  {
    public:
      IXAudio2SourceVoice* m_voice = nullptr;

      // The submitted audio, and the reason this is a member rather than a
      // local: XAudio2 does NOT copy a submitted buffer. It reads the memory on
      // its own thread until the buffer has finished playing, so a block stays
      // allocated and untouched for as long as it is queued — which is what the
      // rotation through BlocksPerChannel guarantees.
      std::vector<signed short> m_blocks;

      // FRAMES per block, and the width of a frame. Every effect voice is mono,
      // so for those two the width is 1 and a frame is a short; the music voice
      // is stereo and a frame is two. Keeping both means no expression below has
      // to guess which unit it is in.
      int m_blockFrames = 0;
      int m_voiceChannels = 1;
      int m_nextBlock = 0;

      // Owned by the SoundSystem callbacks, which count down a sound's trailing
      // silence through it. Per channel, exactly as the DirectSound channel held it.
      int m_silenceRemaining = 0;

      // The rate the voice was told its samples are in. ResetChannel adopts the
      // pending value; SetChannelFrequency then expresses everything as a ratio
      // against it. Zero until the first reset.
      unsigned int m_sourceRate = 0;
      int m_pendingRate = 0;

      // Pitch and doppler are held apart and combined by ApplyFrequencyRatio.
      // SetChannelFrequency owns the first and the X3DAudio pass owns the
      // second, they run at different moments, and a single stored ratio would
      // mean whichever wrote last silently discarded the other's contribution.
      float m_pitchRatio = 1.0f;
      float m_doppler = 1.0f;
      float m_appliedRatio = 1.0f;

      // Last values written, so a repeated set is not an XAudio2 call. The
      // DirectSound backend cached the same for the same reason.
      float m_volume = -1.0f;
      float m_minDist = -1.0f;
      int m_3DMode = -1;

      // True while an X3DAudio matrix is on the voice, so a channel that stops
      // being positioned — channels are reused by whatever sound wins them —
      // gets its panning put back rather than inheriting the last sound's.
      bool m_positionalMatrix = false;

      // Where each filter type ended up in the voice's effect chain, or -1 for
      // one that is not in it. UpdateDspFX addresses an effect by chain INDEX,
      // which is the only handle XAudio2 offers once SetEffectChain has run.
      int m_effectIndex[SoundLibrary3d::NUM_FILTERS];

      // The two effects XAudio2 has no equivalent of. They run over the PCM
      // block during the fill, which is exactly where the DirectSound backend
      // ran them, so what they sound like does not change.
      std::unique_ptr<DspEffect> m_inFillEffects[SoundLibrary3d::NUM_FILTERS];

      bool m_voiceFilterActive = false;

      XAudio2Voice()
      {
        for (int& index : m_effectIndex)
          index = -1;
      }

      DirectX::XMFLOAT3 m_pos{0.0f, 0.0f, 0.0f};
      DirectX::XMFLOAT3 m_vel{0.0f, 0.0f, 0.0f};

      int BlockShorts() const { return m_blockFrames * m_voiceChannels; }

      signed short* Block(int _index) { return m_blocks.data() + static_cast<size_t>(_index) * BlockShorts(); }
  };


  //*****************************************************************************
  // Class XAudio2Data
  //*****************************************************************************

  class XAudio2Data
  {
    public:
      IXAudio2* m_engine = nullptr;
      IXAudio2MasteringVoice* m_masteringVoice = nullptr;
      bool m_ownsCom = false;

      // X3DAudio replaces DS3D. The instance is a plain byte array holding the
      // speaker geometry, computed once from the endpoint's channel mask.
      X3DAUDIO_HANDLE m_x3dInstance{};
      bool m_x3dReady = false;
      unsigned int m_outputChannels = 0;

      X3DAUDIO_LISTENER m_listener{};
      float m_matrix[MaxOutputChannels] = {};

      // Index 0..m_numChannels-1 are the game's channels; the music voice is
      // separate because its id is m_musicChannelId, which is -1.
      std::vector<XAudio2Voice> m_channels;
      XAudio2Voice m_musicChannel;
  };


  //*****************************************************************************
  // Class SoundLibraryXAudio2
  //*****************************************************************************

  SoundLibraryXAudio2::SoundLibraryXAudio2()
    : SoundLibrary3d(),
      m_data(std::make_unique<XAudio2Data>())
  {
  }


  SoundLibraryXAudio2::~SoundLibraryXAudio2() { Shutdown(); }


  void SoundLibraryXAudio2::Shutdown()
  {
    if (!m_data)
      return;

    // Order matters and is not negotiable: a voice must be destroyed before the
    // engine that owns it, and DestroyVoice blocks until the audio thread has
    // stopped reading — which is what makes it safe for m_blocks to be freed
    // when this object goes.
    auto destroyVoice = [](XAudio2Voice& _channel)
    {
      if (_channel.m_voice)
      {
        _channel.m_voice->Stop(0);
        _channel.m_voice->FlushSourceBuffers();
        _channel.m_voice->DestroyVoice();
        _channel.m_voice = nullptr;
      }
    };

    for (XAudio2Voice& channel : m_data->m_channels)
      destroyVoice(channel);
    destroyVoice(m_data->m_musicChannel);

    if (m_data->m_masteringVoice)
    {
      m_data->m_masteringVoice->DestroyVoice();
      m_data->m_masteringVoice = nullptr;
    }

    if (m_data->m_engine)
    {
      m_data->m_engine->Release();
      m_data->m_engine = nullptr;
    }

    // Balanced against the CoInitializeEx in Initialise, and only when that call
    // is the one that initialised COM on this thread. The DirectSound backend
    // never did this half of it.
    if (m_data->m_ownsCom)
    {
      CoUninitialize();
      m_data->m_ownsCom = false;
    }
  }


  void SoundLibraryXAudio2::Initialise(int _numChannels)
  {
    ASSERT_TEXT(_numChannels > 0, "SoundLibrary3d asked to create too few channels");

    // A NOMINAL rate, not a mix rate, and nothing is resampled to it. XAudio2
    // converts every voice to whatever the endpoint runs at, so the old
    // SoundMixFreq preference had nothing left to choose and T8 retired it.
    // What this number still does is size a block and give a voice its opening
    // format, and it is the highest rate any sample in the game uses so that a
    // block is never SHORTER than 50ms once ResetChannel adopts the sample's
    // own rate — a lower nominal would make the ring shrink in wall-clock
    // terms for 44kHz samples.
    m_sampleRate = NominalSampleRate;
    m_numChannels = std::min(_numChannels, GetMaxChannels());
    m_musicChannelId = -1;

    const int blockFrames = std::max(m_sampleRate / BlocksPerSecond, 1);

    // Storage first, and unconditionally: with no device the callbacks still run
    // (see TopUpChannel), so every sound still starts, ends and releases at the
    // rate it would have played at. A missing audio device makes the game
    // silent, not different.
    //
    // GetNumMainChannels(), NOT m_numChannels, and the difference is a bug this
    // deliberately does not inherit. SoundSystem sizes its own channel array
    // from GetNumMainChannels() — one FEWER than m_numChannels — and the main
    // callback indexes that array with the channel number it is handed. Both
    // legacy backends pump every channel up to m_numChannels, so the last one
    // reads a SoundInstanceId one element past the end of SoundSystem::m_channels
    // on every audio tick. Allocating exactly the set SoundSystem drives means
    // the callback can only ever be given an index that array holds.
    m_data->m_channels.resize(GetNumMainChannels());
    for (XAudio2Voice& channel : m_data->m_channels)
    {
      channel.m_blockFrames = blockFrames;
      channel.m_voiceChannels = 1;
      channel.m_blocks.assign(static_cast<size_t>(channel.BlockShorts()) * BlocksPerChannel, 0);
    }

    // The music voice is stereo and stays stereo, rather than being rebuilt to
    // match whatever track is playing. A source voice's channel count is fixed
    // at creation, so matching the sample would mean destroying and recreating
    // the voice on every track change — and the callback has to be able to fan
    // a mono sample out anyway, because every music file the game ships is
    // mono. Once it can do that, a permanently stereo voice costs one extra
    // block of memory and removes the whole recreate path.
    m_data->m_musicChannel.m_blockFrames = blockFrames;
    m_data->m_musicChannel.m_voiceChannels = MusicChannels;
    m_data->m_musicChannel.m_blocks.assign(static_cast<size_t>(m_data->m_musicChannel.BlockShorts()) * BlocksPerChannel, 0);

    //
    // Initialise COM. XAudio2Create needs it, and RPC_E_CHANGED_MODE means
    // somebody above us already picked an apartment — which is fine, COM is up
    // either way; we simply do not own it and must not tear it down.

    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    m_data->m_ownsCom = SUCCEEDED(comResult);

    //
    // Create the engine and the mastering voice. EVERY failure below leaves the
    // library in the silent state rather than asserting: an audio device that is
    // busy, absent or disappearing must never be able to stop the game, which is
    // exactly what the DirectSound backend's SOUNDASSERT did.

    HRESULT result = XAudio2Create(&m_data->m_engine, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(result))
    {
      DebugTrace("SOUND : XAudio2Create failed (0x{:08x}); continuing without audio\n", static_cast<unsigned long>(result));
      m_data->m_engine = nullptr;
      return;
    }

    // Device defaults throughout: XAudio2 resamples every voice to whatever the
    // endpoint runs at, so there is no mix rate for us to choose or for the
    // player to have to choose.
    result = m_data->m_engine->CreateMasteringVoice(&m_data->m_masteringVoice);
    if (FAILED(result))
    {
      DebugTrace("SOUND : CreateMasteringVoice failed (0x{:08x}); continuing without audio\n", static_cast<unsigned long>(result));
      m_data->m_masteringVoice = nullptr;
      m_data->m_engine->Release();
      m_data->m_engine = nullptr;
      return;
    }

    //
    // Set up X3DAudio against the endpoint we actually got. Both numbers come
    // from the mastering voice rather than being assumed: a 5.1 endpoint needs
    // six matrix coefficients per channel and a stereo one needs two.

    XAUDIO2_VOICE_DETAILS masteringDetails = {};
    m_data->m_masteringVoice->GetVoiceDetails(&masteringDetails);
    m_data->m_outputChannels = std::min<unsigned int>(masteringDetails.InputChannels, MaxOutputChannels);

    DWORD channelMask = 0;
    if (SUCCEEDED(m_data->m_masteringVoice->GetChannelMask(&channelMask)) && channelMask != 0)
    {
      if (SUCCEEDED(X3DAudioInitialize(channelMask, X3DAUDIO_SPEED_OF_SOUND, m_data->m_x3dInstance)))
        m_data->m_x3dReady = true;
    }

    if (!m_data->m_x3dReady)
      DebugTrace("SOUND : X3DAudio unavailable; channels will play unpositioned\n");

    // A listener that is valid before the camera has ever reported one. X3DAudio
    // requires OrientFront and OrientTop to be orthonormal and will not produce
    // a matrix if they are not, so it must never be left zeroed.
    m_data->m_listener.OrientFront = {0.0f, 0.0f, 1.0f};
    m_data->m_listener.OrientTop = {0.0f, 1.0f, 0.0f};

    //
    // One mono source voice per channel. The format is the mix rate only as a
    // starting point — ResetChannel replaces it with the sample's own rate the
    // first time a sound lands on the channel.

    auto createVoice = [&](XAudio2Voice& _channel)
    {
      WAVEFORMATEX format = {};
      format.wFormatTag = WAVE_FORMAT_PCM;
      format.nChannels = static_cast<WORD>(_channel.m_voiceChannels);
      format.nSamplesPerSec = static_cast<DWORD>(m_sampleRate);
      format.wBitsPerSample = 16;
      format.nBlockAlign = static_cast<WORD>(format.nChannels * format.wBitsPerSample / 8);
      format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
      format.cbSize = 0;

      // USEFILTER is what makes the resonant low pass free: it is XAudio2's own
      // per-voice biquad, so that effect needs no object in the chain at all.
      // It has to be asked for at creation or SetFilterParameters does nothing.
      const HRESULT voiceResult = m_data->m_engine->CreateSourceVoice(&_channel.m_voice, &format, XAUDIO2_VOICE_USEFILTER, MaxFrequencyRatio);
      if (FAILED(voiceResult))
      {
        DebugTrace("SOUND : CreateSourceVoice failed (0x{:08x}); that channel is silent\n", static_cast<unsigned long>(voiceResult));
        _channel.m_voice = nullptr;
        return;
      }

      _channel.m_sourceRate = static_cast<unsigned int>(m_sampleRate);
      _channel.m_voice->Start(0);
    };

    for (XAudio2Voice& channel : m_data->m_channels)
      createVoice(channel);
    createVoice(m_data->m_musicChannel);

    //
    // Music is never positioned and always full volume, exactly as it was set up
    // for DirectSound.

    SetChannel3DMode(m_musicChannelId, ModeMono);
    SetChannelVolume(m_musicChannelId, 10.0f);
    SetChannelFrequency(m_musicChannelId, 44100);
  }


  XAudio2Voice* SoundLibraryXAudio2::GetVoice(int _channel)
  {
    return const_cast<XAudio2Voice*>(static_cast<SoundLibraryXAudio2 const*>(this)->GetVoice(_channel));
  }


  XAudio2Voice const* SoundLibraryXAudio2::GetVoice(int _channel) const
  {
    if (!m_data)
      return nullptr;

    if (_channel == m_musicChannelId)
      return &m_data->m_musicChannel;

    if (_channel < 0 || _channel >= static_cast<int>(m_data->m_channels.size()))
      return nullptr;

    return &m_data->m_channels[_channel];
  }


  void SoundLibraryXAudio2::FillBlock(int _channel, signed short* _block)
  {
    XAudio2Voice* voice = GetVoice(_channel);
    if (!voice)
      return;

    const unsigned int numFrames = static_cast<unsigned int>(voice->m_blockFrames);
    const unsigned int numShorts = static_cast<unsigned int>(voice->BlockShorts());

    if (_channel == m_musicChannelId)
    {
      if (m_musicCallback)
        m_musicCallback(_block, numFrames, voice->m_voiceChannels, &voice->m_silenceRemaining);
      else
        WriteSilence(_block, numShorts);
    }
    else
    {
      if (m_mainCallback)
        m_mainCallback(static_cast<unsigned int>(_channel), _block, numFrames, &voice->m_silenceRemaining);
      else
        WriteSilence(_block, numShorts);
    }

    // The effects XAudio2 has no equivalent of, run over the block that has
    // just been filled — the same place, on the same 16-bit samples, at the
    // same point in the signal path as the DirectSound backend ran them in
    // PopulateBuffer. Everything else is in the voice's own effect chain.
    //
    // numShorts, not numFrames: these walk samples and know nothing about
    // interleaving. BitCrusher is memoryless and correct either way; Gargle
    // advances an LFO per sample, so on a stereo voice its rate would come out
    // doubled. Only music is stereo and no music blueprint asks for either
    // effect, so that is a property to know about rather than a bug to see —
    // and the place it would first appear is a modded Sounds.txt.
    for (std::unique_ptr<DspEffect> const& effect : voice->m_inFillEffects)
    {
      if (effect)
        effect->Process(_block, numShorts);
    }
  }


  void SoundLibraryXAudio2::TopUpChannel(int _channel)
  {
    XAudio2Voice* voice = GetVoice(_channel);
    if (!voice || voice->m_blocks.empty())
      return;

    // No device. Consume one block per call anyway — Advance runs at the tick
    // rate and a block IS one tick, so a sound still ends when it would have
    // ended and the SoundSystem's channel lifecycle is unchanged.
    if (!voice->m_voice)
    {
      FillBlock(_channel, voice->Block(voice->m_nextBlock));
      voice->m_nextBlock = (voice->m_nextBlock + 1) % BlocksPerChannel;
      return;
    }

    XAUDIO2_VOICE_STATE state = {};
    voice->m_voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);

    // Buffers are consumed in the order they were submitted, so anything not
    // still queued is a block the audio thread has finished with and this is
    // free to overwrite.
    for (int queued = static_cast<int>(state.BuffersQueued); queued < BlocksPerChannel; ++queued)
    {
      signed short* block = voice->Block(voice->m_nextBlock);
      FillBlock(_channel, block);

      XAUDIO2_BUFFER buffer = {};
      buffer.AudioBytes = static_cast<UINT32>(voice->BlockShorts() * sizeof(signed short));
      buffer.pAudioData = reinterpret_cast<BYTE const*>(block);
      voice->m_voice->SubmitSourceBuffer(&buffer);

      voice->m_nextBlock = (voice->m_nextBlock + 1) % BlocksPerChannel;
    }
  }


  void SoundLibraryXAudio2::Advance()
  {
    if (!m_data)
      return;

    // Positioning first, so a block filled below is mixed with the panning that
    // matches where the listener is this tick rather than the previous one.
    // Unlike the DirectSound backend there is no drift threshold deciding
    // whether a move is worth reporting: X3DAudioCalculate is arithmetic and
    // SetOutputMatrix is a write into voice state, so per-tick is both simpler
    // and smoother than moving a channel only once it has drifted far enough.
    START_PROFILE(g_profiler, "Position");
    for (int i = 0; i < static_cast<int>(m_data->m_channels.size()); ++i)
      UpdatePositioning(i);
    END_PROFILE(g_profiler, "Position");

    START_PROFILE(g_profiler, "FillBuf");
    for (int i = 0; i < static_cast<int>(m_data->m_channels.size()); ++i)
      TopUpChannel(i);

    TopUpChannel(m_musicChannelId);
    END_PROFILE(g_profiler, "FillBuf");
  }


  void SoundLibraryXAudio2::ResetChannel(int _channel)
  {
    XAudio2Voice* voice = GetVoice(_channel);
    if (!voice)
      return;

    if (voice->m_voice)
    {
      // Stop and flush first: SetSourceSampleRate is only legal on a voice with
      // nothing queued, and this is the one moment the game gives us — a channel
      // is reset immediately after a new sound is started on it.
      voice->m_voice->Stop(0);
      voice->m_voice->FlushSourceBuffers();

      const unsigned int wanted = static_cast<unsigned int>(
        std::clamp(voice->m_pendingRate, static_cast<int>(XAUDIO2_MIN_SAMPLE_RATE), static_cast<int>(XAUDIO2_MAX_SAMPLE_RATE)));
      if (voice->m_pendingRate > 0 && wanted != voice->m_sourceRate)
      {
        if (SUCCEEDED(voice->m_voice->SetSourceSampleRate(wanted)))
          voice->m_sourceRate = wanted;
      }

      // The new sound is asking to play at exactly its own rate, so the ratio
      // starts at one and every later SetChannelFrequency is a pitch multiplier
      // against it — which is what keeps the ratio inside MaxFrequencyRatio.
      // Doppler starts neutral too: the previous sound's motion is not this
      // one's.
      voice->m_pitchRatio = 1.0f;
      voice->m_doppler = 1.0f;
      voice->m_appliedRatio = 1.0f;
      voice->m_voice->SetFrequencyRatio(1.0f);

      voice->m_voice->Start(0);
    }

    voice->m_nextBlock = 0;
    voice->m_silenceRemaining = 0;

    TopUpChannel(_channel);
  }


  void SoundLibraryXAudio2::SetChannelFrequency(int _channel, int _frequency)
  {
    XAudio2Voice* voice = GetVoice(_channel);
    if (!voice || _frequency <= 0)
      return;

    // Recorded whether or not there is a voice, because the NEXT reset is what
    // adopts it as the source rate.
    voice->m_pendingRate = _frequency;

    if (!voice->m_voice || voice->m_sourceRate == 0)
      return;

    voice->m_pitchRatio = static_cast<float>(_frequency) / static_cast<float>(voice->m_sourceRate);
    ApplyFrequencyRatio(*voice);
  }


  void SoundLibraryXAudio2::SetChannelVolume(int _channel, float _volume)
  {
    XAudio2Voice* voice = GetVoice(_channel);
    if (!voice)
      return;

    if (NearlyEquals(_volume, voice->m_volume))
      return;

    DEBUG_ASSERT(_volume >= 0.0f && _volume <= 10.0f);
    voice->m_volume = _volume;

    // The curve is DirectSound's, unchanged: 0..10 maps onto -5000..0 hundredths
    // of a decibel, the master attenuation adds on top, and the result is
    // clamped before it becomes an amplitude.
    float attenuation = -(5.0f - _volume * 0.5f);
    attenuation *= 1000.0f;
    attenuation += static_cast<float>(m_masterVolume);
    attenuation = std::clamp(attenuation, -10000.0f, 0.0f);

    if (voice->m_voice)
      voice->m_voice->SetVolume(HundredthsOfDecibelToAmplitude(attenuation));
  }


  void SoundLibraryXAudio2::SetChannel3DMode(int _channel, int _mode)
  {
    XAudio2Voice* voice = GetVoice(_channel);
    if (voice)
      voice->m_3DMode = _mode;
  }


  void SoundLibraryXAudio2::SetChannelPosition(int _channel, DirectX::XMFLOAT3 const& _pos, DirectX::XMFLOAT3 const& _vel)
  {
    XAudio2Voice* voice = GetVoice(_channel);
    if (voice)
    {
      voice->m_pos = _pos;
      voice->m_vel = _vel;
    }
  }


  void SoundLibraryXAudio2::SetChannelMinDistance(int _channel, float _minDistance)
  {
    XAudio2Voice* voice = GetVoice(_channel);
    if (voice)
      voice->m_minDist = _minDistance;
  }


  void SoundLibraryXAudio2::SetListenerPosition(DirectX::XMFLOAT3 const& _pos, DirectX::XMFLOAT3 const& _front, DirectX::XMFLOAT3 const& _up,
                                                DirectX::XMFLOAT3 const& _vel)
  {
    m_listenerPos = _pos;

    m_data->m_listener.Position = ToX3DAudio(_pos);
    m_data->m_listener.Velocity = ToX3DAudio(_vel);

    // X3DAudio REQUIRES these two to be orthonormal — DS3D did not, and simply
    // took what the camera gave it. The camera's front and up are close enough
    // to be believed but not close enough to be trusted, so they are made
    // orthonormal here, in the same left-handed convention the software mixer
    // used for its own pan (right = up x front).
    DirectX::XMVECTOR front = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&_front));
    DirectX::XMVECTOR const up = DirectX::XMLoadFloat3(&_up);
    DirectX::XMVECTOR const right = DirectX::XMVector3Cross(up, front);

    // A camera looking straight along its own up vector makes that cross
    // product exactly zero, and XMVector3Normalize answers zero rather than
    // anything usable — the degenerate case that made the trees disappear in
    // directxmath-migration T17. Keep the last good orientation instead of
    // handing X3DAudio a basis it will reject.
    if (DirectX::XMVector3Equal(right, DirectX::XMVectorZero()) || DirectX::XMVector3Equal(front, DirectX::XMVectorZero()))
      return;

    DirectX::XMVECTOR const orthoRight = DirectX::XMVector3Normalize(right);
    DirectX::XMVECTOR const orthoUp = DirectX::XMVector3Cross(front, orthoRight);

    DirectX::XMFLOAT3 frontStore;
    DirectX::XMFLOAT3 upStore;
    DirectX::XMStoreFloat3(&frontStore, front);
    DirectX::XMStoreFloat3(&upStore, orthoUp);

    m_data->m_listener.OrientFront = ToX3DAudio(frontStore);
    m_data->m_listener.OrientTop = ToX3DAudio(upStore);
  }


  void SoundLibraryXAudio2::ApplyFrequencyRatio(XAudio2Voice& _voice)
  {
    if (!_voice.m_voice)
      return;

    const float ratio = std::clamp(_voice.m_pitchRatio * _voice.m_doppler, XAUDIO2_MIN_FREQ_RATIO, MaxFrequencyRatio);
    if (NearlyEquals(ratio, _voice.m_appliedRatio))
      return;

    _voice.m_voice->SetFrequencyRatio(ratio);
    _voice.m_appliedRatio = ratio;
  }


  void SoundLibraryXAudio2::UpdatePositioning(int _channel)
  {
    XAudio2Voice* voice = GetVoice(_channel);
    if (!voice || !voice->m_voice)
      return;

    const bool positioned = m_data->m_x3dReady && voice->m_3DMode == Mode3dPositioned && m_data->m_outputChannels > 0;

    if (!positioned)
    {
      // Put the panning back if this channel used to be positioned. All-ones is
      // what XAudio2 gives a mono source by default, so this restores the state
      // a freshly created voice is in rather than inventing one.
      if (voice->m_positionalMatrix)
      {
        for (unsigned int i = 0; i < m_data->m_outputChannels; ++i)
          m_data->m_matrix[i] = 1.0f;

        voice->m_voice->SetOutputMatrix(nullptr, 1, m_data->m_outputChannels, m_data->m_matrix);
        voice->m_positionalMatrix = false;
      }

      if (!NearlyEquals(voice->m_doppler, 1.0f))
      {
        voice->m_doppler = 1.0f;
        ApplyFrequencyRatio(*voice);
      }

      return;
    }

    X3DAUDIO_EMITTER emitter = {};
    emitter.ChannelCount = 1;
    // The distance at which the sound is at full volume and past which it starts
    // to fall away, which is exactly what DS3D's min distance meant. It must be
    // positive; the blueprints can and do leave it unset.
    emitter.CurveDistanceScaler = voice->m_minDist > 0.0f ? voice->m_minDist : 1.0f;
    emitter.DopplerScaler = DopplerScaler;
    emitter.Position = ToX3DAudio(voice->m_pos);
    emitter.Velocity = ToX3DAudio(voice->m_vel);
    emitter.OrientFront = {0.0f, 0.0f, 1.0f};
    emitter.OrientTop = {0.0f, 1.0f, 0.0f};

    X3DAUDIO_DSP_SETTINGS settings = {};
    settings.SrcChannelCount = 1;
    settings.DstChannelCount = m_data->m_outputChannels;
    settings.pMatrixCoefficients = m_data->m_matrix;

    X3DAudioCalculate(m_data->m_x3dInstance, &m_data->m_listener, &emitter, X3DAUDIO_CALCULATE_MATRIX | X3DAUDIO_CALCULATE_DOPPLER, &settings);

    voice->m_voice->SetOutputMatrix(nullptr, 1, m_data->m_outputChannels, m_data->m_matrix);
    voice->m_positionalMatrix = true;

    voice->m_doppler = settings.DopplerFactor > 0.0f ? settings.DopplerFactor : 1.0f;
    ApplyFrequencyRatio(*voice);
  }


  void SoundLibraryXAudio2::EnableDspFX(int _channel, int _numFilters, int const* _filterTypes)
  {
    ASSERT_TEXT(_numFilters > 0, "Bad argument passed to EnableFilters");

    XAudio2Voice* voice = GetVoice(_channel);
    if (!voice || !voice->m_voice)
      return;

    DisableDspFX(_channel);

    // Effects that XAudio2 implements go into the voice's chain; the two it has
    // no equivalent of are built here and run during the fill instead.
    std::vector<XAUDIO2_EFFECT_DESCRIPTOR> descriptors;

    for (int i = 0; i < _numFilters; ++i)
    {
      const int filterType = _filterTypes[i];
      if (filterType < 0 || filterType >= NUM_FILTERS)
        continue;

      IUnknown* effect = nullptr;

      switch (filterType)
      {
      case DSP_RESONANTLOWPASS:
        voice->m_voiceFilterActive = true;
        continue;

      case DSP_ECHO:
        if (FAILED(CreateFX(__uuidof(FXEcho), &effect)))
          effect = nullptr;
        break;

      case DSP_SIMPLE_REVERB:
      case DSP_I3DL2REVERB:
        if (FAILED(XAudio2CreateReverb(&effect)))
          effect = nullptr;
        break;

      case DSP_GARGLE:
        voice->m_inFillEffects[filterType] = std::make_unique<DspGargle>(m_sampleRate);
        continue;

      case DSP_BITCRUSHER:
        voice->m_inFillEffects[filterType] = std::make_unique<DspBitCrusher>(m_sampleRate);
        continue;

      default:
        // Unreachable while every enumerator above has a case, which is the
        // point of leaving it: a filter added to Effects.txt and the enum but
        // not to this switch plays dry rather than doing something undefined.
        continue;
      }

      if (!effect)
      {
        DebugTrace("SOUND : could not create effect {}; that channel plays dry\n", filterType);
        continue;
      }

      XAUDIO2_EFFECT_DESCRIPTOR descriptor = {};
      descriptor.pEffect = effect;
      descriptor.InitialState = TRUE;
      descriptor.OutputChannels = 1; // The voices are mono; the pan comes later, from the output matrix.

      voice->m_effectIndex[filterType] = static_cast<int>(descriptors.size());
      descriptors.push_back(descriptor);
    }

    if (!descriptors.empty())
    {
      XAUDIO2_EFFECT_CHAIN chain = {};
      chain.EffectCount = static_cast<UINT32>(descriptors.size());
      chain.pEffectDescriptors = descriptors.data();

      // Stopped and flushed around the change, which is the same moment the
      // DirectSound backend stopped and restarted its buffer to attach effects.
      // ResetChannel runs immediately after this and starts the voice again,
      // but starting it here too keeps the channel audible even if it does not.
      voice->m_voice->Stop(0);
      voice->m_voice->FlushSourceBuffers();
      if (FAILED(voice->m_voice->SetEffectChain(&chain)))
      {
        DebugTrace("SOUND : SetEffectChain failed; that channel plays dry\n");
        for (int& index : voice->m_effectIndex)
          index = -1;
      }
      voice->m_voice->Start(0);
    }

    // The chain holds its own reference to each effect, so ours is done. From
    // here on an effect is addressed by its index in the chain, never by
    // pointer, which is the only handle XAudio2 gives back.
    for (XAUDIO2_EFFECT_DESCRIPTOR const& descriptor : descriptors)
      descriptor.pEffect->Release();
  }


  void SoundLibraryXAudio2::UpdateDspFX(int _channel, int _filterType, int _numParams, float const* _params)
  {
    XAudio2Voice* voice = GetVoice(_channel);
    if (!voice || !voice->m_voice || !_params)
      return;

    if (_filterType < 0 || _filterType >= NUM_FILTERS)
      return;

    // The two that never entered the chain: same objects, same parameter
    // layout, same arithmetic as every other backend has always run.
    if (voice->m_inFillEffects[_filterType])
    {
      voice->m_inFillEffects[_filterType]->SetParameters(_params);
      return;
    }

    if (_filterType == DSP_RESONANTLOWPASS)
    {
      if (!voice->m_voiceFilterActive || voice->m_sourceRate == 0)
        return;

      // The same two remappings DspResLowPass applies to its raw parameters,
      // reproduced rather than replaced: the blueprint's "cutoff" is not a
      // frequency until it has been through this, so dropping it would retune
      // every sound that uses the filter.
      const float cutoff = std::exp(_params[0] / 3850.0f + 4.7f);
      const float resonance = std::exp(_params[1] / 4.0f) - 1.0f;

      XAUDIO2_FILTER_PARAMETERS filterParams = {};
      filterParams.Type = LowPassFilter;
      filterParams.Frequency = XAudio2CutoffFrequencyToRadians(cutoff, voice->m_sourceRate);
      filterParams.OneOverQ = 1.0f / std::max(resonance, 0.1f);
      voice->m_voice->SetFilterParameters(&filterParams);
      return;
    }

    const int index = voice->m_effectIndex[_filterType];
    if (index < 0)
      return;

    switch (_filterType)
    {
    case DSP_ECHO:
    {
      // WetDryMix 0..100, Delay in ms, Attenuation 0..100 — DspEcho's layout,
      // and now the only echo layout. T4 rewrote the six DirectSound echo
      // usages into this one: their RightDelay and PanDelay were already being
      // ignored here, because the voices are mono and two delays are one.
      FXECHO_PARAMETERS echo = {};
      echo.WetDryMix = std::clamp(_params[0] * 0.01f, FXECHO_MIN_WETDRYMIX, FXECHO_MAX_WETDRYMIX);
      echo.Delay = std::clamp(_params[1], FXECHO_MIN_DELAY, FXECHO_MAX_DELAY);
      echo.Feedback = std::clamp(_params[2] * 0.01f, FXECHO_MIN_FEEDBACK, FXECHO_MAX_FEEDBACK);

      voice->m_voice->SetEffectParameters(static_cast<UINT32>(index), &echo, sizeof(echo));
      break;
    }

    case DSP_SIMPLE_REVERB:
    case DSP_I3DL2REVERB:
    {
      // I3DL2 is the interchange format: XAudio2 converts it to its own native
      // parameters, and the game's I3DL2 block is field for field the same
      // standard. SimpleReverb carries only a mix and takes the defaults below
      // for everything else. Those are the I3DL2 default preset, and two of
      // them are deliberately finer than Effects.txt can express: the file
      // writes {:8.2f}, so it rounds 0.007 and 0.011 to 0.01. A block that
      // states its own delays overrides these; SimpleReverb gets the exact ones.
      XAUDIO2FX_REVERB_I3DL2_PARAMETERS i3dl2 = {};
      i3dl2.WetDryMix = 100.0f;
      i3dl2.Room = -1000;
      i3dl2.RoomHF = -100;
      i3dl2.RoomRolloffFactor = 0.0f;
      i3dl2.DecayTime = 1.49f;
      i3dl2.DecayHFRatio = 0.83f;
      i3dl2.Reflections = -2602;
      i3dl2.ReflectionsDelay = 0.007f;
      i3dl2.Reverb = 200;
      i3dl2.ReverbDelay = 0.011f;
      i3dl2.Diffusion = 100.0f;
      i3dl2.Density = 100.0f;
      i3dl2.HFReference = 5000.0f;

      if (_filterType == DSP_I3DL2REVERB && _numParams >= 13)
      {
        // WetDryMix leads, then the twelve standard fields in I3DL2 order.
        // DirectSound's Quality knob used to trail them; XAudio2's reverb has
        // no equivalent, so T4 dropped it from the data rather than faking one.
        i3dl2.WetDryMix = std::clamp(_params[0], 0.0f, 100.0f);
        i3dl2.Room = static_cast<INT32>(_params[1]);
        i3dl2.RoomHF = static_cast<INT32>(_params[2]);
        i3dl2.RoomRolloffFactor = _params[3];
        i3dl2.DecayTime = _params[4];
        i3dl2.DecayHFRatio = _params[5];
        i3dl2.Reflections = static_cast<INT32>(_params[6]);
        i3dl2.ReflectionsDelay = _params[7];
        i3dl2.Reverb = static_cast<INT32>(_params[8]);
        i3dl2.ReverbDelay = _params[9];
        i3dl2.Diffusion = _params[10];
        i3dl2.Density = _params[11];
        i3dl2.HFReference = _params[12];
      }
      else if (_filterType == DSP_SIMPLE_REVERB && _numParams >= 1)
      {
        i3dl2.WetDryMix = std::clamp(_params[0], 0.0f, 100.0f);
      }

      XAUDIO2FX_REVERB_PARAMETERS native = {};
      ReverbConvertI3DL2ToNative(&i3dl2, &native);
      voice->m_voice->SetEffectParameters(static_cast<UINT32>(index), &native, sizeof(native));
      break;
    }

    default:
      break;
    }
  }


  void SoundLibraryXAudio2::DisableDspFX(int _channel)
  {
    XAudio2Voice* voice = GetVoice(_channel);
    if (!voice)
      return;

    for (std::unique_ptr<DspEffect>& effect : voice->m_inFillEffects)
      effect.reset();

    if (!voice->m_voice)
      return;

    bool hadChain = false;
    for (int& index : voice->m_effectIndex)
    {
      hadChain = hadChain || index >= 0;
      index = -1;
    }

    if (hadChain)
    {
      voice->m_voice->Stop(0);
      voice->m_voice->FlushSourceBuffers();
      voice->m_voice->SetEffectChain(nullptr);
      voice->m_voice->Start(0);
    }

    if (voice->m_voiceFilterActive)
    {
      // Back to the state a freshly created voice is in: a low pass at the
      // maximum frequency, which passes everything.
      XAUDIO2_FILTER_PARAMETERS filterParams = {};
      filterParams.Type = LowPassFilter;
      filterParams.Frequency = XAUDIO2_MAX_FILTER_FREQUENCY;
      filterParams.OneOverQ = 1.0f;
      voice->m_voice->SetFilterParameters(&filterParams);
      voice->m_voiceFilterActive = false;
    }
  }


  int SoundLibraryXAudio2::GetMaxChannels() { return 64; }


  float SoundLibraryXAudio2::GetChannelHealth(int _channel)
  {
    XAudio2Voice const* voice = GetVoice(_channel);
    if (!voice || !voice->m_voice)
      return 1.0f;

    XAUDIO2_VOICE_STATE state = {};
    voice->m_voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);

    return std::clamp(static_cast<float>(state.BuffersQueued) / static_cast<float>(BlocksPerChannel), 0.0f, 1.0f);
  }


  // FRAMES, and it has to be: the callbacks compare it against a frame count
  // when they work out how much trailing silence a finished sound still owes.
  int SoundLibraryXAudio2::GetChannelBufSize(int _channel) const
  {
    XAudio2Voice const* voice = GetVoice(_channel);
    if (!voice)
      return 0;

    return voice->m_blockFrames * BlocksPerChannel;
  }
} // namespace Neuron
