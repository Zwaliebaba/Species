#include "pch.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include "FilesysUtils.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace
{
  // A directory this test makes and destroys, so nothing here depends on
  // GameData having been deployed next to the test binary.
  class TempDirectory
  {
    public:
      explicit TempDirectory(const char* _name)
        : m_path(std::filesystem::temp_directory_path() / _name)
      {
        std::filesystem::remove_all(m_path);
        std::filesystem::create_directories(m_path);
      }

      ~TempDirectory() { std::filesystem::remove_all(m_path); }

      void AddFile(const char* _name) const { std::ofstream(m_path / _name) << 'x'; }
      void AddSubDirectory(const char* _name) const { std::filesystem::create_directory(m_path / _name); }

      // ListDirectory pastes the filter straight onto the end of this and
      // prefixes full results with it, so the trailing separator belongs here.
      std::string Prefix() const { return m_path.string() + "\\"; }

    private:
      std::filesystem::path m_path;
  };

  // Both listing functions hand back a vector the caller owns, holding names
  // the caller owns — but they do not allocate those names the same way, so
  // these two collectors are not interchangeable. Getting it wrong here is the
  // point: the tests are the place that spells the contract out.
  std::set<std::string> TakeNamesFromNewArray(std::vector<char*>* _result)
  {
    std::set<std::string> names;
    for (char* name : *_result)
    {
      names.insert(name);
      delete[] name;
    }
    delete _result;
    return names;
  }

  std::set<std::string> TakeNamesFromStrdup(std::vector<char*>* _result)
  {
    std::set<std::string> names;
    for (char* name : *_result)
    {
      names.insert(name);
      free(name);
    }
    delete _result;
    return names;
  }
} // namespace

namespace NeuronClientTests
{
  // The path helpers split the content paths that Resource.cpp and LevelFile.cpp
  // hand to the loaders — "Shapes/foo.shp" and friends. They are pure string
  // functions over forward-slash paths, which makes them the part of
  // NeuronClient that can be tested without a window, a GL context or GameData
  // on disk.
  //
  // They return by value as of strings-modernised/T16, so a result outlives the
  // next call and two can be held at once — TwoResultsAreValidAtTheSameTime is
  // the case the old shared static could not support.
  //
  // Paths with no '/' and paths with no '.' USED to be undefined behaviour and
  // were deliberately left uncovered for that reason: GetFilenamePart and
  // GetExtensionPart both added 1 to the result of strrchr before testing it,
  // so a missing separator produced 0x1 rather than null and the read that
  // followed went to address 1. strings-modernised/T4 defined them, so they
  // are covered below — that is the fix the old note was waiting for.
  TEST_CLASS(FilesysUtilsTests)
  {
    public:
      TEST_METHOD(GetDirectoryPartKeepsTheTrailingSlash) { Assert::AreEqual("Shapes/", GetDirectoryPart("Shapes/citizen.shp").c_str()); }

      TEST_METHOD(GetDirectoryPartStopsAtTheLastSlash) { Assert::AreEqual("Sounds/Effects/", GetDirectoryPart("Sounds/Effects/laser.wav").c_str()); }

      // WAS GetDirectoryPartReturnsNullWhenThereIsNoDirectory, and the change of
      // name is the change of contract. A std::string return cannot be null, and
      // the choice between empty and std::optional went to empty because
      // GetExtensionPart already answers a missing part that way — see the test
      // below it, and T4's reasoning for why null would crash the callers.
      // GetDirectoryPart has no caller outside these tests, so nothing depended
      // on the old null.
      TEST_METHOD(GetDirectoryPartIsEmptyWhenThereIsNoDirectory)
      {
        Assert::IsTrue(GetDirectoryPart("Locations.txt").empty());
      }

      TEST_METHOD(GetFilenamePartDropsEveryDirectory) { Assert::AreEqual("laser.wav", GetFilenamePart("Sounds/Effects/laser.wav").c_str()); }

      TEST_METHOD(GetExtensionPartExcludesTheDot) { Assert::AreEqual("shp", GetExtensionPart("Shapes/citizen.shp").c_str()); }

      // The three edges strings-modernised/T4 defined. Each was undefined
      // behaviour before it, so these pin a decision rather than a discovery.
      TEST_METHOD(GetFilenamePartOfABareNameIsTheNameItself)
      {
        Assert::AreEqual("Locations.txt", GetFilenamePart("Locations.txt").c_str());
      }

      TEST_METHOD(GetExtensionPartOfANameWithNoDotIsEmptyNotNull)
      {
        // Empty rather than null on purpose: every caller passes the result
        // straight to stricmp or a BitmapRGBA constructor without checking it,
        // so null would turn a missing extension into a crash.
        const std::string extension = GetExtensionPart("Shapes/citizen");
        Assert::IsTrue(extension.empty());
      }

      // The case the shared static made untestable. Under it, holding the first
      // result across the second call read a buffer that had been overwritten,
      // so a test could only measure whether the string happened to survive.
      // By value, both are simply correct — which is the whole point of T16.
      TEST_METHOD(TwoResultsAreValidAtTheSameTime)
      {
        const std::string directory = GetDirectoryPart("Sounds/Effects/laser.wav");
        const std::string extension = GetExtensionPart("Sounds/Effects/laser.wav");
        const std::string filename = GetFilenamePart("Sounds/Effects/laser.wav");

        Assert::AreEqual("Sounds/Effects/", directory.c_str());
        Assert::AreEqual("wav", extension.c_str());
        Assert::AreEqual("laser.wav", filename.c_str());
      }

      TEST_METHOD(RemoveExtensionKeepsTheDirectory) { Assert::AreEqual("Shapes/citizen", RemoveExtension("Shapes/citizen.shp").c_str()); }

      TEST_METHOD(RemoveExtensionLeavesAnExtensionlessNameAlone) { Assert::AreEqual("Shapes/citizen", RemoveExtension("Shapes/citizen").c_str()); }

      TEST_METHOD(RemoveExtensionStripsOnlyTheLastExtension) { Assert::AreEqual("archive.tar", RemoveExtension("archive.tar.gz").c_str()); }
  };

