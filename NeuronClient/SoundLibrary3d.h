#pragma once


//*****************************************************************************
// This module provides a 3D sound stage with dynamic filtering on each channel
// separately. Typically around 32 channels can be played simultaneously.
// These channels are positioned in the 3D sound
// stage by means of distance attenuation, panning, low-pass filtering and
// reverb. Some of these effects may not be available on low end hardware.
// One implementation remains: SoundLibraryXAudio2. The DirectSound wrapper and
// the software mixer that fed a SoundLibrary2D output layer were deleted by
// sound-xaudio2 T7 and T8. The interface is still virtual because the seam is
// what let the replacement be A/B'd against them.
//*****************************************************************************


#include "NeuronMath.h"


class Profiler;


//*****************************************************************************
// Class SoundLibrary3d
//*****************************************************************************


namespace Neuron
{
  class SoundLibrary3d
  {
    protected:
      int m_numChannels; // Total number of channels including the music channel
      int m_sampleRate;
      int m_masterVolume;
      DirectX::XMFLOAT3 m_listenerPos{0.0f, 0.0f, 0.0f}; // Records the most recent value passed into SetListenerPos

      // This callback is called whenever SoundLibrary3d needs some more sound data for a certain channel.
      // The return value is true if some audio was written, or false if silence was written
      //
      // Both count FRAMES. The main callback has no channel count because an
      // effect channel is mono and nothing may change that: the voices are
      // positioned individually, and X3DAudio pans a mono source. The music
      // callback is handed the width of the music voice, and must write that
      // many interleaved channels per frame whatever the sample it is reading
      // from happens to hold.
      bool (*m_mainCallback)(unsigned int, signed short*, unsigned int, int*);
      bool (*m_musicCallback)(signed short*, unsigned int, int, int*);

    public:
      // THIS ENUM IS GameData/Effects.txt, POSITION FOR POSITION.
      // SoundSystem::LoadEffects appends one blueprint per EFFECT block, so an
      // effect's index is its line position in that file and the two must be
      // edited together. Nothing checks the alignment at build or load time; a
      // mismatch reads the wrong blueprint and the game still runs.
      //
      // The nine DirectX 8 media objects that used to sit above these went with
      // sound-xaudio2 T4. Seven had no usage in the data at all. I3DL2Reverb
      // survives with its prefix dropped, because I3DL2 is not a DirectSound
      // legacy — it is the interchange format XAUDIO2FX speaks natively, which
      // is what ReverbConvertI3DL2ToNative converts from.
      enum
      {
        DSP_RESONANTLOWPASS, // 0
        DSP_BITCRUSHER,
        DSP_GARGLE, // 2
        DSP_ECHO,
        DSP_SIMPLE_REVERB, // 4
        DSP_I3DL2REVERB,
        NUM_FILTERS
      };

      enum
      {
        Mode3dPositioned,
        ModeMono
      };

      int m_musicChannelId;
      Profiler* m_profiler;

    public:
      SoundLibrary3d();
      virtual ~SoundLibrary3d();

      // The hardware-3D flag that used to sit between _numChannels and the
      // buffer sizes is gone with DirectSound. It selected DSBCAPS_LOCHARDWARE,
      // which no machine has granted since Vista made DirectSound a software
      // emulation over WASAPI, so the capability it asked about always answered
      // no. XAudio2 has no equivalent question.
      virtual void Initialise(int _numChannels) = 0;
      void SetMainCallback(bool (*_callback)(unsigned int, signed short*, unsigned int, int*));
      void SetMusicCallback(bool (*_callback)(signed short*, unsigned int, int, int*));
      void SetMasterVolume(int _volume);

      virtual int GetMaxChannels() = 0;
      virtual float GetChannelHealth(int _channel) = 0; // 0.0 = BAD, 1.0 = GOOD
      int GetSampleRate() { return m_sampleRate; }
      virtual int GetChannelBufSize(int _channel) const = 0;
      int GetNumMainChannels() const { return m_numChannels - 1; }

      virtual void ResetChannel(int _channel) = 0; // Refills entire channel with data immediately

      virtual void SetChannel3DMode(int _channel, int _mode) = 0;
      virtual void SetChannelPosition(int _channel, DirectX::XMFLOAT3 const& _pos, DirectX::XMFLOAT3 const& _vel) = 0;
      virtual void SetChannelFrequency(int _channel, int _frequency) = 0;
      virtual void SetChannelMinDistance(int _channel, float _minDistance) = 0;
      virtual void SetChannelVolume(int _channel, float _volume) = 0; // logarithmic, 0.0f - 10.0f

      virtual void EnableDspFX(int _channel, int _numFilters, int const* _filterTypes) = 0;
      virtual void UpdateDspFX(int _channel, int _filterType, int _numParams, float const* _params) = 0;
      virtual void DisableDspFX(int _channel) = 0;

      virtual void SetListenerPosition(DirectX::XMFLOAT3 const& _pos, DirectX::XMFLOAT3 const& _front, DirectX::XMFLOAT3 const& _up,
                                       DirectX::XMFLOAT3 const& _vel) = 0;

      virtual void Advance() = 0;

      // Whether there is an audio device to play through RIGHT NOW -- a LEVEL,
      // not the edge of losing one. False covers three cases that a caller
      // cannot tell apart and should not have to: the machine has no audio
      // hardware, opening it failed, or it was working and has gone.
      //
      // The distinction matters to SoundSystem::Advance, which retries
      // RestartSoundLibrary while this is false but only once it has seen it
      // true. Asking "did you lose a device" instead would stop the retries
      // dead: a rebuild that also fails produces a backend that has lost
      // nothing, and a device unplugged and plugged back in would never return.
      virtual bool HasOutputDevice() const = 0;

      void WriteSilence(signed short* _data, unsigned int _numSamples);
  };


  extern SoundLibrary3d* g_soundLibrary3d;
} // namespace Neuron
