#include "pch.h"

#include <stdlib.h>
#include <string.h>

#include "BinaryStreamReaders.h"
#include "Debug.h"

#include "SoundStreamDecoder.h"

SoundStreamDecoder::SoundStreamDecoder(BinaryReader* _in)
  : m_in(_in),
    m_bits(0),
    m_fileType(TypeUnknown),
    m_numChannels(0),
    m_freq(0),
    m_numSamples(0)
{
  char* fileType = _in->GetFileType();
  if (stricmp(fileType, "wav") == 0)
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

unsigned int SoundStreamDecoder::ReadWavData(signed short* _data, unsigned int _numSamples)
{
  if (_numSamples > m_samplesRemaining)
    _numSamples = m_samplesRemaining;

  if (m_bits == 8)
  {
    for (unsigned int i = 0; i < _numSamples; ++i)
    {
      signed short c = m_in->ReadU8() - 128;
      c <<= 8;
      _data[i] = c;

      if (m_in->m_eof)
      {
        _numSamples = i;
        break;
      }
    }
  }
  else
  {
    int bytesPerSample = 2 * m_numChannels;
    _numSamples = m_in->ReadBytes(_numSamples * bytesPerSample, (unsigned char*)_data);
    _numSamples /= 2;
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

  m_samplesRemaining -= _numSamples;
  return _numSamples;
}

#define IS_BIG_ENDIAN 0

unsigned int SoundStreamDecoder::Read(signed short* _data, unsigned int _numSamples)
{
  switch (m_fileType)
  {
  case TypeUnknown:
    ASSERT_TEXT(0, "Unknown format of sound file {}", m_in->m_filename);
  case TypeWav:
    return ReadWavData(_data, _numSamples);
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
