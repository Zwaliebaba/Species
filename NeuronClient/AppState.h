#pragma once

#include "ControlHelpAccess.h"
#include "RgbColour.h"

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

// The level the next load will bring up, written by the global world, the
// script and the menus and read by Main's load path. Character arrays like
// g_userProfileName above, for the same reason: every writer uses strcpy.
extern char g_requestedMission[256];
extern char g_requestedMap[256];

// Set by the pause key and read by the world advance and the renderer.
extern bool g_paused;

// The colour the frame is cleared to, and the fog colour the world derives
// from it. Set once per level from the level file.
extern RGBAColour g_backgroundColour;

// The RenderNegative preference, read once at startup. The landscape and water
// renderers invert their colours for it.
extern bool g_negativeRenderer;
