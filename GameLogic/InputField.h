#pragma once

#include <string>

#include "RgbColour.h"

#include "SpeciesWindow.h"


// ****************************************************************************
// Class InputField
// ****************************************************************************

// This widget provides an editable text box where the user can type in numbers
// or short strings


namespace Species
{
  class InputField : public BorderlessButton
  {
    public:
      // The text being edited. It was a char[256]. The digit and full-stop
      // branches of Keypress tested the length before appending; the letter
      // branch did not, and wrote two bytes at the end of a full field.
      std::string m_buf;

      enum
      {
        TypeNowt,
        TypeChar,
        TypeInt,
        TypeFloat,
        TypeString
      };

      int m_type;

      unsigned char* m_char;
      int* m_int;
      float* m_float;
      // The registered storage for a text field. This was a char*, written
      // through with strcpy from a 256-byte buffer regardless of how much room
      // the target actually had — CameraMount::m_name has 64. See T5's notes.
      std::string* m_string;

      int m_inputBoxWidth;

      float m_lowBound;
      float m_highBound;

      SpeciesButton* m_callback;

    public:
      InputField();

      virtual void Render(int realX, int realY, bool highlighted, bool clicked);
      virtual void MouseUp();

      // THE EDITING KEYS ONLY — backspace and enter. The letter, digit and
      // full-stop branches this used to carry rebuilt a character out of a
      // virtual key code and a shift flag, which is a US-layout assumption
      // wearing a keyboard file for a disguise: the per-locale files under
      // GameData/Input/Keyboards rebind game CONTROLS, and never made typing
      // correct. Char below takes what Windows decoded instead.
      virtual void Keypress(int keyCode, bool shift, bool ctrl, bool alt);

      // The text. Returns true if the character was taken, which is what tells
      // the input driver the field consumed the key that produced it.
      virtual bool Char(unsigned int character);

      // These functions allow you to register a piece of storage with the button, such that
      // the button's value is written back to the storage whenever the user changes the
      // button's value
      void RegisterChar(unsigned char*);
      void RegisterInt(int*);
      void RegisterFloat(float*);
      void RegisterString(std::string*);

      void ClampToBounds();

      void SetCallback(SpeciesButton* button); // This button will be clicked on Refresh

      void ReloadBuffer(); // Copies the registered value into the edit buffer
      void Refresh();      // Updates the display if the registered variable has changed
  };


  // ****************************************************************************
  // Class InputScroller
  // ****************************************************************************

  class InputScroller : public SpeciesButton
  {
    public:
      InputField* m_inputField;
      float m_change;
      float m_mouseDownStartTime;

    public:
      InputScroller();

      void Render(int realX, int realY, bool highlighted, bool clicked);
      void MouseDown();
      void MouseUp();
  };


  // ****************************************************************************
  // Class ColourWidget
  // ****************************************************************************

  class ColourWidget : public SpeciesButton
  {
    public:
      SpeciesButton* m_callback;
      int* m_value;

    public:
      ColourWidget();

      void Render(int realX, int realY, bool highlighted, bool clicked);
      void MouseUp();

      void SetValue(int* value);
      void SetCallback(SpeciesButton* button); // This button will be clicked on Refresh
  };


  // ****************************************************************************
  // Class ColourWindow
  // ****************************************************************************

  class ColourWindow : public SpeciesWindow
  {
    public:
      SpeciesButton* m_callback;
      int* m_value;

    public:
      ColourWindow(char const* _name);

      void SetValue(int* value);
      void SetCallback(SpeciesButton* button); // This button will be clicked on Refresh

      void Create();
      void Render(bool hasFocus);
  };
} // namespace Species
