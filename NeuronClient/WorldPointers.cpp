#include "pch.h"

#include "WorldPointers.h"

// App creates and destroys these; Main.cpp swaps the location and the editor
// per level. Null until startup builds them.
Location* g_location = nullptr;
GlobalWorld* g_globalWorld = nullptr;
Camera* g_camera = nullptr;
Renderer* g_renderer = nullptr;
ParticleSystem* g_particleSystem = nullptr;
LocationEditor* g_locationEditor = nullptr;
TaskManager* g_taskManager = nullptr;
TaskManagerInterface* g_taskManagerInterface = nullptr;
Script* g_script = nullptr;
UserInput* g_userInput = nullptr;
