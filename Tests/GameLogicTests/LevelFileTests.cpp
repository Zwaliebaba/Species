#include "pch.h"

#include <string>

#include "LevelFile.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Only the default constructor is reachable from a test. The two-argument one
// parses its map and mission files through g_resource and g_appCommands, both
// of which the executable owns, so what it does with its arguments cannot be
// covered here — LevelFile.cpp's own comment and strings-modernised T8's notes
// say which lines that leaves unproven.
//
// What IS covered is the filename state a LevelFile starts in, and that is
// worth a test rather than a reading: two of the five members were left
// INDETERMINATE by this constructor before T8 converted them, and Save() tests
// both with strstr. A char[256] full of stack garbage containing the four
// letters "null" would have skipped the write; one that did not would have
// written a file named after the garbage. Neither is reachable now, and this
// is the test that says so.
namespace GameLogicTests
{
  TEST_CLASS(LevelFileTests)
  {
    public:
      TEST_METHOD(ANewLevelFileNamesTheThreeDefaultColourFiles)
      {
        const LevelFile level;

        Assert::AreEqual(std::string("LandscapeDefault.bmp"), level.m_landscapeColourFilename);
        Assert::AreEqual(std::string("WavesDefault.bmp"), level.m_wavesColourFilename);
        Assert::AreEqual(std::string("WaterDefault.bmp"), level.m_waterColourFilename);
      }

      TEST_METHOD(ANewLevelFileHasNoMapOrMissionFilename)
      {
        const LevelFile level;

        // Empty, and definitely empty. The constructor never assigned these
        // two before T8 and the members were raw arrays, so their contents
        // were whatever the stack held.
        Assert::IsTrue(level.m_mapFilename.empty());
        Assert::IsTrue(level.m_missionFilename.empty());
      }

      TEST_METHOD(ANewLevelFileHasNoDifficultySet)
      {
        const LevelFile level;

        // -1 rather than 0, and it is a sentinel: ParseDifficulty overwrites
        // it and WriteDifficulty is skipped while it holds this value. Pinned
        // alongside the filenames because it is the other thing the default
        // constructor decides.
        Assert::AreEqual(-1, level.m_levelDifficulty);
      }
  };
} // namespace GameLogicTests
