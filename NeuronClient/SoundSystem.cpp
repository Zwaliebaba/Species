#include "pch.h"

#include <stdlib.h> // atof/atoi/qsort below; arrived via Location.h until T17 dropped it
#include "Debug.h"
#include "FilesysUtils.h"
#include "FileWriter.h"
#include "MathUtils.h"
#include "Profiler.h"
#include "Preferences.h"
#include "Resource.h"
#include "StringUtils.h"
#include "TextStreamReaders.h"
#include "WindowManager.h"
#include "LanguageTable.h"


#include "SampleCache.h"
#include "SoundLibrary3d.h"
#include "SoundSystem.h"
#include "SoundStreamDecoder.h"
#include "SoundLibraryXAudio2.h"

#include "GameTime.h"

#include "WorldPointers.h"
#include "WorldTypeNames.h"
#include "AppState.h"


namespace Neuron
{
  SoundSystem* g_soundSystem = nullptr;

#define SOUNDSYSTEM_UPDATEPERIOD 0.05f

  // How long to wait between attempts to rebuild the sound library after the
  // audio device has gone. Long enough that a machine left with a dead device is
  // not rebuilding it constantly, short enough that plugging a headset back in
  // brings sound back before anyone reaches for the options window.
  constexpr float DeviceRetryPeriod = 5.0f;

  //*****************************************************************************
  // Class SoundEventBlueprint
  //*****************************************************************************

