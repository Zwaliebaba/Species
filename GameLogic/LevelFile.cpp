#include "pch.h"
#include "AppCommands.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "Debug.h"
#include "FilesysUtils.h"
#include "FileWriter.h"
#include "Resource.h"
#include "TextStreamReaders.h"
#include "Preferences.h"
#include "PrefsOtherWindow.h"

#include "ResearchItem.h"
#include "Citizen.h"
#include "Officer.h"
#include "AntHill.h"
#include "Incubator.h"
#include "Armour.h"
#include "RadarDish.h"
#include "Rocket.h"
#include "Engineer.h"
#include "InsertionSquad.h"
#include "SpawnPoint.h"
#include "Ai.h"
#include "Triffid.h"
#include "Switch.h"
#include "LaserFence.h"
#include "GenericHub.h"

#include "GlobalWorld.h"
#include "LevelFile.h"
#include "Location.h"
#include "RoutingSystem.h"
#include "Team.h"
#include "Unit.h"
#include "TaskManager.h"
#include "WorldPointers.h"
#include "AppState.h"


//*****************************************************************************
// Class CamAnimNode
//*****************************************************************************

static char const* g_transitionModeNames[] = {"Move", "Cut"};


int CamAnimNode::GetTransitModeId(char const* _word)
{
  for (int i = 0; i < TransitionNumModes; ++i)
  {
    if (stricmp(_word, g_transitionModeNames[i]) == 0)
    {
      return i;
    }
  }

  return -1;
}


char const* CamAnimNode::GetTransitModeName(int _modeId)
{
  DEBUG_ASSERT(_modeId >= 0 && _modeId < TransitionNumModes);
  return g_transitionModeNames[_modeId];
}


//*****************************************************************************
// Class LevelFile
//*****************************************************************************

// ***************
// Private Methods
// ***************

void LevelFile::ParseMissionFile(char const* _filename)
{
  std::unique_ptr<TextReader> inOwned;

  if (!g_editing)
  {
    // Try to load a save game first
    const std::string fullFilename = std::format("{}users/{}/{}", g_appCommands->ProfileDirectory(), g_userProfileName, _filename);
    if (DoesFileExist(fullFilename.c_str()))
      inOwned = std::make_unique<TextFileReader>(fullFilename.c_str());
  }

  if (!inOwned)
  {
    const std::string fullFilename = std::format("Levels/{}", _filename);
    inOwned.reset(g_resource->GetTextReader(fullFilename.c_str()));
  }

  TextReader* in = inOwned.get();

  ASSERT_TEXT(in && in->IsOpen(), "Invalid level specified");

  while (in->ReadLine())
  {
    if (!in->TokenAvailable())
      continue;
    char* word = in->GetNextToken();

    if (stricmp("Landscape_StartDefinition", word) == 0 || stricmp("LandscapeTiles_StartDefinition", word) == 0 ||
        stricmp("LandFlattenAreas_StartDefinition", word) == 0 || stricmp("Lights_StartDefinition", word) == 0)
    {
      DEBUG_ASSERT(0);
    }
    else if (stricmp("CameraMounts_StartDefinition", word) == 0)
    {
      ParseCameraMounts(in);
    }
    else if (stricmp("CameraAnimations_StartDefinition", word) == 0)
    {
      ParseCameraAnims(in);
    }
    else if (stricmp("Buildings_StartDefinition", word) == 0)
    {
      ParseBuildings(in, true);
    }
    else if (stricmp("InstantUnits_StartDefinition", word) == 0)
    {
      ParseInstantUnits(in);
    }
    else if (stricmp("Routes_StartDefinition", word) == 0)
    {
      ParseRoutes(in);
    }
    else if (stricmp("PrimaryObjectives_StartDefinition", word) == 0)
    {
      ParsePrimaryObjectives(in);
    }
    else if (stricmp("RunningPrograms_StartDefinition", word) == 0)
    {
      ParseRunningPrograms(in);
    }
    else if (stricmp("Difficulty_StartDefinition", word) == 0)
    {
      ParseDifficulty(in);
    }
    else
    {
      // Looks like a damaged level file
      DEBUG_ASSERT(0);
    }
  }
}

void LevelFile::ParseMapFile(char const* _levelFilename)
{
  const std::string fullFilename = std::format("Levels/{}", _levelFilename);
  std::unique_ptr<TextReader> const inOwned(g_resource->GetTextReader(fullFilename.c_str()));
  TextReader* in = inOwned.get();
  ASSERT_TEXT(in && in->IsOpen(), "Invalid map file specified ({})", _levelFilename);

  while (in->ReadLine())
  {
    if (!in->TokenAvailable())
      continue;
    char* word = in->GetNextToken();

    if (stricmp("landscape_startDefinition", word) == 0)
    {
      ParseLandscapeData(in);
    }
    else if (stricmp("landscapeTiles_startDefinition", word) == 0)
    {
      ParseLandscapeTiles(in);
    }
    else if (stricmp("landFlattenAreas_startDefinition", word) == 0)
    {
      ParseLandFlattenAreas(in);
    }
    else if (stricmp("Buildings_StartDefinition", word) == 0)
    {
      ParseBuildings(in, false);
    }
    else if (stricmp("Lights_StartDefinition", word) == 0)
    {
      ParseLights(in);
    }
    else
    {
      DEBUG_ASSERT(0);
    }
  }
}


void LevelFile::ParseCameraMounts(TextReader* _in)
{
  while (_in->ReadLine())
  {
    if (!_in->TokenAvailable())
      continue;
    char* word = _in->GetNextToken();

    if (stricmp("CameraMounts_EndDefinition", word) == 0)
      return;

    auto cmntOwned = std::make_unique<CameraMount>();
    CameraMount* cmnt = cmntOwned.get();

    // Read name. The truncation is kept rather than dropped with the array it
    // used to bound: it is what a level file carrying a longer name loads as
    // today, and it is therefore what such a file writes back out. T8 owns the
    // round-trip proof and cannot make it against a load path that changed.
    cmnt->m_name = std::string_view{word}.substr(0, CAMERA_MOUNT_MAX_NAME_LEN);

    // Read pos
    word = _in->GetNextToken();
    cmnt->m_pos.x = atof(word);
    word = _in->GetNextToken();
    cmnt->m_pos.y = atof(word);
    word = _in->GetNextToken();
    cmnt->m_pos.z = atof(word);

    // Read front
    word = _in->GetNextToken();
    cmnt->m_front.x = atof(word);
    word = _in->GetNextToken();
    cmnt->m_front.y = atof(word);
    word = _in->GetNextToken();
    cmnt->m_front.z = atof(word);
    DirectX::XMStoreFloat3(&cmnt->m_front, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&cmnt->m_front)));

    // Read up
    word = _in->GetNextToken();
    cmnt->m_up.x = atof(word);
    word = _in->GetNextToken();
    cmnt->m_up.y = atof(word);
    word = _in->GetNextToken();
    cmnt->m_up.z = atof(word);
    DirectX::XMStoreFloat3(&cmnt->m_up, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&cmnt->m_up)));

    m_cameraMounts.push_back(std::move(cmntOwned));
  }
}


