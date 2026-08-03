#include "pch.h"

#include <stdarg.h>

#include "Debug.h"
#include "HiResTime.h"
#include "Resource.h"
#include "TextRenderer.h"
#include "TextStreamReaders.h"
#include "LanguageTable.h"
#include "FilesysUtils.h"
#include "Preferences.h"
#include "WindowManager.h"

#include "App.h"
#include "Camera.h"
#include "GlobalWorld.h"
#include "LevelFile.h"
#include "Location.h"
#include "Main.h"
#include "Renderer.h"
#include "Script.h"
#include "TaskManager.h"
#include "TaskManagerInterface.h"

#include "SoundSystem.h"

#include "ConstructionYard.h"
#include "GodDish.h"
#include "Rocket.h"
#include "WorldPointers.h"
#include "AppState.h"

//*****************************************************************************
// Private Functions
//*****************************************************************************

//*****************************************************************************
// Public Functions
//*****************************************************************************

Script::Script()
  : m_in(nullptr),
    m_waitUntil(-1.0f),
    m_waitForSpeech(false),
    m_waitForCamera(false),
    m_waitForFade(false),
    m_waitForPlayerNotBusy(false),
    m_requestedLocationId(-1),
    m_waitForRocket(false),
    m_permitEscape(false) {}

bool Script::IsRunningScript() { return (m_in != nullptr); }

void Script::RunCommand_CamCut(const char* _mountName)
{
  if (!g_location)
    return;

  bool mountFound = TheCamera()->SetTarget(_mountName);
  DEBUG_ASSERT(mountFound);
  TheCamera()->CutToTarget();
}

void Script::RunCommand_CamMove(const char* _mountName, float _duration)
{
  if (!g_location)
    return;

  if (TheCamera()->SetTarget(_mountName))
  {
    TheCamera()->SetMoveDuration(_duration);

    TheCamera()->RequestMode(Camera::ModeMoveToTarget);
  }
}

void Script::RunCommand_CamAnim(const char* _animName)
{
  if (!g_location)
    return;

  int animId = g_location->m_levelFile->GetCameraAnimId(_animName);
  ASSERT_TEXT(animId != -1, "Invalid camera animation requested %s", _animName);
  CameraAnimation* camAnim = g_location->m_levelFile->m_cameraAnimations[animId];
  TheCamera()->PlayAnimation(camAnim);
}

void Script::RunCommand_CamFov(float _fov, bool _immediate)
{
  if (_immediate)
    TheCamera()->SetFOV(_fov);
  else
    TheCamera()->SetTargetFOV(_fov);
}

void Script::RunCommand_CamBuildingFocus(int _buildingId, float _range, float _height)
{
  if (!g_location)
    return;

  Building* building = g_location->GetBuilding(_buildingId);

  if (building)
    TheCamera()->RequestBuildingFocusMode(building, _range, _height);
  else
    DebugTrace("SCRIPT ERROR : Tried to target non-existent building %d", _buildingId);
}

void Script::RunCommand_CamBuildingApproach(int _buildingId, float _range, float _height, float _duration)
{
  if (!g_location)
    return;

  Building* building = g_location->GetBuilding(_buildingId);

  if (building)
  {
    TheCamera()->SetTarget(building->m_centrePos, _range, _height);
    TheCamera()->SetMoveDuration(_duration);
    TheCamera()->RequestMode(Camera::ModeMoveToTarget);
  }
  else
    DebugTrace("SCRIPT ERROR : Tried to target non-existent building %d", _buildingId);
}

void Script::RunCommand_CamGlobalWorldFocus() { TheCamera()->RequestSphereFocusMode(); }

void Script::RunCommand_LocationFocus(const char* _locationName, float _fov)
{
  if (g_location)
    return;

  Vector3 targetPos;

  if (stricmp(_locationName, "heaven") == 0)
    targetPos = g_zeroVector;
  else
  {
    int locationId = g_globalWorld->GetLocationId(_locationName);
    if (locationId == -1)
      return;

    targetPos = g_globalWorld->GetLocationPosition(locationId);
  }

  if (!TheCamera()->IsInMode(Camera::ModeSphereWorldScripted))
    TheCamera()->RequestMode(Camera::ModeSphereWorldScripted);

  TheCamera()->SetTargetFOV(_fov);
  TheCamera()->SetTarget(targetPos, Vector3(0, 0, 1), g_upVector);
}

