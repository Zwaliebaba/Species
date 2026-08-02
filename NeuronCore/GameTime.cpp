#include "pch.h"

#include "GameTime.h"

// Definitions moved down from Species/Main.cpp with their initialisers intact:
// the values are load-bearing (g_targetFrameRate seeds the frame pacing and
// g_lastProcessedSequenceId's -1 means "nothing processed yet").
double g_gameTime = 0.0;
float g_advanceTime;
double g_lastServerAdvance;
float g_predictionTime;
float g_targetFrameRate = 20.0f;
int g_lastProcessedSequenceId = -1;
int g_sliceNum;
