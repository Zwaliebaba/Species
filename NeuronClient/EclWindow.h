
/*
        A Window object.
        Essentially a container for Eclipse buttons.

        All co-ordinates are RELATIVE to the window

  */


#pragma once

#include <string>
#include <string_view>
#include <vector>

#define SIZE_ECLWINDOW_NAME 256
#define SIZE_ECLWINDOW_TITLE 256


namespace Neuron
{
  class EclButton;

  class EclWindow
  {
    public:
      int m_x;
      int m_y;
      int m_w;
      int m_h;

      // std::string since strings-modernised T11. Both were char[256] whose
      // setters silently DID NOTHING for a longer value, leaving the window
      // called "" — and a window with the wrong name cannot be found again.
      std::string m_name;
      std::string m_title;

      bool m_movable;
      bool m_resizable;

      std::vector<EclButton*> m_buttons;

    public:
      std::string m_currentTextEdit;

    public:
      EclWindow(std::string_view _name);
      virtual ~EclWindow();

      void SetName(std::string_view _name);
      void SetTitle(std::string_view _title);
      void SetPosition(int _x, int _y);
      void SetSize(int _w, int _h);
      void SetMovable(bool _movable);
      void MakeAllOnScreen();

      void RegisterButton(EclButton* button);
      void RemoveButton(std::string_view _name);

      void BeginTextEdit(std::string_view _name);
      void EndTextEdit();

      virtual EclButton* GetButton(std::string_view _name);
      virtual EclButton* GetButton(int _x, int _y);

      virtual void Create();
      virtual void Remove();
      virtual void Update();
      virtual void Render(bool hasFocus);

      virtual void Keypress(int keyCode, bool shift, bool ctrl, bool alt);

      // A decoded character for the widget currently in text edit. Returns true
      // if it was taken; a window with nothing being edited takes nothing.
      // Keypress keeps its void signature and its job — the editing keys,
      // backspace and enter and whatever a widget binds — because those are
      // keys rather than text and are the same on every layout.
      virtual bool Char(unsigned int character);

      virtual void MouseEvent(bool lmb, bool rmb, bool up, bool down);
  };
} // namespace Neuron
