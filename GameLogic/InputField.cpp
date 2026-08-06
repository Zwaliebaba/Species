#include "pch.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "Debug.h"
#include "HiResTime.h"
#include "Input.h"
#include "TargetCursor.h"
#include "TextRenderer.h"
#include "LanguageTable.h"

#include "KeyDefs.h" // Key code definitions (bit of a hack)


#include "InputField.h"


#define PIXELS_PER_CHAR 7

// What the old char m_buf[256] could hold. The two Keypress branches that
// tested `len < sizeof(m_buf) - 1` still test it, so a full field drops the
// key exactly as it did rather than growing without limit.


namespace Species
{
  static constexpr size_t MAX_EDIT_LENGTH = 255;

  // ****************************************************************************
  // Class InputField
  // ****************************************************************************

  InputField::InputField()
    : m_type(TypeNowt),
      m_string(nullptr),
      m_int(nullptr),
      m_float(nullptr),
      m_char(nullptr),
      m_inputBoxWidth(0),
      m_callback(nullptr),
      m_lowBound(0.0f),
      m_highBound(1e4)
  {
  }


  void InputField::SetCallback(SpeciesButton* button) { m_callback = button; }


  void InputField::Render(int realX, int realY, bool highlighted, bool clicked)
  {
    glColor4f(0.1f, 0.0f, 0.0f, 0.5f);
    float editAreaWidth = m_w * 0.4f;
    glBegin(GL_QUADS);
    glVertex2f(realX + m_w - editAreaWidth, realY);
    glVertex2f(realX + m_w, realY);
    glVertex2f(realX + m_w, realY + m_h);
    glVertex2f(realX + m_w - editAreaWidth, realY + m_h);
    glEnd();

    glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
    glBegin(GL_LINES); // top
    glVertex2f(realX + m_w - editAreaWidth, realY);
    glVertex2f(realX + m_w, realY);
    glEnd();

    glBegin(GL_LINES); // left
    glVertex2f(realX + m_w - editAreaWidth, realY);
    glVertex2f(realX + m_w - editAreaWidth, realY + m_h);
    glEnd();

    glColor4ub(100, 34, 34, 150);
    // glColor4f( 0.9f, 0.3f, 0.3f, 0.3f );        // right
    glBegin(GL_LINES);
    glVertex2f(realX + m_w, realY);
    glVertex2f(realX + m_w, realY + m_h);
    glEnd();

    glBegin(GL_LINES); // bottom
    glVertex2f(realX + m_w - editAreaWidth, realY + m_h);
    glVertex2f(realX + m_w, realY + m_h);
    glEnd();

    int fieldX = realX + m_w - m_inputBoxWidth;

    // The null check that used to guard this was dead: m_currentTextEdit was a
    // char[256] member, so the array always decayed to a non-null pointer.
    if (m_parent->m_currentTextEdit == m_name)
    {
      BorderlessButton::Render(realX, realY, true, clicked);
      if (fmodf(GetHighResTime(), 1.0f) < 0.5f)
      {
        int cursorOffset = static_cast<int>(m_buf.size()) * PIXELS_PER_CHAR + 2;
        glBegin(GL_LINES);
        glVertex2f(fieldX + cursorOffset, realY + 2);
        glVertex2f(fieldX + cursorOffset, realY + m_h - 2);
        glEnd();
      }
    }
    else
    {
      BorderlessButton::Render(realX, realY, highlighted, clicked);
    }


    // Edit by Chris - so we don't need to keep Refreshing, just generate the Buffer here every second

    if (!highlighted && fmodf(GetHighResTime(), 1.0f) < 0.5f)
    {
      ReloadBuffer();
      m_inputBoxWidth = static_cast<int>(m_buf.size()) * PIXELS_PER_CHAR + 7;
    }
    SpeciesWindow* parent = (SpeciesWindow*)m_parent;
    fieldX = realX + m_w - parent->GetMenuSize(m_inputBoxWidth);
    // ONE ARGUMENT, so this is DrawText2D's PLAIN overload and m_buf is text
    // rather than a format string — the formatting template needs an argument
    // after the format and there is none. That is worth stating here because
    // the comment this replaces said the opposite, and because it is what makes
    // the change above safe: Char accepts every printable character, so '%' and
    // '{' are typeable into a name now, and neither is a specifier on this path.
    g_editorFont.DrawText2D(fieldX + 2, realY + 10, parent->GetMenuSize(DEF_FONT_SIZE), m_buf.c_str());
  }


  void InputField::MouseUp() { m_parent->BeginTextEdit(m_name); }