void LevelFile::ParseCameraAnims(TextReader* _in)
{
  while (_in->ReadLine())
  {
    if (!_in->TokenAvailable())
      continue;
    char* word = _in->GetNextToken();

    if (stricmp("CameraAnimations_EndDefinition", word) == 0)
      return;

    auto animOwned = std::make_unique<CameraAnimation>();
    CameraAnimation* anim = animOwned.get();
    anim->m_name = std::string_view{word}.substr(0, CAMERA_ANIM_MAX_NAME_LEN);

    while (_in->ReadLine())
    {
      if (!_in->TokenAvailable())
        continue;
      word = _in->GetNextToken();
      if (stricmp(word, "End") == 0)
      {
        break;
      }

      auto nodeOwned = std::make_unique<CamAnimNode>();
      CamAnimNode* node = nodeOwned.get();

      // Read camera mode
      node->m_transitionMode = CamAnimNode::GetTransitModeId(word);
      ASSERT_TEXT(node->m_transitionMode >= 0 && node->m_transitionMode < static_cast<int>(Neuron::I(CameraAccess::Mode::ModeNumModes)),
                  "Bad camera animation camera mode in level file {}", m_missionFilename);


      word = _in->GetNextToken();
      node->m_mountName = strdup(word);
      if (stricmp(node->m_mountName, MAGIC_MOUNT_NAME_START_POS))
      {
        ASSERT_TEXT(GetCameraMount(node->m_mountName), "Bad camera animation mount name in level file {}", m_missionFilename);
      }

      // Read time
      word = _in->GetNextToken();
      node->m_duration = atof(word);
      ASSERT_TEXT(node->m_duration >= 0.0f && node->m_duration < 60.0f, "Bad camera animation transition time in level file {}", m_missionFilename);

      anim->m_nodes.push_back(std::move(nodeOwned));
    }

    m_cameraAnimations.push_back(std::move(animOwned));
  }
}


void LevelFile::ParseBuildings(TextReader* _in, bool _dynamic)
{
  float loadDifficultyFactor = 1.0;
  if (m_levelDifficulty < 0)
    loadDifficultyFactor = 1.0f + (float)g_difficultyLevel / 5.0f;

  while (_in->ReadLine())
  {
    if (!_in->TokenAvailable())
      continue;
    char* word = _in->GetNextToken();

    if (stricmp("Buildings_EndDefinition", word) == 0)
    {
      return;
    }

    Building* building = Building::CreateBuilding(word);
    if (building)
    {
      building->Read(_in, _dynamic);

      // Make sure that this building's ID isn't already in use
      int uniqueId = building->m_id.GetUniqueId();
      Building* existingBuilding = GetBuilding(uniqueId);
      if (existingBuilding)
      {
        ASSERT_TEXT(0, "{} UniqueId was not unique in {}", Building::GetTypeName(existingBuilding->m_type), _in->GetFilename());
      }

      // Make sure that it's global if it needs to be
      if (building->m_type == Building::TypeTrunkPort || building->m_type == Building::TypeControlTower ||
          building->m_type == Building::TypeRadarDish || building->m_type == Building::TypeIncubator || building->m_type == Building::TypeFenceSwitch)
      {
        ASSERT_TEXT(building->m_isGlobal, "Non-global {} found in {}", Building::GetTypeName(building->m_type), _in->GetFilename());
      }

      // Increase the difficulty by raising the population limits for the opposing forces
      if (building->m_id.GetTeamId() == 1)
      {
        switch (building->m_type)
        {
        case Building::TypeSpawnPopulationLock:
        {
          SpawnPopulationLock* spl = (SpawnPopulationLock*)building;
          spl->m_maxPopulation = int(spl->m_maxPopulation * loadDifficultyFactor);
        }
        break;

        case Building::TypeAntHill:
        {
          AntHill* ah = (AntHill*)building;
          ah->m_numAntsInside = int(ah->m_numAntsInside * loadDifficultyFactor);
        }
        break;

        case Building::TypeAISpawnPoint:
        {
          AISpawnPoint* aisp = (AISpawnPoint*)building;
          aisp->m_period = int(aisp->m_period / loadDifficultyFactor);
        }
        break;

        case Building::TypeTriffid:
        {
          Triffid* t = (Triffid*)building;
          t->m_reloadTime = int(t->m_reloadTime / loadDifficultyFactor);
        }
        break;
        }
      }

      // Building::CreateBuilding still returns a raw owning pointer, so the
      // vector adopts it at the same point it used to be pushed.
      m_buildings.push_back(std::unique_ptr<Building>(building));
    }
  }
}


void LevelFile::ParseInstantUnits(TextReader* _in)
{
  float loadDifficultyFactor = 1.0;
  if (m_levelDifficulty < 0)
    loadDifficultyFactor = 1.0f + (float)g_difficultyLevel / 5.0f;

  while (_in->ReadLine())
  {
    if (!_in->TokenAvailable())
      continue;
    char* word = _in->GetNextToken();

    if (stricmp("InstantUnits_EndDefinition", word) == 0)
    {
      return;
    }

    int entityType = Entity::GetTypeId(word);
    if (entityType == -1)
    {
      continue;
    }

    auto iuOwned = std::make_unique<InstantUnit>();
    InstantUnit* iu = iuOwned.get();
    int numCopies = 0;
    iu->m_type = entityType;

    iu->m_teamId = atoi(_in->GetNextToken());
    iu->m_posX = atof(_in->GetNextToken());
    iu->m_posZ = atof(_in->GetNextToken());
    iu->m_number = atoi(_in->GetNextToken());
    iu->m_inAUnit = atoi(_in->GetNextToken()) ? true : false;
    iu->m_state = atoi(_in->GetNextToken());
    iu->m_spread = atof(_in->GetNextToken());

    if (_in->TokenAvailable())
    {
      iu->m_waypointX = atof(_in->GetNextToken());
      iu->m_waypointZ = atof(_in->GetNextToken());
    }

    if (_in->TokenAvailable())
    {
      iu->m_routeId = atoi(_in->GetNextToken());
      if (_in->TokenAvailable())
      {
        iu->m_routeWaypointId = atoi(_in->GetNextToken());
      }
    }

    // Stop InstantSquaddies bug from crashing Darwinia (for existing bad save files)
    if (iu->m_type == Entity::TypeInsertionSquadie && iu->m_inAUnit == 0)
      continue;

    // If the unit is on the red team then make it more of them
    if (iu->m_teamId == 1)
    {
      iu->m_number = int(iu->m_number * loadDifficultyFactor);

      // It doesn't make sense to make overly
      if (iu->m_type == Entity::TypeCentipede && loadDifficultyFactor > 1)
      {
        int maxCentipedeLength = 10 + int(2.0 * loadDifficultyFactor);
        int segmentUtilisation = maxCentipedeLength + 7;
        if (iu->m_number > maxCentipedeLength)
        {
          numCopies = iu->m_number / segmentUtilisation - 1;
          iu->m_number = maxCentipedeLength;
        }
      }
      else
      {
        if (loadDifficultyFactor > 1.0f)
          iu->m_spread *= pow(1.2, g_difficultyLevel / 5.0);
      }
    }

    m_instantUnits.push_back(std::move(iuOwned));

    // Create some additional centipedes if necessary
    for (int i = 0; i < numCopies; i++)
    {
      auto copyOwned = std::make_unique<InstantUnit>();
      InstantUnit* copy = copyOwned.get();
      *copy = *iu;
      // Spread them out a bit. SYNCHRONISED: these are entity spawn positions,
      // and GenerateSyncValue sums every entity's m_pos, so drawing them from
      // the unsynchronised LCG put a client-local value straight into the
      // desync checksum. determinism.yaml T5.
      copy->m_posX = iu->m_posX + syncsfrand(60);
      copy->m_posZ = iu->m_posZ + syncsfrand(60);
      m_instantUnits.push_back(std::move(copyOwned));
    }
  }
}


