#include "pch.h"

#include <filesystem>
#include <fstream>

#include "Preferences.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronCoreTests
{
  // Characterisation of PrefsManager as it behaves today, written before the
  // stage 3 and stage 4 conversions touch it (containers-replaced T18,
  // strings-modernised T2). Nothing here asserts that the current behaviour is
  // *good* — several of these tests pin quirks. They exist so that a conversion
  // claiming "no behaviour change" has something to prove it against.
  //
  // The preferences file is user-visible and hand-edited, so its content is a
  // format, not an implementation detail: a conversion that silently renumbers,
  // reorders or reformats it breaks every settings file already on disk.
  namespace
  {
    // PrefsManager resolves every path through FileSys, so pointing the home
    // directory at a temporary tree is what keeps these tests out of GameData/
    // and out of the source tree, per docs/TESTING.md.
    //
    // SetHomeDirectory appends "\GameData\" to whatever it is given, so the
    // restore below strips that suffix back off rather than double-appending it.
    class ScopedPrefsHome
    {
      public:
        ScopedPrefsHome()
        {
          m_previous = FileSys::GetHomeDirectory();

          static int counter = 0;
          m_root = std::filesystem::temp_directory_path() /
                   ("SpeciesPrefsTests_" + std::to_string(::GetCurrentProcessId()) + "_" + std::to_string(++counter));
          std::filesystem::create_directories(m_root / "GameData");

          FileSys::SetHomeDirectory(m_root.wstring());
        }

        ~ScopedPrefsHome()
        {
          const std::wstring suffix = L"\\GameData\\";
          if (m_previous.size() >= suffix.size() && m_previous.compare(m_previous.size() - suffix.size(), suffix.size(), suffix) == 0)
          {
            FileSys::SetHomeDirectory(m_previous.substr(0, m_previous.size() - suffix.size()));
          }

          std::error_code ignored;
          std::filesystem::remove_all(m_root, ignored);
        }

        ScopedPrefsHome(const ScopedPrefsHome&) = delete;
        ScopedPrefsHome& operator=(const ScopedPrefsHome&) = delete;

        void Write(const char* _name, const std::string& _contents) const
        {
          std::ofstream out(m_root / "GameData" / _name, std::ios::binary);
          out << _contents;
        }

        // Line endings are normalised away deliberately. PrefsManager opens its
        // file with fopen(..., "w"), so every "\n" it writes reaches the disk as
        // "\r\n" on Windows. Pinning that would be pinning the C runtime rather
        // than the preferences format.
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

  TEST_CLASS(PreferencesTests)
  {
    public:
      // ---------------------------------------------------------------------
      // Parsing: which of the three types a value lands in
      // ---------------------------------------------------------------------

      TEST_METHOD(AWholeNumberParsesAsAnInt)
      {
        ScopedPrefsHome home;
        home.Write("prefs.txt", "SoundChannels = 32\n");
        PrefsManager prefs("prefs.txt");

        Assert::AreEqual(32, prefs.GetInt("SoundChannels"));
      }

      TEST_METHOD(ANegativeWholeNumberParsesAsAnInt)
      {
        ScopedPrefsHome home;
        home.Write("prefs.txt", "Offset = -7\n");
        PrefsManager prefs("prefs.txt");

        Assert::AreEqual(-7, prefs.GetInt("Offset"));
      }

      TEST_METHOD(ANumberWithExactlyOneDotParsesAsAFloat)
      {
        ScopedPrefsHome home;
        home.Write("prefs.txt", "Volume = 1.5\n");
        PrefsManager prefs("prefs.txt");

        Assert::AreEqual(1.5f, prefs.GetFloat("Volume"), 0.0001f);
      }

      // The quirk that matters most in this file. The type guess counts dots,
      // so a dotted quad has too many to be a number and falls back to a
      // string — which is the only reason "ServerAddress = 127.0.0.1" survives
      // a round trip instead of being truncated to 127. A conversion that
      // "tidies" the parser into strtof/strtol without reproducing the
      // dot-counting rule silently breaks every multiplayer address on disk.
      TEST_METHOD(ADottedQuadParsesAsAStringBecauseItHasMoreThanOneDot)
      {
        ScopedPrefsHome home;
        home.Write("prefs.txt", "ServerAddress = 127.0.0.1\n");
        PrefsManager prefs("prefs.txt");

        Assert::AreEqual("127.0.0.1", prefs.GetString("ServerAddress"));
      }

      TEST_METHOD(ANonNumericValueParsesAsAString)
      {
        ScopedPrefsHome home;
        home.Write("prefs.txt", "TextLanguage = english\n");
        PrefsManager prefs("prefs.txt");

        Assert::AreEqual("english", prefs.GetString("TextLanguage"));
      }

      // ---------------------------------------------------------------------
      // Lookup: the accessors are type-checked, and disagreement is silent
      // ---------------------------------------------------------------------

      TEST_METHOD(AskingForTheWrongTypeReturnsTheCallersDefaultRatherThanConverting)
      {
        ScopedPrefsHome home;
        home.Write("prefs.txt", "Volume = 1.5\n");
        PrefsManager prefs("prefs.txt");

        // Stored as a float, so both of these miss — and do so quietly.
        Assert::AreEqual(-1, prefs.GetInt("Volume"));
        Assert::AreEqual(99, prefs.GetInt("Volume", 99));
        Assert::IsNull(prefs.GetString("Volume"));
      }

      TEST_METHOD(AnAbsentKeyReturnsTheCallersDefault)
      {
        ScopedPrefsHome home;
        home.Write("prefs.txt", "SoundChannels = 32\n");
        PrefsManager prefs("prefs.txt");

        Assert::AreEqual(-1, prefs.GetInt("NoSuchKey"));
        Assert::AreEqual(5, prefs.GetInt("NoSuchKey", 5));
        Assert::IsFalse(prefs.DoesKeyExist("NoSuchKey"));
        Assert::IsTrue(prefs.DoesKeyExist("SoundChannels"));
      }

      // Not a null-safety nicety — App reads ControlMethod at startup, and the
      // constructor manufacturing it is why a prefs file that predates the
      // setting still produces a usable control scheme.
      TEST_METHOD(TheConstructorSuppliesControlMethodWhenTheFileDoesNotHaveIt)
      {
        ScopedPrefsHome home;
        home.Write("prefs.txt", "SoundChannels = 32\n");
        PrefsManager prefs("prefs.txt");

        Assert::AreEqual(1, prefs.GetInt("ControlMethod"));
      }

      // ---------------------------------------------------------------------
      // Saving: the loaded file is a template, and its shape is preserved
      // ---------------------------------------------------------------------

      // Comments and blank lines survive verbatim and in place. This is the
      // property that makes the file hand-editable, and it is the one most
      // easily lost by a rewrite that treats the store as a key-value map.
      TEST_METHOD(SaveKeepsCommentsAndBlankLinesInTheirOriginalPositions)
      {
        ScopedPrefsHome home;
        home.Write("prefs.txt", "# a leading comment\n"
                                "SoundChannels = 32\n"
                                "\n"
                                "# another comment\n"
                                "TextLanguage = english\n"
                                "ControlMethod = 1\n");

        {
          PrefsManager prefs("prefs.txt");
          prefs.Save();
        }

        Assert::AreEqual("# a leading comment\n"
                         "SoundChannels = 32\n"
                         "\n"
                         "# another comment\n"
                         "TextLanguage = english\n"
                         "ControlMethod = 1\n",
                         home.Read("prefs.txt").c_str());
      }

      // Whitespace around '=' is NOT preserved: every value-bearing line is
      // regenerated from the parsed item as "key = value". Pinned because it
      // means Save is not a byte-identical rewrite of its input, so a
      // conversion cannot be checked by diffing a saved file against the
      // original one — only against a previously saved one.
      TEST_METHOD(SaveNormalisesSpacingAroundTheEqualsSign)
      {
        ScopedPrefsHome home;
        home.Write("prefs.txt", "SoundChannels    =    32\n");

        {
          PrefsManager prefs("prefs.txt");
          prefs.Save();
        }

        const std::string saved = home.Read("prefs.txt");
        Assert::IsTrue(saved.find("SoundChannels = 32\n") != std::string::npos, L"the line was not regenerated in canonical form");
      }

      TEST_METHOD(SaveWritesFloatsWithExactlyTwoDecimalPlaces)
      {
        ScopedPrefsHome home;
        home.Write("prefs.txt", "Volume = 1.5\n"
                                "ControlMethod = 1\n");

        {
          PrefsManager prefs("prefs.txt");
          prefs.SetFloat("Volume", 0.125f);
          prefs.Save();
        }

        // 0.125 -> "0.13", not "0.125" and not "0.12": %.2f rounds half away
        // from zero here. The precision is part of the file format.
        Assert::AreEqual("Volume = 0.13\n"
                         "ControlMethod = 1\n",
                         home.Read("prefs.txt").c_str());
      }

      TEST_METHOD(SavingThenLoadingRoundTripsEveryType)
      {
        ScopedPrefsHome home;
        home.Write("prefs.txt", "ServerAddress = 127.0.0.1\n"
                                "SoundChannels = 32\n"
                                "Volume = 1.50\n"
                                "ControlMethod = 1\n");

        {
          PrefsManager prefs("prefs.txt");
          prefs.Save();
        }

        PrefsManager reloaded("prefs.txt");
        Assert::AreEqual("127.0.0.1", reloaded.GetString("ServerAddress"));
        Assert::AreEqual(32, reloaded.GetInt("SoundChannels"));
        Assert::AreEqual(1.5f, reloaded.GetFloat("Volume"), 0.0001f);
      }

      // A second save must be a fixed point. If it is not, every launch of the
      // game rewrites the file differently and the user's diff never settles.
      TEST_METHOD(SavingTwiceProducesTheSameBytes)
      {
        ScopedPrefsHome home;
        home.Write("prefs.txt", "# comment\n"
                                "ServerAddress = 127.0.0.1\n"
                                "\n"
                                "Volume = 1.50\n"
                                "ControlMethod = 1\n");

        std::string first;
        {
          PrefsManager prefs("prefs.txt");
          prefs.Save();
          first = home.Read("prefs.txt");
        }
        {
          PrefsManager prefs("prefs.txt");
          prefs.Save();
        }

        Assert::AreEqual(first.c_str(), home.Read("prefs.txt").c_str());
      }

      // ---------------------------------------------------------------------
      // Saving: keys the template did not contain
      // ---------------------------------------------------------------------

      // A key set for the first time is appended after every template line.
      // One key is pinned here because one key has only one possible position.
      TEST_METHOD(AKeyTheTemplateDidNotContainIsAppendedAfterIt)
      {
        ScopedPrefsHome home;
        home.Write("prefs.txt", "SoundChannels = 32\n"
                                "ControlMethod = 1\n");

        {
          PrefsManager prefs("prefs.txt");
          prefs.SetInt("BrandNewSetting", 7);
          prefs.Save();
        }

        Assert::AreEqual("SoundChannels = 32\n"
                         "ControlMethod = 1\n"
                         "BrandNewSetting = 7\n",
                         home.Read("prefs.txt").c_str());
      }

      // Deliberately asserts presence and NOT relative order.
      //
      // This is the finding that shapes containers-replaced T18. The trailing
      // block is emitted by walking m_items — a HashTable — from slot 0 to
      // Size(), so the order among newly added keys is hash order: a function
      // of the key text, the insertion sequence and where the table last
      // doubled. No std:: container reproduces it, and neither would a
      // different hash. So "item order is unchanged" cannot be an acceptance
      // criterion for the conversion as literally written.
      //
      // What must hold instead, and what this test fixes in place: template
      // lines keep their exact positions (above), and every key survives. The
      // order of the appended block is unspecified today and stays that way.
      TEST_METHOD(SeveralNewKeysAllSurviveButTheirRelativeOrderIsUnspecified)
      {
        ScopedPrefsHome home;
        home.Write("prefs.txt", "ControlMethod = 1\n");

        {
          PrefsManager prefs("prefs.txt");
          prefs.SetInt("AlphaSetting", 1);
          prefs.SetInt("BetaSetting", 2);
          prefs.SetString("GammaSetting", "three");
          prefs.Save();
        }

        const std::string saved = home.Read("prefs.txt");
        Assert::IsTrue(saved.starts_with("ControlMethod = 1\n"), L"the template line moved");
        Assert::IsTrue(saved.find("AlphaSetting = 1\n") != std::string::npos, L"AlphaSetting was lost");
        Assert::IsTrue(saved.find("BetaSetting = 2\n") != std::string::npos, L"BetaSetting was lost");
        Assert::IsTrue(saved.find("GammaSetting = three\n") != std::string::npos, L"GammaSetting was lost");
      }

      // ---------------------------------------------------------------------
      // Mutation
      // ---------------------------------------------------------------------

      TEST_METHOD(SettingAnExistingKeyReplacesItsValueInPlace)
      {
        ScopedPrefsHome home;
        home.Write("prefs.txt", "SoundChannels = 32\n"
                                "ControlMethod = 1\n");

        {
          PrefsManager prefs("prefs.txt");
          prefs.SetInt("SoundChannels", 16);
          prefs.Save();
        }

        Assert::AreEqual("SoundChannels = 16\n"
                         "ControlMethod = 1\n",
                         home.Read("prefs.txt").c_str());
      }

      // SetString's own comment warns that the incoming pointer may BE the
      // stored one; the copy-before-free ordering in it is what makes this
      // safe. Pinned so a conversion to std::string keeps the aliasing case
      // working rather than reintroducing a use-after-free.
      TEST_METHOD(SettingAStringFromItsOwnStoredValueIsSafe)
      {
        ScopedPrefsHome home;
        home.Write("prefs.txt", "TextLanguage = english\n");
        PrefsManager prefs("prefs.txt");

        prefs.SetString("TextLanguage", prefs.GetString("TextLanguage"));

        Assert::AreEqual("english", prefs.GetString("TextLanguage"));
      }

      TEST_METHOD(ClearRemovesEveryItem)
      {
        ScopedPrefsHome home;
        home.Write("prefs.txt", "SoundChannels = 32\n");
        PrefsManager prefs("prefs.txt");

        prefs.Clear();

        Assert::IsFalse(prefs.DoesKeyExist("SoundChannels"));
        Assert::AreEqual(-1, prefs.GetInt("SoundChannels"));
      }
  };
} // namespace NeuronCoreTests
