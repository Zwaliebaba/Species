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
//
// This file already carried a `namespace Neuron { class Server; }` block for
// NeuronServer's Server, which T3 qualified. namespace-migration T2 puts the
// whole file in that namespace, so the block is gone and the forward
// declaration is an ordinary one again.
class Server;


namespace Neuron
{
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
extern std::string g_userProfileName;

// The level the next load will bring up, written by the global world, the
// script and the menus and read by Main's load path.
//
// All three were char[256] and every write into them was an unbounded copy —
// from a profile name the user types, or a mission and map name read out of
// GameData — so a long enough name overran a global. strings-modernised/T7.
extern std::string g_requestedMission;
extern std::string g_requestedMap;

// Set by the pause key and read by the world advance and the renderer.
extern bool g_paused;

// The colour the frame is cleared to, and the fog colour the world derives
// from it. Set once per level from the level file.
extern RGBAColour g_backgroundColour;

// The RenderNegative preference, read once at startup. The landscape and water
// renderers invert their colours for it.
extern bool g_negativeRenderer;
} // namespace Neuron
