#include "pch.h"

#include <ctype.h>

#include <stdio.h>
#include <string.h>

#include "MathUtils.h"
#include "TextRenderer.h"
#include "Resource.h"
#include "Input.h"

#include "SpeciesWindow.h"
#include "InputField.h"

#include "ControlBindings.h"

#include "TargetCursor.h"
#include "WorldPointers.h"
#include "AppState.h"


// ****************************************************************************
// Class SpeciesButton
// ****************************************************************************


namespace Species
{
  SpeciesButton::SpeciesButton()
    : EclButton(),
      m_fontSize(11.0f),
      m_centered(false),
      m_disabled(false),
      m_highlightedThisFrame(false),
      m_mouseHighlightMode(false)
  {
  }

  void SpeciesButton::SetShortProperties(char const* _name, int _x, int _y, int _w, int _h, char* _caption, char* _tooltip)
  {
    if (_w == -1)
    {
      _w = strlen(_name) * 7 + 9;
    }

    if (_h == -1)
    {
      _h = 15;
    }

    SetProperties((char*)_name, _x, _y, _w, _h, _caption, _tooltip);
  }

  void SpeciesButton::SetDisabled(bool _disabled) { m_disabled = _disabled; }


  void SpeciesButton::Render(int realX, int realY, bool highlighted, bool clicked)
  {
    //    if      ( clicked )         glColor4f( 0.9f, 0.9f, 1.0f, 0.6f );
    //    else if ( highlighted )     glColor4f( 0.9f, 0.9f, 0.9f, 0.3f );
    //    else                        glColor4f( 0.5f, 0.5f, 0.5f, 0.2f );

    float y = 7.5 + realY + (m_h - m_fontSize) / 2;

    SpeciesWindow* parent = (SpeciesWindow*)m_parent;

    UpdateButtonHighlight();

    if (!m_mouseHighlightMode)
    {
      highlighted = false;
    }

    if (parent->m_buttonOrder[parent->m_currentButton] == this)
    {
      highlighted = true;
    }

    if (highlighted || clicked)
    {
      glShadeModel(GL_SMOOTH);
      glBegin(GL_QUADS);
      glColor4ub(199, 214, 220, 255);
      if (clicked)
        glColor4ub(255, 255, 255, 255);
      glVertex2f(realX, realY);
      glVertex2f(realX + m_w, realY);
      glColor4ub(112, 141, 168, 255);
      if (clicked)
        glColor4ub(162, 191, 208, 255);
      glVertex2f(realX + m_w, realY + m_h);
      glVertex2f(realX, realY + m_h);
      glEnd();

      glShadeModel(GL_FLAT);

      g_editorFont.SetRenderShadow(true);

      if (m_disabled)
        glColor4ub(128, 128, 75, 30);
      else
        glColor4ub(255, 255, 150, 30);

      if (m_centered)
      {
        g_editorFont.DrawText2DCentre(realX + m_w / 2, y, m_fontSize, m_caption);
        g_editorFont.DrawText2DCentre(realX + m_w / 2, y, m_fontSize, m_caption);
      }
      else
      {
        g_editorFont.DrawText2D(realX + 5, y, m_fontSize, m_caption);
        g_editorFont.DrawText2D(realX + 5, y, m_fontSize, m_caption);
      }
      g_editorFont.SetRenderShadow(false);
    }
    else
    {
      glColor4ub(107, 37, 39, 64);
      // glColor4ub( 82, 56, 102, 64 );
      glBegin(GL_QUADS);
      glVertex2f(realX, realY);
      glVertex2f(realX + m_w, realY);
      glVertex2f(realX + m_w, realY + m_h);
      glVertex2f(realX, realY + m_h);
      glEnd();

      glLineWidth(1.0f);
      glBegin(GL_LINES);
      glColor4ub(100, 34, 34, 200);
      glVertex2f(realX, realY + m_h);
      glVertex2f(realX, realY);

      glVertex2f(realX, realY);
      glVertex2f(realX + m_w, realY);

      glColor4f(0.1f, 0.0f, 0.0f, 1.0f);
      glVertex2f(realX + m_w, realY);
      glVertex2f(realX + m_w, realY + m_h);

      glVertex2f(realX + m_w, realY + m_h);
      glVertex2f(realX, realY + m_h);
      glEnd();

      if (m_disabled)
        glColor4f(0.5f, 0.5f, 0.5f, 1.0f);
      else
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

      if (m_centered)
      {
        g_editorFont.DrawText2DCentre(realX + m_w / 2, y, m_fontSize, m_caption);
      }
      else
      {
        g_editorFont.DrawText2D(realX + 5, y, m_fontSize, m_caption);
      }
    }

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    EclButton::Render(realX, realY, highlighted, clicked);
  }

