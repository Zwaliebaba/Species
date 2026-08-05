
/*
 * =========
 * SCROLLBAR
 * =========
 *
 * This is a container class for scrollbar creation/deletion/management,
 * and all associated buttons that are required.
 *
 */

#pragma once

#include <string>
#include <string_view>

#include "Eclipse.h"
#include "SpeciesWindow.h"


namespace Species
{
  class ScrollBar
  {
    public:
      // Both are Eclipse names — m_parentWindow is an EclWindow's and m_name
      // an EclButton's — so they follow those members to std::string in
      // strings-modernised T11 rather than being converted twice.
      std::string m_parentWindow;
      std::string m_name;

      int m_x;
      int m_y;
      int m_w;
      int m_h;
      int m_numRows;
      int m_winSize;
      int m_currentValue;

    public:
      ScrollBar(EclWindow* parent);
      ~ScrollBar();

      void Create(std::string_view name, int x, int y, int w, int h, int numRows, int winSize, int stepSize = 1);

      void Remove();

      void SetNumRows(int newValue);
      void SetWinSize(int newValue);

      void SetCurrentValue(int newValue);
      void ChangeCurrentValue(int newValue);
  };


  class ScrollBarButton : public EclButton
  {
    protected:
      ScrollBar* m_scrollBar;
      int m_grabOffset;

    public:
      ScrollBarButton(ScrollBar* scrollBar);
      void Render(int realX, int realY, bool highlighted, bool clicked);
      void MouseUp();
      void MouseDown();
  };


  class ScrollChangeButton : public SpeciesButton
  {
    protected:
      ScrollBar* m_scrollBar;
      int m_amount;

    public:
      ScrollChangeButton(ScrollBar* scrollbar, int amount);
      void MouseDown();
  };
} // namespace Species
