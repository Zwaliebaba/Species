#include "pch.h"

#include "FilesysUtils.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronClientTests
{
  // The path helpers split the content paths that Resource.cpp and LevelFile.cpp
  // hand to the loaders — "Shapes/foo.shp" and friends. They are pure string
  // functions over forward-slash paths, which makes them the part of
  // NeuronClient that can be tested without a window, a GL context or GameData
  // on disk.
  //
  // Every one of them returns a pointer into a single shared static buffer, so
  // a result must be consumed before the next call. The tests below hold at most
  // one result at a time for that reason, not by accident.
  //
  // Deliberately not covered: paths with no '/' and paths with no '.'.
  // GetFilenamePart and GetExtensionPart both add 1 to the result of strrchr
  // before testing it, so those inputs are undefined behaviour today rather
  // than a defined "returns nullptr". Pinning the current behaviour would make
  // the eventual fix look like a regression; see the note in docs/TESTING.md.
  TEST_CLASS(FilesysUtilsTests)
  {
    public:
      TEST_METHOD(GetDirectoryPartKeepsTheTrailingSlash) { Assert::AreEqual("Shapes/", GetDirectoryPart("Shapes/darwinian.shp")); }

      TEST_METHOD(GetDirectoryPartStopsAtTheLastSlash) { Assert::AreEqual("Sounds/Effects/", GetDirectoryPart("Sounds/Effects/laser.wav")); }

      TEST_METHOD(GetDirectoryPartReturnsNullWhenThereIsNoDirectory) { Assert::IsNull(GetDirectoryPart("Locations.txt")); }

      TEST_METHOD(GetFilenamePartDropsEveryDirectory) { Assert::AreEqual("laser.wav", GetFilenamePart("Sounds/Effects/laser.wav")); }

      TEST_METHOD(GetExtensionPartExcludesTheDot) { Assert::AreEqual("shp", GetExtensionPart("Shapes/darwinian.shp")); }

      TEST_METHOD(RemoveExtensionKeepsTheDirectory) { Assert::AreEqual("Shapes/darwinian", RemoveExtension("Shapes/darwinian.shp")); }

      TEST_METHOD(RemoveExtensionLeavesAnExtensionlessNameAlone) { Assert::AreEqual("Shapes/darwinian", RemoveExtension("Shapes/darwinian")); }

      TEST_METHOD(RemoveExtensionStripsOnlyTheLastExtension) { Assert::AreEqual("archive.tar", RemoveExtension("archive.tar.gz")); }
  };
} // namespace NeuronClientTests
