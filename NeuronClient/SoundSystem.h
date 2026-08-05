#pragma once

#include <memory>
#include <string_view>

#include <vector>

#include "SlotMap.h"
#include "NeuronMath.h"
#include "SoundInstance.h"
#include "SoundSource.h"
#include "WorldObjectId.h"

class Profiler;

//*****************************************************************************
// Class SoundEventBlueprint
//*****************************************************************************


namespace Neuron
{
  class TextReader;
  class SoundInstance;
  class FileWriter;

  class SoundEventBlueprint
  {
    public:
      char* m_eventName;
      SoundInstance* m_instance;

      SoundEventBlueprint();

      void SetEventName(const char* _name);
  };

  //*****************************************************************************
  // Class SoundSourceBlueprint
  //*****************************************************************************

  class SoundSourceBlueprint
  {
    public:
      enum // Other types of Sound Source
      {
        TypeLaser,
        TypeGrenade,
        TypeRocket,
        TypeAirstrikeBomb,
        TypeSpirit,
        TypeSepulveda,
        TypeGesture,
        TypeAmbience,
        TypeMusic,
        TypeInterface,
        NumOtherSoundSources
      };

      std::vector<SoundEventBlueprint*> m_events;

      static int GetSoundSoundType(const char* _name);
      static const char* GetSoundSourceName(int _type);
      static void ListSoundEvents(int _type, std::vector<const char*>* _list);
  };

  //*****************************************************************************
  // Class DspParameterBlueprint
  //*****************************************************************************

  class DspParameterBlueprint
  {
    public:
      char m_name[256];
      float m_min;
      float m_max;
      float m_default;
      int m_dataType; // 0 = float, 1 = long
  };

  //*****************************************************************************
  // Class DspBlueprint
  //*****************************************************************************

  class DspBlueprint
  {
    public:
      char m_name[256];
      SlotMap<DspParameterBlueprint*> m_params;

      char* GetParameter(int _param, float* _min = nullptr, float* _max = nullptr, float* _default = nullptr, int* _dataType = nullptr);
  };

  //*****************************************************************************
  // Class SampleGroup
  //*****************************************************************************

  class SampleGroup
  {
    public:
      char m_name[256];
      std::vector<char*> m_samples;

      ~SampleGroup();
      void SetName(const char* _name);
      void AddSample(const char* _sample);
  };

  //*****************************************************************************
  // Class SoundSystem
  //*****************************************************************************

  class SoundSystem
  {
    public:
      enum
      {
        SoundSourceNoError,
        // Remember to update g_soundSourceErrors in soundsystem.cpp
        SoundSourceNotMono,
        SoundSourceFileDoesNotExist,
        SoundSourceFilenameHasSpace,
        SoundSourceNumErrors
      };

      float m_timeSync;
      bool m_propagateBlueprints; // If true, looping sounds will sync with blueprints
      Profiler* m_mainProfiler;
      Profiler* m_eventProfiler;
      bool m_quitWithoutSave;

      DirectX::XMFLOAT3 m_editorPos{0.0f, 0.0f, 0.0f};
      SoundInstanceId m_editorInstanceId;

      // OWNING. A slot holds the instance until ShutdownSound resets it, and
      // nothing else deletes a registered instance.
      FastSlotMap<std::unique_ptr<SoundInstance>> m_sounds; // All the sounds that want to play
      // OWNING, and deliberately NOT in m_sounds: the music instance is never
      // registered, which is what used to make ShutdownSound serve two
      // populations. It is shut down by resetting these, not through
      // ShutdownSound.
      std::unique_ptr<SoundInstance> m_music; // There can only be one piece of music at a time
      std::unique_ptr<SoundInstance> m_requestedMusic;

      SoundInstanceId* m_channels;
      int m_numChannels;

      SlotMap<std::unique_ptr<SoundSourceBlueprint>> m_entityBlueprints;   // Indexed on Entity::m_type
      SlotMap<std::unique_ptr<SoundSourceBlueprint>> m_buildingBlueprints; // Indexed on Building::m_type
      SlotMap<std::unique_ptr<SoundSourceBlueprint>> m_otherBlueprints;    // Indexed on SoundSourceBlueprint
      SlotMap<std::unique_ptr<DspBlueprint>> m_filterBlueprints;           // Indexed on SoundLibrary3d::FX types

