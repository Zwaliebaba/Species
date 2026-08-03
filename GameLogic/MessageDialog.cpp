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
  : SpeciesWindow(_name),
    m_numLines(0)
{
  char const* lineStart = _message;
  int longestLine = 0;
  while (1)
  {
    if (_message[0] == '\n' || _message[0] == '\0')
    {
      int lineLen = _message - lineStart;
      if (lineLen > longestLine)
        longestLine = lineLen;
      m_messageLines[m_numLines] = new char[lineLen + 1];
      strncpy(m_messageLines[m_numLines], lineStart, lineLen);
      m_messageLines[m_numLines][lineLen] = '\0';
      m_numLines++;
      lineStart = _message + 1;
    }

    if (_message[0] == '\0')
    {
      break;
    }

    _message++;
  }

  m_w = g_editorFont.GetTextWidth(longestLine) + 10;
  m_w = max(m_w, 100);
  m_h = 65 + m_numLines * DEF_FONT_SIZE;
  m_h = max(m_h, 85);
  SetMenuSize(m_w, m_h);
  m_x = g_renderer->ScreenW() / 2 - m_w / 2;
  m_y = g_renderer->ScreenH() / 2 - m_h / 2;
}


MessageDialog::~MessageDialog()
{
  for (int i = 0; i < m_numLines; ++i)
  {
    delete[] m_messageLines[i];
  }
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

  for (int i = 0; i < m_numLines; ++i)
  {
    g_editorFont.DrawText2D(m_x + 10, m_y + GetMenuSize(32) + i * GetMenuSize(DEF_FONT_SIZE), GetMenuSize(DEF_FONT_SIZE), m_messageLines[i]);
  }
}