void Script::RunCommand_CamReset()
{
  if (TheCamera()->IsAnimPlaying())
    TheCamera()->StopAnimation();

  if (g_location)
    TheCamera()->RequestMode(Camera::ModeFreeMovement);
  else
    TheCamera()->RequestMode(Camera::ModeSphereWorld);
}

void Script::RunCommand_EnterLocation(char* _name)
{
  g_requestedLocationId = g_globalWorld->GetLocationId(_name);

  m_requestedLocationId = g_requestedLocationId;

  GlobalLocation* loc = g_globalWorld->GetLocation(g_requestedLocationId);
  DEBUG_ASSERT(loc);

  strcpy(g_requestedMission, loc->m_missionFilename);
  strcpy(g_requestedMap, loc->m_mapFilename);
}

void Script::RunCommand_ExitLocation()
{
  g_requestedLocationId = -1;
  g_requestedMission[0] = '\0';
  g_requestedMap[0] = '\0';

  m_requestedLocationId = g_requestedLocationId;
}

void Script::RunCommand_SetMission(char* _locName, char* _missionName)
{
  GlobalLocation* loc = g_globalWorld->GetLocation(_locName);
  DEBUG_ASSERT(loc);
  strcpy(loc->m_missionFilename, _missionName);
  loc->m_missionCompleted = false;
}

void Script::RunCommand_Say(char* _stringId) {}

void Script::RunCommand_ShutUp() {}

void Script::RunCommand_Wait(double _time) { m_waitUntil = max(m_waitUntil, GetHighResTime() + _time); }

void Script::RunCommand_WaitSay() { m_waitForSpeech = true; }

void Script::RunCommand_WaitCam() { m_waitForCamera = true; }

void Script::RunCommand_WaitFade() { m_waitForFade = true; }

void Script::RunCommand_WaitRocket(int _buildingId, char* _state, int _data)
{
  m_rocketId = _buildingId;
  m_rocketState = EscapeRocket::GetStateId(_state);
  m_rocketData = _data;
  m_waitForRocket = true;
}

void Script::RunCommand_WaitPlayerNotBusy() { m_waitForPlayerNotBusy = true; }

void Script::RunCommand_Highlight(int _buildingId) {}

void Script::RunCommand_ClearHighlights() {}

void Script::RunCommand_TriggerSound(const char* _event)
{
  char eventName[256];
  sprintf(eventName, "Music %s", _event);

  if (g_soundSystem->NumInstancesPlaying(WorldObjectId(), eventName) == 0)
    g_soundSystem->TriggerOtherEvent(nullptr, _event, SoundSourceBlueprint::TypeMusic);
}

void Script::RunCommand_StopSound(const char* _event)
{
  char eventName[256];
  sprintf(eventName, "Music %s", _event);
  g_soundSystem->StopAllSounds(WorldObjectId(), eventName);
}

void Script::RunCommand_DemoGesture(const char* _name) {}

void Script::RunCommand_GiveResearch(const char* _name)
{
  if (stricmp(_name, "modsystem") == 0)
  {
    g_prefsManager->SetInt("ModSystemEnabled", 1);
    g_prefsManager->Save();

    TheTaskManagerInterface()->SetCurrentMessage(TaskManagerInterface::MessageResearch, 999, 4.0f);
  }
  else if (stricmp(_name, "accessallareas") == 0)
  {
    char folderName[512];
    sprintf(folderName, "%susers/", g_appCommands->ProfileDirectory());
    bool success = CreateDirectory(folderName);
    if (!success)
      DebugTrace("failed to create folder %s\n", folderName);

    sprintf(folderName, "%susers/AccessAllAreas/", g_appCommands->ProfileDirectory());
    success = CreateDirectory(folderName);
    if (!success)
      DebugTrace("failed to create folder %s\n", folderName);

    TheTaskManagerInterface()->SetCurrentMessage(TaskManagerInterface::MessageResearch, 998, 4.0f);
  }
  else
  {
    int researchType = GlobalResearch::GetType((char*)_name);
    if (researchType != -1)
    {
      g_globalWorld->m_research->AddResearch(researchType);
      TheTaskManagerInterface()->SetCurrentMessage(TaskManagerInterface::MessageResearch, researchType, 4.0f);
    }
  }
}

void Script::RunCommand_RunCredits() {}

