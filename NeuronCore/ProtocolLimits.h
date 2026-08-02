#pragma once

// Constants both sides of the wire must agree on.
//
// These lived in Species/Globals.h beside the renderer's frame budget and the
// gravity constant, which meant the server could not see them without depending
// on the game. They are protocol facts: change one and old and new builds stop
// agreeing about how many teams exist or how often the server ticks.
//
// Species/Globals.h includes this, so nothing that used those names had to change.

#define NUM_TEAMS 4
#define SERVER_ADVANCE_PERIOD 0.1f // Server ticks at this rate
#define SERVER_ADVANCE_FREQ 10.0f  // Server ticks at this rate
#define IAMALIVE_PERIOD 0.1f       // Clients must send IAmAlive this often