  SoundEventBlueprint::SoundEventBlueprint()
    : m_instance(nullptr)
  {
  }

void SoundEventBlueprint::SetEventName(const char* _name) { m_eventName = _name ? _name : ""; }

//*****************************************************************************
// Class SoundSourceBlueprint
//*****************************************************************************

int SoundSourceBlueprint::GetSoundSoundType(const char* _name)
{
  for (int i = 0; i < NumOtherSoundSources; ++i)
  {
    if (stricmp(_name, GetSoundSourceName(i)) == 0)
      return i;
  }
  return -1;
}

const char* SoundSourceBlueprint::GetSoundSourceName(int _type)
{
  const char* names[] = {"Laser", "Grenade", "Rocket", "AirStrikeBomb", "Spirit", "Sepulveda", "Gesture", "Ambience", "Music", "Interface"};

  DEBUG_ASSERT(_type >= 0 && _type < NumOtherSoundSources);
  return names[_type];
}

void SoundSourceBlueprint::ListSoundEvents(int _type, std::vector<const char*>* _list)
{
  switch (_type)
  {
  case TypeLaser:
    _list->push_back("Create");
    _list->push_back("Richochet");
    _list->push_back("HitGround");
    _list->push_back("HitEntity");
    _list->push_back("HitBuilding");
    break;

  case TypeGrenade:
    _list->push_back("Create");
    _list->push_back("Bounce");
    _list->push_back("Flash");
    _list->push_back("Explode");
    _list->push_back("ExplodeController");
    break;

  case TypeRocket:
    _list->push_back("Create");
    _list->push_back("Explode");
    break;

  case TypeAirstrikeBomb:
    _list->push_back("Create");
    _list->push_back("Bounce");
    _list->push_back("Flash");
    _list->push_back("Explode");
    break;

  case TypeSpirit:
    _list->push_back("Create");
    _list->push_back("PickedUp");
    _list->push_back("Dropped");
    _list->push_back("PlacedIntoEgg");
    _list->push_back("EggDestroyed");
    _list->push_back("BeginAscent");
    break;

  case TypeSepulveda:
    _list->push_back("Appear");
    _list->push_back("Disappear");
    _list->push_back("TextAppear");
    break;

  case TypeGesture:
    _list->push_back("GestureBegin");
    _list->push_back("GestureEnd");
    _list->push_back("GestureSuccess");
    _list->push_back("GestureFail");
    break;

  case TypeAmbience:
    _list->push_back("EnterLocation");
    _list->push_back("ExitLocation");
    _list->push_back("EnterGlobalWorld");
    break;

  case TypeMusic:
    _list->push_back("Cutscene1");
    _list->push_back("Cutscene2");
    _list->push_back("Cutscene3");
    _list->push_back("Cutscene4");
    _list->push_back("Cutscene5");
    _list->push_back("LoaderRaytrace");
    _list->push_back("LoaderSoul");
    _list->push_back("LoaderFodder");
    _list->push_back("LoaderSpeccy");
    _list->push_back("LoaderMatrix");
    _list->push_back("LoaderGameOfLife");
    _list->push_back("StartSequence");
    _list->push_back("EndSequence");
    _list->push_back("Credits");
    _list->push_back("Demo2Intro");
    _list->push_back("Demo2Mid");
    break;

  case TypeInterface:
    _list->push_back("Show");
    _list->push_back("Hide");
    _list->push_back("Slide");
    //_list->push_back( "TaskManagerSelectTask" );
    //_list->push_back( "TaskManagerDeselectTask" );
    _list->push_back("DeleteTask");
    _list->push_back("MouseOverIcon");
    _list->push_back("ShowLogo");
    break;
  }
}

//*****************************************************************************
// Class DspBlueprint
//*****************************************************************************

char* DspBlueprint::GetParameter(int _param, float* _min, float* _max, float* _default, int* _dataType)
{
  if (m_params.ValidIndex(_param))
  {
    DspParameterBlueprint* sb = m_params[_param];
    if (_min)
      *_min = sb->m_min;
    if (_max)
      *_max = sb->m_max;
    if (_default)
      *_default = sb->m_default;
    if (_dataType)
      *_dataType = sb->m_dataType;
    return sb->m_name;
  }

  return nullptr;
}

//*****************************************************************************
// Class SampleGroup
//*****************************************************************************

namespace
{
  // The blueprint name fields are char[256] and stay that way for now: the
  // editor registers them with an InputField that writes through a raw char*,
  // and that is strings-modernised/T5's to change. What this fixes is that the
  // copies into them were unbounded — a name longer than 255 characters ran
  // off the end of the struct.
  template <size_t N> void CopyName(char (&_dest)[N], std::string_view _source)
  {
    size_t length = _source.size();
    if (length > N - 1)
    {
      length = N - 1;
    }
    std::memcpy(_dest, _source.data(), length);
    _dest[length] = '\0';
  }
} // namespace


void SampleGroup::SetName(const char* _name) { CopyName(m_name, _name); }

void SampleGroup::AddSample(const char* _sample) { m_samples.emplace_back(_sample); }

//*****************************************************************************
// Class SoundSystem
//*****************************************************************************

SoundSystem::SoundSystem()
  : m_timeSync(0.0f),
    m_propagateBlueprints(false),
    m_mainProfiler(nullptr),
    m_eventProfiler(nullptr),
    m_quitWithoutSave(false),
    m_music(),
    m_requestedMusic(),
    m_channels(nullptr),
    m_numChannels(0)
{
#ifdef PROFILER_ENABLED
  m_eventProfiler = new Profiler();
  m_mainProfiler = new Profiler();
#endif
}

SoundSystem::~SoundSystem()
{
#ifdef PROFILER_ENABLED
  delete m_eventProfiler;
  delete m_mainProfiler;
#endif
  delete[] m_channels;

  m_sounds.Empty();

  delete g_soundLibrary3d;
  g_soundLibrary3d = nullptr;
}

void SoundSystem::Initialise()
{
  LoadEffects();
  LoadBlueprints();

  RestartSoundLibrary();
}

void SoundSystem::RestartSoundLibrary()
{
  //
  // Shut down existing sound library

  if (m_channels)
  {
    delete[] m_channels;
    delete g_soundLibrary3d;
    g_soundLibrary3d = nullptr;
  }

  //
  // Start up a new sound library

  int volume = g_prefsManager->GetInt("SoundMasterVolume", 255);
  m_numChannels = g_prefsManager->GetInt("SoundChannels", 32);

  // One backend, so no choice to make and no SoundLibrary preference to read.
  // T7 deleted DirectSound and T8 the software mixer; a preferences file still
  // naming either is simply ignored rather than honoured, because there is
  // nothing left for it to select.
  g_soundLibrary3d = new SoundLibraryXAudio2();

  g_soundLibrary3d->SetMasterVolume(volume);
  g_soundLibrary3d->Initialise(m_numChannels);

  m_numChannels = g_soundLibrary3d->GetNumMainChannels();
  m_channels = new SoundInstanceId[m_numChannels];

  g_soundLibrary3d->SetMainCallback(&SoundLibraryMainCallback);
  g_soundLibrary3d->SetMusicCallback(&SoundLibraryMusicCallback);
}

void SoundSystem::StopAllDSPEffects()
{
  for (int i = 0; i < m_sounds.Size(); ++i)
  {
    if (m_sounds.ValidIndex(i))
    {
      SoundInstance* instance = m_sounds[i].get();
      // Was `while (ValidIndex(0))` — a subscript standing in for an
      // emptiness test, which the legacy list answered and std::vector does not.
      for (DspHandle* handle : instance->m_dspFX)
        delete handle;
      instance->m_dspFX.clear();
    }
  }
}

// AN EFFECT CHANNEL IS MONO, so here a frame is a short and the two counts are
// the same number. That is not an assumption, it is enforced below.
bool SoundSystem::SoundLibraryMainCallback(unsigned int _channel, signed short* _data, unsigned int _numSamples, int* _silenceRemaining)
{
  if (!g_soundSystem)
    return false;

  SoundInstanceId soundId = g_soundSystem->m_channels[_channel];
  SoundInstance* instance = g_soundSystem->GetSoundInstance(soundId);

  // A non-mono sample on an effect channel would read m_numChannels shorts per
  // frame into a block sized for one, which is a write past the end of the
  // voice's ring — not a wrong noise, memory corruption. SoundSourceNotMono is
  // supposed to catch this at load, but its only caller is IsSoundSourceOK,
  // reached only from LoadtimeVerify, whose one call site is commented out. So
  // the rule is enforced where the damage would happen instead: a modded
  // Sounds.txt naming a stereo sample makes that sound silent, and nothing else.
  if (instance && instance->m_cachedSampleHandle && instance->m_cachedSampleHandle->GetNumChannels() != 1)
  {
    DebugTrace("SOUND : sample on channel {} is not mono; effect channels are mono only, so it is silent\n", _channel);
    g_soundLibrary3d->WriteSilence(_data, _numSamples);
    return false;
  }

  if (instance && instance->m_cachedSampleHandle)
  {
#ifdef PROFILER_ENABLED
    g_soundSystem->m_eventProfiler->StartProfile(instance->m_eventName);
#endif

    //
    // Fill the space with sample data

    int numSamplesWritten = instance->m_cachedSampleHandle->Read(_data, _numSamples);

    if (numSamplesWritten < _numSamples)
    {
      signed short* loopStart = _data + numSamplesWritten;
      unsigned int numSamplesRemaining = _numSamples - numSamplesWritten;

      if (instance->m_loopType == SoundInstance::Looped || instance->m_loopType == SoundInstance::LoopedADSR)
      {
        while (numSamplesRemaining > 0)
        {
          bool looped = instance->AdvanceLoop();
          if (looped)
          {
            unsigned int numWritten = instance->m_cachedSampleHandle->Read(loopStart, numSamplesRemaining);
            loopStart += numWritten;
            numSamplesRemaining -= numWritten;
          }
          else
          {
            g_soundLibrary3d->WriteSilence(loopStart, numSamplesRemaining);
            numSamplesRemaining = 0;
          }
        }
      }
      else if (instance->m_loopType == SoundInstance::SinglePlay)
      {
        if (numSamplesWritten > 0)
        {
          // The sound just came to an end, so write a whole buffers worth of silence
          g_soundLibrary3d->WriteSilence(loopStart, numSamplesRemaining);
          *_silenceRemaining = g_soundLibrary3d->GetChannelBufSize(_channel) - numSamplesRemaining;
        }
        else
        {
          // The sound came to an end and now we are writing silence
          g_soundLibrary3d->WriteSilence(loopStart, numSamplesRemaining);
          *_silenceRemaining -= numSamplesRemaining;
          if (*_silenceRemaining <= 0)
            instance->BeginRelease(false);
        }
      }
    }

#ifdef PROFILER_ENABLED
    g_soundSystem->m_eventProfiler->EndProfile(instance->m_eventName);
#endif
    return true;
  }
  g_soundLibrary3d->WriteSilence(_data, _numSamples);
  return false;
}

namespace
{
  // Reads frames from a music sample into a buffer that is _numChannels wide,
  // whatever width the sample itself is. Returns FRAMES written.
  //
  // Every music file the game ships is mono and the music voice is stereo, so
  // this fan-out is the normal path rather than the exceptional one. It reads
  // the mono frames into the front of the block and then expands BACKWARDS, so
  // a frame is never overwritten before it has been read — frame N's source
  // sits at index N and its destination starts at N * _numChannels, which is at
  // or ahead of it for every N.
  unsigned int ReadMusicFrames(CachedSampleHandle* _handle, signed short* _data, unsigned int _numFrames, int _numChannels)
  {
    const int sampleChannels = static_cast<int>(_handle->GetNumChannels());

    if (sampleChannels == _numChannels)
      return _handle->Read(_data, _numFrames);

    // A sample wider than the voice has no meaning to give it; the decoder
    // caps at two channels, so with a stereo music voice this is unreachable
    // today. Silence rather than a guess, and never a partial interleave.
    if (sampleChannels != 1)
      return 0;

    const unsigned int framesRead = _handle->Read(_data, _numFrames);

    for (int frame = static_cast<int>(framesRead) - 1; frame >= 0; --frame)
    {
      const signed short sample = _data[frame];
      for (int channel = 0; channel < _numChannels; ++channel)
        _data[frame * _numChannels + channel] = sample;
    }

    return framesRead;
  }
} // namespace

// COUNTS ARE FRAMES AND OFFSETS ARE SHORTS, which is why every pointer step
// below carries _numChannels and no count does. _silenceRemaining is a frame
// count too, because it is compared against GetChannelBufSize.
bool SoundSystem::SoundLibraryMusicCallback(signed short* _data, unsigned int _numFrames, int _numChannels, int* _silenceRemaining)
{
  if (!g_soundSystem)
    return false;

  SoundInstance* instance = g_soundSystem->m_music.get();

  if (instance && instance->m_cachedSampleHandle)
  {
#ifdef PROFILER_ENABLED
    g_soundSystem->m_eventProfiler->StartProfile(instance->m_eventName);
#endif

    //
    // Fill the space with sample data

    int numFramesWritten = ReadMusicFrames(instance->m_cachedSampleHandle, _data, _numFrames, _numChannels);

    if (numFramesWritten < _numFrames)
    {
      signed short* loopStart = _data + numFramesWritten * _numChannels;
      unsigned int numFramesRemaining = _numFrames - numFramesWritten;

      if (instance->m_loopType == SoundInstance::Looped || instance->m_loopType == SoundInstance::LoopedADSR)
      {
        while (numFramesRemaining > 0)
        {
          bool looped = instance->AdvanceLoop();
          if (looped)
          {
            unsigned int numWritten = ReadMusicFrames(instance->m_cachedSampleHandle, loopStart, numFramesRemaining, _numChannels);

            // A loop that yields nothing would spin here forever. The old code
            // could not reach that state because Read only returned zero at the
            // end of a sample, which AdvanceLoop had just rewound; ReadMusicFrames
            // can also return zero for a sample too wide for the voice.
            if (numWritten == 0)
            {
              g_soundLibrary3d->WriteSilence(loopStart, numFramesRemaining * _numChannels);
              numFramesRemaining = 0;
              break;
            }

            loopStart += numWritten * _numChannels;
            numFramesRemaining -= numWritten;
          }
          else
          {
            g_soundLibrary3d->WriteSilence(loopStart, numFramesRemaining * _numChannels);
            numFramesRemaining = 0;
          }
        }
      }
      else if (instance->m_loopType == SoundInstance::SinglePlay)
      {
        if (numFramesWritten > 0)
        {
          // The sound just came to an end, so write a whole buffers worth of silence
          g_soundLibrary3d->WriteSilence(loopStart, numFramesRemaining * _numChannels);
          *_silenceRemaining = g_soundLibrary3d->GetChannelBufSize(g_soundLibrary3d->m_musicChannelId) - numFramesRemaining;
        }
        else
        {
          // The sound came to an end and now we are writing silence
          g_soundLibrary3d->WriteSilence(loopStart, numFramesRemaining * _numChannels);
          *_silenceRemaining -= numFramesRemaining;
          if (*_silenceRemaining <= 0)
            instance->BeginRelease(false);
        }
      }
    }

#ifdef PROFILER_ENABLED
    g_soundSystem->m_eventProfiler->EndProfile(instance->m_eventName);
#endif
    return true;
  }
  g_soundLibrary3d->WriteSilence(_data, _numFrames * _numChannels);
  return false;
}

void SoundSystem::LoadEffects()
{
  m_filterBlueprints.SetSize(SoundLibrary3d::NUM_FILTERS);

  TextReader* in = g_resource->GetTextReader("Effects.txt");
  ASSERT_TEXT(in && in->IsOpen(), "Couldn't load effects.txt");

  while (in->ReadLine())
  {
    if (!in->TokenAvailable())
      continue;
    char* effect = in->GetNextToken();
    DEBUG_ASSERT(stricmp(effect, "EFFECT") == 0);

    auto owned = std::make_unique<DspBlueprint>();
    DspBlueprint* bp = owned.get();
    m_filterBlueprints.PutData(std::move(owned));
    CopyName(bp->m_name, in->GetNextToken());

    in->ReadLine();
    char* param = in->GetNextToken();
    while (stricmp(param, "END") != 0)
    {
      auto sb = new DspParameterBlueprint();
      bp->m_params.PutData(sb);

      CopyName(sb->m_name, param);
      sb->m_min = atof(in->GetNextToken());
      sb->m_max = atof(in->GetNextToken());
      sb->m_default = atof(in->GetNextToken());
      sb->m_dataType = atoi(in->GetNextToken());

      in->ReadLine();
      param = in->GetNextToken();
    }
  }

  delete in;
}

bool SoundSystem::AreBlueprintsModified()
{
  // Currently broken due to mod support
  // ie sounds_new.txt and sounds.txt coule be anywhere,
  // you just don't know anymore :)
  return false;
}

void SoundSystem::LoadBlueprints()
{
  // No roster means no game attached — a sound system in a test DLL, or one
  // built before App installed GameLogic's. There is nothing to index the
  // blueprint tables by, so there are no blueprints to load. Silence is the
  // right answer here, not a crash.
  if (!g_worldTypeNames)
    return;

  m_entityBlueprints.SetSize(g_worldTypeNames->NumEntityTypes());
  m_buildingBlueprints.SetSize(g_worldTypeNames->NumBuildingTypes());
  m_otherBlueprints.SetSize(SoundSourceBlueprint::NumOtherSoundSources);

  TextReader* in = g_resource->GetTextReader("Sounds.txt");
  ASSERT_TEXT(in && in->IsOpen(), "Couldn't open sounds.txt");

  std::string objectName;

  while (in->ReadLine())
  {
    if (!in->TokenAvailable())
      continue;
    char* group = in->GetNextToken();
    char* type = in->GetNextToken();
    bool event = false;
    SoundSourceBlueprint* ssb = nullptr;

    if (stricmp(group, "ENTITY") == 0)
    {
      objectName = type;
      int entityType = g_worldTypeNames->EntityTypeId(type);
      DEBUG_ASSERT(entityType >= 0 && entityType < g_worldTypeNames->NumEntityTypes());
      DEBUG_ASSERT(!m_entityBlueprints.ValidIndex(entityType));

      auto owned = std::make_unique<SoundSourceBlueprint>();
      ssb = owned.get();
      m_entityBlueprints.PutData(std::move(owned), entityType);
      event = true;
    }
    else if (stricmp(group, "BUILDING") == 0)
    {
      objectName = type;
      int buildingType = g_worldTypeNames->BuildingTypeId(type);
      DEBUG_ASSERT(buildingType >= 0 && buildingType < g_worldTypeNames->NumBuildingTypes());
      DEBUG_ASSERT(!m_buildingBlueprints.ValidIndex(buildingType));

      auto owned = std::make_unique<SoundSourceBlueprint>();
      ssb = owned.get();
      m_buildingBlueprints.PutData(std::move(owned), buildingType);
      event = true;
    }
    else if (stricmp(group, "OTHER") == 0)
    {
      objectName = type;
      int otherType = SoundSourceBlueprint::GetSoundSoundType(type);
      DEBUG_ASSERT(otherType >= 0 && otherType < SoundSourceBlueprint::NumOtherSoundSources);
      DEBUG_ASSERT(!m_otherBlueprints.ValidIndex(otherType));

      auto owned = std::make_unique<SoundSourceBlueprint>();
      ssb = owned.get();
      m_otherBlueprints.PutData(std::move(owned), otherType);
      event = true;
    }
    else if (stricmp(group, "SAMPLEGROUP") == 0)
    {
      objectName = "sample group";
      SampleGroup* sampleGroup = NewSampleGroup(type);
      ParseSampleGroup(in, sampleGroup);
    }

    if (event)
    {
      while (in->ReadLine())
      {
        if (in->TokenAvailable())
        {
          char* word = in->GetNextToken();
          if (stricmp(word, "END") == 0)
            break;
          DEBUG_ASSERT(stricmp(word, "EVENT") == 0);
          ParseSoundEvent(in, ssb, objectName);
        }
      }
    }
  }

  //
  // Verify the data we just loaded - make sure all the samples exist, are
  // in the right format etc.

  // LoadtimeVerify();

  //
  // Fill in the non-specified sound sources with blanks

  for (int i = 0; i < g_worldTypeNames->NumEntityTypes(); ++i)
  {
    if (!m_entityBlueprints.ValidIndex(i))
    {
      m_entityBlueprints.PutData(std::make_unique<SoundSourceBlueprint>(), i);
    }
  }

  for (int i = 0; i < g_worldTypeNames->NumBuildingTypes(); ++i)
  {
    if (!m_buildingBlueprints.ValidIndex(i))
    {
      m_buildingBlueprints.PutData(std::make_unique<SoundSourceBlueprint>(), i);
    }
  }

  for (int i = 0; i < SoundSourceBlueprint::NumOtherSoundSources; ++i)
  {
    if (!m_otherBlueprints.ValidIndex(i))
    {
      m_otherBlueprints.PutData(std::make_unique<SoundSourceBlueprint>(), i);
    }
  }

  delete in;
}

void SoundSystem::SaveBlueprints() { SaveBlueprints("Sounds.txt"); }

void SoundSystem::SaveBlueprints(const char* _filename)
{
  FileWriter* file = g_resource->GetFileWriter(_filename, false);
  // Not always possible - this may be a release version with a data.dat
  if (!file)
    return;

  for (int i = 0; i < m_entityBlueprints.Size(); ++i)
  {
    DEBUG_ASSERT(m_entityBlueprints.ValidIndex(i));
    SoundSourceBlueprint* ssb = m_entityBlueprints[i].get();

    if (static_cast<int>(ssb->m_events.size()) > 0)
    {
      file->printf("# =========================================================\n");
      file->printf("ENTITY {}\n", g_worldTypeNames->EntityTypeName(i));

      for (int j = 0; j < static_cast<int>(ssb->m_events.size()); ++j)
      {
        SoundEventBlueprint* seb = ssb->m_events[j];
        WriteSoundEvent(file, seb);
      }

      file->printf("END\n\n\n");
    }
  }

  for (int i = 0; i < m_buildingBlueprints.Size(); ++i)
  {
    DEBUG_ASSERT(m_buildingBlueprints.ValidIndex(i));
    SoundSourceBlueprint* ssb = m_buildingBlueprints[i].get();

    if (static_cast<int>(ssb->m_events.size()) > 0)
    {
      file->printf("# =========================================================\n");
      file->printf("BUILDING {}\n", g_worldTypeNames->BuildingTypeName(i));

      for (int j = 0; j < static_cast<int>(ssb->m_events.size()); ++j)
      {
        SoundEventBlueprint* seb = ssb->m_events[j];
        WriteSoundEvent(file, seb);
      }

      file->printf("END\n\n\n");
    }
  }

  for (int i = 0; i < m_otherBlueprints.Size(); ++i)
  {
    DEBUG_ASSERT(m_otherBlueprints.ValidIndex(i));
    SoundSourceBlueprint* ssb = m_otherBlueprints[i].get();

    if (static_cast<int>(ssb->m_events.size()) > 0)
    {
      file->printf("# =========================================================\n");
      file->printf("OTHER {}\n", SoundSourceBlueprint::GetSoundSourceName(i));

      for (int j = 0; j < static_cast<int>(ssb->m_events.size()); ++j)
      {
        SoundEventBlueprint* seb = ssb->m_events[j];
        WriteSoundEvent(file, seb);
      }

      file->printf("END\n\n\n");
    }
  }

  for (int i = 0; i < m_sampleGroups.Size(); ++i)
  {
    if (m_sampleGroups.ValidIndex(i))
    {
      SampleGroup* group = m_sampleGroups[i].get();

      file->printf("# =========================================================\n");
      file->printf("SAMPLEGROUP {}\n", group->m_name);

      WriteSampleGroup(file, group);

      file->printf("END\n\n\n");
    }
  }

  delete file;
}

void SoundSystem::ParseSoundEvent(TextReader* _in, SoundSourceBlueprint* _source, std::string_view _entityName)
{
  auto seb = new SoundEventBlueprint();
  seb->SetEventName(_in->GetNextToken());

  seb->m_instance = new SoundInstance();
  seb->m_instance->SetEventName(_entityName, seb->m_eventName);

  int oldUserPriority; // backwards compatability, no longer used

  _in->ReadLine();
  char* fieldName = _in->GetNextToken();
  while (stricmp(fieldName, "END") != 0)
  {
    if (stricmp(fieldName, "SOUNDNAME") == 0)
    {
      char* soundName = _in->GetNextToken();
      StrToLower(soundName);
      const std::string extensionRemoved = RemoveExtension(soundName);
      seb->m_instance->SetSoundName(extensionRemoved.c_str());
    }
    else if (stricmp(fieldName, "SOURCETYPE") == 0)
      seb->m_instance->m_sourceType = atoi(_in->GetNextToken());
    else if (stricmp(fieldName, "POSITIONTYPE") == 0)
      seb->m_instance->m_positionType = atoi(_in->GetNextToken());
    else if (stricmp(fieldName, "INSTANCETYPE") == 0)
      seb->m_instance->m_instanceType = atoi(_in->GetNextToken());
    else if (stricmp(fieldName, "LOOPTYPE") == 0)
      seb->m_instance->m_loopType = atoi(_in->GetNextToken());
    else if (stricmp(fieldName, "PRIORITY") == 0)
      oldUserPriority = atoi(_in->GetNextToken());
    else if (stricmp(fieldName, "MINDISTANCE") == 0)
      seb->m_instance->m_minDistance = atof(_in->GetNextToken());
    else if (stricmp(fieldName, "VOLUME") == 0)
      seb->m_instance->m_volume.Read(_in);
    else if (stricmp(fieldName, "FREQUENCY") == 0)
      seb->m_instance->m_freq.Read(_in);
    else if (stricmp(fieldName, "ATTACK") == 0)
      seb->m_instance->m_attack.Read(_in);
    else if (stricmp(fieldName, "SUSTAIN") == 0)
      seb->m_instance->m_sustain.Read(_in);
    else if (stricmp(fieldName, "RELEASE") == 0)
      seb->m_instance->m_release.Read(_in);
    else if (stricmp(fieldName, "LOOPDELAY") == 0)
      seb->m_instance->m_loopDelay.Read(_in);
    else if (stricmp(fieldName, "EFFECT") == 0)
      ParseSoundEffect(_in, seb);
    else
      DEBUG_ASSERT(false);

    // This is bad, we have a looping sound that won't be attached
    // to any one object
    //        DEBUG_ASSERT( !( seb->m_instance->m_loopType &&
    //                        seb->m_instance->m_positionType != SoundInstance::Type3DAttachedToObject ) );

    _in->ReadLine();
    fieldName = _in->GetNextToken();
  }

  _source->m_events.push_back(seb);
}

void SoundSystem::ParseSoundEffect(TextReader* _in, SoundEventBlueprint* _blueprint)
{
  char* effectName = _in->GetNextToken();
  int fxType = -1;

  for (int i = 0; i < m_filterBlueprints.Size(); ++i)
  {
    DspBlueprint* seb = m_filterBlueprints[i].get();
    if (stricmp(seb->m_name, effectName) == 0)
    {
      fxType = i;
      break;
    }
  }

  DEBUG_ASSERT(fxType != -1);

  auto effect = new DspHandle();
  effect->m_type = fxType;

  int paramIndex = 0;
  while (true)
  {
    _in->ReadLine();
    char* paramName = _in->GetNextToken();
    if (stricmp(paramName, "END") == 0)
      break;
    effect->m_params[paramIndex].Read(_in);
    ++paramIndex;
  }
  _blueprint->m_instance->m_dspFX.push_back(effect);
}

void SoundSystem::ParseSampleGroup(TextReader* _in, SampleGroup* _group)
{
  while (true)
  {
    _in->ReadLine();
    char* paramType = _in->GetNextToken();
    if (stricmp(paramType, "END") == 0)
      break;

    char* sample = _in->GetNextToken();
    StrToLower(sample);
    // The (char*) cast this used to carry was noise: AddSample takes a
    // const char* and always did.
    const std::string extensionRemoved = RemoveExtension(sample);
    _group->AddSample(extensionRemoved.c_str());
  }
}

void SoundSystem::WriteSoundEvent(FileWriter* _file, SoundEventBlueprint* _event)
{
  DEBUG_ASSERT(_event);
  DEBUG_ASSERT(_event->m_instance);

  // m_eventName is empty whenever the blueprint line carried no token, and it
  // was a null char* before T9 made it a std::string. The C formatter this file
  // used to write through printed MSVC's "(null)" for that null, so the string
  // is preserved rather than becoming an empty field — the bytes of the sound
  // blueprint file do not change on this task. strings-modernised T17 and T9.
  std::string_view const eventName = _event->m_eventName.empty() ? "(null)" : _event->m_eventName;

  _file->printf("\tEVENT {:<20}\n"
                "\t\tSOUNDNAME          {}\n"
                "\t\tSOURCETYPE         {:d}\n"
                "\t\tPOSITIONTYPE       {:d}\n"
                "\t\tINSTANCETYPE       {:d}\n"
                "\t\tLOOPTYPE           {:d}\n"
                "\t\tMINDISTANCE        {:2.2f}\n",
                eventName, _event->m_instance->m_soundName, _event->m_instance->m_sourceType, _event->m_instance->m_positionType,
                _event->m_instance->m_instanceType, _event->m_instance->m_loopType, _event->m_instance->m_minDistance);

  _event->m_instance->m_volume.Write(_file, "VOLUME", 2);

  if (!_event->m_instance->m_freq.IsFixedValue(1.0f))
    _event->m_instance->m_freq.Write(_file, "FREQUENCY", 2);
  if (!_event->m_instance->m_attack.IsFixedValue(0.0f))
    _event->m_instance->m_attack.Write(_file, "ATTACK", 2);
  if (!_event->m_instance->m_sustain.IsFixedValue(0.0f))
    _event->m_instance->m_sustain.Write(_file, "SUSTAIN", 2);
  if (!_event->m_instance->m_release.IsFixedValue(0.0f))
    _event->m_instance->m_release.Write(_file, "RELEASE", 2);
  if (!_event->m_instance->m_loopDelay.IsFixedValue(0.0f))
    _event->m_instance->m_loopDelay.Write(_file, "LOOPDELAY", 2);

  for (int i = 0; i < static_cast<int>(_event->m_instance->m_dspFX.size()); ++i)
  {
    DspHandle* effect = _event->m_instance->m_dspFX[i];
    DspBlueprint* blueprint = m_filterBlueprints[effect->m_type].get();

    _file->printf("\t\tEFFECT             {}\n", blueprint->m_name);
    int paramIndex = 0;
    while (true)
    {
      char* paramName = blueprint->GetParameter(paramIndex);
      if (!paramName)
        break;
      SoundParameter* param = &effect->m_params[paramIndex];
      param->Write(_file, paramName, 3);
      ++paramIndex;
    }
    _file->printf("\t\tEND\n");
  }

  _file->printf("\tEND");
  _file->printf("\n");
}

void SoundSystem::WriteSampleGroup(FileWriter* _file, SampleGroup* _group)
{
  for (int i = 0; i < static_cast<int>(_group->m_samples.size()); ++i)
  {
    std::string const& sample = _group->m_samples[i];
    _file->printf("\tSAMPLE  {}\n", sample);
  }
}

bool SoundSystem::InitialiseSound(std::unique_ptr<SoundInstance> _instance)
{
  bool createNewSound = true;

  if (_instance->m_instanceType != SoundInstance::Polyphonic)
  {
    // This is a monophonic sound, so look for an exisiting
    // instance of the same sound
    for (int i = 0; i < m_sounds.Size(); ++i)
    {
      if (m_sounds.ValidIndex(i))
      {
        SoundInstance* thisInstance = m_sounds[i].get();
        if (thisInstance->m_instanceType != SoundInstance::Polyphonic && stricmp(thisInstance->m_eventName, _instance->m_eventName) == 0)
        {
          for (int j = 0; j < static_cast<int>(_instance->m_objIds.size()); ++j)
          {
            WorldObjectId* id = _instance->m_objIds[j];
            thisInstance->m_objIds.push_back(new WorldObjectId(*id));
          }
          createNewSound = false;
          break;
        }
      }
    }
  }

  if (createNewSound)
  {
    // The observer is taken before the move, because everything after works
    // through it -- the slot holds the instance from here on.
    SoundInstance* instance = _instance.get();
    instance->m_id.m_index = m_sounds.PutData(std::move(_instance));
    instance->m_id.m_uniqueId = SoundInstanceId::GenerateUniqueId();
    instance->m_restartAttempts = 3; // int( 1.0f + (float) _instance->m_volume.GetOutput() / 5.0f );
    return true;
  }

  // The folding path. The object ids were merged into the existing instance
  // above, and this one is destroyed HERE rather than in the caller's
  // ShutdownSound -- the one program point this conversion moves, stated in
  // the header and in ownership T8.
  return false;
}

int SoundSystem::FindBestAvailableChannel()
{
  int emptyChannelIndex = -1; // Channel without a current sound or requested sound

  int lowestPriorityChannelIndex = -1; // Channel with very low priority
  float lowestPriorityChannel = 999999.9f;

  for (int i = 0; i < m_numChannels; ++i)
  {
    SoundInstanceId soundId = m_channels[i];
    SoundInstance* currentSound = GetSoundInstance(soundId);

    float channelPriority = 0.0f;
    if (currentSound)
      channelPriority = currentSound->m_calculatedPriority;
    else
    {
      emptyChannelIndex = i;
      break;
    }

    if (channelPriority < lowestPriorityChannel)
    {
      lowestPriorityChannel = channelPriority;
      lowestPriorityChannelIndex = i;
    }
  }

  //
  // Did we find any empty channels?

  if (emptyChannelIndex != -1)
    return emptyChannelIndex;
  return lowestPriorityChannelIndex;
}

void SoundSystem::ShutdownSound(SoundInstance* _instance)
{
  const int index = _instance->m_id.m_index;

  // THE IDENTITY CHECK IS A FIX, not part of the conversion -- see ownership
  // T8. The old code tested ValidIndex alone, so after an index was reused it
  // would un-register whoever held the slot NOW and delete the argument
  // anyway. Sound destructors stop audio channels, so that failure was
  // silent. The slot is released only when it still holds this instance.
  if (m_sounds.ValidIndex(index) && m_sounds[index].get() == _instance)
  {
    // Reset the slot while operator[] is still valid; MarkNotUsed only clears
    // the occupancy bit and would leave the instance alive and unreachable.
    std::unique_ptr<SoundInstance> const dying = std::move(m_sounds[index]);
    m_sounds.MarkNotUsed(index);
    dying->StopPlaying();
    return;
  }

  // Not ours. Registered instances are the whole contract, so there is
  // nothing to release and nothing to delete.
  _instance->StopPlaying();
}

SoundInstance* SoundSystem::GetSoundInstance(SoundInstanceId id)
{
  if (!m_sounds.ValidIndex(id.m_index))
    return nullptr;

  SoundInstance* found = m_sounds[id.m_index].get();
  if (found->m_id == id)
    return found;

  return nullptr;
}

void SoundSystem::TriggerEntityEvent(SoundSource const& _source, const char* _eventName)
{
  if (!m_channels)
    return;

  START_PROFILE(m_mainProfiler, "TriggerEntityEvent");
  WorldObjectId objId(_source.m_id);

  if (m_entityBlueprints.ValidIndex(_source.m_type))
  {
    SoundSourceBlueprint* sourceBlueprint = m_entityBlueprints[_source.m_type].get();
    for (int i = 0; i < static_cast<int>(sourceBlueprint->m_events.size()); ++i)
    {
      SoundEventBlueprint* seb = sourceBlueprint->m_events[i];
      if (stricmp(seb->m_eventName.c_str(), _eventName) == 0)
      {
        DEBUG_ASSERT(seb->m_instance);
        auto instance = std::make_unique<SoundInstance>();
        instance->Copy(seb->m_instance);
        instance->m_objIds.push_back(new WorldObjectId(objId));
        instance->m_pos = _source.m_pos;
        instance->m_vel = _source.m_vel;
        // InitialiseSound owns it either way -- no ShutdownSound on failure.
        InitialiseSound(std::move(instance));
      }
    }
  }

  END_PROFILE(m_mainProfiler, "TriggerEntityEvent");
}

void SoundSystem::TriggerBuildingEvent(SoundSource const& _source, const char* _eventName)
{
  if (!m_channels)
    return;

  START_PROFILE(m_mainProfiler, "TriggerBuildingEvent");
  if (m_buildingBlueprints.ValidIndex(_source.m_type))
  {
    SoundSourceBlueprint* sourceBlueprint = m_buildingBlueprints[_source.m_type].get();
    for (int i = 0; i < static_cast<int>(sourceBlueprint->m_events.size()); ++i)
    {
      SoundEventBlueprint* seb = sourceBlueprint->m_events[i];
      if (stricmp(seb->m_eventName.c_str(), _eventName) == 0)
      {
        DEBUG_ASSERT(seb->m_instance);
        auto instance = std::make_unique<SoundInstance>();
        instance->Copy(seb->m_instance);
        instance->m_objIds.push_back(new WorldObjectId(_source.m_id));
        instance->m_pos = _source.m_pos;
        InitialiseSound(std::move(instance));
      }
    }
  }

  END_PROFILE(m_mainProfiler, "TriggerBuildingEvent");
}

// The unattached form. Kept as a separate overload rather than a null
// SoundSource*: 30 of the 47 call sites pass nothing, and `TriggerOtherEvent(
// "GestureSuccess", TypeGesture)` says what they mean better than a nullptr
// first argument did.
void SoundSystem::TriggerOtherEvent(const char* _eventName, int _type)
{
  TriggerOtherEvent(static_cast<SoundSource const*>(nullptr), _eventName, _type);
}


void SoundSystem::TriggerOtherEvent(SoundSource const& _source, const char* _eventName, int _type) { TriggerOtherEvent(&_source, _eventName, _type); }


void SoundSystem::TriggerOtherEvent(SoundSource const* _other, const char* _eventName, int _type)
{
  if (!m_channels)
    return;

  START_PROFILE(m_mainProfiler, "TriggerOtherEvent");

  static int musicType = -1;
  if (musicType == -1)
    musicType = SoundSourceBlueprint::GetSoundSoundType("music");

  if (m_otherBlueprints.ValidIndex(_type))
  {
    // Search for the event blueprint matching _eventName
    SoundSourceBlueprint* sourceBlueprint = m_otherBlueprints[_type].get();
    for (int i = 0; i < static_cast<int>(sourceBlueprint->m_events.size()); ++i)
    {
      SoundEventBlueprint* seb = sourceBlueprint->m_events[i];
      if (stricmp(seb->m_eventName.c_str(), _eventName) == 0)
      {
        // We have a match
        DEBUG_ASSERT(seb->m_instance);
        auto instance = std::make_unique<SoundInstance>();
        instance->Copy(seb->m_instance);
        if (_type == musicType)
        {
          // if( m_music && stricmp( m_music->m_eventName+6, _eventName ) == 0 )
          if (m_music && stricmp(m_music->m_soundName, seb->m_instance->m_soundName) == 0)
          {
            // The music is already playing. `instance` dies here -- with the
            // raw pointer it was simply leaked, every time this branch ran.
          }
          else
          {
            m_requestedMusic = std::move(instance);
            if (m_music)
              m_music->BeginRelease(true);
          }
        }
        else
        {
          if (_other)
          {
            instance->m_pos = _other->m_pos;
            instance->m_objIds.push_back(new WorldObjectId(_other->m_id));
          }
          InitialiseSound(std::move(instance));
        }
      }
    }
  }

  END_PROFILE(m_mainProfiler, "TriggerOtherEvent");
}

void SoundSystem::TriggerDuplicateSound(SoundInstance* _instance)
{
  auto newInstanceOwned = std::make_unique<SoundInstance>();
  SoundInstance* newInstance = newInstanceOwned.get();
  newInstance->Copy(_instance);
  newInstance->m_parent = _instance->m_parent;
  newInstance->m_pos = _instance->m_pos;
  newInstance->m_vel = _instance->m_vel;

  for (int i = 0; i < static_cast<int>(_instance->m_objIds.size()); ++i)
  {
    WorldObjectId* id = _instance->m_objIds[i];
    newInstance->m_objIds.push_back(new WorldObjectId(*id));
  }

  // The observer is only valid if registration succeeded; on the folding path
  // InitialiseSound has already destroyed it.
  const bool success = InitialiseSound(std::move(newInstanceOwned));
  if (success && newInstance->m_positionType == SoundInstance::TypeInEditor)
    m_editorInstanceId = newInstance->m_id;
}

void SoundSystem::StopAllSounds(WorldObjectId _id, const char* _eventName)
{
  if (strstr(_eventName, "Music"))
  {
    if (m_music)
      m_music->BeginRelease(true);
  }
  else
  {
    for (int i = 0; i < m_sounds.Size(); ++i)
    {
      if (m_sounds.ValidIndex(i))
      {
        SoundInstance* instance = m_sounds[i].get();

        if (instance->m_objId == _id)
        {
          if (!_eventName || stricmp(instance->m_eventName, _eventName) == 0)
          {
            if (instance->IsPlaying())
              instance->BeginRelease(true);
            else
              ShutdownSound(instance);
          }
        }
      }
    }
  }
}

int SoundSystem::IsSoundPlaying(SoundInstanceId _id)
{
  for (int i = 0; i < m_numChannels; ++i)
  {
    SoundInstanceId soundId = m_channels[i];
    if (_id == soundId)
      return i;
  }

  return -1;
}

int SoundSystem::NumInstancesPlaying(WorldObjectId _id, const char* _eventName)
{
  int result = 0;

  for (int i = 0; i < m_numChannels; ++i)
  {
    SoundInstanceId soundId = m_channels[i];
    SoundInstance* instance = GetSoundInstance(soundId);
    bool instanceMatch = !_id.IsValid() || instance->m_objId == _id;

    if (instance && instanceMatch && stricmp(instance->m_eventName, _eventName) == 0)
      ++result;
  }

  return result;
}

int SoundSystem::NumInstances(WorldObjectId _id, const char* _eventName)
{
  int result = 0;

  for (int i = 0; i < m_sounds.Size(); ++i)
  {
    if (m_sounds.ValidIndex(i))
    {
      SoundInstance* instance = m_sounds[i].get();
      bool instanceMatch = !_id.IsValid() || instance->m_objId == _id;

      if (instance && instanceMatch && stricmp(instance->m_eventName, _eventName) == 0)
        ++result;
    }
  }

  return result;
}

int SoundSystem::NumSoundInstances() { return m_sounds.NumUsed(); }

int SoundSystem::NumChannelsUsed()
{
  int numUsed = 0;
  for (int i = 0; i < m_numChannels; ++i)
  {
    SoundInstanceId id = m_channels[i];
    if (GetSoundInstance(id) != nullptr)
      numUsed++;
  }

  return numUsed;
}

int SoundSystem::NumSoundsDiscarded() { return NumSoundInstances() - NumChannelsUsed(); }

int SoundInstanceCompare(const void* elem1, const void* elem2)
{
  SoundInstanceId id1 = *((SoundInstanceId*)elem1);
  SoundInstanceId id2 = *((SoundInstanceId*)elem2);

  SoundInstance* instance1 = g_soundSystem->GetSoundInstance(id1);
  SoundInstance* instance2 = g_soundSystem->GetSoundInstance(id2);

  DEBUG_ASSERT(instance1);
  DEBUG_ASSERT(instance2);

  if (instance1->m_perceivedVolume < instance2->m_perceivedVolume)
    return +1;
  if (instance1->m_perceivedVolume > instance2->m_perceivedVolume)
    return -1;
  return 0;
}

void SoundSystem::Advance()
{
  if (g_requestQuit && !m_quitWithoutSave)
  {
    if (AreBlueprintsModified())
      g_requestQuit = false;
  }

  if (!m_channels)
    return;

#ifdef PROFILER_ENABLED
  m_mainProfiler->Advance();
  m_eventProfiler->Advance();
#endif

  m_timeSync += g_advanceTime;
  if (m_timeSync >= SOUNDSYSTEM_UPDATEPERIOD)
  {
    m_timeSync -= SOUNDSYSTEM_UPDATEPERIOD;

    // DEVICE RECOVERY. A backend with no output device gets one rebuild attempt
    // every DeviceRetryPeriod seconds, for as long as it has none.
    // RestartSoundLibrary is the same call the options window's Apply button
    // makes, so a mid-game rebuild is a supported thing to do rather than a new
    // risk taken here.
    //
    // m_hadOutputDevice is what stops that from being a busy loop on a machine
    // with no sound card: retries begin only once a device has been seen
    // working at least once. It is remembered HERE rather than asked of the
    // backend because RestartSoundLibrary destroys the backend — a rebuilt one
    // that also fails has no memory of anything having worked, and the retries
    // would stop on the first failed attempt, which is exactly the case they
    // exist for.
    if (g_soundLibrary3d)
    {
      if (g_soundLibrary3d->HasOutputDevice())
      {
        m_hadOutputDevice = true;
        m_deviceRetryTimer = 0.0f;
      }
      else if (m_hadOutputDevice)
      {
        m_deviceRetryTimer += SOUNDSYSTEM_UPDATEPERIOD;
        if (m_deviceRetryTimer >= DeviceRetryPeriod)
        {
          m_deviceRetryTimer = 0.0f;
          RestartSoundLibrary();
        }
      }
    }

    START_PROFILE(g_profiler, "Advance SoundSystem");

    //
    // Advance music

    if (m_music)
    {
      bool amIDone = m_music->Advance();
      if (amIDone)
      {
        START_PROFILE(g_profiler, "Shutdown Music");
        // Never registered in m_sounds, so it is released here rather than
        // through ShutdownSound -- that is what "registered instances only"
        // buys. Same program point as before.
        m_music->StopPlaying();
        m_music.reset();
        END_PROFILE(g_profiler, "Shutdown Music");
      }
    }

    if (!m_music && m_requestedMusic)
    {
      m_music = std::move(m_requestedMusic);
      m_music->OpenStream(false);
      m_music->m_channelIndex = g_soundLibrary3d->m_musicChannelId;
      g_soundLibrary3d->ResetChannel(m_music->m_channelIndex);
      g_soundLibrary3d->SetChannelFrequency(m_music->m_channelIndex, m_music->m_cachedSampleHandle->m_cachedSample->m_freq);
    }

    //
    // Resync with blueprints (changed by editor)

    START_PROFILE(g_profiler, "Propagate Blueprints");
    if (m_propagateBlueprints)
      PropagateBlueprints();
    END_PROFILE(g_profiler, "Propagate Blueprints");

    //
    // First pass : Recalculate all Perceived Sound Volumes
    // Throw away sounds that have had their chance
    // Build a list of instanceIDs for sorting

    START_PROFILE(g_profiler, "Allocate Sorted Array");
    static int sortedArraySize = 128;
    static SoundInstanceId* sortedIds = nullptr;
    if (m_sounds.NumUsed() > sortedArraySize)
    {
      delete[] sortedIds;
      sortedIds = nullptr;
      while (sortedArraySize < m_sounds.NumUsed())
      {
        sortedArraySize *= 2;
      }
    }
    if (!sortedIds)
      sortedIds = new SoundInstanceId[sortedArraySize];

    int numSortedIds = 0;
    END_PROFILE(g_profiler, "Allocate Sorted Array");

    START_PROFILE(g_profiler, "Perceived Volumes");
    for (int i = 0; i < m_sounds.Size(); ++i)
    {
      if (m_sounds.ValidIndex(i))
      {
        SoundInstance* instance = m_sounds[i].get();
        if (!instance->IsPlaying() && !instance->m_loopType)
          instance->m_restartAttempts--;
        if (instance->m_restartAttempts < 0)
          ShutdownSound(instance);
        else if (instance->m_positionType == SoundInstance::Type3DAttachedToObject && !instance->ResolveAttachedObject())
          ShutdownSound(instance);
        else
        {
          instance->CalculatePerceivedVolume();
          sortedIds[numSortedIds] = instance->m_id;
          numSortedIds++;
        }
      }
    }
    END_PROFILE(g_profiler, "Perceived Volumes");

    //
    // Sort sounds into perceived volume order
    // NOTE : There are exactly numSortedId elements in sortedIds.
    // NOTE : It isn't safe to assume numSortedIds == m_sounds.NumUsed()

    START_PROFILE(g_profiler, "Sort Samples");
    qsort(sortedIds, numSortedIds, sizeof(SoundInstanceId), SoundInstanceCompare);
    END_PROFILE(g_profiler, "Sort Samples");

    //
    // Second pass : Recalculate all Sound Priorities starting with the nearest sounds
    // Reduce priorities as more of the same sounds are played

    // Keyed on m_eventName, never iterated — only looked up, inserted and
    // decayed — so an unordered map is a faithful replacement here. Nothing
    // downstream depends on the order these were visited in.
    std::unordered_map<std::string, float> existingInstances;

    //
    // Also look out for the highest priority new sound to swap in

    START_PROFILE(g_profiler, "Recalculate Priorities");

    SoundInstance* newInstance = nullptr;
    float highestInstancePriority = 0.0f;

    for (int i = 0; i < numSortedIds; ++i)
    {
      SoundInstanceId id = sortedIds[i];
      SoundInstance* instance = GetSoundInstance(id);
      DEBUG_ASSERT(instance);

      instance->m_calculatedPriority = instance->m_perceivedVolume;

      auto existingInstance = existingInstances.find(instance->m_eventName);
      if (existingInstance != existingInstances.end())
      {
        instance->m_calculatedPriority *= existingInstance->second;
        existingInstance->second *= 0.75f;
      }
      else
        existingInstances.emplace(instance->m_eventName, 0.75f);

      if (!instance->IsPlaying() && instance->m_calculatedPriority > highestInstancePriority)
      {
        newInstance = instance;
        highestInstancePriority = instance->m_calculatedPriority;
      }
    }

    END_PROFILE(g_profiler, "Recalculate Priorities");

    if (newInstance)
    {
      // Find worst old sound to get rid of
      START_PROFILE(g_profiler, "Find best Channel");
      int bestAvailableChannel = FindBestAvailableChannel();
      END_PROFILE(g_profiler, "Find best Channel");

      START_PROFILE(g_profiler, "Stop Old Sound");
      // Stop the old sound
      SoundInstance* existingInstance = GetSoundInstance(m_channels[bestAvailableChannel]);
      if (existingInstance && !existingInstance->m_loopType)
        ShutdownSound(existingInstance);
      else if (existingInstance)
        existingInstance->StopPlaying();
      END_PROFILE(g_profiler, "Stop Old Sound");

      START_PROFILE(g_profiler, "Start New Sound");
      // Start the new sound
      bool success = newInstance->StartPlaying(bestAvailableChannel);
      if (success)
        m_channels[bestAvailableChannel] = newInstance->m_id;
      else
      {
        // This is fairly bad, the sound failed to play
        // Which means it failed to load, or to go into a channel
        ShutdownSound(newInstance);
      }
      END_PROFILE(g_profiler, "Start New Sound");

      START_PROFILE(g_profiler, "Reset Channel");
      g_soundLibrary3d->ResetChannel(bestAvailableChannel);
      END_PROFILE(g_profiler, "Reset Channel");
    }

    //
    // Advance all sound channels

    START_PROFILE(g_profiler, "Advance All Channels");
    for (int i = 0; i < m_numChannels; ++i)
    {
      SoundInstanceId soundId = m_channels[i];
      SoundInstance* currentSound = GetSoundInstance(soundId);
      if (currentSound)
      {
        bool amIDone = currentSound->Advance();
        if (amIDone)
        {
          START_PROFILE(g_profiler, "Shutdown Sound");
          ShutdownSound(currentSound);
          END_PROFILE(g_profiler, "Shutdown Sound");
        }
      }
    }
    END_PROFILE(g_profiler, "Advance All Channels");

    //
    // Update our listener position

    START_PROFILE(g_profiler, "UpdateListener");

    DirectX::XMFLOAT3 camUp = g_camera->GetUp();
    if (g_prefsManager->GetInt("SoundSwapStereo", 0) == 0)
      DirectX::XMStoreFloat3(&camUp, DirectX::XMVectorNegate(DirectX::XMLoadFloat3(&camUp)));

    DirectX::XMFLOAT3 const cameraVelocity = g_camera->GetVel();
    DirectX::XMFLOAT3 camVel;
    DirectX::XMStoreFloat3(&camVel, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&cameraVelocity), 0.2f));
    g_soundLibrary3d->SetListenerPosition(g_camera->GetPos(), g_camera->GetFront(), camUp, camVel);

