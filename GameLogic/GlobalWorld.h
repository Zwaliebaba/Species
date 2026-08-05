#pragma once

#include <memory>
#include <string>
#include <vector>

#include "SphereRenderer.h"
#include "NeuronMath.h"



// ****************************************************************************
// GlobalLocation
// ****************************************************************************

namespace Neuron
{
  class FileWriter;
  class TextReader;
  class Shape;
} // namespace Neuron


namespace Species
{
  class Building;
  class GlobalInternet;

  class GlobalLocation
  {
    public:
      int m_id;
      DirectX::XMFLOAT3 m_pos{0.0f, 0.0f, 0.0f};
      bool m_available; // Is it connected on the transit system

      std::string m_name;
      std::string m_mapFilename;
      std::string m_missionFilename;
      bool m_missionCompleted;

      int m_numSpirits; // Number of spirits that have died

    public:
      GlobalLocation();

      void AddSpirits(int _count = 1);
  };


  // ****************************************************************************
  // GlobalBuilding
  // ****************************************************************************

  class GlobalBuilding
  {
    public:
      int m_id;
      int m_teamId;
      int m_locationId;
      DirectX::XMFLOAT3 m_pos{0.0f, 0.0f, 0.0f};
      int m_type;
      bool m_online;
      int m_link;
      Shape* m_shape;

    public:
      GlobalBuilding();
  };


  // ****************************************************************************
  // Class GlobalEvent + guests
  // ****************************************************************************

  class GlobalEventCondition
  {
    public:
      enum
      {
        AlwaysTrue,      // 0
        BuildingOnline,  // 1
        BuildingOffline, // 2
        ResearchOwned,   // 3
        NotInLocation,   // 4
        DebugKey,        // 5
        NeverTrue,       // 6
        NumConditions    // Remember to update GetTypeName
      };
      int m_type;
      int m_id;
      int m_locationId;
      char* m_stringId; // Brief description
      char* m_cutScene; // Filename of cutscene to run

    public:
      GlobalEventCondition();
      GlobalEventCondition(const GlobalEventCondition& _other);
      ~GlobalEventCondition();

      bool Evaluate();

      void SetStringId(char const* _stringId);
      void SetCutScene(char* _cutScene);

      void Save(FileWriter* _out);

      static char const* GetTypeName(int _type);
      static int GetType(char const* _typeName);
  };


  class GlobalEventAction
  {
    public:
      enum
      {
        SetMission,
        RunScript,
        MakeAvailable,
        NumActionTypes
      };
      int m_type;
      int m_locationId;
      std::string m_filename;

    public:
      GlobalEventAction();

      void Read(TextReader* _in);
      void Write(FileWriter* _file);
      void Execute();

      static char const* GetTypeName(int _type);
  };


  class GlobalEvent
  {
    public:
      // Owns both. Before ownership T11 this class had no destructor at all, so
      // every condition and every unexecuted action leaked with the event.
      std::vector<std::unique_ptr<GlobalEventCondition>> m_conditions;
      std::vector<std::unique_ptr<GlobalEventAction>> m_actions;

    public:
      GlobalEvent();
      GlobalEvent(GlobalEvent& _other); // Copy constructor only used by TestHarness

      void Read(TextReader* _in);
      void Write(FileWriter* _file);
      bool Evaluate();
      bool Execute(); // Returns true when all done

      void MakeAlwaysTrue();
  };


  // ****************************************************************************
  // Class GlobalResearch
  // ****************************************************************************

#define GLOBALRESEARCH_TIMEPERPOINT 10
#define GLOBALRESEARCH_POINTS_CONTROLTOWER 22

class GlobalResearch
{
  public:
    enum
    {
      TypeCitizen,
      TypeOfficer,
      TypeSquad,
      TypeLaser,
      TypeGrenade,
      TypeRocket,
      TypeController,
      TypeAirStrike,
      TypeArmour,
      TypeTaskManager,
      TypeEngineer,
      NumResearchItems
    };

    int m_researchLevel[NumResearchItems];
    int m_researchProgress[NumResearchItems];
    int m_currentResearch;
    int m_researchPoints;
    float m_researchTimer;

  public:
    GlobalResearch();

    bool HasResearch(int _type);
    int CurrentProgress(int _type);
    int CurrentLevel(int _type);

    void AddResearch(int _type);
    void SetCurrentProgress(int _type, int _progress);

    void IncreaseProgress(int _amount);
    void DecreaseProgress(int _amount);
    int RequiredProgress(int _level); // Progress required to reach this level