void LevelFile::ParseLandscapeData(TextReader* _in)
{
  while (_in->ReadLine())
  {
    char* word = _in->GetNextToken();
    char* secondWord = nullptr;

    if (_in->TokenAvailable())
      secondWord = _in->GetNextToken();

    if (stricmp("cellSize", word) == 0)
    {
      m_landscape.m_cellSize = atof(secondWord);
    }
    else if (stricmp("worldSizeX", word) == 0)
    {
      m_landscape.m_worldSizeX = atoi(secondWord);
    }
    else if (stricmp("worldSizeZ", word) == 0)
    {
      m_landscape.m_worldSizeZ = atoi(secondWord);
    }
    else if (stricmp("outsideHeight", word) == 0)
    {
      m_landscape.m_outsideHeight = atof(secondWord);
    }
    else if (stricmp("landColourFile", word) == 0)
    {
      m_landscapeColourFilename = secondWord;
    }
    else if (stricmp("wavesColourFile", word) == 0)
    {
      m_wavesColourFilename = secondWord;
    }
    else if (stricmp("waterColourFile", word) == 0)
    {
      m_waterColourFilename = secondWord;
    }
    else if (stricmp("landscape_endDefinition", word) == 0)
    {
      return;
    }
  }
}


void LevelFile::ParseLandscapeTiles(TextReader* _in)
{
  while (_in->ReadLine())
  {
    if (!_in->TokenAvailable())
      continue;
    char* word = _in->GetNextToken();

    if (stricmp(word, "landscapeTiles_endDefinition") == 0)
    {
      return;
    }

    m_landscape.m_tiles.push_back(std::make_unique<LandscapeTile>());
    LandscapeTile* def = m_landscape.m_tiles.back().get();

    def->m_posX = atoi(word);

    word = _in->GetNextToken();
    def->m_posY = (float)atof(word);

    word = _in->GetNextToken();
    def->m_posZ = atoi(word);

    word = _in->GetNextToken();
    def->m_size = atoi(word);

    word = _in->GetNextToken();
    def->m_fractalDimension = (float)atof(word);

    word = _in->GetNextToken();
    def->m_heightScale = (float)atof(word);

    word = _in->GetNextToken();
    def->m_desiredHeight = (float)atof(word);

    word = _in->GetNextToken();
    def->m_generationMethod = (float)atoi(word);

    word = _in->GetNextToken();
    def->m_randomSeed = atoi(word);

    word = _in->GetNextToken();
    def->m_lowlandSmoothingFactor = (float)atof(word);

    word = _in->GetNextToken();
    int guidePower = atoi(word);
    def->GuideGridSetPower(guidePower);

    if (def->m_guideGridPower > 0)
    {
      word = _in->GetNextToken();
      def->GuideGridFromString(word);
    }
  }
}


void LevelFile::ParseLandFlattenAreas(TextReader* _in)
{
  while (_in->ReadLine())
  {
    if (!_in->TokenAvailable())
      continue;
    char* word = _in->GetNextToken();

    if (stricmp("landFlattenAreas_endDefinition", word) == 0)
    {
      return;
    }

    m_landscape.m_flattenAreas.push_back(std::make_unique<LandscapeFlattenArea>());
    LandscapeFlattenArea* def = m_landscape.m_flattenAreas.back().get();

    def->m_centre.x = (float)atof(word);

    word = _in->GetNextToken();
    def->m_centre.y = (float)atof(word);

    word = _in->GetNextToken();
    def->m_centre.z = (float)atof(word);

    word = _in->GetNextToken();
    def->m_size = (float)atof(word);
  }
}


void LevelFile::ParseLights(TextReader* _in)
{
  bool ignoreLights = false;

  if (!m_lights.empty())
  {
    // This function is called first when parsing the level file and
    // secondly when parsing the map file. We only get here if the
    // level file specified some lights. In which case, these lights
    // are to be used in preference to the map lights. So we need
    // to ignore all the lights we encounter.
    ignoreLights = true;
  }

  while (_in->ReadLine())
  {
    if (!_in->TokenAvailable())
      continue;
    char* word = _in->GetNextToken();

    if (stricmp("Lights_EndDefinition", word) == 0)
    {
      return;
    }

    if (ignoreLights)
      continue;

    m_lights.push_back(std::make_unique<Light>());
    Light* light = m_lights.back().get();

    light->m_front[0] = atof(word);

    word = _in->GetNextToken();
    light->m_front[1] = atof(word);

    word = _in->GetNextToken();
    light->m_front[2] = atof(word);
    light->m_front[3] = 0.0f; // Set to be an infinitely distant light
    light->Normalise();

    word = _in->GetNextToken();
    light->m_colour[0] = atof(word);

    word = _in->GetNextToken();
    light->m_colour[1] = atof(word);

    word = _in->GetNextToken();
    light->m_colour[2] = atof(word);
    light->m_colour[3] = 0.0f;
  }
}


void LevelFile::ParseRoute(TextReader* _in, int _id)
{
  auto rOwned = std::make_unique<Route>(_id);
  Route* r = rOwned.get();

  while (_in->ReadLine())
  {
    if (!_in->TokenAvailable())
      continue;
    char* word = _in->GetNextToken();

    if (stricmp("end", word) == 0)
      break;
    DEBUG_ASSERT(isdigit(word[0]));

    int type = atoi(word);
    // Braced: the TypeGroundPos case below writes x and z and leaves y to the
    // default, which Vector3's constructor zeroed and XMFLOAT3's does not.
    DirectX::XMFLOAT3 pos{0.0f, 0.0f, 0.0f};
    int buildingId = -1;

    switch (type)
    {
    case WayPoint::TypeGroundPos:
      pos.x = atof(_in->GetNextToken());
      pos.z = atof(_in->GetNextToken());
      break;
    case WayPoint::Type3DPos:
      pos.x = atof(_in->GetNextToken());
      pos.y = atof(_in->GetNextToken());
      pos.z = atof(_in->GetNextToken());
      break;
    case WayPoint::TypeBuilding:
      buildingId = atoi(_in->GetNextToken());
      break;
    }

    auto wp = std::make_unique<WayPoint>(type, pos);
    if (buildingId != -1)
    {
      wp->m_buildingId = buildingId;
    }
    r->m_wayPoints.push_back(std::move(wp));
  }

  m_routes.push_back(std::move(rOwned));
}


