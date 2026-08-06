#include "pch.h"

#include <stdlib.h>
#include <string.h>

#include "BinaryStreamReaders.h"
#include "Debug.h"
#include "StringUtils.h"

#include "SoundStreamDecoder.h"


namespace Neuron
{
  SoundStreamDecoder::SoundStreamDecoder(BinaryReader* _in)
    : m_in(_in),
      m_bits(0),
      m_fileType(TypeUnknown),
      m_numChannels(0),
      m_freq(0),
      m_numSamples(0)
  {
    std::string_view const fileType = _in->GetFileType();
    if (StrEqualsIgnoreCase(fileType, "wav"))
    {
      m_fileType = TypeWav;
      ReadWavHeader();
    }
    else
      ASSERT_TEXT(0, "Unknown sound file format {}", m_in->m_filename);
  }

  SoundStreamDecoder::~SoundStreamDecoder() { delete m_in; }

  void SoundStreamDecoder::ReadWavHeader()
  {
    char buffer[25];
    int chunkLength;

    // Check RIFF header
    m_in->ReadBytes(12, (unsigned char*)buffer);
    if (memcmp(buffer, "RIFF", 4) || memcmp(buffer + 8, "WAVE", 4))
      return;

    while (!m_in->m_eof)
    {
      if (m_in->ReadBytes(4, (unsigned char*)buffer) != 4)
        break;

      chunkLength = m_in->ReadS32(); // read chunk length

      if (memcmp(buffer, "fmt ", 4) == 0)
      {
        int i = m_in->ReadS16(); // should be 1 for PCM data
        chunkLength -= 2;
        if (i != 1)
          return;

        m_numChannels = m_in->ReadS16(); // mono or stereo data
        chunkLength -= 2;
        if ((m_numChannels != 1) && (m_numChannels != 2))
          return;

        m_freq = m_in->ReadS32(); // sample frequency
        chunkLength -= 4;

        m_in->ReadS32(); // skip six bytes
        m_in->ReadS16();
        chunkLength -= 6;

        m_bits = m_in->ReadS16(); // 8 or 16 bit data?
        chunkLength -= 2;
        if ((m_bits != 8) && (m_bits != 16))
          return;
      }
      else if (memcmp(buffer, "data", 4) == 0)
      {
        m_dataStartOffset = m_in->Tell();

        int bytesPerSample = m_numChannels * m_bits / 8;
        m_numSamples = chunkLength / bytesPerSample;

        m_samplesRemaining = m_numSamples;

        return;
      }

      // Skip the remainder of the chunk
      while (chunkLength > 0)
      {
        if (m_in->ReadU8() == EOF)
          break;

        chunkLength--;
      }
    }
  }

  // FRAMES IN, FRAMES OUT, and both branches below say so. m_numSamples and
  // m_samplesRemaining come from ReadWavHeader as chunkLength divided by
  // channels * bits/8, so they have always been frame counts; what this
  // function returned had not been. The 16-bit branch returned bytes/2, which
  // is a count of SHORTS, and the caller then subtracted it from a frame
  // budget. For mono the two are the same number, which is why 1332 mono files
  // never showed it. For stereo it decoded twice as far as it reported, walked
  // m_samplesRemaining down at double rate, and left the caller believing it
  // had a frame's worth of data in half the space.
  unsigned int SoundStreamDecoder::ReadWavData(signed short* _data, unsigned int _numFrames)
  {
    if (_numFrames > m_samplesRemaining)
      _numFrames = m_samplesRemaining;

    if (m_bits == 8)
    {
      // One byte per sample, m_numChannels samples per frame, so the loop runs
      // over samples and the return converts back.
      const unsigned int numSamples = _numFrames * m_numChannels;
      unsigned int written = numSamples;

      for (unsigned int i = 0; i < numSamples; ++i)
      {
        signed short c = m_in->ReadU8() - 128;
        c <<= 8;
        _data[i] = c;

        if (m_in->m_eof)
        {
          written = i;
          break;
        }
      }

      // A truncated file can end mid-frame. Reporting the partial frame would
      // hand the caller an interleave that is one sample out of phase for
      // everything after it, so it is dropped.
      _numFrames = written / m_numChannels;
    }
    else
    {
      const unsigned int bytesPerFrame = 2 * m_numChannels;
      const unsigned int bytesRead = m_in->ReadBytes(_numFrames * bytesPerFrame, (unsigned char*)_data);
      _numFrames = bytesRead / bytesPerFrame;
      //		float prevVal = 0.0f;
      //		float smooth = sqrtf(0.9f);
      //		float oneMinusSmooth = 1.0f - smooth;
      //		for (int i = 0; i < _numSamples; ++i)
      //		{
      //			float newVal = sfrand(64000.0f);
      //			newVal = newVal * oneMinusSmooth + prevVal * smooth;
      //			_data[i] = Round(newVal);
      //			prevVal = newVal;
      //		}
    }

    m_samplesRemaining -= _numFrames;
    return _numFrames;
  }

#define IS_BIG_ENDIAN 0

  unsigned int SoundStreamDecoder::Read(signed short* _data, unsigned int _numFrames)
  {
    switch (m_fileType)
    {
    case TypeUnknown:
      ASSERT_TEXT(0, "Unknown format of sound file {}", m_in->m_filename);
    case TypeWav:
      return ReadWavData(_data, _numFrames);
    }

    DEBUG_ASSERT(0);
    return 0;
  }

void SoundStreamDecoder::Restart()
{
  if (m_fileType == TypeWav)
  {
    m_in->Seek(m_dataStartOffset, SEEK_SET);
    m_samplesRemaining = m_numSamples;
  }
}
} // namespace Neuron
