#include "pch.h"

#include "WorldPointers.h"

// App creates and destroys these; Main.cpp swaps the location and the editor
// per level. Null until startup builds them.


namespace Neuron
{
  Species::Location* g_location = nullptr;
  LocationAccess* g_locationAccess = nullptr;
  WorldTypeNames* g_worldTypeNames = nullptr;
  Species::GlobalWorld* g_globalWorld = nullptr;
  CameraAccess* g_camera = nullptr;
  RendererAccess* g_renderer = nullptr;
  Species::ParticleSystem* g_particleSystem = nullptr;
  LocationEditorAccess* g_locationEditor = nullptr;
  GameCursorAccess* g_gameCursor = nullptr;
  Species::TaskManager* g_taskManager = nullptr;
  TaskManagerInterfaceAccess* g_taskManagerInterface = nullptr;
  ScriptAccess* g_script = nullptr;
  UserInputAccess* g_userInput = nullptr;
} // namespace Neuron