  void InputField::Keypress(int keyCode, bool shift, bool ctrl, bool alt)
  {
    if (m_parent->m_currentTextEdit == "None")
      return;

    if (keyCode == KEY_BACKSPACE)
    {
      if (!m_buf.empty())
        m_buf.pop_back();
      if (m_type == TypeString)
      {
        *m_string = m_buf;
        Refresh();
      }
    }
    else if (keyCode == KEY_ENTER)
    {
      m_parent->EndTextEdit();

      if (m_float)
      {
        *m_float = atof(m_buf.c_str());
      }
      else if (m_int)
      {
        *m_int = atoi(m_buf.c_str());
      }
      else if (m_string)
      {
        *m_string = m_buf;
      }
      else if (m_char)
      {
        *m_char = (unsigned char)atoi(m_buf.c_str());
      }

      ClampToBounds();
      Refresh();
    }
    else
    {
      // EVERY OTHER KEY IS SOMEBODY ELSE'S. Text arrives through Char; the
      // arrows, escape, delete and the rest are key events this widget has
      // never handled and still does not. Returning here matters rather than
      // falling through harmlessly: the branch that used to end this function
      // wrote the buffer back and called Refresh — and Refresh clicks the
      // registered callback button — for keys it had just decided to ignore.
      return;
    }

    m_inputBoxWidth = static_cast<int>(m_buf.size()) * PIXELS_PER_CHAR + 7;
  }


  bool InputField::Char(unsigned int _character)
  {
    // The window only offers a character to the widget it has in text edit, so
    // this is belt and braces for a direct caller — and for the frame in which
    // enter has just committed, where the WM_CHAR for the enter itself is still
    // behind it in the queue.
    if (m_parent->m_currentTextEdit != m_name)
      return false;

    // CONTROL CODES ARE NOT TEXT. Windows sends WM_CHAR for enter, escape, tab
    // and backspace as well, and those reach this widget as KEY events through
    // Keypress. Taking them here would be worse than useless: consuming a
    // character is what tells the driver the UI swallowed the key, so accepting
    // enter's carriage return would stop enter from committing the field.
    //
    // The upper bound is the window's, not a choice. The class is registered
    // with RegisterClassA, so WM_CHAR carries one byte in the active code page
    // — which is also what m_buf and the 8-bit font downstream of it hold.
    if (_character < 32 || _character == 127 || _character > 255)
      return false;

    const char character = static_cast<char>(_character);

    // A numeric field takes exactly what the virtual-key path took: digits and
    // the decimal point. Letters were only ever appended for a string field.
    if (m_type != TypeString && !(character >= '0' && character <= '9') && character != '.')
      return false;

    // The length test now covers letters too. It did not before, and that was
    // the defect rather than the design: the digit and full-stop branches
    // checked before appending, and the letter branch wrote at m_buf[len] and
    // [len + 1] of a char[256] with nothing stopping it.
    if (m_buf.size() < MAX_EDIT_LENGTH)
      m_buf += character;

    if (m_type == TypeString)
    {
      *m_string = m_buf;
      Refresh();
    }

    m_inputBoxWidth = static_cast<int>(m_buf.size()) * PIXELS_PER_CHAR + 7;
    return true;
  }


  void InputField::RegisterChar(unsigned char* _char)
  {
    DEBUG_ASSERT(m_type == TypeNowt);
    m_type = TypeChar;
    m_char = _char;
  }


  void InputField::RegisterInt(int* _int)
  {
    DEBUG_ASSERT(m_type == TypeNowt);
    m_type = TypeInt;
    m_int = _int;
  }


  void InputField::RegisterFloat(float* _float)
  {
    DEBUG_ASSERT(m_type == TypeNowt);
    m_type = TypeFloat;
    m_float = _float;
  }


  void InputField::RegisterString(std::string* _string)
  {
    DEBUG_ASSERT(m_type == TypeNowt);
    m_type = TypeString;
    m_string = _string;
  }


  void InputField::ClampToBounds()
  {
    switch (m_type)
    {
    case TypeChar:
      if (*m_char > m_highBound)
        *m_char = m_highBound;
      else if (*m_char < m_lowBound)
        *m_char = m_lowBound;
      break;
    case TypeInt:
      if (*m_int > m_highBound)
        *m_int = m_highBound;
      else if (*m_int < m_lowBound)
        *m_int = m_lowBound;
      break;
    case TypeFloat:
      if (*m_float > m_highBound)
        *m_float = m_highBound;
      else if (*m_float < m_lowBound)
        *m_float = m_lowBound;
      break;
    }
  }


  // Renders the registered value into the edit buffer. Render did this inline
  // and Refresh did it again identically; the only difference between them was
  // Refresh's callback, so the shared half is named rather than duplicated.
  void InputField::ReloadBuffer()
  {
    switch (m_type)
    {
    case TypeChar:
      m_buf = std::format("{}", (int)*m_char);
      break;
    case TypeInt:
      m_buf = std::format("{}", *m_int);
      break;
    case TypeFloat:
      m_buf = std::format("{:.2f}", *m_float);
      break;
    case TypeString:
      m_buf = *m_string;
      break;
    }
  }


  void InputField::Refresh()
  {
    ReloadBuffer();

    if (m_callback)
    {
      m_callback->MouseUp();
    }
  }


  // ****************************************************************************
  // Class InputScroller
  // ****************************************************************************

