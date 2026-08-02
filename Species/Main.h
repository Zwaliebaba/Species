#pragma once

extern double g_gameTime; // Updated from GetHighResTime every frame
extern float g_advanceTime; // How long the last frame took
extern double g_lastServerAdvance; // Time of last server advance
extern float g_predictionTime; // Time between last server advance and start of render
extern float g_targetFrameRate;
extern int g_lastProcessedSequenceId;
extern int g_sliceNum; // Most recently advanced slice

void AppMain();

