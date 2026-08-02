#pragma once

// NUM_TEAMS and the server tick constants are protocol facts and live in
// NeuronCore, where the server can reach them. Included here so every existing
// user of Globals.h still sees them.
#include "ProtocolLimits.h"

#define NUM_SLICES_PER_FRAME    10                              // Num slices to break up heavy weight physics into
#define MINIMUM_RENDER_PERIOD   1.0f                            // Render at least this frequently
#define GRAVITY	                10.0f


