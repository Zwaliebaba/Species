#include "pch.h"

#include "Resource.h"
#include "SampleCache.h"
#include "SoundStreamDecoder.h"


namespace Neuron
{
  CachedSampleManager g_cachedSampleManager;
  bool g_deletingCachedSampleHandle = false;


  //*****************************************************************************
  // Class CachedSample
  //*****************************************************************************

  CachedSample::CachedSample(char const* _sampleName)
    : m_amountCached(0)
  {
    const std::string fullPath = std::format("Sounds/{}", _sampleName);

    m_soundStreamDecoder = g_resource->GetSoundStreamDecoder(fullPath.c_str());
    ASSERT_TEXT(m_soundStreamDecoder, "Failed to open sound stream decoder : {}", fullPath);

    m_numChannels = m_soundStreamDecoder->m_numChannels;
    m_numSamples = m_soundStreamDecoder->m_numSamples;
    m_freq = m_soundStreamDecoder->m_freq;

    m_rawSampleData = new signed short[m_numChannels * m_numSamples];
  }


  CachedSample::~CachedSample()
  {
    delete m_soundStreamDecoder;
    m_soundStreamDecoder = nullptr;
    delete[] m_rawSampleData;
    m_rawSampleData = nullptr;
  }


  void CachedSample::Read(signed short* _data, unsigned int _startSample, unsigned int _numSamples)
  {
    if (m_soundStreamDecoder)
    {
      int highestSampleRequested = _startSample + _numSamples - 1;
      if (highestSampleRequested >= m_amountCached)
      {
        int amountToRead = highestSampleRequested - m_amountCached + 1;
        unsigned int amountRead = m_soundStreamDecoder->Read(&m_rawSampleData[m_amountCached], amountToRead);
        m_amountCached += amountRead;
        DEBUG_ASSERT(m_amountCached <= m_numSamples);
        if (m_amountCached == m_numSamples)
        {
          delete m_soundStreamDecoder;
          m_soundStreamDecoder = nullptr;
        }
      }
    }

    memcpy(_data, &m_rawSampleData[_startSample], sizeof(signed short) * m_numChannels * _numSamples);
  }


  //*****************************************************************************
  // Class CachedSampleHandle
  //*****************************************************************************

  CachedSampleHandle::CachedSampleHandle(CachedSample* _sample)
    : m_nextSampleIndex(0),
      m_cachedSample(_sample)
  {
  }


  CachedSampleHandle::~CachedSampleHandle()
  {
    m_cachedSample = nullptr;
    m_nextSampleIndex = 0xffffffff;
  }


  unsigned int CachedSampleHandle::Read(signed short* _data, unsigned int _numSamples)
  {
    int samplesRemaining = m_cachedSample->m_numSamples - m_nextSampleIndex;
    if (_numSamples > samplesRemaining)
    {
      _numSamples = samplesRemaining;
    }

    m_cachedSample->Read(_data, m_nextSampleIndex, _numSamples);
    m_nextSampleIndex += _numSamples;

    return _numSamples;
  }


  void CachedSampleHandle::Restart() { m_nextSampleIndex = 0; }


  //*****************************************************************************
  // Class CachedSampleManager
  //*****************************************************************************

  CachedSampleManager::~CachedSampleManager()
  {
    // The map owns its samples now, so ~unordered_map is the whole destructor.
  }


  CachedSampleHandle* CachedSampleManager::GetSample(char const* _sampleName)
  {
    auto existing = m_cache.find(_sampleName);
    CachedSample* cachedSample = existing == m_cache.end() ? nullptr : existing->second.get();

    if (!cachedSample)
    {
      // The observer is taken before the owner moves into the map, because
      // everything below works through it. Same shape as ownership T1's
      // StartProfile.
      auto owned = std::make_unique<CachedSample>(_sampleName);
      cachedSample = owned.get();
      m_cache.emplace(_sampleName, std::move(owned));
    }

    CachedSampleHandle* rv = new CachedSampleHandle(cachedSample);
    return rv;
  }


  void CachedSampleManager::EmptyCache() { m_cache.clear(); }


  int CachedSampleManager::GetMemoryUsage()
  {
    int memoryUsage = 0;

    for (auto const& entry : m_cache)
    {
      const CachedSample* sample = entry.second.get();
      int sampleSize = sizeof(signed short) * sample->m_numChannels * sample->m_numSamples;
      memoryUsage += sampleSize;
    }

    return memoryUsage;
  }
} // namespace Neuron
