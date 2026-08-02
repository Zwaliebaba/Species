#pragma once

// The layer under test. Test sources compile against exactly what NeuronServer's
// own translation units compile against — NeuronServer/pch.h includes this and
// nothing else — so a header that works inside the library works inside a test.
// "pch.h" resolves to this file rather than NeuronServer/pch.h because the
// compiler searches the including file's own directory first.
#include "NeuronServer.h"

#include <CppUnitTest.h>
