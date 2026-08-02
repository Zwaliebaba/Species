#include "pch.h"

#include "AppCommands.h"

// Null until App installs itself, which it does before anything below Species
// can call through it — and stays null in the test DLLs, which is exactly why
// this definition lives here rather than in the executable.
AppCommands* g_appCommands = nullptr;