void Script::RunCommand_GameOver()
{
  //
  // Go into the outro camera mode

  TheCamera()->RequestMode(Camera::ModeSphereWorldOutro);

  //
  // Kill global world ambiences

  g_soundSystem->StopAllSounds(WorldObjectId(), "Ambience EnterGlobalWorld");
}

void Script::RunCommand_ResetResearch()
{
  m_darwinianResearchLevel = g_globalWorld->m_research->m_researchLevel[GlobalResearch::TypeDarwinian];
  g_globalWorld->m_research->m_researchLevel[GlobalResearch::TypeDarwinian] = 1;
}

void Script::RunCommand_RestoreResearch()
{
  g_globalWorld->m_research->m_researchLevel[GlobalResearch::TypeDarwinian] = m_darwinianResearchLevel;
}

GodDish* GetGodDish()
{
  for (int i = 0; i < g_location->m_buildings.Size(); ++i)
  {
    if (g_location->m_buildings.ValidIndex(i))
    {
      Building* building = g_location->m_buildings[i];
      if (building && building->m_type == Building::TypeGodDish)
      {
        auto dish = static_cast<GodDish*>(building);
        return dish;
      }
    }
  }

  return nullptr;
}

void Script::RunCommand_GodDishActivate()
{
  GodDish* dish = GetGodDish();
  if (dish)
    dish->Activate();
}

void Script::RunCommand_GodDishDeactivate()
{
  GodDish* dish = GetGodDish();
  if (dish)
    dish->DeActivate();
}

void Script::RunCommand_GodDishSpawnSpam()
{
  GodDish* dish = GetGodDish();
  if (dish)
    dish->SpawnSpam(false);
}

void Script::RunCommand_GodDishSpawnResearch()
{
  GodDish* dish = GetGodDish();
  if (dish)
    dish->SpawnSpam(true);
}

void Script::RunCommand_SpamTrigger()
{
  GodDish* dish = GetGodDish();
  if (dish)
    dish->TriggerSpam();
}

void Script::RunCommand_PurityControl()
{
  //
  // Delete the save game

  char saveDir[256];
  sprintf(saveDir, "users/%s/", g_userProfileName);
  // Neither the names nor the vector are freed. The exit(0) below is why that
  // has never mattered.
  std::vector<char*>* allFiles = ListDirectory(saveDir, "*.*");

  for (const char* filename : *allFiles)
    DeleteThisFile(filename);

  //
  // Open up our store website

  g_windowManager->OpenWebsite("http://www.darwinia.co.uk/store/");

  //
  // Shut down

  exit(0);
}

void Script::RunCommand_ShowDarwinLogo()
{
  TheRenderer()->m_renderDarwinLogo = GetHighResTime();
  g_soundSystem->TriggerOtherEvent(nullptr, "ShowLogo", SoundSourceBlueprint::TypeInterface);
}

void Script::RunCommand_ShowDemoEndSequence() {}

void Script::RunCommand_PermitEscape() { m_permitEscape = true; }

void Script::RunCommand_DestroyBuilding(int _buildingId, float _intensity)
{
  Building* b = g_location->GetBuilding(_buildingId);
  if (b)
    b->Destroy(_intensity);
}

void Script::RunCommand_ActivateTrunkPort(int _buildingId, bool _fullActivation)
{
  Building* b = g_location->GetBuilding(_buildingId);
  if (b && b->m_type == Building::TypeTrunkPort)
  {
    if (_fullActivation)
      b->ReprogramComplete();
    else
    {
      GlobalBuilding* gb = g_globalWorld->GetBuilding(b->m_id.GetUniqueId(), g_locationId);
      gb->m_online = true;
    }
  }
}

// Opens a script file and returns. The script will only actually be run when
// Script::Advance gets called
void Script::RunScript(const char* _filename)
{
  if (strstr(_filename, ".txt"))
  {

    // Run a script, speficied by filename
    char fullFilename[256] = "Scripts/";
    strcat(fullFilename, _filename);
    m_in = g_resource->GetTextReader(fullFilename);
    DEBUG_ASSERT(m_in);
  }
  else
  {
    // This script is specified as a string id, eg "cutscenealpha"
    // Meaning we want to say all strings like "cutscenealpha_1", "cutscenealpha_2" etc
    // Simply dump all matching strings into Sepulveda's queue
    int stringIndex = 1;
    while (true)
    {
      char stringName[256];
      sprintf(stringName, "%s_%d", _filename, stringIndex);
      if (!ISLANGUAGEPHRASE_ANY(stringName))
        break;

      ++stringIndex;
    }
  }
}

