#include "pch.h"

#include "EclWindow.h"
#include "InputField.h"
#include "KeyDefs.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// GameLogic is in namespace Species since namespace-migration T4, and unlike
// namespace Neuron it has no tree-wide using-directive to reach it by. A test
// source is the right place for one: it is a .cpp, so nothing includes it.
using namespace Species;

namespace GameLogicTests
{
  namespace
  {
    // InputField is the editor's text widget: it accumulates keystrokes into
    // its own buffer and writes the result back into storage a caller
    // registered with it. These tests characterise that write-back, because
    // strings-modernised T5 changed what the registered storage IS — a raw
    // char* before, a std::string* now — and the only claim a conversion
    // commit makes is that behaviour did not move.
    //
    // WHAT A KEYSTROKE IS CHANGED UNDER THEM AT input-native-events T6. Text no
    // longer arrives as a virtual key code plus a shift flag for the widget to
    // reassemble — an assumption that is only true on a US layout — but as the
    // character Windows already decoded, through Char. Type() below therefore
    // sends CHARACTERS, and the tests that used to assert the reassembly ('A'
    // with no shift means 'a') now assert the absence of it: what you typed is
    // what lands. Press() still sends virtual key codes, because backspace and
    // enter still are ones.
    //
    // The widget needs a parent window and nothing else. EclWindow's
    // constructor touches no global and no renderer, and both entry points
    // reach the parent only for the text-edit mode flag, so this runs headless.
    class EditSession
    {
      public:
        EclWindow m_window{"test window"};
        InputField m_field;
        std::string m_target;

        EditSession(char const* _initial = "")
        {
          // SetParent rather than RegisterButton: the window deletes every
          // button it holds, and this one lives on the stack.
          m_field.SetParent(&m_window);
          m_target = _initial;
          m_field.RegisterString(&m_target);
          m_field.Refresh();
          m_window.BeginTextEdit(m_field.m_name);
        }

        // Characters, as WM_CHAR delivers them. Signed char is widened through
        // unsigned char so a byte above 127 — an umlaut in the active code page
        // — arrives as that byte rather than as a sign-extended negative.
        void Type(char const* _text)
        {
          for (char const* c = _text; *c; ++c)
          {
            m_field.Char(static_cast<unsigned char>(*c));
          }
        }

        bool TypeOne(unsigned int _character) { return m_field.Char(_character); }

        void Press(int _keyCode, bool _shift = false) { m_field.Keypress(_keyCode, _shift, false, false); }

        std::string Target() const { return m_target; }
    };


    // A numeric field, which filters what it accepts where a string field does
    // not. Separate because RegisterFloat and RegisterString both assert the
    // field has no type yet, so one session cannot be both.
    class FloatEditSession
    {
      public:
        EclWindow m_window{"test window"};
        InputField m_field;
        float m_target{0.0f};

        FloatEditSession()
        {
          m_field.SetParent(&m_window);
          m_field.RegisterFloat(&m_target);
          m_field.Refresh();
          // Refresh renders the registered 0.0f into the buffer as "0.00", which
          // is what the widget shows and what a real edit would start from. The
          // filter is what these tests are about, so the buffer is emptied here
          // rather than backspaced four times.
          m_field.m_buf.clear();
          m_window.BeginTextEdit(m_field.m_name);
        }

        void Type(char const* _text)
        {
          for (char const* c = _text; *c; ++c)
            m_field.Char(static_cast<unsigned char>(*c));
        }
    };
  } // namespace