  void SpeciesButton::UpdateButtonHighlight()
  {
    SpeciesWindow* parent = (SpeciesWindow*)m_parent;

    if (parent->m_buttonChangedThisUpdate)
    {
      m_mouseHighlightMode = false;
    }


    if (EclMouseInButton(m_parent, this))
    {
      if (!m_highlightedThisFrame)
      {
        parent->SetCurrentButton(this);
        m_highlightedThisFrame = true;
        m_mouseHighlightMode = true;
      }
    }
    else
    {
      m_highlightedThisFrame = false;
    }
  }

  // ============================================================================


  BorderlessButton::BorderlessButton()
    : SpeciesButton()
  {
  }


  void BorderlessButton::SetShortProperties(char const* _name, int _x, int _y, int _w, int _h, char* _caption, char* _tooltip)
  {
    if (_w == -1)
    {
      _w = strlen(_name) * 7 + 9;
    }

    if (_h == -1)
    {
      _h = 15;
    }

    SetProperties((char*)_name, _x, _y, _w, _h, _caption, _tooltip);
  }


  void BorderlessButton::Render(int realX, int realY, bool highlighted, bool clicked)
  {
    SpeciesWindow* parent = (SpeciesWindow*)m_parent;
    if (parent->m_buttonOrder[parent->m_currentButton] == this)
    {
      clicked = true;
    }
    if (clicked)
    {
      glShadeModel(GL_SMOOTH);
      glBegin(GL_QUADS);
      glColor4ub(199, 214, 220, 255);
      glVertex2f(realX, realY);
      glVertex2f(realX + m_w, realY);
      glColor4ub(112, 141, 168, 255);
      glVertex2f(realX + m_w, realY + m_h);
      glVertex2f(realX, realY + m_h);
      glEnd();
      glShadeModel(GL_FLAT);

      g_editorFont.SetRenderShadow(true);
      glColor4ub(255, 255, 150, 30);
      if (m_centered)
      {
        g_editorFont.DrawText2DCentre(realX + m_w / 2, realY + 10, parent->GetMenuSize(m_fontSize), m_caption);
        g_editorFont.DrawText2DCentre(realX + m_w / 2, realY + 10, parent->GetMenuSize(m_fontSize), m_caption);
      }
      else
      {
        g_editorFont.DrawText2D(realX + 5, realY + 10, parent->GetMenuSize(m_fontSize), m_caption);
        g_editorFont.DrawText2D(realX + 5, realY + 10, parent->GetMenuSize(m_fontSize), m_caption);
      }
      g_editorFont.SetRenderShadow(false);
    }
    else
    {
      glColor4ub(107, 37, 39, 64);

      if (highlighted)
      {
        parent->SetCurrentButton(this);
        glLineWidth(1.0f);
        glBegin(GL_LINES);
        glColor4ub(100, 34, 34, 250);
        glVertex2f(realX, realY + m_h);
        glVertex2f(realX, realY);

        glVertex2f(realX, realY);
        glVertex2f(realX + m_w, realY);

        glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
        glVertex2f(realX + m_w, realY);
        glVertex2f(realX + m_w, realY + m_h);

        glVertex2f(realX + m_w, realY + m_h);
        glVertex2f(realX, realY + m_h);
        glEnd();
      }

      glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
      if (m_centered)
      {
        g_editorFont.DrawText2DCentre(realX + m_w / 2, realY + 10, parent->GetMenuSize(m_fontSize), m_caption);
      }
      else
      {
        g_editorFont.DrawText2D(realX + 5, realY + 10, parent->GetMenuSize(m_fontSize), m_caption);
      }

      if (highlighted)
      {
        glColor4f(1.0f, 1.0f, 1.0f, 0.5f);
        if (m_centered)
        {
          g_editorFont.DrawText2DCentre(realX + m_w / 2, realY + 10, parent->GetMenuSize(m_fontSize), m_caption);
        }
        else
        {
          g_editorFont.DrawText2D(realX + 5, realY + 10, parent->GetMenuSize(m_fontSize), m_caption);
        }
      }
    }

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    EclButton::Render(realX, realY, highlighted, clicked);
  }


