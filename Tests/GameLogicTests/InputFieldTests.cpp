#include "pch.h"

#include "EclWindow.h"
#include "InputField.h"
#include "KeyDefs.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

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
    // They were written against the char* version and committed before it, so
    // the evidence is in the history: the conversion commit changes four lines
    // here, all of them declaring or registering the target, and not one
    // assertion.
    //
    // The widget needs a parent window and nothing else. EclWindow's
    // constructor touches no global and no renderer, and Keypress reaches the
    // parent only for the text-edit mode flag, so this runs headless.
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

        void Type(char const* _keys, bool _shift = false)
        {
          for (char const* k = _keys; *k; ++k)
          {
            m_field.Keypress(*k, _shift, false, false);
          }
        }

        void Press(int _keyCode, bool _shift = false) { m_field.Keypress(_keyCode, _shift, false, false); }

        std::string Target() const { return m_target; }
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

      TEST_METHOD(LettersArriveLowercaseWithoutShift)
      {
        EditSession session;
        session.Type("ABC");
        Assert::AreEqual(std::string("abc"), session.Target());
      }

      TEST_METHOD(ShiftKeepsLettersUppercase)
      {
        EditSession session;
        session.Type("ABC", true);
        Assert::AreEqual(std::string("ABC"), session.Target());
      }

      TEST_METHOD(DigitsAppend)
      {
        EditSession session;
        session.Type("A");
        session.Press(KEY_0);
        session.Press(KEY_9);
        Assert::AreEqual(std::string("a09"), session.Target());
      }

      TEST_METHOD(TheStopKeyAppendsAFullStop)
      {
        EditSession session;
        session.Type("MAP");
        session.Press(KEY_STOP);
        session.Type("TXT");
        Assert::AreEqual(std::string("map.txt"), session.Target());
      }

      TEST_METHOD(BackspaceRemovesTheLastCharacter)
      {
        EditSession session;
        session.Type("ABC");
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
        session.Type("A");
        Assert::AreEqual(std::string("a"), session.Target());
        session.Type("B");
        Assert::AreEqual(std::string("ab"), session.Target());
      }

      TEST_METHOD(EnterCommitsAndLeavesTextEditMode)
      {
        EditSession session;
        session.Type("DONE");
        session.Press(KEY_ENTER);
        Assert::AreEqual(std::string("done"), session.Target());
        Assert::AreEqual(std::string("None"), std::string(session.m_window.m_currentTextEdit));
      }

      // Keypress returns immediately unless the parent window says this field
      // has the edit focus. EndTextEdit is what clears it, so keys arriving
      // after a commit are dropped rather than appended to the committed value.
      TEST_METHOD(KeysAreIgnoredOutsideTextEditMode)
      {
        EditSession session;
        session.Type("AB");
        session.Press(KEY_ENTER);
        session.Type("CD");
        Assert::AreEqual(std::string("ab"), session.Target());
      }

      TEST_METHOD(EditingStartsFromTheExistingValue)
      {
        EditSession session("Map");
        session.Type("S");
        Assert::AreEqual(std::string("Maps"), session.Target());
      }
  };
} // namespace GameLogicTests