void LevelFile::ParseRoutes(TextReader* _in)
{
  while (_in->ReadLine())
  {
    if (!_in->TokenAvailable())
      continue;
    char* word = _in->GetNextToken();

    if (stricmp("Routes_EndDefinition", word) == 0)
    {
      return;
    }

    if (stricmp("Route", word) == 0)
    {
      word = _in->GetNextToken();
      int id = atoi(word);
      DEBUG_ASSERT(id >= 0 && id < 10000);
      ParseRoute(_in, id);
    }
  }
}


void LevelFile::ParsePrimaryObjectives(TextReader* _in)
{
  while (_in->ReadLine())
  {
    char* word = _in->GetNextToken();

    if (stricmp("PrimaryObjectives_EndDefinition", word) == 0)
    {
      return;
    }

    auto conditionOwned = std::make_unique<GlobalEventCondition>();
    GlobalEventCondition* condition = conditionOwned.get();
    condition->m_type = condition->GetType(word);
    DEBUG_ASSERT(condition->m_type != -1);

    switch (condition->m_type)
    {
    case GlobalEventCondition::AlwaysTrue:
    case GlobalEventCondition::NotInLocation:
      DEBUG_ASSERT(false);
      break;

    case GlobalEventCondition::BuildingOffline:
    case GlobalEventCondition::BuildingOnline:
    {
      // condition->m_locationId = g_globalWorld->GetLocationIdFromMapFilename( m_mapFilename );
      condition->m_locationId = g_globalWorld->GetLocationId(_in->GetNextToken());
      condition->m_id = atoi(_in->GetNextToken());
      DEBUG_ASSERT(condition->m_locationId != -1);
      break;
    }

    case GlobalEventCondition::ResearchOwned:
      condition->m_id = GlobalResearch::GetType(_in->GetNextToken());
      DEBUG_ASSERT(condition->m_id != -1);
      break;

    case GlobalEventCondition::DebugKey:
      condition->m_id = atoi(_in->GetNextToken());
      break;
    }

    if (_in->TokenAvailable())
    {
      char* stringId = _in->GetNextToken();
      condition->SetStringId(stringId);
    }

    if (_in->TokenAvailable())
    {
      char* cutScene = _in->GetNextToken();
      condition->SetCutScene(cutScene);
    }

    m_primaryObjectives.push_back(std::move(conditionOwned));
  }
}


void LevelFile::GenerateAutomaticObjectives()
{
  //
  // Create a NeverTrue objective for cutscene mission files

  if (m_primaryObjectives.empty())
  {
    auto objective = std::make_unique<GlobalEventCondition>();
    objective->m_type = GlobalEventCondition::NeverTrue;
    m_primaryObjectives.push_back(std::move(objective));
  }

  //
  // Add secondary objectives for trunk ports and research items

  for (auto const& building : m_buildings)
  {
    if (building->m_type != Building::TypeResearchItem && building->m_type != Building::TypeTrunkPort)
    {
      continue;
    }

    // Make sure this building isn't already in the primary objectives list
    bool found = false;
    for (auto const& primaryObjective : m_primaryObjectives)
    {
      if (primaryObjective->m_id == building->m_id.GetUniqueId())
      {
        found = true;
        break;
      }

      if (primaryObjective->m_type == GlobalEventCondition::ResearchOwned && building->m_type == Building::TypeResearchItem &&
          ((ResearchItem*)building.get())->m_researchType == primaryObjective->m_id)
      {
        found = true;
        break;
      }
    }

    if (!found)
    {
      int locationId = g_globalWorld->GetLocationIdFromMapFilename(m_mapFilename.c_str());

      if (building->m_type == Building::TypeResearchItem)
      {
        ResearchItem* item = (ResearchItem*)GetBuilding(building->m_id.GetUniqueId());
        int currentLevel = g_globalWorld->m_research->CurrentLevel(item->m_researchType);
        if (currentLevel == 0 /*|| currentLevel < item->m_level*/)
        {
          // NOTE : We SHOULD really allow objectives to be created when the current research level
          // is below that of this ResearchItem - eg we have level 1 and this item is level 2.
          // However GlobalEventCondition isn't currently able to store the level data, so it
          // ends up being an auto-completed objective.
          auto condition = std::make_unique<GlobalEventCondition>();
          condition->m_locationId = locationId;
          condition->m_type = GlobalEventCondition::ResearchOwned;
          condition->m_id = item->m_researchType;
          condition->SetStringId("objective_research");
          m_secondaryObjectives.push_back(std::move(condition));
        }
      }
      else if (building->m_type == Building::TypeTrunkPort)
      {
        GlobalBuilding* gb = g_globalWorld->GetBuilding(building->m_id.GetUniqueId(), locationId);
        if (gb && !gb->m_online)
        {
          //
          // Is there a Control Tower that can enable this trunk port?
          bool towerFound = false;
          for (auto const& thisBuilding : m_buildings)
          {
            if (thisBuilding->m_type == Building::TypeControlTower && thisBuilding->GetBuildingLink() == building->m_id.GetUniqueId())
            {
              towerFound = true;
              break;
            }
          }

          if (towerFound)
          {
            auto condition = std::make_unique<GlobalEventCondition>();
            condition->m_locationId = locationId;
            condition->m_type = GlobalEventCondition::BuildingOnline;
            condition->m_id = building->m_id.GetUniqueId();
            condition->SetStringId("objective_capture_trunk");
            m_secondaryObjectives.push_back(std::move(condition));
          }
        }
      }
    }
  }
}


void LevelFile::WriteInstantUnits(FileWriter* _out)
{
  _out->printf("InstantUnits_StartDefinition\n");
  _out->printf("\t# Type         team    x       z   count  inUnit state   spread  waypointX waypointZ  routeId\n");
  _out->printf("\t# ==================================================================================\n");

  for (int i = 0; i < static_cast<int>(m_instantUnits.size()); i++)
  {
    InstantUnit* iu = m_instantUnits[i].get();
    _out->printf("\t%-15s %2d %7.1f %7.1f %6d %4d %7d %7.1f %7.1f %7.1f %4d %4d\n", Entity::GetTypeName(iu->m_type), iu->m_teamId, iu->m_posX,
                 iu->m_posZ, iu->m_number, iu->m_inAUnit, iu->m_state, iu->m_spread, iu->m_waypointX, iu->m_waypointZ, iu->m_routeId,
                 iu->m_routeWaypointId);
  }
  _out->printf("InstantUnits_EndDefinition\n\n");
}


void LevelFile::WriteLights(FileWriter* _out)
{
  _out->printf("Lights_StartDefinition\n");
  _out->printf("\t# x      y      z        r      g      b\n");
  _out->printf("\t# =========================================\n");

  if (g_location)
  {
    for (int i = 0; i < g_location->m_lights.Size(); ++i)
    {
      Light* light = g_location->m_lights.GetData(i);
      _out->printf("\t%6.2f %6.2f %6.2f   %6.2f %6.2f %6.2f\n", light->m_front[0], light->m_front[1], light->m_front[2], light->m_colour[0],
                   light->m_colour[1], light->m_colour[2]);
    }
  }

  _out->printf("Lights_EndDefinition\n\n");
}


