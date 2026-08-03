#pragma once

#include "WorldTypeNames.h"

// GameLogic's answer to NeuronClient's WorldTypeNames.
//
// The roster — Citizen, Officer, Squad, RadarDish and the rest — is the game's
// knowledge, so it stays here and the dependency inverts rather than the table
// moving down. `InstallWorldTypeRoster()` points `g_worldTypeNames` at the one
// instance; App calls it during startup, before SoundSystem::Initialise reads
// any blueprint that has to resolve a type name.
//
// It is an explicit call rather than a static initialiser on purpose. GameLogic
// is a static library, and a translation unit nothing else references is not
// guaranteed to be linked in at all — a self-installing object would work until
// the day the linker decided it was unused. See tasks/layering-inversion.yaml
// T17.
void InstallWorldTypeRoster();
