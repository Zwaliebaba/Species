#include "pch.h"

#include <xaudio2.h>
#include <x3daudio.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "Debug.h"
#include "MathUtils.h"
#include "Profiler.h"

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
      int m_blockSamples = 0;
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

      DirectX::XMFLOAT3 m_pos{0.0f, 0.0f, 0.0f};
      DirectX::XMFLOAT3 m_vel{0.0f, 0.0f, 0.0f};

      signed short* Block(int _index) { return m_blocks.data() + static_cast<size_t>(_index) * m_blockSamples; }
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


  void SoundLibraryXAudio2::Initialise(int _mixFreq, int _numChannels, bool _hw3d, int _mainBufNumSamples, int _musicBufNumSamples)
  {
    ASSERT_TEXT(_numChannels > 0, "SoundLibrary3d asked to create too few channels");

    m_sampleRate = _mixFreq;
    m_hw3dDesired = _hw3d;
    m_numChannels = std::min(_numChannels, GetMaxChannels());
    m_musicChannelId = -1;

    // NOT derived from _mainBufNumSamples or _musicBufNumSamples. Those are the
    // DirectSound ring sizes — 20,000 and 200,000 samples — and reproducing them
    // would reproduce the latency they cost. Both are ignored deliberately.
    const int blockSamples = std::max(m_sampleRate / BlocksPerSecond, 1);

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
      channel.m_blockSamples = blockSamples;
      channel.m_blocks.assign(static_cast<size_t>(blockSamples) * BlocksPerChannel, 0);
    }
    m_data->m_musicChannel.m_blockSamples = blockSamples;
    m_data->m_musicChannel.m_blocks.assign(static_cast<size_t>(blockSamples) * BlocksPerChannel, 0);

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

    WAVEFORMATEX format = {};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 1;
    format.nSamplesPerSec = static_cast<DWORD>(m_sampleRate);
    format.wBitsPerSample = 16;
    format.nBlockAlign = static_cast<WORD>(format.nChannels * format.wBitsPerSample / 8);
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
    format.cbSize = 0;

    auto createVoice = [&](XAudio2Voice& _channel)
    {
      const HRESULT voiceResult = m_data->m_engine->CreateSourceVoice(&_channel.m_voice, &format, 0, MaxFrequencyRatio);
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

    const unsigned int numSamples = static_cast<unsigned int>(voice->m_blockSamples);

    if (_channel == m_musicChannelId)
    {
      if (m_musicCallback)
        m_musicCallback(_block, numSamples, &voice->m_silenceRemaining);
      else
        WriteSilence(_block, numSamples);
    }
    else
    {
      if (m_mainCallback)
        m_mainCallback(static_cast<unsigned int>(_channel), _block, numSamples, &voice->m_silenceRemaining);
      else
        WriteSilence(_block, numSamples);
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
      buffer.AudioBytes = static_cast<UINT32>(voice->m_blockSamples * sizeof(signed short));
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
    // sound-xaudio2 T3 gives these bodies: the built-in per-voice filter for the
    // resonant low pass, stock XAPOs for echo and reverb, and one adapter
    // hosting the existing DspEffect classes. Until then a channel with effects
    // plays dry rather than not playing.
  }


  void SoundLibraryXAudio2::UpdateDspFX(int _channel, int _filterType, int _numParams, float const* _params) {}


  void SoundLibraryXAudio2::DisableDspFX(int _channel) {}


  bool SoundLibraryXAudio2::Hardware3DSupport() { return false; }


  int SoundLibraryXAudio2::GetMaxChannels() { return 64; }


  int SoundLibraryXAudio2::GetCPUOverhead() { return 0; }


  float SoundLibraryXAudio2::GetChannelHealth(int _channel)
  {
    XAudio2Voice const* voice = GetVoice(_channel);
    if (!voice || !voice->m_voice)
      return 1.0f;

    XAUDIO2_VOICE_STATE state = {};
    voice->m_voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);

    return std::clamp(static_cast<float>(state.BuffersQueued) / static_cast<float>(BlocksPerChannel), 0.0f, 1.0f);
  }


  int SoundLibraryXAudio2::GetChannelBufSize(int _channel) const
  {
    XAudio2Voice const* voice = GetVoice(_channel);
    if (!voice)
      return 0;

    return voice->m_blockSamples * BlocksPerChannel;
  }
} // namespace Neuron