void LevelFile::WriteCameraMounts(FileWriter* _out)
{
  _out->printf("CameraMounts_StartDefinition\n");
  _out->printf("\t# Name	          Pos                   Front          Up\n");
  _out->printf("\t# =========================================================================\n");

  for (int i = 0; i < static_cast<int>(m_cameraMounts.size()); i++)
  {
    CameraMount* cmnt = m_cameraMounts[i].get();
    _out->printf("\t%-15s %7.2f %7.2f %7.2f %4.2f %4.2f %4.2f %4.2f %4.2f %4.2f\n", cmnt->m_name.c_str(), cmnt->m_pos.x, cmnt->m_pos.y, cmnt->m_pos.z,
                 cmnt->m_front.x, cmnt->m_front.y, cmnt->m_front.z, cmnt->m_up.x, cmnt->m_up.y, cmnt->m_up.z);
  }

  _out->printf("CameraMounts_EndDefinition\n\n");
}


void LevelFile::WriteCameraAnims(FileWriter* _out)
{
  _out->printf("CameraAnimations_StartDefinition\n");

  for (auto const& anim : m_cameraAnimations)
  {
    _out->printf("\t%s\n", anim->m_name.c_str());

    for (int j = 0; j < static_cast<int>(anim->m_nodes.size()); ++j)
    {
      CamAnimNode* node = anim->m_nodes[j].get();
      char const* camModeName = CamAnimNode::GetTransitModeName(node->m_transitionMode);
      _out->printf("\t\t%-8s %-15s %.2f\n", camModeName, node->m_mountName, node->m_duration);
    }
    _out->printf("\t\tEnd\n");
  }

  _out->printf("CameraAnimations_EndDefinition\n\n");
}


void LevelFile::WriteBuildings(FileWriter* _out, bool _dynamic)
{
  _out->printf("Buildings_StartDefinition\n");
  _out->printf("\t# Type              id      x       z       tm      rx      rz      isGlobal\n");
  _out->printf("\t# ==========================================================================\n");

  for (auto const& building : m_buildings)
  {
    if (building->m_dynamic == _dynamic)
    {
      building->Write(_out);
      _out->printf("\n");
    }
  }
  _out->printf("Buildings_EndDefinition\n\n");
}


void LevelFile::WriteLandscapeData(FileWriter* _out)
{
  _out->printf("Landscape_StartDefinition\n");
  _out->printf("\tworldSizeX %d\n", m_landscape.m_worldSizeX);
  _out->printf("\tworldSizeZ %d\n", m_landscape.m_worldSizeZ);
  _out->printf("\tcellSize %.2f\n", m_landscape.m_cellSize);
  _out->printf("\toutsideHeight %.2f\n", m_landscape.m_outsideHeight);
  _out->printf("\tlandColourFile %s\n", m_landscapeColourFilename.c_str());
  _out->printf("\twavesColourFile %s\n", m_wavesColourFilename.c_str());
  _out->printf("\twaterColourFile %s\n", m_waterColourFilename.c_str());
  _out->printf("Landscape_EndDefinition\n\n");
}


void LevelFile::WriteLandscapeTiles(FileWriter* _out)
{
  _out->printf("LandscapeTiles_StartDefinition\n");
  _out->printf("\t#                            frac  height desired gen         lowland\n");
  _out->printf("\t# x       y       z    size   dim  scale  height  method seed smooth  guideGrid\n");
  _out->printf("\t# =============================================================================\n");

  for (int i = 0; i < static_cast<int>(m_landscape.m_tiles.size()); ++i)
  {
    LandscapeTile* _def = m_landscape.m_tiles[i].get();
    _out->printf("\t%6d %6.2f %6d ", _def->m_posX, _def->m_posY, _def->m_posZ);
    _out->printf("%6d ", _def->m_size);
    _out->printf("%5.2f ", _def->m_fractalDimension);
    _out->printf("%6.2f ", _def->m_heightScale);
    _out->printf("%6.0f ", _def->m_desiredHeight);
    _out->printf("%6d ", _def->m_generationMethod);
    _out->printf("%6d ", _def->m_randomSeed);
    _out->printf("%6.2f", _def->m_lowlandSmoothingFactor);
    _out->printf("%6d", _def->m_guideGridPower);

    if (_def->m_guideGridPower > 0)
      _out->printf("   %s", _def->GuideGridToString());

    _out->printf("\n");
  }
  _out->printf("LandscapeTiles_EndDefinition\n\n");
}


void LevelFile::WriteLandFlattenAreas(FileWriter* _out)
{
  _out->printf("LandFlattenAreas_StartDefinition\n");
  _out->printf("\t# x      y       z      size\n");
  _out->printf("\t# ==========================\n");
  for (auto const& area : m_landscape.m_flattenAreas)
  {
    _out->printf("\t%6.1f %6.1f %6.1f %6.1f\n", area->m_centre.x, area->m_centre.y, area->m_centre.z, area->m_size);
  }
  _out->printf("LandFlattenAreas_EndDefinition\n\n");
}


void LevelFile::WriteRoutes(FileWriter* _out)
{
  _out->printf("Routes_StartDefinition\n");
  for (auto const& r : m_routes)
  {
    _out->printf("\tRoute %d\n", r->m_id);

    for (int j = 0; j < static_cast<int>(r->m_wayPoints.size()); ++j)
    {
      WayPoint* wp = r->m_wayPoints[j].get();
      DirectX::XMFLOAT3 const pos = wp->GetPos();
      if (wp->m_type == WayPoint::Type3DPos)
      {
        _out->printf("\t\t%-3d %6.2f %6.2f %6.2f\n", wp->m_type, pos.x, pos.y, pos.z);
      }
      else if (wp->m_type == WayPoint::TypeGroundPos)
      {
        _out->printf("\t\t%-3d %6.2f %6.2f\n", wp->m_type, pos.x, pos.z);
      }
      else if (wp->m_type == WayPoint::TypeBuilding)
      {
        _out->printf("\t\t%-3d %6d\n", wp->m_type, wp->m_buildingId);
      }
    }

    _out->printf("\t\tEnd\n");
  }
  _out->printf("Routes_EndDefinition\n\n");
}


void LevelFile::WritePrimaryObjectives(FileWriter* _out)
{
  _out->printf("PrimaryObjectives_StartDefinition\n");

  for (auto const& gec : m_primaryObjectives)
  {
    //_out->printf( "\t%s:%d", gec->GetTypeName(gec->m_type), gec->m_id);
    _out->printf("\t");
    gec->Save(_out);

    if (gec->m_stringId)
      _out->printf("\t%s", gec->m_stringId);
    if (gec->m_cutScene)
      _out->printf("\t%s", gec->m_cutScene);

    _out->printf("\n");
  }

  _out->printf("PrimaryObjectives_EndDefinition\n");
}


// **************
// Public Methods
// **************

LevelFile::LevelFile()
  : m_landscapeColourFilename("LandscapeDefault.bmp"),
    m_wavesColourFilename("WavesDefault.bmp"),
    m_waterColourFilename("WaterDefault.bmp")
{
  m_levelDifficulty = -1;
}