    END_PROFILE(g_profiler, "UpdateListener");

    //
    // Advance our sound library

    START_PROFILE(g_profiler, "SoundLibrary3d Advance");
    g_soundLibrary3d->Advance();
    END_PROFILE(g_profiler, "SoundLibrary3d Advance");

    END_PROFILE(g_profiler, "Advance SoundSystem");
  }
}

void SoundSystem::RuntimeVerify()
{
  //
  // Make sure there aren't any SoundInstances on more than one channel
  // Make sure all playing samples have sensible channel handles

  /*
      for( int i = 0; i < m_numChannels; ++i )
      {
          SoundInstanceId id1 = m_channels[i];
          SoundInstance *currentSound = GetSoundInstance( id1 );
          DEBUG_ASSERT( !currentSound ||
                       !(currentSound->IsPlaying() && currentSound->m_channelIndex == -1) );

          if( currentSound )
          {
              for( int j = 0; j < m_numChannels; ++j )
              {
                  if( i != j )
                  {
                      SoundInstanceId id2 = m_channels[j];
                      if( GetSoundInstance(id2) )
                      {
                          DEBUG_ASSERT( !(id1 == id2) );
                      }
                  }
              }
          }
      }
  */

  //
  // Make sure all sounds that believe they are playing have an opened sound stream

  for (int i = 0; i < m_numChannels; ++i)
  {
    SoundInstanceId id1 = m_channels[i];
    SoundInstance* currentSound = GetSoundInstance(id1);
    if (currentSound && !currentSound->m_cachedSampleHandle)
      int b = 10;
  }
}

