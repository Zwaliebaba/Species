#include "pch.h"
#include <stdio.h>
#include <string.h>

#include "Eclipse.h"
#include "EclWindow.h"


namespace Neuron
{
  EclWindow::EclWindow(std::string_view _name)
    : m_x(0),
      m_y(0),
      m_w(0),
      m_h(0),
      m_resizable(true)
  {
    SetName(_name);
    SetTitle("New Window");
    SetMovable(true);
    m_currentTextEdit = "None";
  }

  EclWindow::~EclWindow()
  {
    for (EclButton* button : m_buttons)
      delete button;
    m_buttons.clear();
  }

  // Both setters used to RETURN WITHOUT WRITING for a value longer than 256,
  // which left the window nameless rather than truncating it — and a window
  // whose name is not the one the caller asked for can never be found again.
  // std::string holds whatever it is given.
  void EclWindow::SetName(std::string_view _name) { m_name = _name; }

  void EclWindow::SetTitle(std::string_view _title) { m_title = _title; }

  void EclWindow::SetPosition(int _x, int _y)
  {
    m_x = _x;
    m_y = _y;
  }

  void EclWindow::SetSize(int _w, int _h)
  {
    m_w = _w;
    m_h = _h;
  }

  void EclWindow::SetMovable(bool _movable) { m_movable = _movable; }

  void EclWindow::MakeAllOnScreen()
  {
    int screenW = EclGetScreenW();
    int screenH = EclGetScreenH();
    if (m_x < 10)
      m_x = 10;
    if (m_y < 10)
      m_y = 10;
    if (m_x + m_w > screenW - 10)
      m_x = screenW - m_w - 10;
    if (m_y + m_h > screenH - 10)
      m_y = screenH - m_h - 10;
  }

  void EclWindow::BeginTextEdit(std::string_view _name) { m_currentTextEdit = _name; }

  void EclWindow::EndTextEdit() { m_currentTextEdit = "None"; }

  void EclWindow::RegisterButton(EclButton* button)
  {
    button->SetParent(this);

    m_buttons.insert(m_buttons.begin(), button);

    if (button->m_y + button->m_h + 10 > m_h)
    {
      SetSize(m_w, button->m_y + button->m_h + 10);
      MakeAllOnScreen();
    }
  }

  void EclWindow::RemoveButton(std::string_view _name)
  {
    for (int i = 0; i < m_buttons.size(); ++i)
    {
      EclButton* button = m_buttons[i];
      if (button->m_name == _name)
      {
        m_buttons.erase(m_buttons.begin() + (i));
        delete button;
      }
    }
  }

  EclButton* EclWindow::GetButton(std::string_view _name)
  {
    for (int i = 0; i < m_buttons.size(); ++i)
    {
      EclButton* button = m_buttons[i];
      if (button->m_name == _name)
        return button;
    }

    return nullptr;
  }

  EclButton* EclWindow::GetButton(int _x, int _y)
  {
    for (int i = 0; i < m_buttons.size(); ++i)
    {
      EclButton* button = m_buttons[i];

      if (_x >= button->m_x && _x <= button->m_x + button->m_w && _y >= button->m_y && _y <= button->m_y + button->m_h)
        return button;
    }

    return nullptr;
  }

  void EclWindow::Create() {}

  void EclWindow::Remove() {}

  void EclWindow::Update() {}

  void EclWindow::Keypress(int keyCode, bool shift, bool ctrl, bool alt)
  {
    EclButton* currentTextEdit = GetButton(m_currentTextEdit);
    if (currentTextEdit)
      currentTextEdit->Keypress(keyCode, shift, ctrl, alt);
  }

  bool EclWindow::Char(unsigned int _character)
  {
    EclButton* currentTextEdit = GetButton(m_currentTextEdit);
    return currentTextEdit && currentTextEdit->Char(_character);
  }

  void EclWindow::MouseEvent(bool lmb, bool rmb, bool up, bool down) {}

  void EclWindow::Render(bool hasFocus)
  {
    for (int i = m_buttons.size() - 1; i >= 0; --i)
    {
      EclButton* button = m_buttons[i];
      bool highlighted = EclMouseInButton(this, button) || m_currentTextEdit == button->m_name;
      bool clicked = (hasFocus && button->m_name == EclGetCurrentClickedButton());
      button->Render(m_x + button->m_x, m_y + button->m_y, highlighted, clicked);
    }
  }
} // namespace Neuron
