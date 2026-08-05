#include "pch.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "FileSys.h"
#include "FileWriter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// FileWriter::printf is the API every level, location and profile file is
// written through — 207 call sites across 29 files — and nothing pinned a byte
// of it. These tests exist so that strings-modernised T8, T19 and T17 have
// something to prove "byte-identical output" against, in that order: T8 and T19
// change the TYPES the writers hand it, and T17 replaces the variadic printf
// itself with std::format templates. Field widths are where printf and
// std::format are least alike, and a profile written with different padding is
// a different file even though every character of content matches.
//
// The formats below are copied from the call sites rather than invented,
// because the point is to pin what the game writes and not what printf can do.
// Two tests feed a real format synthetic values — an over-long name and an
// empty one — to pin the edges of the padding rather than only its middle.
namespace NeuronClientTests
{
  namespace
  {
    // FileWriter resolves its path through FileSys, so pointing the home
    // directory at a temporary tree is what keeps these tests out of GameData/
    // and out of the source tree, per docs/TESTING.md. Same shape as
    // NeuronCoreTests' ScopedPrefsHome, including the restore that strips the
    // "\GameData\" the setter appends.
    class ScopedWriterHome
    {
      public:
        ScopedWriterHome()
        {
          m_previous = Neuron::FileSys::GetHomeDirectory();

          static int counter = 0;
          m_root = std::filesystem::temp_directory_path() /
                   ("SpeciesFileWriterTests_" + std::to_string(::GetCurrentProcessId()) + "_" + std::to_string(++counter));
          std::filesystem::create_directories(m_root / "GameData");

          Neuron::FileSys::SetHomeDirectory(m_root.wstring());
        }

        ~ScopedWriterHome()
        {
          const std::wstring suffix = L"\\GameData\\";
          if (m_previous.size() >= suffix.size() && m_previous.compare(m_previous.size() - suffix.size(), suffix.size(), suffix) == 0)
          {
            Neuron::FileSys::SetHomeDirectory(m_previous.substr(0, m_previous.size() - suffix.size()));
          }

          std::error_code ignored;
          std::filesystem::remove_all(m_root, ignored);
        }

        ScopedWriterHome(const ScopedWriterHome&) = delete;
        ScopedWriterHome& operator=(const ScopedWriterHome&) = delete;

        // Line endings are normalised away deliberately. FileWriter opens with
        // fopen(..., "w"), so every "\n" it writes reaches the disk as "\r\n"
        // on Windows. Pinning that would be pinning the C runtime rather than
        // the file format.
        [[nodiscard]] std::string Read(const char* _name) const
        {
          std::ifstream in(m_root / "GameData" / _name, std::ios::binary);
          std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
          std::erase(contents, '\r');
          return contents;
        }

      private:
        std::wstring m_previous;
        std::filesystem::path m_root;
    };
  } // namespace