      SlotMap<std::unique_ptr<SampleGroup>> m_sampleGroups;

    protected:
      void ParseSoundEvent(TextReader* _in, SoundSourceBlueprint* _source, std::string_view _entityName);
      void ParseSoundEffect(TextReader* _in, SoundEventBlueprint* _blueprint);
      void ParseSampleGroup(TextReader* _in, SampleGroup* _group);

      void WriteSoundEvent(FileWriter* _file, SoundEventBlueprint* _event);
      void WriteSampleGroup(FileWriter* _file, SampleGroup* _group);

      void SaveBlueprints(const char* _filename);

      int FindBestAvailableChannel();

      static bool SoundLibraryMainCallback(unsigned int _channel, signed short* _data, unsigned int _numSamples, int* _silenceRemaining);
      static bool SoundLibraryMusicCallback(signed short* _data, unsigned int _numSamples, int* _silenceRemaining);

    public:
      SoundSystem();
      ~SoundSystem();

      void Initialise();
      void RestartSoundLibrary();

      void LoadEffects();
      void LoadBlueprints();
      void SaveBlueprints();
      bool AreBlueprintsModified();

      void Advance();

      // TAKES OWNERSHIP EITHER WAY. On success the instance is registered in
      // m_sounds and lives there; on the monophonic folding path its object ids
      // are merged into the existing instance and it is destroyed here. The
      // caller never holds something it must clean up, which is what the raw
      // parameter made undecidable rather than merely untidy.
      bool InitialiseSound(std::unique_ptr<SoundInstance> _instance);

      // REGISTERED INSTANCES ONLY -- one contract, unlike the raw-pointer
      // version which was called both with instances m_sounds held and with
      // instances it never accepted. Stops the sound and destroys it. The slot
      // is released only when it still holds THIS instance; see the .cpp.
      void ShutdownSound(SoundInstance* _instance);

      int IsSoundPlaying(SoundInstanceId _id);
      int NumInstancesPlaying(WorldObjectId _id, const char* _eventName);
      int NumInstances(WorldObjectId _id, const char* _eventName);

      int NumSoundInstances();
      int NumChannelsUsed();
      int NumSoundsDiscarded();

      // These took an Entity*, a Building* and a WorldObject* until
      // tasks/layering-inversion.yaml T17. Each body read the same four fields
      // off whatever it was handed, so SoundSource carries those instead and no
      // GameLogic type appears in this header. GameLogic/SoundSources.h fills one
      // from a live object.
      void TriggerEntityEvent(SoundSource const& _source, const char* _eventName);
      void TriggerBuildingEvent(SoundSource const& _source, const char* _eventName);

      // A sound with no object behind it — gestures, music, the interface.
      void TriggerOtherEvent(const char* _eventName, int _type);
      void TriggerOtherEvent(SoundSource const& _source, const char* _eventName, int _type);

    private:
      // Both public forms land here. Nullable because the blueprint machinery
      // treats an unattached sound as one with no position of its own.
      void TriggerOtherEvent(SoundSource const* _source, const char* _eventName, int _type);

    public:
      void StopAllSounds(WorldObjectId _id, const char* _eventName = nullptr); // Pass in nullptr to stop every event.
      // Full event name required, eg "Citizen SeenThreat"

      void StopAllDSPEffects();

      void TriggerDuplicateSound(SoundInstance* _instance);

      void PropagateBlueprints(); // Call this to update all looping sounds
      void RuntimeVerify();       // Verifies that the sound system has screwed it's own datastructures
      void LoadtimeVerify();      // Verifies that the data load from sounds.txt is OK
      const char* IsSoundSourceOK(const char* _soundName);
      // Tests that file names and file formats are OK, returns an error code from the SoundSource enum
      bool IsSampleUsed(const char* _soundName); // Looks to see if that sound name is used in any blueprints

      SampleGroup* GetSampleGroup(const char* _name);
      SampleGroup* NewSampleGroup(const char* _name = nullptr);
      bool RenameSampleGroup(const char* _oldName, const char* _newName);

      SoundInstance* GetSoundInstance(SoundInstanceId id);
  };

  // Owned by App, which assigns this during startup. Declared here so the layers
  // below Species can reach the subsystem without including App.h — see
  // tasks/layering-inversion.yaml T8.
  extern SoundSystem* g_soundSystem;
} // namespace Neuron