LevelFile::LevelFile(char const* _missionFilename, char const* _mapFilename)
  : m_missionFilename(_missionFilename),
    m_mapFilename(_mapFilename),
    m_landscapeColourFilename("LandscapeDefault.bmp"),
    m_wavesColourFilename("WavesDefault.bmp"),
    m_waterColourFilename("WaterDefault.bmp")
{
  m_levelDifficulty = -1;

  // Make sure that the current game difficulty setting
  // is consistent with the preferences (it can become inconsistent
  // when a level is loaded that was saved with a different difficulty
  // level to what the preferences say).
  g_appCommands->UpdateDifficultyFromPreferences();

  if (stricmp(_missionFilename, "null") != 0)
  {
    ParseMissionFile(m_missionFilename.c_str());
  }
  ParseMapFile(m_mapFilename.c_str());

  GenerateAutomaticObjectives();
}


// Defined here rather than defaulted in the header: the nine vectors hold
// unique_ptrs to types the header only forward-declares, and destroying one
// needs the complete type. This translation unit has them all.
LevelFile::~LevelFile() = default;


void LevelFile::Save()
{
  // Write the mission file
  if (m_missionFilename.find("null") == std::string::npos)
  {
    SaveMissionFile(m_missionFilename.c_str());
  }

  // Write the map file
  if (m_mapFilename.find("null") == std::string::npos)
  {
    SaveMapFile(m_mapFilename.c_str());
  }
}


void LevelFile::SaveMapFile(char const* _filename)
{
  const std::string fullFilename = std::format("Levels/{}", _filename);

  std::unique_ptr<FileWriter> const out(g_resource->GetFileWriter(fullFilename.c_str(), false));
  WriteLandscapeData(out.get());
  WriteLandscapeTiles(out.get());
  WriteLandFlattenAreas(out.get());
  WriteLights(out.get());
  WriteBuildings(out.get(), false);
}


void LevelFile::SaveMissionFile(char const* _filename)
{
  std::unique_ptr<FileWriter> outOwned;

  if (!g_editing)
  {
    const std::string fullFilename = std::format("{}users/{}/{}", g_appCommands->ProfileDirectory(), g_userProfileName, _filename);
#ifdef TARGET_DEBUG
    outOwned = std::make_unique<FileWriter>(fullFilename.c_str(), false);
#else
    outOwned = std::make_unique<FileWriter>(fullFilename.c_str(), true);
#endif
  }

  if (!outOwned)
  {
    const std::string fullFilename = std::format("Levels/{}", _filename);
    outOwned.reset(g_resource->GetFileWriter(fullFilename.c_str(), false));
  }

  FileWriter* out = outOwned.get();

  WriteDifficulty(out);
  WriteCameraMounts(out);
  WriteCameraAnims(out);
  WriteBuildings(out, true);
  WriteInstantUnits(out);
  WriteRoutes(out);
  WritePrimaryObjectives(out);
  WriteRunningPrograms(out);
}


Building* LevelFile::GetBuilding(int _id)
{
  for (auto const& building : m_buildings)
  {
    if (building->m_id.GetUniqueId() == _id)
    {
      return building.get();
    }
  }
  return nullptr;
}


CameraMount* LevelFile::GetCameraMount(char const* _name)
{
  for (int i = 0; i < static_cast<int>(m_cameraMounts.size()); ++i)
  {
    CameraMount* mount = m_cameraMounts[i].get();
    if (stricmp(mount->m_name.c_str(), _name) == 0)
    {
      return mount;
    }
  }
  return nullptr;
}


int LevelFile::GetCameraAnimId(char const* _name)
{
  for (int i = 0; i < static_cast<int>(m_cameraAnimations.size()); ++i)
  {
    CameraAnimation* anim = m_cameraAnimations[i].get();
    if (stricmp(anim->m_name.c_str(), _name) == 0)
    {
      return i;
    }
  }
  return -1;
}


CameraAnimation* LevelFile::GetCameraAnim(int _id)
{
  if (_id < 0 || _id >= static_cast<int>(m_cameraAnimations.size()))
    return nullptr;
  return m_cameraAnimations[_id].get();
}


InstantUnit* LevelFile::GetInstantUnit(int _id)
{
  if (_id < 0 || _id >= static_cast<int>(m_instantUnits.size()))
    return nullptr;
  return m_instantUnits[_id].get();
}


void LevelFile::RemoveBuilding(int _id)
{
  for (int i = 0; i < static_cast<int>(m_buildings.size()); ++i)
  {
    Building* building = m_buildings[i].get();
    if (building->m_id.GetUniqueId() == _id)
    {
      // The erase destroys it; it used to be erased and then deleted.
      m_buildings.erase(m_buildings.begin() + i);
      break;
    }
  }
}


int LevelFile::GenerateNewRouteId()
{
  for (int i = 0; i < static_cast<int>(m_routes.size()); ++i)
  {
    bool idNotUsed = true;
    for (auto const& r : m_routes)
    {
      if (i == r->m_id)
      {
        idNotUsed = false;
        break;
      }
    }

    if (idNotUsed)
    {
      return i;
    }
  }

  return static_cast<int>(m_routes.size());
}


Route* LevelFile::GetRoute(int _id)
{
  for (auto const& route : m_routes)
  {
    if (route->m_id == _id)
    {
      return route.get();
    }
  }

  return nullptr;
}


