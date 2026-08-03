#pragma once

#include <stdlib.h>
#include "WorldObject.h"
#include "Landscape.h"


#define CAMERA_MOUNT_MAX_NAME_LEN 63
#define CAMERA_ANIM_MAX_NAME_LEN 63
#define MAX_FILENAME_LEN 256
#define MAGIC_MOUNT_NAME_START_POS "CamPosBefore"


class Building;
class TextReader;
class Route;
class GlobalEventCondition;
class FileWriter;


class CameraMount
{
  public:
    char m_name[CAMERA_MOUNT_MAX_NAME_LEN + 1];
    Vector3 m_pos;
    Vector3 m_front;
    Vector3 m_up;
};


class CamAnimNode
{
  public:
    enum
    {
      TransitionMove,
      TransitionCut,
      TransitionNumModes
    };

    int m_transitionMode;
    char* m_mountName;
    float m_duration;

  public:
    CamAnimNode()
      : m_mountName(nullptr),
        m_transitionMode(CamAnimNode::TransitionMove),
        m_duration(1.0f)
    {
    }

    ~CamAnimNode()
    {
      free(m_mountName);
      m_mountName = nullptr;
    }

    static int GetTransitModeId(char const* _word);
    static char const* GetTransitModeName(int _modeId);
};


class CameraAnimation
{
  public:
    std::vector<CamAnimNode*> m_nodes;
    char m_name[CAMERA_ANIM_MAX_NAME_LEN + 1];

    ~CameraAnimation()
    {
      for (CamAnimNode* node : m_nodes)
        delete node;
      m_nodes.clear();
    }
};


class InstantUnit
{
  public:
    InstantUnit()
      : m_type(-1),
        m_teamId(-1),
        m_posX(0.0f),
        m_posZ(0.0f),
        m_number(0),
        m_inAUnit(false),
        m_state(-1),
        m_spread(200.0f),
        m_waypointX(0.0f),
        m_waypointZ(0.0f),
        m_routeId(-1),
        m_routeWaypointId(-1)
    {
    }

    int m_type;
    int m_teamId;
    float m_posX;
    float m_posZ;
    int m_number;
    bool m_inAUnit;
    int m_state;
    float m_spread;
    float m_waypointX;
    float m_waypointZ;
    int m_routeId;
    int m_routeWaypointId;
};


class LandscapeFlattenArea
{
  public:
    Vector3 m_centre;
    float m_size;
};


class LandscapeDef
{
  public:
    std::vector<LandscapeTile*> m_tiles;
    std::vector<LandscapeFlattenArea*> m_flattenAreas;
    float m_cellSize;
    int m_worldSizeX;
    int m_worldSizeZ;
    float m_outsideHeight;

    LandscapeDef()
      : m_cellSize(12.0f),
        m_worldSizeX(2000),
        m_worldSizeZ(2000),
        m_outsideHeight(-10)
    {
    }

    ~LandscapeDef()
    {
      for (LandscapeTile* tile : m_tiles)
        delete tile;
      m_tiles.clear();
      for (LandscapeFlattenArea* area : m_flattenAreas)
        delete area;
      m_flattenAreas.clear();
    }
};


class RunningProgram
{
  public:
    int m_type;
    int m_count;
    int m_state;
    int m_data;
    float m_waypointX;
    float m_waypointZ;

    float m_positionX[10];
    float m_positionZ[10];
    int m_health[10];
};


// ***************************************************************************
// Class LevelFile
// ***************************************************************************

class LevelFile
{
  private:
    void ParseMissionFile(char const* _filename);
    void ParseMapFile(char const* _filename);

    void ParseCameraMounts(TextReader* _in);
    void ParseCameraAnims(TextReader* _in);
    void ParseBuildings(TextReader* _in, bool _dynamic);
    void ParseInstantUnits(TextReader* _in);
    void ParseLandscapeData(TextReader* _in);
    void ParseLandscapeTiles(TextReader* _in);
    void ParseLandFlattenAreas(TextReader* _in);
    void ParseLights(TextReader* _in);
    void ParseRoute(TextReader* _in, int _id);
    void ParseRoutes(TextReader* _in);
    void ParsePrimaryObjectives(TextReader* _in);
    void ParseRunningPrograms(TextReader* _in);
    void ParseDifficulty(TextReader* _in);

    void GenerateAutomaticObjectives();

    void WriteCameraMounts(FileWriter* _out);
    void WriteCameraAnims(FileWriter* _out);
    void WriteBuildings(FileWriter* _out, bool _dynamic);
    void WriteInstantUnits(FileWriter* _out);
    void WriteLights(FileWriter* _out);
    void WriteLandscapeData(FileWriter* _out);
    void WriteLandscapeTiles(FileWriter* _out);
    void WriteLandFlattenAreas(FileWriter* _out);
    void WriteRoutes(FileWriter* _out);
    void WritePrimaryObjectives(FileWriter* _out);
    void WriteRunningPrograms(FileWriter* _out);
    void WriteDifficulty(FileWriter* _out);

  public:
    char m_missionFilename[MAX_FILENAME_LEN];
    char m_mapFilename[MAX_FILENAME_LEN];

    char m_landscapeColourFilename[MAX_FILENAME_LEN];
    char m_wavesColourFilename[MAX_FILENAME_LEN];
    char m_waterColourFilename[MAX_FILENAME_LEN];

    std::vector<CameraMount*> m_cameraMounts;
    std::vector<CameraAnimation*> m_cameraAnimations;
    std::vector<Building*> m_buildings;
    std::vector<InstantUnit*> m_instantUnits;
    std::vector<Light*> m_lights;
    std::vector<Route*> m_routes;
    std::vector<RunningProgram*> m_runningPrograms;
    std::vector<GlobalEventCondition*> m_primaryObjectives;
    std::vector<GlobalEventCondition*> m_secondaryObjectives; // This data isn't stored in the map or mission files
                                                              // directly, but is calculated at load time for your
                                                              // convenience
    int m_levelDifficulty;                                    // The difficulty factor that this level represents.

    LandscapeDef m_landscape;

    LevelFile();
    LevelFile(char const* _missionFilename, char const* _mapFilename);
    ~LevelFile();

    void Save();
    void SaveMapFile(char const* _filename);
    void SaveMissionFile(char const* _filename);

    Building* GetBuilding(int _id);
    CameraMount* GetCameraMount(char const* _name);
    int GetCameraAnimId(char const* _name);

    // Returns nullptr when _id is out of range. The editor holds animation ids
    // across frames — LocationEditor::m_selectionId and
    // CameraAnimSecondaryEditWindow::m_animId — and deleting an animation from
    // the main window does not renumber them, so a held id can outlive the
    // entry it names. The legacy list's GetData answered that with nullptr;
    // std::vector would not, so the check lives here rather than at each call
    // site.
    CameraAnimation* GetCameraAnim(int _id);

    // Also nullptr out of range, and here the null is load-bearing rather than
    // defensive: the instant-unit team buttons render every frame the editor
    // window is up and pass LocationEditor::m_selectionId straight in, which is
    // -1 whenever nothing is selected. Both already test the result.
    InstantUnit* GetInstantUnit(int _id);
    void RemoveBuilding(int _id);
    int GenerateNewRouteId();
    Route* GetRoute(int _id);

    void GenerateInstantUnits();
    void GenerateDynamicBuildings();
};