    void EvaluateLevel(int _type);

    void SetCurrentResearch(int _type);
    void GiveResearchPoints(int _numPoints);
    void AdvanceResearch();

    void Write(FileWriter* _out);
    void Read(TextReader* _in);

    static char const* GetTypeName(int _type);
    static int GetType(char* _name);

    static char const* GetTypeNameTranslated(int _type);
};


// ****************************************************************************
// Class SphereWorld
// ****************************************************************************

class SphereWorld
{
  public:
    Shape* m_shapeOuter;
    Shape* m_shapeMiddle;
    Shape* m_shapeInner;

    // One list of in-flight spirit positions per location, indexed by location
    // id. This was a hand-grown array of lists: AddLocation allocated a bigger
    // block, copied the old lists across and deleted the old block, with
    // m_numLocations tracking the length by hand. resize does all of that.
    std::vector<std::vector<float>> m_spirits;

  public:
    SphereWorld();

    void AddLocation(int _locationId);

    void Render();
    void RenderWorldShape();
    void RenderIslands();
    void RenderTrunkLinks();
    void RenderHeaven();
    void RenderSpirits();
};


// ****************************************************************************
// Class GlobalWorld
// ****************************************************************************

class GlobalWorld
{
  public:
    // GlobalInternet is only forward-declared here, so ~GlobalWorld stays
    // declared here and defined in the .cpp where the type is complete.
    std::unique_ptr<GlobalInternet> m_globalInternet;
    std::unique_ptr<SphereWorld> m_sphereWorld;
    std::unique_ptr<GlobalResearch> m_research;

    // GlobalWorld::m_buildings is NOT Location::m_buildings, which is a
    // FastSlotMap whose indices are network identity, and is NOT
    // LevelFile::m_buildings. Four members share the name, which is why
    // check_containers.py skips it.
    std::vector<std::unique_ptr<GlobalLocation>> m_locations;
    std::vector<std::unique_ptr<GlobalBuilding>> m_buildings;
    std::vector<std::unique_ptr<GlobalEvent>> m_events;
    int m_myTeamId;

    int m_editorMode;
    int m_editorSelectionId;

  protected:
    void WriteLocations(FileWriter* _out);
    void WriteBuildings(FileWriter* _out);
    void WriteEvents(FileWriter* _out);
    void WriteTutorial(FileWriter* _out);

    void ParseLocations(TextReader* _in);
    void ParseBuildings(TextReader* _in);
    void ParseEvents(TextReader* _in);
    void ParseTutorial(TextReader* _in);

    void AddLevelBuildingToGlobalBuildings(Building* _building, int _locId);

    int m_nextLocationId;
    int m_nextBuildingId;

    int m_locationRequested; // Stores the location a user has clicked on while we fade out. -1 means no request yet.

  public:
    GlobalWorld();
    GlobalWorld(GlobalWorld&); // Copy constructor only used in TestHarness
    ~GlobalWorld();

    void Advance();
    void Render();

    int LocationHit(DirectX::XMFLOAT3 const& _pos, DirectX::XMFLOAT3 const& _dir, float locationRadius = 5000.0f);

    // Both take ownership, and say so in the signature rather than in a
    // comment a caller can miss.
    void AddLocation(std::unique_ptr<GlobalLocation> _location);
    void AddBuilding(std::unique_ptr<GlobalBuilding> _building);

    GlobalLocation* GetLocation(int _id);
    GlobalLocation* GetLocation(char const* _name);
    GlobalLocation* GetHighlightedLocation(); // ie whats under the mouse
    int GetLocationId(char const* _name);
    int GetLocationIdFromMapFilename(char const* _mapFilename);
    // Both answer nullptr for an id no location has, and Species/Main.cpp
    // tests that before transferring spirits — which is why these return a
    // pointer rather than a std::string that could only say "empty".
    char const* GetLocationName(int _id);
    char const* GetLocationNameTranslated(int _id);
    DirectX::XMFLOAT3 GetLocationPosition(int _id);

    GlobalBuilding* GetBuilding(int _id, int _locationId);
    int GenerateBuildingId();

    bool EvaluateEvents(); // Returns true if an event was triggered
    void TransferSpirits(int _locationId);

    void LoadGame(char const* _filename);
    void SaveGame(char const* _filename);

    void LoadLocations(char const* _filename);
    void SaveLocations(char const* _filename);

    void SetupLights();
    void SetupFog();

    float GetSize();
};
} // namespace Species
