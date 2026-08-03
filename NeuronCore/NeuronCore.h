#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <exception>
#include <format>
#include <functional>
#include <future>
#include <iterator>
#include <map>
#include <memory>
#include <queue>
#include <ranges>
#include <set>
#include <stack>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Use the C++ standard templated min/max
#define NOMINMAX

// DirectX apps don't need GDI
#define NODRAWTEXT
// #define NOGDI
#define NOBITMAP

// Include <mcx.h> if you need this
#define NOMCX

// Include <winsvc.h> if you need this
#define NOSERVICE

// WinHelp is deprecated
#define NOHELP

#if !defined WIN32_LEAN_AND_MEAN
// #define WIN32_LEAN_AND_MEAN
#endif

#include <WinSock2.h>

#include <Windows.h>
#include <hstring.h>
#include <restrictederrorinfo.h>
#include <unknwn.h>

#pragma comment(lib, "Ws2_32.lib")

// Undefine GetCurrentTime macro to prevent
// conflict with Storyboard::GetCurrentTime
#undef GetCurrentTime

#include "Debug.h"
#include "FileSys.h"
#include "NeuronHelper.h"

using namespace Neuron;

#define TARGET_DEBUG

#define SPECIES_VERSION "1.5.11"
#define SPECIES_EXE_VERSION 1, 5, 11, 0
#define STR_SPECIES_EXE_VERSION "1, 5, 11, 0\0"

#define DEBUG_RENDER_ENABLED

// #define USE_CRASHREPORTING

#ifndef _OPENMP
#define PROFILER_ENABLED
#endif

#ifdef TARGET_DEBUG
#define SPECIES_GAMETYPE "debug"
#define LOCATION_EDITOR
#define CHEATMENU_ENABLED
#endif

#ifndef PROFILER_ENABLED
#define SPECIES_VERSION_PROFILER "-np"
#else
#define SPECIES_VERSION_PROFILER ""
#endif

#define SPECIES_VERSION_STRING SPECIES_PLATFORM "-" SPECIES_GAMETYPE "-" SPECIES_VERSION SPECIES_VERSION_PROFILER

#include <stdio.h>
#include <math.h>

#pragma warning(disable : 4244 4305 4800 4018)

// Defines that will enable you to double click on a #pragma message
// in the Visual Studio output window.
#define MESSAGE_LINENUMBERTOSTRING(linenumber) #linenumber
#define MESSAGE_LINENUMBER(linenumber) MESSAGE_LINENUMBERTOSTRING(linenumber)
#define MESSAGE(x) message(__FILE__ "(" MESSAGE_LINENUMBER(__LINE__) "): " x)

#include <crtdbg.h>

// A compatibility #define mapping C99's bounded formatting function onto the
// old MSVC underscore-prefixed variant used to sit here, for a compiler that
// predates the standard one by two decades. Nothing in the tree called it, so
// it was dead — and worse than dead: the underscore variant does not
// null-terminate on truncation, so the first person to write a standard call
// would have got silently different behaviour from the one they read about.

// Visual studio 2005 insists that we use the underscored versions
#include <string.h>
#define stricmp _stricmp
#define strupr _strupr
#define strnicmp _strnicmp
#define strlwr _strlwr
#define strdup _strdup

#include <stdlib.h>
#define itoa _itoa

#define SPECIES_PLATFORM "win32"

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINDOWS 0x0500 // for IsDebuggerPresent
#include "windows.h"

#define HAVE_DSOUND

#include <GL/gl.h>
#include <GL/glu.h>

#define SAFE_FREE(x) \
  {                  \
    free(x);         \
    x = nullptr;     \
  }
#define SAFE_DELETE(x) \
  {                    \
    delete x;          \
    x = nullptr;       \
  }
#define SAFE_DELETE_ARRAY(x) \
  {                          \
    delete[] x;              \
    x = nullptr;             \
  }
#define SAFE_RELEASE(x) \
  {                     \
    if (x)              \
    {                   \
      (x)->Release();   \
      x = nullptr;      \
    }                   \
  }
