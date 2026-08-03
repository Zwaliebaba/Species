#include "pch.h"

#include "WorldPointers.h"

// App creates and destroys these; Main.cpp swaps the location and the editor
// per level. Null until startup builds them.
Location* g_location = nullptr;
GlobalWorld* g_globalWorld = nullptr;
CameraAccess* g_camera = nullptr;
RendererAccess* g_renderer = nullptr;
ParticleSystem* g_particleSystem = nullptr;
LocationEditor* g_locationEditor = nullptr;
TaskManager* g_taskManager = nullptr;
TaskManagerInterfaceAccess* g_taskManagerInterface = nullptr;
ScriptAccess* g_script = nullptr;
UserInputAccess* g_userInput = nullptr;
