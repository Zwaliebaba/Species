#pragma once

// Simulation constants shared by every layer. This header was in Species,
// which meant GameLogic reached GRAVITY only by including a Species header that
// happened to pull it in — Tripod.cpp got it through Location.h. It has no
// dependencies of its own beyond ProtocolLimits, so it belongs here.
//
// NUM_TEAMS and the server tick constants are protocol facts and live in
// ProtocolLimits.h, where the server can reach them. Included here so every
// existing user of Globals.h still sees them.
#include "ProtocolLimits.h"

#define NUM_SLICES_PER_FRAME 10    // Num slices to break up heavy weight physics into
#define MINIMUM_RENDER_PERIOD 1.0f // Render at least this frequently
#define GRAVITY 10.0f