bool Script::Skip()
{
  m_waitUntil = g_gameTime;
  m_waitForCamera = false;
  m_waitForRocket = false;
  m_waitForPlayerNotBusy = false;
  TheRenderer()->m_renderDarwinLogo = -1.0f;

  if (m_permitEscape)
  {
    // Quick exit the entire cutscene
    delete m_in;
    m_in = nullptr;
    g_soundSystem->StopAllSounds(WorldObjectId(), "Music");
    m_permitEscape = false;
    if (g_location)
      TheCamera()->RequestMode(Camera::ModeFreeMovement);
    else
      TheCamera()->RequestMode(Camera::ModeSphereWorld);
    return true;
  }

  return false;
}

void Script::Advance()
{
  if (g_inputManager->controlEvent(ControlSkipCutscene))
    if (Skip())
      return;

  if (m_permitEscape)
    TheTaskManagerInterface()->SetVisible(false);

  if (m_waitForFade && !TheRenderer()->IsFadeComplete())
    return;
  if (m_waitUntil > g_gameTime)
    return;
  if (m_waitForCamera && TheCamera()->IsAnimPlaying())
    return;

  if (m_waitForRocket)
  {
    auto rocket = static_cast<EscapeRocket*>(g_location->GetBuilding(m_rocketId));
    if (!rocket || rocket->m_type != Building::TypeEscapeRocket)
    {
      m_waitForRocket = false;
      return;
    }

    if (rocket->m_state < m_rocketState)
      return;

    if (rocket->m_state == m_rocketState && m_rocketState == EscapeRocket::StateCountdown && static_cast<int>(rocket->m_countdown) >
      m_rocketData)
      return;
  }

  if (m_requestedLocationId != -1)
  {
    if (g_locationId != m_requestedLocationId)
      return;
    m_requestedLocationId = -1;
  }

  m_waitForSpeech = false;
  m_waitForCamera = false;
  m_waitForFade = false;
  m_waitForRocket = false;
  m_waitForPlayerNotBusy = false;

  if (m_in)
  {
    if (m_in->ReadLine())
      AdvanceScript();
    else
    {
      delete m_in;
      m_in = nullptr;
      m_permitEscape = false;
    }
  }
}

