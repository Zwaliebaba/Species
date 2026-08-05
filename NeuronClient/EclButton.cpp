#include "pch.h"
#include "Eclipse.h"
#include "EclButton.h"


namespace Neuron
{
  EclButton::EclButton()
    : m_name("New Button"),
      m_x(0),
      m_y(0),
      m_w(0),
      m_h(0),
      m_tooltip(" "),
      m_parent(nullptr)
  {
  }

  // THE DESTRUCTOR IS GONE, and with it the last delete[] in this class.
  // m_caption and m_tooltip were raw owning char* — the destructor freed them,
  // and freeing an array with plain `delete` rather than `delete[]` had been a
  // documented defect here. std::string owns them now, so ~EclButton is
  // `= default` in the header and there is nothing to get wrong.

  // _caption and _tooltip stay char const* rather than becoming string_view,
  // because NULL is one of the values they take and means something: a null
  // caption means "use the name", and a null tooltip means "empty". A
  // string_view cannot carry that, and giving them defaults instead would
  // change what SetProperties(name, ..., nullptr) does at 60 call sites.
  void EclButton::SetProperties(std::string_view _name, int _x, int _y, int _w, int _h, char const* _caption, char const* _tooltip)
  {
    // The old code REFUSED a name longer than SIZE_ECLBUTTON_NAME, leaving the
    // button called "New Button" so that every lookup by the intended name
    // missed. std::string simply holds it.
    m_name = _name;

    m_x = _x;
    m_y = _y;
    m_w = _w;
    m_h = _h;
    SetCaption(_caption ? std::string_view(_caption) : _name);
    SetTooltip(_tooltip ? std::string_view(_tooltip) : std::string_view());
  }

  void EclButton::SetCaption(std::string_view _caption) { m_caption = _caption; }

  void EclButton::SetTooltip(std::string_view _tooltip) { m_tooltip = _tooltip; }

  void EclButton::SetParent(EclWindow* _parent) { m_parent = _parent; }

  void EclButton::Render(int realX, int realY, bool highlighted, bool clicked) {}

  void EclButton::MouseUp() {}

  void EclButton::MouseDown() {}

  void EclButton::MouseMove() {}

  void EclButton::Keypress(int keyCode, bool shift, bool ctrl, bool alt) {}
} // namespace Neuron
