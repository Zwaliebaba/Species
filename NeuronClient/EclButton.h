#pragma once

#include <string>
#include <string_view>

// Kept for the callers that still size a buffer by it. The members below are
// std::string since strings-modernised T11 and are not bounded by it.
#define SIZE_ECLBUTTON_NAME     256


namespace Neuron
{
  class EclWindow;

  class EclButton
  {
    public:
      // std::string since strings-modernised T11. m_name used to be a
      // char[256] that SetProperties silently REFUSED to write when the name
      // was longer — the button kept "New Button" and every lookup by that
      // name failed. m_caption and m_tooltip were raw owning char*, new[] and
      // delete[] with a strcpy each.
      std::string m_name;
      int m_x;
      int m_y;
      int m_w;
      int m_h;
      std::string m_caption;
      std::string m_tooltip;

    protected:
      EclWindow* m_parent;

    public:
      EclButton();
      virtual ~EclButton() = default;

      virtual void SetProperties(std::string_view _name, int _x, int _y, int _w, int _h, char const* _caption = nullptr,
                                 char const* _tooltip = nullptr);

      virtual void SetCaption(std::string_view _caption);
      virtual void SetTooltip(std::string_view _tooltip);
      virtual void SetParent(EclWindow* _parent);

      virtual void Render(int realX, int realY, bool highlighted, bool clicked);
      virtual void MouseUp();
      virtual void MouseDown();
      virtual void MouseMove();
      virtual void Keypress(int keyCode, bool shift, bool ctrl, bool alt);
  };
} // namespace Neuron
