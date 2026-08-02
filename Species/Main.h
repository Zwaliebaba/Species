#pragma once

// The frame clock moved down to NeuronCore so GameLogic and NeuronClient can
// read it without reaching up into the executable. Included here so Species
// code that has always got it from Main.h still does.
#include "GameTime.h"

void AppMain();