  // ****************************************************************************
  // Class SpeciesWindow
  // ****************************************************************************

  SpeciesWindow::SpeciesWindow(std::string_view name)
    : EclWindow(name),
      m_currentButton(0),
      m_buttonChangedThisUpdate(false)
  {
    SetTitle(name);
    // Was strupr(), which uppercases a char buffer in place. m_title is a
    // std::string now, so the same thing is spelled out.
    for (char& c : m_title)
      c = static_cast<char>(toupper(static_cast<unsigned char>(c)));

    EclSetCurrentFocus(m_name);

    // Guarded because a window built during App's construction would run this
    // before InitialiseInputManager. Nothing does today, and a menu that
    // existed before the input system did could not have worked anyway; the
    // guard is here so the failure would be a dead key rather than a crash.
    if (g_inputManager)
    {
      m_menuActivateSubscription = g_inputManager->subscribe(ControlType::ControlMenuActivate, [this] { OnMenuActivate(); });
      m_menuCloseSubscription = g_inputManager->subscribe(ControlType::ControlMenuClose, [this] { OnMenuClose(); });
    }
  }


  // THE FOCUS TEST MOVED WITH THE HANDLER rather than being lost. Update()
  // wrapped all four of its polls in it, and a subscriber is offered the
  // control whether or not this window is the one in front.
  void SpeciesWindow::OnMenuActivate()
  {
    if (m_name != EclGetCurrentFocus())
      return;

    // The empty-order case was already unguarded here and indexes out of range;
    // it is checked now because moving code is a bad moment to carry a latent
    // crash across untouched.
    if (m_buttonOrder.empty())
      return;

    EclButton* button = m_buttonOrder[m_currentButton];
    if (button)
      button->MouseUp();
  }


  void SpeciesWindow::OnMenuClose()
  {
    if (m_name != EclGetCurrentFocus() || g_atMainMenu)
      return;

    // DESTROYS THIS WINDOW, AND THEREFORE THIS SUBSCRIPTION, from inside the
    // handler. That is the case InputManager::FireSubscriptions copies the
    // handler out for, and it is the reason this pilot is worth having: a
    // conversion that only worked for handlers which outlive their call would
    // not be usable by the code that most wants it.
    EclRemoveWindow(m_name);
  }

  SpeciesWindow::~SpeciesWindow()
  {
    std::vector<std::unique_ptr<EclWindow>>* windows = EclGetWindows();
    // The emptiness check is not redundant. the legacy list's GetData returned a null T()
    // for an out-of-range read — LListTests pins that as
    // OutOfRangeReadsReturnZero — so this `if` was doing double duty as a
    // bounds check. std::vector would be undefined behaviour on an empty list.
    if (!windows->empty() && (*windows)[0])
    {
      EclSetCurrentFocus((*windows)[0]->m_name);
    }
  }


  InputField* SpeciesWindow::CreateInputField(char const* name, int y, float _lowBound, float _highBound, SpeciesButton* callback, int x, int w,
                                              bool isTextField)
  {
    if (x == -1)
      x = 10;
    if (w == -1)
      w = m_w - x * 2;

    InputField* input = new InputField();

    // A text field spans the full width; a value field leaves 37 pixels for the
    // two scroller buttons. That is the whole of what the old dataType switch
    // decided here.
    input->SetShortProperties((char*)name, x, y, isTextField ? w : w - 37, GetMenuSize(15));

    input->m_lowBound = _lowBound;
    input->m_highBound = _highBound;
    input->SetCallback(callback);
    return input;
  }

  void SpeciesWindow::CreateValueScrollers(char const* name, InputField* input, int y, float change)
  {
    // Both scroller names were formatted into a char[64] from a control name of
    // unbounded length, so a long label overran them. The names themselves stay
    // char const* on the Eclipse side until T11 converts that.
    std::string const nameLeft = std::format("{} left", name);
    InputScroller* left = new InputScroller();
    left->SetProperties(nameLeft.c_str(), input->m_x + input->m_w + 5, y, 15, 15, "<", "Value left");
    left->m_inputField = input;
    left->m_change = -change;
    RegisterButton(left);

    std::string const nameRight = std::format("{} right", name);
    InputScroller* right = new InputScroller();
    right->SetProperties(nameRight.c_str(), input->m_x + input->m_w + 22, y, 15, 15, ">", "Value right");
    right->m_inputField = input;
    right->m_change = change;
    RegisterButton(right);
  }

