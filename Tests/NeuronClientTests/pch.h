#pragma once

// The layer under test. Test sources compile against exactly what NeuronClient's
// own translation units compile against — NeuronClient/pch.h includes this and
// nothing else — so a header that works inside the library works inside a test.
// "pch.h" resolves to this file rather than NeuronClient/pch.h because the
// compiler searches the including file's own directory first.
#include "NeuronClient.h"

#include <CppUnitTest.h>
