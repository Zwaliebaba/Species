#include "pch.h"

#include "Random.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronCoreTests
{
  // speciesRandom is the tree's UNSYNCHRONISED generator — cosmetics only.
  // This comment used to call it "the simulation's only random source", which
  // was wrong: syncrand() in NeuronCore/MathUtils.cpp is the lockstep stream,
  // and that error cost two false determinism alarms. See
  // tasks/determinism.yaml T3 and CODING_STANDARDS.md, Determinism.
  //
  // Its sequence is still worth pinning. Terrain and tree generation reseed it
  // and replay it expecting the same output, so changing the constants, the
  // shift or the mask changes what every level looks like — while every build
  // stays green.
  //
  // The expected values are the MSVC rand() sequence for seed 1, pinned so a
  // reimplementation has to declare itself rather than land quietly.
  TEST_CLASS(RandomTests)
  {
    public:
      TEST_METHOD(SeedOneProducesTheKnownSequence)
      {
        constexpr int expected[] = {41, 18467, 6334, 26500, 19169};

        speciesSeedRandom(1);
        for (int value : expected)
          Assert::AreEqual(value, speciesRandom());
      }

      TEST_METHOD(TheSameSeedReplaysTheSameSequence)
      {
        speciesSeedRandom(12345);
        int first[16];
        for (int& value : first)
          value = speciesRandom();

        speciesSeedRandom(12345);
        for (int value : first)
          Assert::AreEqual(value, speciesRandom());
      }

      TEST_METHOD(ResultsNeverExceedSpeciesRandMax)
      {
        speciesSeedRandom(7);
        for (int i = 0; i < 10000; ++i)
        {
          const int value = speciesRandom();
          Assert::IsTrue(value >= 0 && value <= SPECIES_RAND_MAX);
        }
      }
  };
} // namespace NeuronCoreTests