  // The registration order below — properties, bounds, callback, Register<type>,
  // RegisterButton, then the scrollers — is the order the single tagged function
  // used. m_buttons order is render and tab order, so it is preserved exactly.

  void SpeciesWindow::CreateValueControl(char const* name, unsigned char* value, int y, float change, float _lowBound, float _highBound,
                                         SpeciesButton* callback, int x, int w)
  {
    InputField* input = CreateInputField(name, y, _lowBound, _highBound, callback, x, w, false);
    input->RegisterChar(value);
    RegisterButton(input);
    CreateValueScrollers(name, input, y, change);
  }

  void SpeciesWindow::CreateValueControl(char const* name, bool* value, int y, float change, float _lowBound, float _highBound,
                                         SpeciesButton* callback, int x, int w)
  {
    InputField* input = CreateInputField(name, y, _lowBound, _highBound, callback, x, w, false);

    // Byte-for-byte what the void* version did with these four call sites: the
    // pointer arrived as void* and was cast to unsigned char*. Every one of them
    // passes bounds of 0 to 1, so the only values written are 0 and 1 and the
    // bool stays valid. Kept rather than corrected because correcting it means
    // deciding what an editor field for a bool should be, and that is a change to
    // the editor rather than to the type of this parameter.
    input->RegisterChar(reinterpret_cast<unsigned char*>(value));
    RegisterButton(input);
    CreateValueScrollers(name, input, y, change);
  }

  void SpeciesWindow::CreateValueControl(char const* name, int* value, int y, float change, float _lowBound, float _highBound,
                                         SpeciesButton* callback, int x, int w)
  {
    InputField* input = CreateInputField(name, y, _lowBound, _highBound, callback, x, w, false);
    input->RegisterInt(value);
    RegisterButton(input);
    CreateValueScrollers(name, input, y, change);
  }

  void SpeciesWindow::CreateValueControl(char const* name, float* value, int y, float change, float _lowBound, float _highBound,
                                         SpeciesButton* callback, int x, int w)
  {
    InputField* input = CreateInputField(name, y, _lowBound, _highBound, callback, x, w, false);
    input->RegisterFloat(value);
    RegisterButton(input);
    CreateValueScrollers(name, input, y, change);
  }

  void SpeciesWindow::CreateValueControl(char const* name, std::string* value, int y, float change, float _lowBound, float _highBound,
                                         SpeciesButton* callback, int x, int w)
  {
    // change is unused for a text field, as it was before: the old function
    // created no scrollers for TypeString and change fed only those.
    InputField* input = CreateInputField(name, y, _lowBound, _highBound, callback, x, w, true);
    input->RegisterString(value);
    RegisterButton(input);
  }

  void SpeciesWindow::RemoveValueControl(char* name)
  {
    RemoveButton(name);

    RemoveButton(std::format("{} left", name).c_str());
    RemoveButton(std::format("{} right", name).c_str());
  }

  void SpeciesWindow::CreateColourControl(char const* name, int* value, int y, SpeciesButton* callback, int x, int w)
  {
    if (x == -1)
      x = 10;
    if (w == -1)
      w = m_w - x;

    ColourWidget* cw = new ColourWidget();
    cw->SetShortProperties((char*)name, x, y, w - 13);
    cw->SetCallback(callback);
    cw->SetValue(value);
    RegisterButton(cw);
  }

  void SpeciesWindow::Create()
  {
    CloseButton* close = new CloseButton();
    close->SetProperties("Close", m_w - 12, 2, 10, 10, " ", "Close this window");
    close->m_iconised = true;
    RegisterButton(close);
  }


  void SpeciesWindow::Remove()
  {
    while (m_buttons.size() > 0)
    {
      EclButton* button = m_buttons[0];
      RemoveButton(button->m_name);
    }
    m_buttonOrder.clear();
    m_currentButton = 0;
  }

  // Get the coordinates of the drawable area on the rectangle
  int SpeciesWindow::GetClientRectX1() { return 2; }

  int SpeciesWindow::GetClientRectX2() { return m_w - 2; }

  int SpeciesWindow::GetClientRectY1() { return GetMenuSize(15) + 1; }

  int SpeciesWindow::GetClientRectY2() { return m_h - 2; }

