#pragma once

#include <memory>

#include <string>
#include <unordered_map>


class SoundStreamDecoder;


//*****************************************************************************
// Class CachedSample
//*****************************************************************************

class CachedSample
{
  protected:
    SoundStreamDecoder* m_soundStreamDecoder; // nullptr once sample has been read fully once
    signed short* m_rawSampleData;
    unsigned int m_amountCached; // Zero at first, ranging up to m_numSamples once sample has been read fully

  public:
    unsigned int m_numChannels;
    unsigned int m_freq;
    unsigned int m_numSamples;

    CachedSample(char const* _sampleName);
    ~CachedSample();

    void Read(signed short* _data, unsigned int _startSample, unsigned int _numSamples);
};


//*****************************************************************************
// Class CachedSampleHandle
//*****************************************************************************

class CachedSampleHandle
{
  protected:
    unsigned int m_nextSampleIndex; // Index of first sample to return when Read is called

  public:
    CachedSample* m_cachedSample;

    CachedSampleHandle(CachedSample* _sample);
    ~CachedSampleHandle();

    unsigned int Read(signed short* _data, unsigned int _numSamples);
    void Restart();
};


//*****************************************************************************
// Class CachedSampleManager
//*****************************************************************************

// The legacy hash table hashed with the case bit masked off and compared with
// stricmp, so the sample cache was CASE-INSENSITIVE. A plain std::unordered_map is not, and
// the difference is silent: two spellings of one sample name would each get
// their own entry, so the file would be decoded and held in memory twice rather
// than shared. These keep the old behaviour explicit.
struct CaseInsensitiveHash
{
    size_t operator()(std::string const& _key) const
    {
      size_t hash = 14695981039346656037ULL;
      for (char c : _key)
      {
        hash ^= static_cast<size_t>(tolower(static_cast<unsigned char>(c)));
        hash *= 1099511628211ULL;
      }
      return hash;
    }
};

struct CaseInsensitiveEqual
{
    bool operator()(std::string const& _a, std::string const& _b) const { return stricmp(_a.c_str(), _b.c_str()) == 0; }
};


class CachedSampleManager
{
  protected:
    std::unordered_map<std::string, std::unique_ptr<CachedSample>, CaseInsensitiveHash, CaseInsensitiveEqual> m_cache;

  public:
    ~CachedSampleManager();

    CachedSampleHandle* GetSample(char const* _sampleName); // Delete the returned object when you are finished with it

    void EmptyCache(); // Deletes all cached sample data

    int GetMemoryUsage();
};


extern CachedSampleManager g_cachedSampleManager;

extern bool g_deletingCachedSampleHandle;
