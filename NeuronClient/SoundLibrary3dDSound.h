#pragma once


#include "SoundLibrary3d.h"


struct IDirectSoundBuffer;


//*****************************************************************************
// Class SoundLibrary3dDirectSound
//*****************************************************************************


namespace Neuron
{
  class DirectSoundChannel;
  class DirectSoundData;

  class SoundLibrary3dDirectSound : public SoundLibrary3d
  {
    protected:
      DirectSoundChannel* m_channels;
      DirectSoundChannel* m_musicChannel;
      DirectSoundData* m_directSound;

    protected:
      IDirectSoundBuffer* CreateSecondaryBuffer(int _numSamples);
      void RefreshCapabilities();
      long CalcWrappedDelta(long a, long b, unsigned long bufferSize);
      void PopulateBuffer(int _channel, int _fromSample, int _numSamples, bool _isMusic);
      void CommitChanges(); // Commits all pos/or/vel etc changes
      void AdvanceChannel(int _channel, int _frameNum);
      int GetNumFilters(int _channel);
      void Verify();

    public:
      SoundLibrary3dDirectSound();
      ~SoundLibrary3dDirectSound();

      void Initialise(int _mixFreq, int _numChannels, bool hw3d, int _mainBufNumSamples, int _musicBufNumSamples);

      bool Hardware3DSupport();
      int GetMaxChannels();
      int GetCPUOverhead();
      float GetChannelHealth(int _channel); // 0.0 = BAD, 1.0 = GOOD
      int GetChannelBufSize(int _channel) const;

      void ResetChannel(int _channel); // Refills entire channel with data immediately

      void SetChannel3DMode(int _channel, int _mode); // 0 = 3d, 1 = head relative, 2 = disabled
      void SetChannelPosition(int _channel, DirectX::XMFLOAT3 const& _pos, DirectX::XMFLOAT3 const& _vel);
      void SetChannelFrequency(int _channel, int _frequency);
      void SetChannelMinDistance(int _channel, float _minDistance);
      void SetChannelVolume(int _channel, float _volume); // logarithmic, 0.0 - 10.0, 0=practially silent

      void EnableDspFX(int _channel, int _numFilters, int const* _filterTypes);
      void UpdateDspFX(int _channel, int _filterType, int _numParams, float const* _params);
      void DisableDspFX(int _channel);

      void SetListenerPosition(DirectX::XMFLOAT3 const& _pos, DirectX::XMFLOAT3 const& _front, DirectX::XMFLOAT3 const& _up,
                               DirectX::XMFLOAT3 const& _vel);

      void Advance();
  };
} // namespace Neuron