void SoundSystem::LoadtimeVerify()
{
  // If you want to comment this out then comment out the call.

  //
  // Verify that the samples referred to in SampleGroups are valid

  FILE* soundErrors = fopen("sounderrors.txt", "wt");
  bool errorFound = false;

  for (int i = 0; i < m_sampleGroups.Size(); ++i)
  {
    if (!m_sampleGroups.ValidIndex(i))
      continue;

    SampleGroup* sg = m_sampleGroups[i].get();
    int size = static_cast<int>(sg->m_samples.size());
    for (int j = 0; j < size; ++j)
    {
      std::string const& soundName = sg->m_samples[j];
      const char* err = IsSoundSourceOK(soundName.c_str());
      if (err != nullptr)
      {
        fprintf(soundErrors, "%s: %s In sound group %s\n", err, soundName.c_str(), sg->m_name);
        errorFound = true;
      }
    }
  }

  //
  // Verify that the samples referred to in SoundEventBlueprints are valid

  // Entities
  for (int i = 0; i < m_entityBlueprints.Size(); ++i)
  {
    if (!m_entityBlueprints.ValidIndex(i))
      continue;

    SoundSourceBlueprint* ssb = m_entityBlueprints[i].get();
    int size = static_cast<int>(ssb->m_events.size());

    for (int j = 0; j < size; ++j)
    {
      SoundEventBlueprint* seb = ssb->m_events[j];
      SoundInstance* si = seb->m_instance;
      if (si->m_sourceType == SoundInstance::Sample)
      {
        const char* err = IsSoundSourceOK(si->m_soundName);
        if (err != nullptr)
        {
          fprintf(soundErrors, "%s: %s In entity %s, event %s\n", err, si->m_soundName, g_worldTypeNames->EntityTypeName(i),
                  seb->m_eventName.c_str());
          errorFound = true;
        }
      }
      else
      {
        // Just need to verify that the group exists here
        SampleGroup* sg = GetSampleGroup(si->m_soundName);
        if (!sg)
        {
          fprintf(soundErrors,
                  "Sound Group %s does not exist. "
                  "Referenced by entity %s, event %s\n",
                  si->m_soundName, g_worldTypeNames->EntityTypeName(i), seb->m_eventName.c_str());
          errorFound = true;
        }
      }
    }
  }

  // Buildings
  for (int i = 0; i < m_buildingBlueprints.Size(); ++i)
  {
    if (!m_buildingBlueprints.ValidIndex(i))
      continue;

    SoundSourceBlueprint* ssb = m_buildingBlueprints[i].get();
    int size = static_cast<int>(ssb->m_events.size());

    for (int j = 0; j < size; ++j)
    {
      SoundEventBlueprint* seb = ssb->m_events[j];
      SoundInstance* si = seb->m_instance;
      if (si->m_sourceType == SoundInstance::Sample)
      {
        const char* err = IsSoundSourceOK(si->m_soundName);
        if (err != nullptr)
        {
          fprintf(soundErrors, "%s: %s In building %s, event %s\n", err, si->m_soundName, g_worldTypeNames->BuildingTypeName(i),
                  seb->m_eventName.c_str());
          errorFound = true;
        }
      }
      else
      {
        // Just need to verify that the group exists here
        SampleGroup* sg = GetSampleGroup(si->m_soundName);
        if (!sg)
        {
          fprintf(soundErrors,
                  "Sound Group %s does not exist. "
                  "Referenced by building %s, event %s\n",
                  si->m_soundName, g_worldTypeNames->BuildingTypeName(i), seb->m_eventName.c_str());
          errorFound = true;
        }
      }
    }
  }

  // Others
  for (int i = 0; i < m_otherBlueprints.Size(); ++i)
  {
    if (!m_otherBlueprints.ValidIndex(i))
      continue;

    SoundSourceBlueprint* ssb = m_otherBlueprints[i].get();
    int size = static_cast<int>(ssb->m_events.size());

    for (int j = 0; j < size; ++j)
    {
      SoundEventBlueprint* seb = ssb->m_events[j];
      SoundInstance* si = seb->m_instance;
      if (si->m_sourceType == SoundInstance::Sample)
      {
        const char* err = IsSoundSourceOK(si->m_soundName);
        if (err != nullptr)
        {
          fprintf(soundErrors, "%s: %s In Others %s, event %s\n", err, si->m_soundName, ssb->GetSoundSourceName(i), seb->m_eventName.c_str());
          errorFound = true;
        }
      }
      else
      {
        // Just need to verify that the group exists here
        SampleGroup* sg = GetSampleGroup(si->m_soundName);
        if (!sg)
        {
          fprintf(soundErrors,
                  "Sound Group %s does not exist. "
                  "Referenced by \"others\" %s, event %s\n",
                  si->m_soundName, ssb->GetSoundSourceName(i), seb->m_eventName.c_str());
          errorFound = true;
        }
      }
    }
  }

  fclose(soundErrors);
  ASSERT_TEXT(!errorFound, "Errors found in sounds.txt : refer to sounderrors.txt for details");
}