  // ListDirectory and ListSubDirectoryNames used to return an LList<char*>;
  // tasks/containers-replaced.yaml T6 made them std::vector<char*>. Nothing
  // pinned what they return or who owns it, which is why the callers had drifted
  // into three different opinions about how to free the results. These tests
  // pin the answer to both questions.
  //
  // Not covered: a directory that does not exist. _findfirst returns -1 and both
  // functions return an empty vector, but no caller relies on that today and
  // pinning it would freeze a behaviour nobody has chosen.
  TEST_CLASS(DirectoryListingTests)
  {
    public:
      TEST_METHOD(ListDirectoryAppliesTheFilter)
      {
        const TempDirectory dir("SpeciesListDirectoryFilter");
        dir.AddFile("alpha.txt");
        dir.AddFile("beta.txt");
        dir.AddFile("gamma.dat");

        const std::set<std::string> names = TakeNamesFromNewArray(ListDirectory(dir.Prefix().c_str(), "*.txt", false));

        Assert::AreEqual(2, static_cast<int>(names.size()));
        Assert::IsTrue(names.contains("alpha.txt"));
        Assert::IsTrue(names.contains("beta.txt"));
      }

      TEST_METHOD(ListDirectoryTreatsAnEmptyFilterAsEverything)
      {
        const TempDirectory dir("SpeciesListDirectoryEmptyFilter");
        dir.AddFile("alpha.txt");
        dir.AddFile("gamma.dat");

        Assert::AreEqual(2, static_cast<int>(TakeNamesFromNewArray(ListDirectory(dir.Prefix().c_str(), "", false)).size()));
        Assert::AreEqual(2, static_cast<int>(TakeNamesFromNewArray(ListDirectory(dir.Prefix().c_str(), nullptr, false)).size()));
      }

      TEST_METHOD(ListDirectoryPrefixesTheDirectoryForLongResults)
      {
        const TempDirectory dir("SpeciesListDirectoryLongResults");
        dir.AddFile("alpha.txt");

        const std::set<std::string> names = TakeNamesFromNewArray(ListDirectory(dir.Prefix().c_str(), "*.txt", true));

        Assert::AreEqual(1, static_cast<int>(names.size()));
        Assert::AreEqual(dir.Prefix() + "alpha.txt", *names.begin());
      }

      TEST_METHOD(ListDirectorySkipsSubDirectories)
      {
        const TempDirectory dir("SpeciesListDirectorySkipsDirs");
        dir.AddFile("alpha.txt");
        dir.AddSubDirectory("nested");

        const std::set<std::string> names = TakeNamesFromNewArray(ListDirectory(dir.Prefix().c_str(), "*", false));

        Assert::AreEqual(1, static_cast<int>(names.size()));
        Assert::IsTrue(names.contains("alpha.txt"));
      }

      TEST_METHOD(ListSubDirectoryNamesReturnsOnlyDirectories)
      {
        const TempDirectory dir("SpeciesListSubDirectoryNames");
        dir.AddFile("alpha.txt");
        dir.AddSubDirectory("nested");
        dir.AddSubDirectory("other");

        // The wildcard is part of the argument here, unlike ListDirectory —
        // callers pass paths like "users/*.*".
        const std::set<std::string> names = TakeNamesFromStrdup(ListSubDirectoryNames((dir.Prefix() + "*").c_str()));

        Assert::AreEqual(2, static_cast<int>(names.size()));
        Assert::IsTrue(names.contains("nested"));
        Assert::IsTrue(names.contains("other"));
      }
  };
} // namespace NeuronClientTests