  TEST_CLASS(FileWriterTests)
  {
    public:
      // ---------------------------------------------------------------------
      // The padded row. GlobalWorld::WriteLocations is the only writer in the
      // tree that specifies field widths, and it is the one thing in these
      // formats that std::format spells differently.
      // ---------------------------------------------------------------------

      TEST_METHOD(TheLocationRowRightAlignsFourFourThirtyAndForty)
      {
        ScopedWriterHome home;
        {
          FileWriter out("locations.txt", false);
          out.printf("\t%4d %4d %30s %40s\n", 2, 1, "MapGarden.txt", "MissionGardenLiberate.txt");
        }

        // 4 and 4 wide for the two integers, then 30 and 40 wide RIGHT-aligned
        // for the two filenames: 17 leading spaces for a 13-character map name
        // and 15 for a 25-character mission name, each after the separating
        // space the format itself carries.
        Assert::AreEqual(std::string("\t   2    1                  MapGarden.txt                MissionGardenLiberate.txt\n"),
                         home.Read("locations.txt"));
      }

      TEST_METHOD(AFieldWiderThanItsPaddingIsNotTruncated)
      {
        ScopedWriterHome home;
        {
          FileWriter out("locations.txt", false);
          out.printf("\t%4d %4d %30s %40s\n", 10000, 1, "MapAnExcessivelyLongLocationName.txt", "null");
        }

        // printf's width is a MINIMUM, so an over-long value pushes the row
        // wider rather than being cut — and the same is true of the integer.
        // A conversion that clamped instead would still look right on every
        // shipped level and corrupt a hand-edited one.
        Assert::AreEqual(std::string("\t10000    1 MapAnExcessivelyLongLocationName.txt                                     null\n"),
                         home.Read("locations.txt"));
      }

      TEST_METHOD(AnEmptyStringStillOccupiesItsField)
      {
        ScopedWriterHome home;
        {
          FileWriter out("locations.txt", false);
          out.printf("[%30s]\n", "");
        }

        Assert::AreEqual(std::string("[                              ]\n"), home.Read("locations.txt"));
      }

      // ---------------------------------------------------------------------
      // The unpadded formats, which are the rest of the level file
      // ---------------------------------------------------------------------

      TEST_METHOD(TheColourFileLinesAreUnpadded)
      {
        ScopedWriterHome home;
        {
          FileWriter out("map.txt", false);
          out.printf("\tlandColourFile %s\n", "LandscapeDefault.bmp");
          out.printf("\twavesColourFile %s\n", "WavesDefault.bmp");
          out.printf("\twaterColourFile %s\n", "WaterDefault.bmp");
        }

        Assert::AreEqual(std::string("\tlandColourFile LandscapeDefault.bmp\n"
                                     "\twavesColourFile WavesDefault.bmp\n"
                                     "\twaterColourFile WaterDefault.bmp\n"),
                         home.Read("map.txt"));
      }

      TEST_METHOD(TwoDecimalPlacesKeepTrailingZeros)
      {
        ScopedWriterHome home;
        {
          FileWriter out("map.txt", false);
          out.printf("\tcellSize %.2f\n", 4.0f);
          out.printf("\toutsideHeight %.2f\n", -1.75f);
        }

        // %.2f is fixed rather than shortest-round-trip, so the trailing zeros
        // are part of the format: std::format's "{}" would write "4" here and
        // change every map file in GameData.
        Assert::AreEqual(std::string("\tcellSize 4.00\n"
                                     "\toutsideHeight -1.75\n"),
                         home.Read("map.txt"));
      }

      // ---------------------------------------------------------------------
      // The encrypted form, which the profile writer uses outside Debug
      // ---------------------------------------------------------------------

      TEST_METHOD(TheEncryptedFormOpensWithRedshirt2)
      {
        ScopedWriterHome home;
        {
          FileWriter out("save.txt", true);
        }

        Assert::AreEqual(std::string("redshirt2"), home.Read("save.txt"));
      }

      TEST_METHOD(TheUnencryptedFormWritesNoHeader)
      {
        ScopedWriterHome home;
        {
          FileWriter out("map.txt", false);
          out.printf("Landscape_StartDefinition\n");
        }

        // Map files are written unencrypted and profiles encrypted, so the
        // header is the whole difference between the two paths, and it belongs
        // in a test rather than in a reader's memory.
        Assert::AreEqual(std::string("Landscape_StartDefinition\n"), home.Read("map.txt"));
      }

      TEST_METHOD(TheEncryptionShiftsPrintableBytesAndCarriesItsOffsetAcrossCalls)
      {
        ScopedWriterHome home;
        {
          FileWriter out("save.txt", true);
          out.printf("AB\n");
          out.printf("A\n");
        }

        // The offset table starts {31, 7, 9, 1, ...} and the index is
        // incremented BEFORE it is used, so the first shifted byte takes 7 and
        // not 31: 'A' + 7 = 'H', 'B' + 9 = 'K'. The newline is untouched
        // because the loop only shifts bytes above 32. The second call
        // continues the same sequence rather than restarting it — 'A' + 1 =
        // 'B' — which is why a save file cannot be decrypted a line at a time.
        Assert::AreEqual(std::string("redshirt2HK\nB\n"), home.Read("save.txt"));
      }
  };
} // namespace NeuronClientTests