  InputScroller::InputScroller()
    : SpeciesButton(),
      m_inputField(nullptr),
      m_change(0.0f),
      m_mouseDownStartTime(-1.0f)
  {
  }


#define INTEGER_INCREMENT_PERIOD 0.1f

void InputScroller::Render(int realX, int realY, bool highlighted, bool clicked)
{
  SpeciesButton::Render(realX, realY, highlighted, clicked);

  if (m_mouseDownStartTime > 0.0f && m_inputField && g_inputManager->controlEvent(ControlType::ControlEclipseLMousePressed) &&
      g_target->X() >= realX && g_target->X() < realX + m_w && g_target->Y() >= realY && g_target->Y() < realY + m_h)
  {
    float change = m_change;
    if (g_inputManager->controlEvent(ControlType::ControlScrollSpeedup))
      change *= 5.0f;

    float timeDelta = GetHighResTime() - m_mouseDownStartTime;
    if (m_inputField->m_type == InputField::TypeChar)
    {
      if (timeDelta > INTEGER_INCREMENT_PERIOD)
      {
        *m_inputField->m_char += change;
        m_mouseDownStartTime += INTEGER_INCREMENT_PERIOD;
      }
    }
    else if (m_inputField->m_type == InputField::TypeInt)
    {
      if (timeDelta > INTEGER_INCREMENT_PERIOD)
      {
        *m_inputField->m_int += change;
        m_mouseDownStartTime += INTEGER_INCREMENT_PERIOD;
      }
    }
    else if (m_inputField->m_type == InputField::TypeFloat)
    {
      change = change * timeDelta * 0.2f;
      *m_inputField->m_float += change;
    }

    m_inputField->ClampToBounds();
    m_inputField->Refresh();
  }
}


void InputScroller::MouseDown()
{
  if (g_inputManager->controlEvent(ControlType::ControlEclipseLMousePressed))
  {
    m_mouseDownStartTime = GetHighResTime() - INTEGER_INCREMENT_PERIOD;
  }
}


void InputScroller::MouseUp() { m_mouseDownStartTime = -1.0f; }


// ****************************************************************************
// Class ColourWidget
// ****************************************************************************


ColourWidget::ColourWidget()
  : SpeciesButton(),
    m_callback(nullptr),
    m_value(nullptr)
{
}


void ColourWidget::Render(int realX, int realY, bool highlighted, bool clicked)
{
  SpeciesButton::Render(realX, realY, highlighted, clicked);

  glColor4ubv((unsigned char*)m_value);
  glBegin(GL_QUADS);
  glVertex2i(realX + m_w - 30, realY + 4);
  glVertex2i(realX + m_w - 5, realY + 4);
  glVertex2i(realX + m_w - 5, realY + m_h - 3);
  glVertex2i(realX + m_w - 30, realY + m_h - 3);
  glEnd();
}


void ColourWidget::MouseUp()
{
  auto owned = std::make_unique<ColourWindow>(LANGUAGEPHRASE("editor_coloureditor"));
  ColourWindow* cw = owned.get();
  cw->SetSize(200, 100);
  cw->SetValue(m_value);
  cw->SetCallback(m_callback);
  EclRegisterWindow(std::move(owned), m_parent);
}


void ColourWidget::SetValue(int* value) { m_value = value; }


void ColourWidget::SetCallback(SpeciesButton* button) { m_callback = button; }


// ****************************************************************************
// Class ColourButton
// ****************************************************************************

ColourWindow::ColourWindow(char const* _name)
  : SpeciesWindow(_name),
    m_callback(nullptr),
    m_value(nullptr)
{
}


void ColourWindow::SetValue(int* value) { m_value = value; }


void ColourWindow::SetCallback(SpeciesButton* button) { m_callback = button; }


void ColourWindow::Create()
{
  SpeciesWindow::Create();

  int y = 25;
  int h = 18;

  unsigned char* r = ((unsigned char*)m_value) + 0;
  unsigned char* g = ((unsigned char*)m_value) + 1;
  unsigned char* b = ((unsigned char*)m_value) + 2;
  unsigned char* a = ((unsigned char*)m_value) + 3;

  CreateValueControl(LANGUAGEPHRASE("editor_red"), r, y, 1, 0, 255, m_callback, -1, m_w - 80);
  CreateValueControl(LANGUAGEPHRASE("editor_green"), g, y += h, 1, 0, 255, m_callback, -1, m_w - 80);
  CreateValueControl(LANGUAGEPHRASE("editor_blue"), b, y += h, 1, 0, 255, m_callback, -1, m_w - 80);
  CreateValueControl(LANGUAGEPHRASE("editor_alpha"), a, y += h, 1, 0, 255, m_callback, -1, m_w - 80);
}


void ColourWindow::Render(bool hasFocus)
{
  SpeciesWindow::Render(hasFocus);

  glColor4ubv((unsigned char*)m_value);
  glBegin(GL_QUADS);
  glVertex2i(m_x + m_w - 60, m_y + 25);
  glVertex2i(m_x + m_w - 10, m_y + 25);
  glVertex2i(m_x + m_w - 10, m_y + m_h - 10);
  glVertex2i(m_x + m_w - 60, m_y + m_h - 10);
  glEnd();
}
} // namespace Species
