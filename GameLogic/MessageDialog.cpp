#include "pch.h"

#include <string.h>

#include "MathUtils.h"
#include "TextRenderer.h"
#include "LanguageTable.h"

#include "MessageDialog.h"

#include "WorldPointers.h"


//*****************************************************************************
// Class OKButton
//*****************************************************************************


namespace Species
{
  class OKButton : public SpeciesButton
  {
    protected:
      MessageDialog* m_parent;

    public:
      OKButton(MessageDialog* _parent)
        : m_parent(_parent)
      {
      }

      void MouseUp() { EclRemoveWindow(m_parent->m_name); }
  };


  //*****************************************************************************
  // Class MessageDialog
  //*****************************************************************************

  MessageDialog::MessageDialog(char const* _name, char const* _message)
    : SpeciesWindow(_name)
  {
    // Splits on '\n' exactly as the character walk this replaces did, trailing
    // empty line included: a message ending in a newline still produces one,
    // because the old loop pushed a line at the terminator too.
    std::string_view remaining(_message);
    int longestLine = 0;
    while (true)
    {
      size_t const newline = remaining.find('\n');
      std::string_view const line = remaining.substr(0, newline);
      longestLine = std::max(longestLine, static_cast<int>(line.size()));
      m_messageLines.emplace_back(line);

      if (newline == std::string_view::npos)
        break;

      remaining.remove_prefix(newline + 1);
    }

    m_w = g_editorFont.GetTextWidth(longestLine) + 10;
    m_w = std::max(m_w, 100);
    m_h = 65 + static_cast<int>(m_messageLines.size()) * DEF_FONT_SIZE;
    m_h = std::max(m_h, 85);
    SetMenuSize(m_w, m_h);
    m_x = g_renderer->ScreenW() / 2 - m_w / 2;
    m_y = g_renderer->ScreenH() / 2 - m_h / 2;
  }


  void MessageDialog::Create()
  {
    int const buttonWidth = GetMenuSize(40);
    int const buttonHeight = GetMenuSize(18);
    OKButton* button = new OKButton(this);

    char const* caption = "Close";
    if (g_langTable)
      caption = LANGUAGEPHRASE("dialog_close");

    button->SetShortProperties(caption, (m_w - buttonWidth) / 2, m_h - GetMenuSize(30), buttonWidth, buttonHeight);
    button->m_fontSize = GetMenuSize(11);
    RegisterButton(button);
  }


  void MessageDialog::Render(bool _hasFocus)
  {
    SpeciesWindow::Render(_hasFocus);

    for (int i = 0; i < static_cast<int>(m_messageLines.size()); ++i)
    {
      g_editorFont.DrawText2D(m_x + 10, m_y + GetMenuSize(32) + i * GetMenuSize(DEF_FONT_SIZE), GetMenuSize(DEF_FONT_SIZE), m_messageLines[i]);
    }
  }
} // namespace Species