  void SpeciesWindow::Render(bool hasFocus)
  {
    //
    // Main body fill

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/InterfaceRed.bmp"));
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    float texH = 1.0f;
    float texW = texH * 512.0f / 64.0f;

    // glColor4f( 0.3f, 0.3f, 0.4f, 0.95f );
    glColor4f(1.0f, 1.0f, 1.0f, 0.96f);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(m_x, m_y);
    glTexCoord2f(texW, 0.0f);
    glVertex2f(m_x + m_w, m_y);
    glTexCoord2f(texW, texH);
    glVertex2f(m_x + m_w, m_y + m_h);
    glTexCoord2f(0.0f, texH);
    glVertex2f(m_x, m_y + m_h);
    glEnd();

    glDisable(GL_TEXTURE_2D);

    //
    // Title bar fill

    float titleBarHeight = GetClientRectY1() - 1;
    glShadeModel(GL_SMOOTH);
    glBegin(GL_QUADS);
    glColor4ub(199, 214, 220, 255);
    glVertex2f(m_x, m_y);
    glVertex2f(m_x + m_w, m_y);
    glColor4ub(112, 141, 168, 255);
    glVertex2f(m_x + m_w, m_y + titleBarHeight);
    glVertex2f(m_x, m_y + titleBarHeight);
    glEnd();
    glShadeModel(GL_FLAT);

    // glColor4ub( 112, 141, 168, 120 );
    // glBegin( GL_LINES );
    //     glVertex2f( m_x, m_y+1 );
    //     glVertex2f( m_x+m_w, m_y+1 );
    // glEnd();

    //
    // Border lines

    glLineWidth(2.0f);
    // glColor4ub( 100, 34, 34, 255 );
    glColor4ub(199, 214, 220, 255);
    glBegin(GL_LINES); // top
    glVertex2f(m_x, m_y);
    glVertex2f(m_x + m_w, m_y);
    glEnd();

    glBegin(GL_LINES); // left
    glVertex2f(m_x, m_y);
    glVertex2f(m_x, m_y + m_h);
    glEnd();

    // glColor4f( 0.0f, 0.0f, 0.1f, 1.0f );
    glBegin(GL_LINES);
    glVertex2f(m_x + m_w, m_y); // right
    glVertex2f(m_x + m_w, m_y + m_h);
    glEnd();

    glBegin(GL_LINES); // bottom
    glVertex2f(m_x, m_y + m_h);
    glVertex2f(m_x + m_w, m_y + m_h);
    glEnd();

    glLineWidth(1.0f);
    glColor4ub(42, 56, 82, 255);
    glBegin(GL_LINE_LOOP);
    glVertex2f(m_x - 2, m_y - 2);
    glVertex2f(m_x + m_w + 1, m_y - 2);
    glVertex2f(m_x + m_w + 1, m_y + m_h + 1);
    glVertex2f(m_x - 2, m_y + m_h + 1);
    glEnd();

    // glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
    g_gameFont.SetRenderShadow(true);
    glColor4ub(255, 255, 150, 30);
    int y = m_y + 9;
    int fontSize = GetMenuSize(12);
    if (g_largeMenus)
    {
      y = m_y + fontSize / 2;
    }
    g_gameFont.DrawText2DCentre(m_x + m_w / 2, y, fontSize, m_title);
    g_gameFont.DrawText2DCentre(m_x + m_w / 2, y, fontSize, m_title);
    g_gameFont.SetRenderShadow(false);


    EclWindow::Render(hasFocus);
  }

  int SpeciesWindow::GetMenuSize(int _value)
  {
    if (g_largeMenus)
    {
      // int h = m_originalH;
      // float scale = float(m_h)/float(h);
      int screenH = g_renderer->ScreenH();
      float scale = 0.96f * (float(screenH) / 460.0f);

      return _value * scale;
    }
    else
    {
      return _value;
    }
  }

  void SpeciesWindow::SetMenuSize(int _w, int _h)
  {
    if (g_largeMenus)
    {
      int screenH = g_renderer->ScreenH();

      float ratio = 0.96f * (float(screenH) / 460.0f);

      _h *= ratio;
      _w *= ratio;
    }

    SetSize(_w, _h);
  }

