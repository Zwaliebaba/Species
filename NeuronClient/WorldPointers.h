#pragma once

#include "CameraAccess.h"
#include "RendererAccess.h"
#include "GameCursorAccess.h"
#include "LocationAccess.h"
#include "WorldTypeNames.h"
#include "LocationEditorAccess.h"
#include "ScriptAccess.h"
#include "TaskManagerInterfaceAccess.h"
#include "UserInputAccess.h"

// The running game's subsystems, reachable from every layer.
//
// These used to be members of Species::App, so anything below Species that
// wanted the world, the camera or the renderer had to include App.h and reach
// through g_app. They are globals here instead — and the ONLY storage, not an
// alias of an App member: Main.cpp reassigns the location on every level load
// and GameLogic itself reassigns the renderer, so a mirrored copy would go
// stale the first time an assignment site forgot to update it.
//
// Forward declarations are enough to declare the pointers. A file that
// dereferences one still includes that type's own header, exactly as it did
// before. See tasks/layering-inversion.yaml T8.
class Location;
class GlobalWorld;
class ParticleSystem;
class TaskManager;


namespace Neuron
{
  extern Location* g_location;
  extern GlobalWorld* g_globalWorld;
  extern CameraAccess* g_camera;
  extern RendererAccess* g_renderer;
  extern ParticleSystem* g_particleSystem;
  extern LocationEditorAccess* g_locationEditor;
  extern GameCursorAccess* g_gameCursor;
  extern TaskManager* g_taskManager;
  extern TaskManagerInterfaceAccess* g_taskManagerInterface;
  extern ScriptAccess* g_script;
  extern UserInputAccess* g_userInput;
} // namespace Neuron
