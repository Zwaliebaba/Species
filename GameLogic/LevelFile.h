#pragma once

#include <stdlib.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "NeuronMath.h"
#include "WorldObject.h"
#include "Landscape.h"


#define CAMERA_MOUNT_MAX_NAME_LEN 63
#define CAMERA_ANIM_MAX_NAME_LEN 63
#define MAGIC_MOUNT_NAME_START_POS "CamPosBefore"

namespace Neuron
{
  class TextReader;
  class FileWriter;
} // namespace Neuron


namespace Species
{
  class Building;
  class Route;
  class GlobalEventCondition;

  class CameraMount
  {
    public:
      // InputField edits this in the camera-mount window and wrote through a raw
      // char* from a 256-byte buffer, so a long enough name overran the 64 bytes
      // this used to be. See strings-modernised T5.
      std::string m_name;
      DirectX::XMFLOAT3 m_pos{0.0f, 0.0f, 0.0f};
      DirectX::XMFLOAT3 m_front{0.0f, 0.0f, 0.0f};
      DirectX::XMFLOAT3 m_up{0.0f, 0.0f, 0.0f};
  };


  class CamAnimNode
  {
    public:
      // Scoped by language-hygiene T13. This enum was anonymous and
      // m_transitionMode was an int, which is how LevelFile.cpp came to bound a
      // parsed transition against CameraAccess::Mode::ModeNumModes -- sixteen,
      // for a two-valued enum. Naming the type is what makes that comparison
      // stop compiling rather than stop being noticed.
      //
      // The values are pinned. A level file stores a transition by NAME, so the
      // file format does not depend on them; the editor's drop-down does,
      // because DropDownMenu::AddOption assigns option values 0, 1, ... in
      // declaration order and writes the selected one straight back here.
      enum class Transition : int
      {
        TransitionMove = 0,
        TransitionCut = 1,
        TransitionNumModes
      };

      Transition m_transitionMode;
      char* m_mountName;
      float m_duration;

    public:
      CamAnimNode()
        : m_transitionMode(Transition::TransitionMove),
          m_mountName(nullptr),
          m_duration(1.0f)
      {
      }

      ~CamAnimNode()
      {
        free(m_mountName);
        m_mountName = nullptr;
      }

      // Answers nullopt for a word that names no transition. This used to be a
      // -1 sentinel in an int, which is exactly what let the caller check it
      // against the wrong enum's bound; there is no numeric bound to get wrong
      // now, and the caller cannot forget to test it.
      static std::optional<Transition> GetTransitModeId(char const* _word);
      static char const* GetTransitModeName(Transition _mode);
  };


  class CameraAnimation
  {
    public:
      // Owns its nodes. Everything outside this class observes them.
      std::vector<std::unique_ptr<CamAnimNode>> m_nodes;
      std::string m_name;
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
      DirectX::XMFLOAT3 m_centre{0.0f, 0.0f, 0.0f};
      float m_size;
  };


  class LandscapeDef
  {
    public:
      // Both owning. The vectors release their contents; nothing else does.
      std::vector<std::unique_ptr<LandscapeTile>> m_tiles;
      std::vector<std::unique_ptr<LandscapeFlattenArea>> m_flattenAreas;
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
      // Empty until the two-argument constructor or the map file names them. The
      // default constructor left the first two INDETERMINATE while they were
      // arrays, and Save() tests both with strstr — see LevelFileTests.
      std::string m_missionFilename;
      std::string m_mapFilename;

      std::string m_landscapeColourFilename;
      std::string m_wavesColourFilename;
      std::string m_waterColourFilename;

      // The level file owns everything it parses. These nine vectors are the
      // whole of that ownership -- the destructor used to be nine delete loops
      // and is now defaulted. Everything that reaches in from outside observes,
      // and says .get() to prove it.
      std::vector<std::unique_ptr<CameraMount>> m_cameraMounts;
      std::vector<std::unique_ptr<CameraAnimation>> m_cameraAnimations;
      std::vector<std::unique_ptr<Building>> m_buildings;
      std::vector<std::unique_ptr<InstantUnit>> m_instantUnits;
      std::vector<std::unique_ptr<Light>> m_lights;
      std::vector<std::unique_ptr<Route>> m_routes;
      std::vector<std::unique_ptr<RunningProgram>> m_runningPrograms;
      std::vector<std::unique_ptr<GlobalEventCondition>> m_primaryObjectives;
      std::vector<std::unique_ptr<GlobalEventCondition>> m_secondaryObjectives; // This data isn't stored in the map or mission files
                                                                                // directly, but is calculated at load time for your
                                                                                // convenience
      int m_levelDifficulty;                                                    // The difficulty factor that this level represents.

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
} // namespace Species