static const char* g_soundSourceErrors[SoundSystem::SoundSourceNumErrors] = {nullptr, "Sound sample not mono", "Sound sample does not exist",
                                                                             "Sound sample name contains spaces"};

const char* SoundSystem::IsSoundSourceOK(const char* _soundName)
{
#ifdef _DEBUG
  if (strchr(_soundName, ' '))
  {
    // File name contains a space
    return g_soundSourceErrors[SoundSourceFilenameHasSpace];
  }

  const std::string fullPath = std::format("Sounds/{}", _soundName);

  SoundStreamDecoder* sound = g_resource->GetSoundStreamDecoder(fullPath.c_str());
  if (!sound)
  {
    // File does not exist
    return g_soundSourceErrors[SoundSourceFileDoesNotExist];
  }

  if (sound->m_numChannels != 1)
  {
    // File isn't mono
    return g_soundSourceErrors[SoundSourceNotMono];
  }
  delete sound;
#endif

  // Everything is dandy
  return g_soundSourceErrors[SoundSourceNoError];
}

bool SoundSystem::IsSampleUsed(const char* _soundName)
{
  //
  // Entity blueprints

  for (int i = 0; i < m_entityBlueprints.Size(); ++i)
  {
    if (m_entityBlueprints.ValidIndex(i))
    {
      SoundSourceBlueprint* sourceBlueprint = m_entityBlueprints[i].get();
      for (int j = 0; j < static_cast<int>(sourceBlueprint->m_events.size()); ++j)
      {
        SoundEventBlueprint* eventBlueprint = sourceBlueprint->m_events[j];
        if (eventBlueprint->m_instance)
        {
          SoundInstance* instance = eventBlueprint->m_instance;
          if (instance->m_sourceType == SoundInstance::Sample && stricmp(instance->m_soundName, _soundName) == 0)
            return true;
          if (instance->m_sourceType != SoundInstance::Sample)
          {
            SampleGroup* group = GetSampleGroup(instance->m_soundName);
            if (group)
            {
              for (int k = 0; k < static_cast<int>(group->m_samples.size()); ++k)
              {
                std::string const& thisSample = group->m_samples[k];
                if (stricmp(thisSample.c_str(), _soundName) == 0)
                  return true;
              }
            }
          }
        }
      }
    }
  }

  //
  // Building blueprints

  for (int i = 0; i < m_buildingBlueprints.Size(); ++i)
  {
    if (m_buildingBlueprints.ValidIndex(i))
    {
      SoundSourceBlueprint* sourceBlueprint = m_buildingBlueprints[i].get();
      for (int j = 0; j < static_cast<int>(sourceBlueprint->m_events.size()); ++j)
      {
        SoundEventBlueprint* eventBlueprint = sourceBlueprint->m_events[j];
        if (eventBlueprint->m_instance)
        {
          SoundInstance* instance = eventBlueprint->m_instance;
          if (instance->m_sourceType == SoundInstance::Sample && stricmp(instance->m_soundName, _soundName) == 0)
            return true;
          if (instance->m_sourceType != SoundInstance::Sample)
          {
            SampleGroup* group = GetSampleGroup(instance->m_soundName);
            if (group)
            {
              for (int k = 0; k < static_cast<int>(group->m_samples.size()); ++k)
              {
                std::string const& thisSample = group->m_samples[k];
                if (stricmp(thisSample.c_str(), _soundName) == 0)
                  return true;
              }
            }
          }
        }
      }
    }
  }

  //
  // Other blueprints

  for (int i = 0; i < m_otherBlueprints.Size(); ++i)
  {
    if (m_otherBlueprints.ValidIndex(i))
    {
      SoundSourceBlueprint* sourceBlueprint = m_otherBlueprints[i].get();
      for (int j = 0; j < static_cast<int>(sourceBlueprint->m_events.size()); ++j)
      {
        SoundEventBlueprint* eventBlueprint = sourceBlueprint->m_events[j];
        if (eventBlueprint->m_instance)
        {
          SoundInstance* instance = eventBlueprint->m_instance;
          if (instance->m_sourceType == SoundInstance::Sample && stricmp(instance->m_soundName, _soundName) == 0)
            return true;
          if (instance->m_sourceType != SoundInstance::Sample)
          {
            SampleGroup* group = GetSampleGroup(instance->m_soundName);
            if (group)
            {
              for (int k = 0; k < static_cast<int>(group->m_samples.size()); ++k)
              {
                std::string const& thisSample = group->m_samples[k];
                if (stricmp(thisSample.c_str(), _soundName) == 0)
                  return true;
              }
            }
          }
        }
      }
    }
  }

  return false;
}

