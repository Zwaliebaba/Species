#pragma once

// The layer under test. Test sources compile against exactly what NeuronCore's
// own translation units compile against — NeuronCore/pch.h includes this and
// nothing else — so a header that works inside the library works inside a test.
// "pch.h" resolves to this file rather than NeuronCore/pch.h because the
// compiler searches the including file's own directory first.
#include "NeuronCore.h"

#include <CppUnitTest.h>
