#include "pch.h"
#include "Eclipse.h"
#include "EclButton.h"

EclButton::EclButton()
  : m_x(0),
    m_y(0),
    m_w(0),
    m_h(0),
    m_caption(nullptr),
    m_tooltip(nullptr),
    m_parent(nullptr)
{
  strcpy(m_name, "New Button");
  EclButton::SetTooltip(" ");
}

EclButton::~EclButton()
{
  // delete[], not delete. Both are allocated with new char[] in SetCaption and
  // SetTooltip, and both of those release with delete[] correctly — only the
  // destructor had the mismatched form. Freeing an array with plain delete is
  // undefined behaviour that nothing diagnoses.
  delete[] m_caption;
  delete[] m_tooltip;
}

void EclButton::SetProperties(const char* _name, int _x, int _y, int _w, int _h, const char* _caption, const char* _tooltip)
{
  if (!_caption)
    _caption = _name;

  if (strlen(_name) > SIZE_ECLBUTTON_NAME) {}
  else
    strcpy(m_name, _name);

  m_x = _x;
  m_y = _y;
  m_w = _w;
  m_h = _h;
  SetCaption(_caption);
  SetTooltip(_tooltip);
}

void EclButton::SetCaption(const char* _caption)
{
  if (m_caption)
    delete [] m_caption;
  if (_caption)
  {
    m_caption = new char [strlen(_caption) + 1];
    strcpy(m_caption, _caption);
  }
  else
  {
    m_caption = new char[1];
    *m_caption = '\0';
  }
}

void EclButton::SetTooltip(const char* _tooltip)
{
  if (!_tooltip)
    _tooltip = "";
  if (m_tooltip)
    delete [] m_tooltip;
  m_tooltip = new char [strlen(_tooltip) + 1];
  strcpy(m_tooltip, _tooltip);
}

void EclButton::SetParent(EclWindow* _parent) { m_parent = _parent; }

void EclButton::Render(int realX, int realY, bool highlighted, bool clicked) {}

void EclButton::MouseUp() {}

void EclButton::MouseDown() {}

void EclButton::MouseMove() {}

void EclButton::Keypress(int keyCode, bool shift, bool ctrl, bool alt) {}