  TEST_CLASS(InputFieldTests)
  {
    public:
      TEST_METHOD(RegistrationSetsTheStringType)
      {
        EditSession session;
        Assert::AreEqual(static_cast<int>(InputField::TypeString), session.m_field.m_type);
      }

      TEST_METHOD(RefreshLoadsTheRegisteredValueIntoTheBuffer)
      {
        EditSession session("Loaded");
        Assert::AreEqual(std::string("Loaded"), session.m_field.m_buf);
      }

      TEST_METHOD(CharactersLandExactlyAsTyped)
      {
        EditSession session;
        session.Type("abc");
        Assert::AreEqual(std::string("abc"), session.Target());
      }

      // The old path derived case from a shift FLAG, because a virtual key code
      // carries none: KEY_A is 'A' whether or not shift is down. Windows has
      // already applied shift, caps lock and the layout by the time it sends a
      // character, so there is nothing left to derive and no flag to consult.
      TEST_METHOD(CaseComesFromTheCharacterNotFromAShiftFlag)
      {
        EditSession session;
        session.Type("AbC");
        Assert::AreEqual(std::string("AbC"), session.Target());
      }

      // THE ACCEPTANCE CRITERION'S NAMED CASE. On a German layout the key in the
      // QWERTY 'Y' position sends virtual key 89 and the character 'z'. The old
      // widget read the key code, so it typed 'y' — every German player's name
      // came out with the wrong letters, and no keyboard file could fix it,
      // because those rebind game controls rather than text.
      TEST_METHOD(TheVirtualKeyContributesNothingOnANonUsLayout)
      {
        EditSession session;
        session.Press(KEY_Y);
        session.Type("z");
        Assert::AreEqual(std::string("z"), session.Target(), L"the character is what lands; the key code is not text");
      }

      // A shifted character the old path could not produce at all. Its letter
      // branch handled KEY_A..KEY_Z and nothing else, so every punctuation mark
      // above the number row was silently dropped.
      TEST_METHOD(AShiftedPunctuationCharacterIsTakenFromTheMessage)
      {
        EditSession session;
        session.Type("a#b");
        Assert::AreEqual(std::string("a#b"), session.Target());
      }

      // Codes above 127 are ordinary characters in the active code page — the
      // window is registered with RegisterClassA, so WM_CHAR carries one byte.
      // 0xE4 is a-umlaut in code page 1252.
      TEST_METHOD(AnAccentedCharacterIsOneByteAndIsKept)
      {
        EditSession session;
        Assert::IsTrue(session.TypeOne(0xE4u));
        Assert::AreEqual(std::string(1, static_cast<char>(0xE4)), session.Target());
      }

      TEST_METHOD(DigitsAppend)
      {
        EditSession session;
        session.Type("a09");
        Assert::AreEqual(std::string("a09"), session.Target());
      }

      // KEY_STOP had a branch of its own because a full stop had no other way
      // in. It is a character like any other now, and one that arrives from
      // whichever key the layout puts it on rather than only from VK 190.
      TEST_METHOD(AFullStopIsJustACharacter)
      {
        EditSession session;
        session.Type("map.txt");
        Assert::AreEqual(std::string("map.txt"), session.Target());
      }

      TEST_METHOD(BackspaceRemovesTheLastCharacter)
      {
        EditSession session;
        session.Type("abc");
        session.Press(KEY_BACKSPACE);
        Assert::AreEqual(std::string("ab"), session.Target());
      }

      TEST_METHOD(BackspaceOnAnEmptyFieldIsHarmless)
      {
        EditSession session;
        session.Press(KEY_BACKSPACE);
        session.Press(KEY_BACKSPACE);
        Assert::AreEqual(std::string(""), session.Target());
      }

      // The write-back is per keystroke, not on commit. Anything that reads the
      // registered storage while the field has focus sees each partial edit,
      // and the editor windows rely on that to redraw as you type.
      TEST_METHOD(EachKeystrokeWritesThroughImmediately)
      {
        EditSession session;
        session.Type("a");
        Assert::AreEqual(std::string("a"), session.Target());
        session.Type("b");
        Assert::AreEqual(std::string("ab"), session.Target());
      }

      TEST_METHOD(EnterCommitsAndLeavesTextEditMode)
      {
        EditSession session;
        session.Type("done");
        session.Press(KEY_ENTER);
        Assert::AreEqual(std::string("done"), session.Target());
        Assert::AreEqual(std::string("None"), std::string(session.m_window.m_currentTextEdit));
      }

      // Both entry points return immediately unless the parent window says this
      // field has the edit focus. EndTextEdit is what clears it, so keystrokes
      // arriving after a commit are dropped rather than appended to the
      // committed value.
      TEST_METHOD(KeystrokesAreIgnoredOutsideTextEditMode)
      {
        EditSession session;
        session.Type("ab");
        session.Press(KEY_ENTER);
        session.Type("cd");
        Assert::AreEqual(std::string("ab"), session.Target());
      }

      TEST_METHOD(EditingStartsFromTheExistingValue)
      {
        EditSession session("Map");
        session.Type("s");
        Assert::AreEqual(std::string("Maps"), session.Target());
      }

      // WINDOWS SENDS A WM_CHAR FOR ENTER, ESCAPE, TAB AND BACKSPACE TOO, and
      // taking those here would be worse than dropping them: consuming a
      // character is what tells the driver the UI swallowed the key, so
      // accepting enter's carriage return would stop enter committing the field
      // and escape's would stop escape closing the window.
      TEST_METHOD(ControlCodesAreNotTextAndAreNotConsumed)
      {
        EditSession session;
        session.Type("ab");
        Assert::IsFalse(session.TypeOne(13u), L"enter");
        Assert::IsFalse(session.TypeOne(27u), L"escape");
        Assert::IsFalse(session.TypeOne(9u), L"tab");
        Assert::IsFalse(session.TypeOne(8u), L"backspace");
        Assert::AreEqual(std::string("ab"), session.Target());
      }

      // Consumption is the capture rule's signal, so it is worth asserting on
      // its own rather than only through the buffer: a character the field took
      // is a key that must not also fire the game binding it is bound to.
      TEST_METHOD(TakingACharacterIsReportedToTheCaller)
      {
        EditSession session;
        Assert::IsTrue(session.TypeOne('a'));

        session.Press(KEY_ENTER);
        Assert::IsFalse(session.TypeOne('b'), L"nothing is being edited, so nothing is captured");
      }

      // A numeric field took digits and the decimal point and refused letters,
      // and it still does — the filter moved from the key code to the character
      // without changing what gets through.
      TEST_METHOD(ANumericFieldTakesDigitsAndTheDecimalPointOnly)
      {
        FloatEditSession session;
        session.Type("1a2.5z");
        Assert::AreEqual(std::string("12.5"), session.m_field.m_buf);

        session.m_field.Keypress(KEY_ENTER, false, false, false);
        Assert::AreEqual(12.5f, session.m_target, 0.0001f);
      }
  };
} // namespace GameLogicTests