void Script::AdvanceScript()
{
  if (!m_in->TokenAvailable())
    return;

  int opCode = GetOpCode(m_in->GetNextToken());
  char* nextWord = nullptr;
  float nextFloat = 0.0f;
  if (m_in->TokenAvailable())
  {
    nextWord = m_in->GetNextToken();
    nextFloat = atof(nextWord);
  }

  switch (opCode)
  {
  case OpCamMove:
    {
      float duration = atof(m_in->GetNextToken());
      RunCommand_CamMove(nextWord, duration);
      break;
    }
  case OpCamCut:
    RunCommand_CamCut(nextWord);
    break;
  case OpCamAnim:
    RunCommand_CamAnim(nextWord);
    break;
  case OpCamFov:
    {
      int immediate = m_in->TokenAvailable() ? atoi(m_in->GetNextToken()) : true;
      RunCommand_CamFov(nextFloat, immediate);
      break;
    }

  case OpCamBuildingFocus:
    {
      float range = atof(m_in->GetNextToken());
      float height = atof(m_in->GetNextToken());
      RunCommand_CamBuildingFocus(static_cast<int>(nextFloat), range, height);
      break;
    }

  case OpCamBuildingApproach:
    {
      float range = atof(m_in->GetNextToken());
      float height = atof(m_in->GetNextToken());
      float duration = atof(m_in->GetNextToken());
      RunCommand_CamBuildingApproach(static_cast<int>(nextFloat), range, height, duration);
      break;
    }

  case OpCamLocationFocus:
    {
      float fov = atof(m_in->GetNextToken());
      RunCommand_LocationFocus(nextWord, fov);
      break;
    }

  case OpCamGlobalWorldFocus:
    {
      RunCommand_CamGlobalWorldFocus();
      break;
    }

  case OpCamReset:
    RunCommand_CamReset();
    break;

  case OpEnterLocation:
    RunCommand_EnterLocation(nextWord);
    break;
  case OpExitLocation:
    RunCommand_ExitLocation();
    break;

  case OpSay:
    RunCommand_Say(nextWord);
    break;
  case OpShutUp:
    RunCommand_ShutUp();
    break;
  case OpWait:
    RunCommand_Wait(nextFloat);
    break;
  case OpWaitSay:
    RunCommand_WaitSay();
    break;
  case OpWaitCam:
    RunCommand_WaitCam();
    break;
  case OpWaitFade:
    RunCommand_WaitFade();
    break;

  case OpWaitRocket:
    {
      char* state = m_in->GetNextToken();
      int data = atoi(m_in->GetNextToken());
      RunCommand_WaitRocket(static_cast<int>(nextFloat), state, data);
      break;
    }

  case OpWaitPlayerNotBusy:
    RunCommand_WaitPlayerNotBusy();
    break;

  case OpHighlight:
    RunCommand_Highlight(static_cast<int>(nextFloat));
    break;
  case OpClearHighlights:
    RunCommand_ClearHighlights();
    break;

  case OpTriggerSound:
    RunCommand_TriggerSound(nextWord);
    break;
  case OpStopSound:
    RunCommand_StopSound(nextWord);
    break;

  case OpDemoGesture:
    RunCommand_DemoGesture(nextWord);
    break;
  case OpGiveResearch:
    RunCommand_GiveResearch(nextWord);
    break;

  case OpSetMission:
    {
      char* missionName = m_in->GetNextToken();
      RunCommand_SetMission(nextWord, missionName);
      break;
    }

  case OpResetResearch:
    RunCommand_ResetResearch();
    break;
  case OpRestoreResearch:
    RunCommand_RestoreResearch();
    break;

  case OpGameOver:
    RunCommand_GameOver();
    break;
  case OpRunCredits:
    RunCommand_RunCredits();
    break;

  case OpSetCutsceneMode:
    {
      int cutsceneMode = atoi(nextWord);
      break;
    }

  case OpGodDishActivate:
    RunCommand_GodDishActivate();
    break;
  case OpGodDishDeactivate:
    RunCommand_GodDishDeactivate();
    break;
  case OpGodDishSpawnSpam:
    RunCommand_GodDishSpawnSpam();
    break;
  case OpGodDishSpawnResearch:
    RunCommand_GodDishSpawnResearch();
    break;
  case OpTriggerSpam:
    RunCommand_SpamTrigger();
    break;
  case OpPurityControl:
    RunCommand_PurityControl();
    break;

  case OpShowDarwinLogo:
    RunCommand_ShowDarwinLogo();
    break;
  case OpShowDemoEndSequence:
    RunCommand_ShowDemoEndSequence();
    break;

  case OpPermitEscape:
    RunCommand_PermitEscape();
    break;

  case OpDestroyBuilding:
    {
      float intensity = atof(m_in->GetNextToken());
      RunCommand_DestroyBuilding(static_cast<int>(nextFloat), intensity);
      break;
    }

  case OpActivateTrunkPort:
  case OpActivateTrunkPortFull:
    {
      RunCommand_ActivateTrunkPort(static_cast<int>(nextFloat), opCode == OpActivateTrunkPortFull);
      break;
    }

  default: DEBUG_ASSERT(false);
    break;
  }
}

static const char* g_opCodeNames[] = {
  "CamCut", "CamMove", "CamAnim", "CamFov", "CamBuildingFocus", "CamBuildingApproach", "CamLocationFocus", "CamGlobalWorldFocus",
  "CamReset", "EnterLocation", "ExitLocation", "Say", "ShutUp", "Wait", "WaitSay", "WaitCam", "WaitFade", "WaitRocket", "WaitPlayerNotBusy",
  "Highlight", "ClearHighlights", "TriggerSound", "StopSound", "DemoGesture", "GiveResearch", "SetMission", "GameOver", "ResetResearch",
  "RestoreResearch", "RunCredits", "SetCutsceneMode", "GodDishActivate", "GodDishDeactivate", "GodDishSpawnSpam", "GodDishSpawnResearch",
  "TriggerSpam", "PurityControl", "ShowDarwinLogo", "ShowDemoEndSequence", "PermitEscape", "DestroyBuilding", "ActivateTrunkPort",
  "ActivateTrunkPortFull"
};

int Script::GetOpCode(const char* _word)
{
  DEBUG_ASSERT(sizeof(g_opCodeNames) / sizeof(char *) == OpNumOps);

  for (unsigned int i = 0; i < OpNumOps; ++i)
  {
    if (stricmp(_word, g_opCodeNames[i]) == 0)
      return i;
  }

  return -1;
}
