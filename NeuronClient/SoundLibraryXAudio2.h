#pragma once

#include <memory>

#include "SoundLibrary3d.h"

//*****************************************************************************
// Class SoundLibraryXAudio2
//*****************************************************************************
//
// The native backend. One mono source voice per game channel plus one for
// music, all feeding the mastering voice; XAudio2 owns the mixing, the
// resampling and the device.
//
// What it does NOT do, and deliberately: chase a play cursor. The DirectSound
// backend it replaces kept one looping secondary buffer per channel and worked
// out where the hardware had got to, simulating the cursor on alternate frames
// because asking was slow. XAudio2 consumes whole buffers and reports how many
// are still queued, so the same job is "top the queue back up", which is what
// Advance does.
//
// Everything above SoundLibrary3d is unchanged by this: the callbacks that fill
// a block are the SoundSystem ones, called on the main thread from Advance at
// the SoundSystem tick, so no audio work moves onto another thread.

namespace Neuron
{
  // Defined in the .cpp, so xaudio2.h stays out of every file that includes
  // this header. Same shape as the DirectSoundData it replaces.
  class XAudio2Data;

  class SoundLibraryXAudio2 : public SoundLibrary3d
  {
    private:
      std::unique_ptr<XAudio2Data> m_data;

      // Both return nullptr for a channel index this library does not have,
      // which is what makes every entry point below safe to call before
      // Initialise and after a failed device open.
      class XAudio2Voice* GetVoice(int _channel);
      class XAudio2Voice const* GetVoice(int _channel) const;

      // Fills every block a voice is not currently playing, submitting each in
      // turn. With no device it consumes exactly one block per call so that
      // sounds still finish at the rate they would have played at.
      void TopUpChannel(int _channel);
      void FillBlock(int _channel, signed short* _block);

      void Shutdown();

    public:
      SoundLibraryXAudio2();
      ~SoundLibraryXAudio2() override;

      void Initialise(int _mixFreq, int _numChannels, bool _hw3d, int _mainBufNumSamples, int _musicBufNumSamples) override;

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

      void EnableDspFX(int _channel, int _numFilters, int const* _filterTypes) override;
      void UpdateDspFX(int _channel, int _filterType, int _numParams, float const* _params) override;
      void DisableDspFX(int _channel) override;

      void SetListenerPosition(DirectX::XMFLOAT3 const& _pos, DirectX::XMFLOAT3 const& _front, DirectX::XMFLOAT3 const& _up,
                               DirectX::XMFLOAT3 const& _vel) override;

      void Advance() override;
  };
} // namespace Neuron