void LevelFile::GenerateInstantUnits()
{
  m_instantUnits.clear();

  //
  // Record all the full size UNITS that exist

  for (int t = 0; t < NUM_TEAMS; ++t)
  {
    Team* team = &g_location->m_teams[t];
    if (team->m_teamType == Team::TeamTypeCPU)
    {
      for (int u = 0; u < team->m_units.Size(); ++u)
      {
        if (team->m_units.ValidIndex(u))
        {
          Unit* unit = team->m_units[u];

          // Accumulated into, so it has to start at zero -- Vector3's default
          // constructor did that and XMFLOAT3's does not.
          DirectX::XMVECTOR centrePosSum = DirectX::XMVectorZero();
          float roamRange = 0;
          int numFound = 0;
          for (int i = 0; i < unit->m_entities.Size(); ++i)
          {
            if (unit->m_entities.ValidIndex(i))
            {
              Entity* entity = unit->m_entities[i];
              centrePosSum = DirectX::XMVectorAdd(centrePosSum, DirectX::XMLoadFloat3(&entity->m_spawnPoint));
              roamRange += entity->m_roamRange;
              numFound++;
            }
          }

          DirectX::XMFLOAT3 centrePos;
          DirectX::XMStoreFloat3(&centrePos, DirectX::XMVectorScale(centrePosSum, 1.0f / (float)numFound));
          roamRange /= (float)numFound;

          auto instantOwned = std::make_unique<InstantUnit>();
          InstantUnit* instant = instantOwned.get();
          instant->m_type = unit->m_troopType;
          instant->m_teamId = unit->m_teamId;
          instant->m_posX = centrePos.x;
          instant->m_posZ = centrePos.z;
          instant->m_number = numFound;
          instant->m_inAUnit = true;
          instant->m_spread = roamRange;
          instant->m_routeId = unit->m_routeId;
          instant->m_routeWaypointId = unit->m_routeWayPointId;
          m_instantUnits.push_back(std::move(instantOwned));
        }
      }
    }
  }


  //
  // Record all other entities that exist
  // If they are in transit in a teleport, ignore them
  // (dealt with later)

  for (int t = 0; t < NUM_TEAMS; ++t)
  {
    Team* team = &g_location->m_teams[t];
    if (team->m_teamType == Team::TeamTypeCPU)
    {
      for (int i = 0; i < team->m_others.Size(); ++i)
      {
        if (team->m_others.ValidIndex(i))
        {
          Entity* entity = team->m_others[i];
          if (entity->m_enabled)
          {
            bool insideSpawnArea = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(
                                     DirectX::XMLoadFloat3(&entity->m_pos), DirectX::XMLoadFloat3(&entity->m_spawnPoint)))) < entity->m_roamRange;

            auto unitOwned = std::make_unique<InstantUnit>();
            InstantUnit* unit = unitOwned.get();
            unit->m_type = entity->m_type;
            unit->m_teamId = t;
            unit->m_posX = insideSpawnArea ? entity->m_spawnPoint.x : entity->m_pos.x;
            unit->m_posZ = insideSpawnArea ? entity->m_spawnPoint.z : entity->m_pos.z;
            unit->m_spread = insideSpawnArea ? entity->m_roamRange : 0;
            unit->m_number = 1;
            unit->m_inAUnit = false;
            unit->m_routeId = entity->m_routeId;
            unit->m_routeWaypointId = entity->m_routeWayPointId;

            if (entity->m_type == Entity::TypeCitizen)
            {
              Citizen* citizen = (Citizen*)entity;
              unit->m_posX = citizen->m_pos.x;
              unit->m_posZ = citizen->m_pos.z;
              unit->m_waypointX = citizen->m_wayPoint.x;
              unit->m_waypointZ = citizen->m_wayPoint.z;
              unit->m_spread = 0.0f; // Citizens should be placed exactly where they were when the game was saved
              if (citizen->m_state == Citizen::StateFollowingOrders)
              {
                unit->m_state = Citizen::StateFollowingOrders;
              }
            }

            m_instantUnits.push_back(std::move(unitOwned));
          }
        }
      }
    }
    if (team->m_teamType == Team::TeamTypeLocalPlayer)
    {
      for (int i = 0; i < team->m_others.Size(); ++i)
      {
        if (team->m_others.ValidIndex(i))
        {
          Entity* entity = team->m_others[i];
          if (entity->m_type == Entity::TypeOfficer && entity->m_enabled)
          {
            Officer* officer = (Officer*)entity;
            auto unitOwned = std::make_unique<InstantUnit>();
            InstantUnit* unit = unitOwned.get();
            unit->m_type = entity->m_type;
            unit->m_teamId = t;
            unit->m_posX = entity->m_pos.x;
            unit->m_posZ = entity->m_pos.z;
            unit->m_spread = 0;
            unit->m_number = 1;
            unit->m_inAUnit = false;
            unit->m_state = officer->m_orders;
            unit->m_waypointX = officer->m_orderPosition.x;
            unit->m_waypointZ = officer->m_orderPosition.z;
            unit->m_routeId = officer->m_routeId;
            unit->m_routeWaypointId = officer->m_routeWayPointId;
            m_instantUnits.push_back(std::move(unitOwned));
          }
          else if (entity->m_type == Entity::TypeArmour)
          {
            bool taskControlled = false;
            for (int i = 0; i < static_cast<int>(g_taskManager->m_tasks.size()); ++i)
            {
              Task* task = g_taskManager->m_tasks[i].get();
              if (task->m_type == GlobalResearch::TypeArmour && task->m_objId == entity->m_id)
              {
                taskControlled = true;
                break;
              }
            }
            if (!taskControlled)
            {
              Armour* armour = (Armour*)entity;
              auto unitOwned = std::make_unique<InstantUnit>();
              InstantUnit* unit = unitOwned.get();
              unit->m_type = Entity::TypeArmour;
              unit->m_teamId = t;
              unit->m_posX = armour->m_pos.x;
              unit->m_posZ = armour->m_pos.z;
              unit->m_spread = 0;
              unit->m_number = 1;
              unit->m_inAUnit = false;
              unit->m_state = armour->m_state;
              unit->m_waypointX = armour->m_wayPoint.x;
              unit->m_waypointZ = armour->m_wayPoint.z;
              unit->m_routeId = armour->m_routeId;
              unit->m_routeWaypointId = armour->m_routeWayPointId;
              m_instantUnits.push_back(std::move(unitOwned));
            }
          }
        }
      }
    }
  }


  //
  // Record all entities in transit in a Radar Dish beam

  for (int i = 0; i < g_location->m_buildings.Size(); ++i)
  {
    if (g_location->m_buildings.ValidIndex(i))
    {
      Building* building = g_location->m_buildings[i];
      if (building && building->m_type == Building::TypeRadarDish)
      {
        RadarDish* dish = (RadarDish*)building;
        DirectX::XMFLOAT3 exitPos, exitFront;
        dish->GetExit(exitPos, exitFront);

        for (int e = 0; e < static_cast<int>(dish->m_inTransit.size()); ++e)
        {
          WorldObjectId id = *&dish->m_inTransit[e];
          Entity* entity = g_location->GetEntity(id);

          if (entity == nullptr)
            continue;

          if (entity->m_type == Entity::TypeInsertionSquadie)
          {
            // InsertionSquaddies are running programs and will be saved there, so no
            // need to create an InstantUnit for them. However, we do need to adjust the
            // position of the squaddies so that they are not dunked in the water when revived
            entity->m_pos.x = exitPos.x;
            entity->m_pos.z = exitPos.z;
            entity->m_pos.y = g_location->m_landscape.m_heightMap->GetValue(entity->m_pos.x, entity->m_pos.z) + 0.1f;
          }
          else
          {
            auto unitOwned = std::make_unique<InstantUnit>();
            InstantUnit* unit = unitOwned.get();
            unit->m_type = entity->m_type;
            unit->m_teamId = id.GetTeamId();
            unit->m_posX = exitPos.x;
            unit->m_posZ = exitPos.z;
            unit->m_spread = 50;
            unit->m_number = 1;
            unit->m_inAUnit = false;
            unit->m_state = 0;
            unit->m_routeId = entity->m_routeId;
            unit->m_routeWaypointId = entity->m_routeWayPointId;
            // unit->m_waypointX = officer->m_orderPosition.x;
            // unit->m_waypointZ = officer->m_orderPosition.z;
            m_instantUnits.push_back(std::move(unitOwned));
          }
        }
      }
    }
  }
}


