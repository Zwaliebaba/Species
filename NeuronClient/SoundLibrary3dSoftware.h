#pragma once

#include "SoundLibrary3d.h"

class Profiler;

//*****************************************************************************
// Class SoundLibrary3dSoftware
//*****************************************************************************


namespace Neuron
{
  class SoftwareChannel;
  class StereoSample;

  class SoundLibrary3dSoftware : public SoundLibrary3d
  {
    protected:
      SoftwareChannel* m_channels;
      float* m_left; // Temp buffers used by mixer
      float* m_right;

      DirectX::XMFLOAT3 m_listenerFront{0.0f, 0.0f, 0.0f};
      DirectX::XMFLOAT3 m_listenerUp{0.0f, 0.0f, 0.0f};
      DirectX::XMFLOAT3 m_listenerRight{0.0f, 0.0f, 0.0f};

      void GetChannelData(float _duration);
      void ApplyDspFX(float _duration);
      void MixSameFreqFixedVol(signed short* in, unsigned int numSamples, float volL, float volR);
      void MixDiffFreqFixedVol(signed short* in, unsigned int numSamples, float volL, float volR, float relFreq);
      void MixSameFreqRampVol(signed short* in, unsigned int num, float volL1, float volR1, float volL2, float volR2);
      void MixDiffFreqRampVol(signed short* in, unsigned int num, float volL1, float volR1, float volL2, float volR2, float relFreq);
      void CalcChannelVolumes(int _channelIndex, float* _left, float* _right);

    public:
      SoundLibrary3dSoftware();
      ~SoundLibrary3dSoftware() override;

      void Initialise(int _mixFreq, int _numChannels, bool hw3d, int _mainBufNumSamples, int _musicBufNumSamples) override;

      void Advance() override;
      void Callback(StereoSample* _buf, unsigned int _numSamples); // Called from SoundLibrary2d

      bool Hardware3DSupport() override;
      int GetMaxChannels() override;
      int GetCPUOverhead() override;
      float GetChannelHealth(int _channel) override; // 0.0 = BAD, 1.0 = GOOD
      int GetChannelBufSize(int _channel) const override;

      void ResetChannel(int _channel) override; // Refills entire channel with data immediately

      void SetChannel3DMode(int _channel, int _mode) override;
      void SetChannelPosition(int _channel, DirectX::XMFLOAT3 const& _pos, DirectX::XMFLOAT3 const& _vel) override;
      void SetChannelFrequency(int _channel, int _frequency) override;
      void SetChannelMinDistance(int _channel, float _minDistance) override;
      void SetChannelVolume(int _channel, float _volume) override; // logarithmic, 0.0f - 10.0f

      void EnableDspFX(int _channel, int _numFilters, const int* _filterTypes) override;
      void UpdateDspFX(int _channel, int _filterType, int _numParams, const float* _params) override;
      void DisableDspFX(int _channel) override;

      void SetListenerPosition(DirectX::XMFLOAT3 const& _pos, DirectX::XMFLOAT3 const& _front, DirectX::XMFLOAT3 const& _up,
                               DirectX::XMFLOAT3 const& _vel) override;
  };
} // namespace Neuron
