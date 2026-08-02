#include "pch.h"

#include "Generic.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronServerTests
{
  // NeuronServer is a stub: one header that includes NeuronCore.h, and a pch.
  // There is no behaviour here to test yet, and writing a test class full of
  // empty methods would only make the suite look larger than it is.
  //
  // What this file does test is the wiring, which is the thing that breaks
  // silently. A test project whose include path or project references have come
  // loose still compiles right up until someone writes the first real test, and
  // then fails in a way that looks like the new test's fault. Reaching NeuronCore
  // through the NeuronServer layer header and calling into it proves the include
  // path, the precompiled header and both project references are intact.
  //
  // Replace this with real coverage as the authoritative host gains code. Delete
  // it once there is any; it is scaffolding, not a permanent test.
  TEST_CLASS(LayerWiringTests)
  {
    public:
      TEST_METHOD(NeuronCoreIsReachableThroughTheLayerHeader) { Assert::AreEqual(0x0100007F, ConvertIPToInt("127.0.0.1")); }
  };
} // namespace NeuronServerTests