void LevelFile::GenerateDynamicBuildings()
{
  //
  // Update buildings if they are dynamic
  // Remove all dynamic buildings from the list
  // that aren't on the level anymore

  for (int i = 0; i < static_cast<int>(m_buildings.size()); ++i)
  {
    Building* building = m_buildings[i].get();
    if (building && building->m_dynamic)
    {
      Building* locBuilding = g_location->GetBuilding(building->m_id.GetUniqueId());
      if (!locBuilding)
      {
        m_buildings.erase(m_buildings.begin() + i);
        --i;
      }
      else
      {
        if (building->m_type == Building::TypeAntHill)
        {
          ((AntHill*)building)->m_numAntsInside = ((AntHill*)locBuilding)->m_numAntsInside;
        }
        else if (building->m_type == Building::TypeIncubator)
        {
          ((Incubator*)building)->m_numStartingSpirits = ((Incubator*)locBuilding)->NumSpiritsInside();
        }
        else if (building->m_type == Building::TypeEscapeRocket)
        {
          ((EscapeRocket*)building)->m_fuel = ((EscapeRocket*)locBuilding)->m_fuel;
          ((EscapeRocket*)building)->m_passengers = ((EscapeRocket*)locBuilding)->m_passengers;
          ((EscapeRocket*)building)->m_spawnCompleted = ((EscapeRocket*)locBuilding)->m_spawnCompleted;
        }
        else if (building->m_type == Building::TypeFenceSwitch)
        {
          ((FenceSwitch*)building)->m_locked = ((FenceSwitch*)locBuilding)->m_locked;
          ((FenceSwitch*)building)->m_switchValue = ((FenceSwitch*)locBuilding)->m_switchValue;
        }
        else if (building->m_type == Building::TypeLaserFence)
        {
          ((LaserFence*)building)->m_mode = ((LaserFence*)locBuilding)->m_mode;
        }
        else if (building->m_type == Building::TypeDynamicHub)
        {
          ((DynamicHub*)building)->m_currentScore = ((DynamicHub*)locBuilding)->m_currentScore;
        }
        else if (building->m_type == Building::TypeDynamicNode)
        {
          ((DynamicNode*)building)->m_scoreSupplied = ((DynamicNode*)locBuilding)->m_scoreSupplied;
        }
        else if (building->m_type == Building::TypeAISpawnPoint)
        {
          ((AISpawnPoint*)building)->m_spawnLimit = ((AISpawnPoint*)locBuilding)->m_spawnLimit;
        }
      }
    }
  }


  //
  // Search for new dynamic buildings on the level

  for (int i = 0; i < g_location->m_buildings.Size(); ++i)
  {
    if (g_location->m_buildings.ValidIndex(i))
    {
      Building* building = g_location->m_buildings[i];
      if (building && building->m_dynamic)
      {
        Building* levelFileBuilding = GetBuilding(building->m_id.GetUniqueId());
        if (!levelFileBuilding)
        {
          Building* newBuilding = Building::CreateBuilding(building->m_type);
          newBuilding->m_id = building->m_id;
          newBuilding->m_pos = building->m_pos;
          newBuilding->m_front = building->m_front;
          newBuilding->m_type = building->m_type;
          newBuilding->m_dynamic = building->m_dynamic;
          newBuilding->m_isGlobal = building->m_isGlobal;
          m_buildings.push_back(std::unique_ptr<Building>(newBuilding));
        }
      }
    }
  }
}


void LevelFile::ParseRunningPrograms(TextReader* _in)
{
  while (_in->ReadLine())
  {
    if (!_in->TokenAvailable())
      continue;
    char* word = _in->GetNextToken();

    if (stricmp("RunningPrograms_EndDefinition", word) == 0)
    {
      return;
    }

    auto programOwned = std::make_unique<RunningProgram>();
    RunningProgram* program = programOwned.get();
    program->m_type = Entity::GetTypeId(word);
    program->m_count = atoi(_in->GetNextToken());
    program->m_state = atoi(_in->GetNextToken());
    program->m_data = atoi(_in->GetNextToken());
    program->m_waypointX = atof(_in->GetNextToken());
    program->m_waypointZ = atof(_in->GetNextToken());

    for (int i = 0; i < program->m_count; ++i)
    {
      program->m_positionX[i] = atof(_in->GetNextToken());
      program->m_positionZ[i] = atof(_in->GetNextToken());
      program->m_health[i] = atoi(_in->GetNextToken());
    }

    m_runningPrograms.push_back(std::move(programOwned));
  }
}


void LevelFile::WriteRunningPrograms(FileWriter* _out)
{
  if (!g_editing)
  {
    Team* team = g_location->GetMyTeam();

    _out->printf("\nRunningPrograms_StartDefinition\n");
    //_out->printf( "\t# x      y       z      size\n");
    _out->printf("\t# ==========================\n");


    //
    // Engineer     count   state   numSpirits  waypointX waypointZ    (positionX positionZ health)
    // Squaddie     count   state   weaponType  waypointX waypointZ    (positionX positionZ health)

    for (int t = 0; t < static_cast<int>(g_taskManager->m_tasks.size()); ++t)
    {
      Task* task = g_taskManager->m_tasks[t].get();
      if (task->m_state == Task::StateRunning)
      {
        if (task->m_type == GlobalResearch::TypeEngineer)
        {
          Engineer* engineer = (Engineer*)g_location->GetEntitySafe(task->m_objId, Entity::TypeEngineer);
          if (engineer)
          {
            _out->printf("\t%-15s %6d %6d %6d %8.2f %8.2f %8.2f %8.2f %d\n", Entity::GetTypeName(Entity::TypeEngineer), 1, engineer->m_state,
                         engineer->GetNumSpirits(), engineer->m_wayPoint.x, engineer->m_wayPoint.z, engineer->m_pos.x, engineer->m_pos.z,
                         engineer->m_stats[Entity::StatHealth]);
          }
        }

        if (task->m_type == GlobalResearch::TypeSquad)
        {
          InsertionSquad* squad = (InsertionSquad*)g_location->GetUnit(task->m_objId);
          if (squad && squad->m_troopType == Entity::TypeInsertionSquadie)
          {
            _out->printf("\t%-15s %6d %6d %6d %8.2f %8.2f", Entity::GetTypeName(Entity::TypeInsertionSquadie), squad->m_entities.NumUsed(), 0,
                         squad->m_weaponType, squad->GetWayPoint().x, squad->GetWayPoint().z);

            for (int e = 0; e < squad->m_entities.Size(); ++e)
            {
              if (squad->m_entities.ValidIndex(e))
              {
                Entity* entity = squad->m_entities[e];

                _out->printf(" %8.2f %8.2f %6d", entity->m_pos.x, entity->m_pos.z, entity->m_stats[Entity::StatHealth]);
              }
            }

            _out->printf("\n");
          }
        }
      }
    }

    _out->printf("RunningPrograms_EndDefinition\n");
  }
}


void LevelFile::ParseDifficulty(TextReader* _in)
{
  m_levelDifficulty = -1;
  while (_in->ReadLine())
  {
    if (!_in->TokenAvailable())
      continue;
    char* word = _in->GetNextToken();

    if (stricmp("Difficulty_EndDefinition", word) == 0)
      return;
    else if (stricmp(word, "CreatedAsDifficulty") == 0)
    {
      // The difficulty setting is 1-based in the file, but 0-based internally
      m_levelDifficulty = atoi(_in->GetNextToken()) - 1;
      if (m_levelDifficulty < 0)
        m_levelDifficulty = 0;
    }
  }
}

void LevelFile::WriteDifficulty(FileWriter* _out)
{
  // When we write the difficulty setting to a file it should be 1-based
  // (internally it is 0 based).
  _out->printf("Difficulty_StartDefinition\n");
  _out->printf("\tCreatedAsDifficulty %d\n", m_levelDifficulty + 1);
  _out->printf("Difficulty_EndDefinition\n\n");
}
