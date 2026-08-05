#include "pch.h"
#include "Debug.h"
// #include "Input.h"
#include "TextRenderer.h"
#include "Preferences.h"
#include "WindowManager.h"
#include "LanguageTable.h"

#include "DebugMenu.h"
#include "NetworkWindow.h"
#include "PrefsScreenWindow.h"
#include "PrefsGraphicsWindow.h"
#include "PrefsSoundWindow.h"
#include "ProfileWindow.h"
#include "CheatWindow.h"
#include "UserProfileWindow.h"
#include "ReallyQuitWindow.h"

#include "WorldPointers.h"
#include "AppState.h"

// ****************************************************************************
// Menu Buttons
// ****************************************************************************

#ifdef PROFILER_ENABLED


namespace Species
{
  class ProfileButton : public SpeciesButton
  {
    public:
      void MouseUp() { DebugKeyBindings::ProfileButton(); }
  };
#endif // PROFILER_ENABLED


class NetworkButton : public SpeciesButton
{
  public:
    void MouseUp() { DebugKeyBindings::NetworkButton(); }
};


#ifdef LOCATION_EDITOR
class EditorButton : public SpeciesButton
{
  public:
    void MouseUp() { DebugKeyBindings::EditorButton(); }
};
#endif // LOCATION_EDITOR


class DebugCameraButton : public SpeciesButton
{
  public:
    void MouseUp() { DebugKeyBindings::DebugCameraButton(); }
};


class FPSButton : public SpeciesButton
{
  public:
    void MouseUp() { DebugKeyBindings::FPSButton(); }
};


class PrefsScreenButton : public SpeciesButton
{
  public:
    void MouseUp()
    {
      if (!EclGetWindow(LANGUAGEPHRASE("dialog_screenoptions")))
      {
        EclRegisterWindow(std::make_unique<PrefsScreenWindow>());
      }
    }
};


class PrefsGfxDetailButton : public SpeciesButton
{
  public:
    void MouseUp()
    {
      if (!EclGetWindow(LANGUAGEPHRASE("dialog_graphicsoptions")))
      {
        EclRegisterWindow(std::make_unique<PrefsGraphicsWindow>());
      }
    }
};


class PrefsSoundButton : public SpeciesButton
{
  public:
    void MouseUp()
    {
      if (!EclGetWindow(LANGUAGEPHRASE("dialog_soundoptions")))
      {
        EclRegisterWindow(std::make_unique<PrefsSoundWindow>());
      }
    };
};


#ifdef CHEATMENU_ENABLED
class CheatButton : public SpeciesButton
{
  public:
    void MouseUp() { DebugKeyBindings::CheatButton(); }
};
#endif


// ****************************************************************************
// Class DebugMenu
// ****************************************************************************

DebugMenu::DebugMenu(char* name)
  : SpeciesWindow(name)
{
  m_x = 10;
  m_y = 20;
  m_w = 170;
  m_h = 75;
}


void DebugMenu::Advance() {}


void DebugMenu::Create()
{
  SpeciesWindow::Create();

  int pitch = 18;
  int y = 5;

  SpeciesButton* button;

#ifdef PROFILER_ENABLED
  button = new ProfileButton();
  button->SetShortProperties("Profile (F6)", 10, y += pitch, m_w - 20);
  RegisterButton(button);
#endif // PROFILER_ENABLED

  button = new NetworkButton();
  button->SetShortProperties("Network Stats", 10, y += pitch, m_w - 20);
  RegisterButton(button);

  button = new FPSButton();
  button->SetShortProperties("Display FPS (F5)", 10, y += pitch, m_w - 20);
  RegisterButton(button);

  y += pitch / 2.0f;

  y += pitch / 2.0f;

  button = new DebugCameraButton();
  button->SetShortProperties("Dbg Cam (F2)", 10, y += pitch, m_w - 20);
  RegisterButton(button);

  y += pitch / 2.0f;

  bool modsEnabled = g_prefsManager->GetInt("ModSystemEnabled", 0) != 0;

#ifdef LOCATION_EDITOR
  if (modsEnabled)
  {
    button = new EditorButton();
    button->SetShortProperties("Toggle Editor (F3)", 10, y += pitch, m_w - 20);
    RegisterButton(button);
  }
#endif // LOCATION_EDITOR

#ifdef CHEATMENU_ENABLED
  button = new CheatButton();
  button->SetShortProperties("Cheat Menu (F4)", 10, y += pitch, m_w - 20);
  RegisterButton(button);
#endif


  y += pitch / 2.0f;
}


void DebugMenu::Render(bool hasFocus)
{
  Advance();

  SpeciesWindow::Render(hasFocus);

  EclButton* camDbgButton = GetButton("Dbg Cam (F2)");
  DEBUG_ASSERT(camDbgButton);
  int y = m_y + camDbgButton->m_y + 11;

  switch (g_camera->GetDebugMode())
  {
  case CameraAccess::DebugModeAlways:
    g_editorFont.DrawText2D(m_x + m_w - 47, y, 10, "Always");
    break;
  case CameraAccess::DebugModeAuto:
    g_editorFont.DrawText2D(m_x + m_w - 47, y, 10, "Auto");
    break;
  case CameraAccess::DebugModeNever:
    g_editorFont.DrawText2D(m_x + m_w - 47, y, 10, "Never");
    break;
  }
}


// ****************************************************************************
// Class DebugKeyBindings
// ****************************************************************************

void DebugKeyBindings::DebugMenu()
{
  char* debugMenuWindowName = LANGUAGEPHRASE("dialog_toolsmenu");
  if (EclGetWindow(debugMenuWindowName))
    EclRemoveWindow(debugMenuWindowName);
  else
    // Species::DebugMenu, not ::DebugMenu: the leading :: was here to pick the
    // CLASS over the enclosing DebugKeyBindings::DebugMenu method, and the
    // class moved into the game namespace. namespace-migration T4.
    EclRegisterWindow(std::make_unique<Species::DebugMenu>(debugMenuWindowName));
}

#ifdef PROFILER_ENABLED
void DebugKeyBindings::ProfileButton()
{
  if (EclGetWindow("Profiler"))
  {
    EclRemoveWindow("Profiler");
  }
  else
  {
    auto ownedPw = std::make_unique<ProfileWindow>("Profiler");
    ProfileWindow* pw = ownedPw.get();
    pw->m_w = 570;
    pw->m_h = 450;
    pw->m_x = g_renderer->ScreenW() - pw->m_w - 20;
    pw->m_y = 30;
    EclRegisterWindow(std::move(ownedPw));
  }
}
#endif


void DebugKeyBindings::NetworkButton()
{
  if (!EclGetWindow("Network Stats"))
  {
    auto ownedNw = std::make_unique<NetworkWindow>("Network Stats");
    NetworkWindow* nw = ownedNw.get();
    nw->m_w = 200;
    nw->m_h = 200;
    nw->m_x = 10;
    nw->m_y = g_renderer->ScreenH() - nw->m_h;
    EclRegisterWindow(std::move(ownedNw));
  }
}


#ifdef LOCATION_EDITOR
void DebugKeyBindings::EditorButton() { g_requestToggleEditing = true; }
#endif // LOCATION_EDITOR


void DebugKeyBindings::DebugCameraButton() { g_camera->SetNextDebugMode(); }

void DebugKeyBindings::FPSButton() { g_renderer->SetDisplayFps(!g_renderer->DisplayFps()); }


#ifdef CHEATMENU_ENABLED
void DebugKeyBindings::CheatButton()
{
  if (!EclGetWindow("Cheat Window"))
  {
    auto owned = std::make_unique<CheatWindow>("Cheat Window");
    CheatWindow* window = owned.get();
    window->m_w = 200;
    window->m_h = 200;
    window->m_x = 250;
    window->m_y = 50;
    EclRegisterWindow(std::move(owned));
  }
}
#endif


void DebugKeyBindings::ReallyQuitButton()
{
  // Bring up a really quit window
  if (!EclGetWindow(REALLYQUIT_WINDOWNAME))
    EclRegisterWindow(std::make_unique<ReallyQuitWindow>());
}

void DebugKeyBindings::ToggleFullscreenButton()
{
  bool switchingToWindowed;
  SetWindowed(!g_windowManager->Windowed(), true, switchingToWindowed);
}
} // namespace Species
