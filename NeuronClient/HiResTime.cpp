#include "pch.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <math.h>
#include "Main.h"

#include "HiResTime.h"

#define USE_PENTIUM_COUNTER 0

static unsigned int g_ticksPerSec = 0;
static double g_tickInterval = 1.0;
static double g_lastGetHighResTime = 0.0;
static double g_timeShift = 0.0;
static bool g_usingFakeTimeMode = false;
static double g_fakeTime;

// *** InitialiseHighResTime
void InitialiseHighResTime()
{
  // Start be getting the frequency the Performance Counter uses.
  // We need to use the Performance Counter to calibrate the Pentium
  // Counter, if we are going to use it.
  LARGE_INTEGER count;
  QueryPerformanceFrequency(&count);
  g_tickInterval = 1.0 / static_cast<double>(count.QuadPart);
}

inline double GetLowLevelTime()
{
  LARGE_INTEGER count;

  static LARGE_INTEGER startTime;
  static bool initted = false;

  if (!initted)
  {
    QueryPerformanceCounter(&startTime);
    initted = true;
  }

  QueryPerformanceCounter(&count);
  return static_cast<double>(count.QuadPart - startTime.QuadPart) * g_tickInterval;
}

// *** GetHighResTime
double GetHighResTime()
{
  if (g_usingFakeTimeMode)
  {
    g_lastGetHighResTime = g_fakeTime;
    return g_fakeTime;
  }

  double timeNow = GetLowLevelTime();
  timeNow -= g_timeShift;

  //    if( timeNow > g_lastGetHighResTime + 1.0f )
  //    {
  //        g_timeShift += timeNow - g_lastGetHighResTime;
  //        timeNow -= timeNow - g_lastGetHighResTime;
  //    }

  g_lastGetHighResTime = timeNow;
  return timeNow;
}

void SetFakeTimeMode()
{
  g_fakeTime = GetHighResTime();
  g_usingFakeTimeMode = true;
}

void SetRealTimeMode()
{
  g_usingFakeTimeMode = false;
  double realTime = GetLowLevelTime();
  g_timeShift = realTime - g_fakeTime;
}

void IncrementFakeTime(double _increment) { g_fakeTime += _increment; }
