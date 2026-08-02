#pragma once

// The layer under test. GameLogic/pch.h includes NeuronClient.h, not a
// GameLogic.h — the layer has no header of its own — so this file matches it.
// "pch.h" resolves to this file rather than GameLogic/pch.h because the
// compiler searches the including file's own directory first.
#include "NeuronClient.h"

#include <CppUnitTest.h>