void SoundSystem::PropagateBlueprints()
{
  for (int i = 0; i < m_sounds.Size(); ++i)
  {
    if (m_sounds.ValidIndex(i))
    {
      SoundInstance* instance = m_sounds[i].get();
      instance->PropagateBlueprints();
    }
  }
}

SampleGroup* SoundSystem::GetSampleGroup(const char* _name)
{
  for (int i = 0; i < m_sampleGroups.Size(); ++i)
  {
    if (m_sampleGroups.ValidIndex(i))
    {
      SampleGroup* group = m_sampleGroups[i].get();
      if (strcmp(group->m_name, _name) == 0)
        return group;
    }
  }

  return nullptr;
}

SampleGroup* SoundSystem::NewSampleGroup(const char* _name)
{
  auto owned = std::make_unique<SampleGroup>();
  SampleGroup* group = owned.get();
  m_sampleGroups.PutData(std::move(owned));

  if (_name)
  {
    group->SetName(_name);
    return group;
  }
  int i = 1;
  while (true)
  {
    const std::string nameCandidate = std::format("newsamplegroup{}", i);
    if (!GetSampleGroup(nameCandidate.c_str()))
    {
      group->SetName(nameCandidate.c_str());
      return group;
    }
    ++i;
  }
}

bool SoundSystem::RenameSampleGroup(const char* _oldName, const char* _newName)
{
  //
  // Check the new name is unique

  if (GetSampleGroup(_newName))
    return false;

  //
  // Rename it

  SampleGroup* group = GetSampleGroup(_oldName);
  if (group)
  {
    group->SetName(_newName);

    //
    // Update entity blueprints

    for (int i = 0; i < m_entityBlueprints.Size(); ++i)
    {
      if (m_entityBlueprints.ValidIndex(i))
      {
        SoundSourceBlueprint* blueprint = m_entityBlueprints[i].get();
        for (int j = 0; j < static_cast<int>(blueprint->m_events.size()); ++j)
        {
          SoundEventBlueprint* eventBlueprint = blueprint->m_events[j];
          if (eventBlueprint->m_instance && eventBlueprint->m_instance->m_sourceType > SoundInstance::Sample &&
              strcmp(eventBlueprint->m_instance->m_soundName, _oldName) == 0)
            eventBlueprint->m_instance->SetSoundName(_newName);
        }
      }
    }

    //
    // Update building blueprints

    for (int i = 0; i < m_buildingBlueprints.Size(); ++i)
    {
      if (m_buildingBlueprints.ValidIndex(i))
      {
        SoundSourceBlueprint* blueprint = m_buildingBlueprints[i].get();
        for (int j = 0; j < static_cast<int>(blueprint->m_events.size()); ++j)
        {
          SoundEventBlueprint* eventBlueprint = blueprint->m_events[j];
          if (eventBlueprint->m_instance && eventBlueprint->m_instance->m_sourceType > SoundInstance::Sample &&
              strcmp(eventBlueprint->m_instance->m_soundName, _oldName) == 0)
            eventBlueprint->m_instance->SetSoundName(_newName);
        }
      }
    }

    //
    // Update other blueprints

    for (int i = 0; i < m_otherBlueprints.Size(); ++i)
    {
      if (m_otherBlueprints.ValidIndex(i))
      {
        SoundSourceBlueprint* blueprint = m_otherBlueprints[i].get();
        for (int j = 0; j < static_cast<int>(blueprint->m_events.size()); ++j)
        {
          SoundEventBlueprint* eventBlueprint = blueprint->m_events[j];
          if (eventBlueprint->m_instance && eventBlueprint->m_instance->m_sourceType > SoundInstance::Sample &&
              strcmp(eventBlueprint->m_instance->m_soundName, _oldName) == 0)
            eventBlueprint->m_instance->SetSoundName(_newName);
        }
      }
    }
  }

  return true;
}
} // namespace Neuron
