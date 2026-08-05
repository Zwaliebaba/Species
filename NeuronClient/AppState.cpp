#include "pch.h"

#include "AppState.h"


namespace Neuron
{
  bool g_editing = false;
  int g_locationId = -1;
  bool g_requestQuit = false;
  int g_difficultyLevel = 0;
  bool g_largeMenus = false;
  int g_requestedLocationId = -1;
  int g_gameMode = GameModeNone;
  bool g_atMainMenu = false;
  bool g_requestToggleEditing = false;
  Server* g_server = nullptr;
  ControlHelpAccess* g_controlHelpSystem = nullptr;
  std::string g_userProfileName;
  std::string g_requestedMission;
  std::string g_requestedMap;
  bool g_paused = false;
  RGBAColour g_backgroundColour;
  bool g_negativeRenderer = false;
} // namespace Neuron
