#include "pch.h"

#include <MMSYSTEM.H>

#include "Debug.h"
#include "Preferences.h"
#include "SystemInfo.h"

#include "SoundLibrary2d.h"

// Moved up from below the two definitions by namespace-migration T2: an
// #include cannot sit inside the namespace wrapper without pulling those
// headers into namespace Neuron along with it. Neither header depends on
// anything declared between here and where they used to be.
#include "HiResTime.h"
#include "SoundSystem.h"


namespace Neuron
{
  static HWAVEOUT s_device;
  SoundLibrary2d* g_soundLibrary2d = nullptr;

  //*****************************************************************************
  // Class SoundLib2dBuf
  //*****************************************************************************

  SoundLib2dBuf::SoundLib2dBuf()
  {
    // Allocate the buffer
    unsigned int numSamples = g_prefsManager->GetInt("SoundBufferSize", 2000);
    m_buffer = new StereoSample[numSamples];

    // Clear the buffer
    memset(m_buffer, 0, numSamples * sizeof(StereoSample));

    // Register the buffer with Windows
    memset(&m_header, 0, sizeof(WAVEHDR));
    m_header.lpData = (char*)m_buffer;
    int blockAlign = 4; // 2 channels * 2 bytes per sample
    m_header.dwBufferLength = numSamples * blockAlign;
    m_header.dwFlags = WHDR_DONE;
    int result = waveOutPrepareHeader(s_device, &m_header, sizeof(WAVEHDR));
    ASSERT_TEXT(result == MMSYSERR_NOERROR, "Couldn't init buffer");

    // Play the buffer
    static int count = 0;
    if (count < 4)
    {
      count++;
      result = waveOutWrite(s_device, &m_header, sizeof(WAVEHDR));
      ASSERT_TEXT(result == MMSYSERR_NOERROR, "Couldn't send sound data");
    }
  }

  SoundLib2dBuf::~SoundLib2dBuf()
  {
    waveOutUnprepareHeader(s_device, &m_header, sizeof(WAVEHDR));
    delete[] m_buffer;
    m_buffer = nullptr;
  }

  //*****************************************************************************
  // Class SoundLibrary2d
  //*****************************************************************************

  void CALLBACK WaveOutProc(HWAVEOUT _dev, UINT _msg, DWORD _userData, DWORD _param1, DWORD _param2)
  {
    if (_msg != WOM_DONE)
      return;
    if (!s_device)
      return;
    if (!g_soundLibrary2d || !g_soundLibrary2d->m_callback)
      return;

    g_soundLibrary2d->m_fillsRequested++;
  }

  SoundLibrary2d::SoundLibrary2d()
    : m_wavOutput(nullptr),
      m_numBuffers(10),
      m_nextBuffer(0),
      m_fillsRequested(0),
      m_callback(nullptr)
  {
    ASSERT_TEXT(!g_soundLibrary2d, "SoundLibrary2d already exists");

    m_freq = g_prefsManager->GetInt("SoundMixFreq", 44100);
    m_samplesPerBuffer = g_prefsManager->GetInt("SoundBufferSize", 2000);

    //
    // Initialise the output device

    WAVEFORMATEX format = {};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 2;
    format.nSamplesPerSec = m_freq;
    format.wBitsPerSample = 16;
    format.nBlockAlign = 4; // 2 channels * 2 bytes per sample
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
    int result = waveOutOpen(&s_device, WAVE_MAPPER, &format, (DWORD_PTR)&WaveOutProc, 0, CALLBACK_FUNCTION);
    const char* errString = nullptr;
    switch (result)
    {
    case MMSYSERR_ALLOCATED:
      errString = "Specified resource is already allocated";
      break;
    case MMSYSERR_BADDEVICEID:
      errString = "Specified device ID is out of range";
      break;
    case MMSYSERR_NODRIVER:
      errString = "No device driver is present";
      break;
    case MMSYSERR_NOMEM:
      errString = "Unable to allocate or lock memory";
      break;
    case WAVERR_BADFORMAT:
      errString = "Attempted to open with an unsupported waveform-audio format";
      break;
    case WAVERR_SYNC:
      errString = "Device is synchronous but waveOutOpen called without WAVE_ALLOWSYNC flag";
      break;
    }
    ASSERT_TEXT(result == MMSYSERR_NOERROR, "Failed to open audio output device: \"{}\"", errString);

    //
    // Create the sound buffers

    m_buffers = new SoundLib2dBuf[m_numBuffers];
  }

  SoundLibrary2d::~SoundLibrary2d()
  {
    waveOutReset(s_device);
    delete[] m_buffers;
    m_buffers = nullptr;
    waveOutClose(s_device);
    s_device = nullptr;
    g_soundLibrary2d = nullptr;
  }

  void SoundLibrary2d::SetCallback(void (*_callback)(StereoSample*, unsigned int)) { m_callback = _callback; }

  void SoundLibrary2d::TopupBuffer()
  {
    if (m_wavOutput)
    {
      static double nextOutputTime = -1.0;
      if (nextOutputTime < 0.0)
        nextOutputTime = GetHighResTime();

      if (GetHighResTime() > nextOutputTime)
      {
        StereoSample buf[5000];
        int samplesPerSecond = g_prefsManager->GetInt("SoundMixFreq", 44100);
        int samplesPerUpdate = static_cast<int>((double)samplesPerSecond / 20.0);
        g_soundLibrary2d->m_callback(buf, samplesPerUpdate);
        fwrite(buf, samplesPerUpdate, sizeof(StereoSample), m_wavOutput);
        nextOutputTime += 1.0 / 20.0;
      }
    }
    else
    {
      while (m_fillsRequested)
      {
        SoundLib2dBuf* buf = &m_buffers[m_nextBuffer];

        g_soundLibrary2d->m_callback(buf->m_buffer, g_soundLibrary2d->m_samplesPerBuffer);

        int result = waveOutWrite(s_device, &buf->m_header, sizeof(WAVEHDR));
        if (result != MMSYSERR_NOERROR)
          break;
        m_nextBuffer++;
        m_nextBuffer %= m_numBuffers;
        m_fillsRequested--;
      }
    }
  }

  void SoundLibrary2d::Stop()
  {
    //	#warning "WARNING: Write me, called in Finalise()"
    // this is suppose to stop the callback from being invoked.
    // (important on mac and linux where the sound is in a separate thread).
  }
} // namespace Neuron
