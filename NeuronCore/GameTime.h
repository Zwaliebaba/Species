#pragma once

// The frame clock. These live in NeuronCore rather than beside the main loop
// because both GameLogic and NeuronClient advance against them, and NeuronCore
// is the only layer both can see. Species/Main.cpp still drives them; this is
// where everyone else reads them.
extern double g_gameTime;          // Updated from GetHighResTime every frame
extern float g_advanceTime;        // How long the last frame took
extern double g_lastServerAdvance; // Time of last server advance
extern float g_predictionTime;     // Time between last server advance and start of render
extern float g_targetFrameRate;
extern int g_lastProcessedSequenceId;
extern int g_sliceNum; // Most recently advanced slice