  void SpeciesWindow::Update()
  {
    m_buttonChangedThisUpdate = false;

    // THE m_skipUpdate EARLY RETURN THAT USED TO BE HERE IS GONE. The flag was
    // initialised false and only ever CLEARED — nothing anywhere in the tree
    // ever set it true — so the branch it guarded was unreachable and the
    // member was a bool that could only hold one value. Found while converting
    // the two controls below to subscriptions in T9, because a guard a
    // subscription would have bypassed is worth checking before bypassing it.

    // MenuActivate and MenuClose are SUBSCRIPTIONS now — see the constructor.
    // These two stay polled because they write m_buttonChangedThisUpdate, which
    // this function clears three lines up; see the note beside their tokens in
    // the header.
    if (m_name == EclGetCurrentFocus())
    {
      if (g_inputManager->controlEvent(ControlType::ControlMenuDown))
      {
        m_buttonChangedThisUpdate = true;
        m_currentButton++;
        m_currentButton = std::min(m_currentButton, static_cast<int>(m_buttonOrder.size()) - 1);
      }
      if (g_inputManager->controlEvent(ControlType::ControlMenuUp))
      {
        m_buttonChangedThisUpdate = true;
        m_currentButton--;
        m_currentButton = std::max(0, m_currentButton);
      }
    }
  }

  void SpeciesWindow::SetCurrentButton(EclButton* button)
  {
    for (int i = 0; i < static_cast<int>(m_buttonOrder.size()); ++i)
    {
      if (m_buttonOrder[i] == button)
      {
        m_currentButton = i;
        return;
      }
    }
  }

  // ****************************************************************************
  // Class GameExitButton
  // ****************************************************************************

  void GameExitButton::MouseUp()
  {
    g_requestQuit = true;
    // g_atMainMenu = true;
    // g_renderer->StartFadeOut();
  }


  // ****************************************************************************
  // Class CloseButton
  // ****************************************************************************

  CloseButton::CloseButton()
    : SpeciesButton(),
      m_iconised(false)
  {
  }

  void CloseButton::MouseUp() { EclRemoveWindow(m_parent->m_name); }


  void CloseButton::Render(int realX, int realY, bool highlighted, bool clicked)
  {
    if (m_iconised)
    {
      if (highlighted || clicked)
        glColor4ub(160, 137, 139, 64);
      else
        glColor4ub(60, 37, 39, 64);

      glBegin(GL_QUADS);
      glVertex2f(realX, realY);
      glVertex2f(realX + m_w, realY);
      glVertex2f(realX + m_w, realY + m_h);
      glVertex2f(realX, realY + m_h);
      glEnd();

      glLineWidth(1.0f);
      glBegin(GL_LINES);
      glColor4ub(0, 0, 150, 100);
      glVertex2f(realX, realY + m_h);
      glVertex2f(realX, realY);

      glVertex2f(realX, realY);
      glVertex2f(realX + m_w, realY);

      glColor4f(0.1f, 0.0f, 0.0f, 1.0f);
      glVertex2f(realX + m_w, realY);
      glVertex2f(realX + m_w, realY + m_h);

      glVertex2f(realX + m_w, realY + m_h);
      glVertex2f(realX, realY + m_h);
      glEnd();
    }
    else
    {
      SpeciesButton::Render(realX, realY, highlighted, clicked);
    }
  }


  // ****************************************************************************
  // Class InvertexBox
  // ****************************************************************************

  void InvertedBox::Render(int realX, int realY, bool highlighted, bool clicked)
  {
    // SpeciesButton::Render( realX, realY, highlighted, clicked );

    glColor4f(0.05f, 0.0f, 0.0f, 0.4f);
    glBegin(GL_QUADS);
    glVertex2f(realX, realY);
    glVertex2f(realX + m_w, realY);
    glVertex2f(realX + m_w, realY + m_h);
    glVertex2f(realX, realY + m_h);
    glEnd();

    //
    // Border lines

    glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
    glBegin(GL_LINES); // top
    glVertex2f(realX, realY);
    glVertex2f(realX + m_w, realY);
    glEnd();

    glBegin(GL_LINES); // left
    glVertex2f(realX, realY);
    glVertex2f(realX, realY + m_h);
    glEnd();

    glColor4ub(100, 34, 34, 150);
    ; // right
    glBegin(GL_LINES);
    glVertex2f(realX + m_w, realY);
    glVertex2f(realX + m_w, realY + m_h);
    glEnd();

    glBegin(GL_LINES); // bottom
    glVertex2f(realX, realY + m_h);
    glVertex2f(realX + m_w, realY + m_h);
    glEnd();
  }


  void LabelButton::Render(int realX, int realY, bool highlighted, bool clicked)
  {
    if (m_disabled)
      glColor4f(0.5f, 0.5f, 0.5f, 1.0f);
    else
      glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    g_editorFont.DrawText2D(realX + 5, realY + 10, 11.0f, m_caption);
  }
} // namespace Species
