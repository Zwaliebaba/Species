#pragma once

#include "ControlHelpAccess.h"

// The application state the layers below Species read.
//
// These were members of Species::App, which is why 49 GameLogic and
// NeuronClient files still included App.h after the subsystem pointers moved
// out in T8. As with those pointers this MOVES the storage rather than
// aliasing it — there is one variable, written wherever it was written before
// — so no copy can go stale. See tasks/layering-inversion.yaml T14.
//
// The game-mode enum comes with them: it was an anonymous enum inside App, so
// comparing g_gameMode against it would otherwise still drag App.h down here.
class Server;

enum
{
  GameModeNone,
  GameModePrologue,
  GameModeCampaign,
  GameModeMultiwinia,
  NumGameModes
};

extern bool g_editing;
extern int g_locationId;
extern bool g_requestQuit;
extern int g_difficultyLevel;
extern bool g_largeMenus;
extern int g_requestedLocationId;
extern int g_gameMode;
extern bool g_atMainMenu;
extern bool g_requestToggleEditing;
extern Server* g_server;
extern ControlHelpAccess* g_controlHelpSystem;
extern char g_userProfileName[256];
