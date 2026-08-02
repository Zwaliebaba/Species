#include "pch.h"

#include "Random.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronCoreTests
{
  // speciesRandom is the simulation's only random source, and its output
  // sequence is part of the multiplayer contract: deterministic lockstep works
  // because every client makes the same sequence of calls against the same
  // generator. Changing the constants, the shift, or the mask desyncs every
  // client against every other build — while every build stays green.
  //
  // The expected values are the MSVC rand() sequence for seed 1, pinned so a
  // reimplementation has to declare itself rather than land quietly. See
  // CODING_STANDARDS.md, Determinism.
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
