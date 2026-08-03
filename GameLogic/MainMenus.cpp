#include "pch.h"
#include "MainMenus.h"
#include "GlobalWorld.h"
#include "LanguageTable.h"
#include "Preferences.h"
#include "PrefsGraphicsWindow.h"
#include "PrefsKeybindingsWindow.h"
#include "PrefsOtherWindow.h"
#include "PrefsScreenWindow.h"
#include "PrefsSoundWindow.h"
#include "Resource.h"
#include "TextRenderer.h"
#include "UserProfileWindow.h"
#include "Win32EventHandler.h"
#include "WindowManager.h"
#include "WorldPointers.h"
#include "AppState.h"
#include "AppCommands.h"

class WebsiteButton;

class SkipPrologueWindowButton : public SpeciesButton
{
  void MouseUp() override
  {
    if (!EclGetWindow(LANGUAGEPHRASE("dialog_skipprologue")))
      EclRegisterWindow(new SkipPrologueWindow(), m_parent);
  }
};

class SkipPrologueButton : public SpeciesButton
{
  void MouseUp() override
  {
    std::vector<EclWindow*>* windows = EclGetWindows();
    while (windows->size() > 0)
    {
      EclWindow* w = (*windows)[0];
      EclRemoveWindow(w->m_name);
    }

    g_script->Skip();
    g_appCommands->LoadCampaign();
  }
};

class PlayPrologueButton : public SpeciesButton
{
  void MouseUp() override
  {
    std::vector<EclWindow*>* windows = EclGetWindows();
    while (windows->size() > 0)
    {
      EclWindow* w = (*windows)[0];
      EclRemoveWindow(w->m_name);
    }

    g_script->Skip();
    g_appCommands->LoadPrologue();
  }
};

class PlayPrologueWindowButton : public SpeciesButton
{
  void MouseUp() override
  {
    if (!EclGetWindow(LANGUAGEPHRASE("dialogue_playprologue")))
      EclRegisterWindow(new PlayPrologueWindow(), m_parent);
  }
};

class AboutSpeciesButton : public SpeciesButton
{
  void MouseUp() override
  {
    if (!EclGetWindow(LANGUAGEPHRASE("about_darwinia")))
      EclRegisterWindow(new AboutSpeciesWindow(), m_parent);
  }
};

class MainMenuUserProfileButton : public SpeciesButton
{
  void MouseUp() override
  {
    if (!EclGetWindow(LANGUAGEPHRASE("dialog_profile")))
      EclRegisterWindow(new UserProfileWindow(), m_parent);
  }
};

class OptionsButton : public SpeciesButton
{
  void MouseUp() override
  {
    if (!EclGetWindow(LANGUAGEPHRASE("dialog_options")))
      EclRegisterWindow(new OptionsMenuWindow(), m_parent);
  }
};

class ScreenOptionsButton : public SpeciesButton
{
  void MouseUp() override
  {
    if (!EclGetWindow(LANGUAGEPHRASE("dialog_screenoptions")))
      EclRegisterWindow(new PrefsScreenWindow(), m_parent);
  }
};

class GraphicsOptionsButton : public SpeciesButton
{
  void MouseUp() override
  {
    if (!EclGetWindow(LANGUAGEPHRASE("dialog_graphicsoptions")))
      EclRegisterWindow(new PrefsGraphicsWindow(), m_parent);
  }
};

class SoundOptionsButton : public SpeciesButton
{
  void MouseUp() override
  {
    if (!EclGetWindow(LANGUAGEPHRASE("dialog_soundoptions")))
      EclRegisterWindow(new PrefsSoundWindow(), m_parent);
  }
};

class OtherOptionsButton : public SpeciesButton
{
  void MouseUp() override
  {
    if (!EclGetWindow(LANGUAGEPHRASE("dialog_otheroptions")))
      EclRegisterWindow(new PrefsOtherWindow(), m_parent);
  }
};

class KeybindingsOptionsButton : public SpeciesButton
{
  void MouseUp() override
  {
    if (!EclGetWindow(LANGUAGEPHRASE("dialog_inputoptions")))
      EclRegisterWindow(new PrefsKeybindingsWindow, m_parent);
  }
};

// ****************************************************************************
// Class MainMenuWindow
// ****************************************************************************

MainMenuWindow::MainMenuWindow()
  : SpeciesWindow(LANGUAGEPHRASE("dialog_mainmenu"))
{
  int screenW = g_renderer->ScreenW();
  int screenH = g_renderer->ScreenH();

  SetMenuSize(220, 260);
  SetPosition(screenW / 2.0f - m_w / 2.0f, screenH / 2.0f - m_h / 2.0f);
}

void MainMenuWindow::Render(bool _hasFocus) { SpeciesWindow::Render(_hasFocus); }

// ***************************************************************************
// Class OptionsMenuWindow
// ***************************************************************************

OptionsMenuWindow::OptionsMenuWindow()
  : SpeciesWindow(LANGUAGEPHRASE("dialog_options"))
{
  int screenW = g_renderer->ScreenW();
  int screenH = g_renderer->ScreenH();

  SetMenuSize(240, 230);
  //    SetPosition( screenW/2.0f - m_w/2.0f,
  //                 screenH/2.0f - m_h/2.0f );
}

void OptionsMenuWindow::Create()
{
  SpeciesWindow::Create();

  int fontSize = GetMenuSize(13);
  int y = GetClientRectY1();
  int border = GetClientRectX1() + 10;
  int buttonH = GetMenuSize(20);
  int buttonW = m_w - border * 2;
  int h = buttonH + border;

  auto screen = new ScreenOptionsButton();
  screen->SetShortProperties(LANGUAGEPHRASE("dialog_screenoptions"), border, y += border, buttonW, buttonH);
  screen->m_fontSize = fontSize;
  screen->m_centered = true;
  RegisterButton(screen);
  m_buttonOrder.PutData(screen);

  auto graphics = new GraphicsOptionsButton();
  graphics->SetShortProperties(LANGUAGEPHRASE("dialog_graphicsoptions"), border, y += h, buttonW, buttonH);
  graphics->m_fontSize = fontSize;
  graphics->m_centered = true;
  RegisterButton(graphics);
  m_buttonOrder.PutData(graphics);

  auto sound = new SoundOptionsButton();
  sound->SetShortProperties(LANGUAGEPHRASE("dialog_soundoptions"), border, y += h, buttonW, buttonH);
  sound->m_fontSize = fontSize;
  sound->m_centered = true;
  RegisterButton(sound);
  m_buttonOrder.PutData(sound);

  auto keys = new KeybindingsOptionsButton();
  keys->SetShortProperties(LANGUAGEPHRASE("dialog_inputoptions"), border, y += h, buttonW, buttonH);
  keys->m_fontSize = fontSize;
  keys->m_centered = true;
  RegisterButton(keys);
  m_buttonOrder.PutData(keys);

  auto other = new OtherOptionsButton();
  other->SetShortProperties(LANGUAGEPHRASE("dialog_otheroptions"), border, y += h, buttonW, buttonH);
  other->m_fontSize = fontSize;
  other->m_centered = true;
  RegisterButton(other);
  m_buttonOrder.PutData(other);

  auto close = new CloseButton();
  close->SetShortProperties(LANGUAGEPHRASE("dialog_close"), border, m_h - h, buttonW, buttonH);
  close->m_fontSize = fontSize;
  close->m_centered = true;
  RegisterButton(close);
  m_buttonOrder.PutData(close);
}

// ============================================================================

class ResetLevelButton : public SpeciesButton
{
  void MouseUp() override { EclRegisterWindow(new ResetLocationWindow(), m_parent); }
};

class ExitLevelButton : public SpeciesButton
{
  void MouseUp() override
  {
    EclRemoveWindow(m_parent->m_name);

    g_requestedLocationId = -1;
  }
};

class WebsiteButton : public SpeciesButton
{
  public:
    char m_website[256];

    void MouseUp() override
    {
      int windowed = g_prefsManager->GetInt("ScreenWindowed", 1);
      if (!windowed)
      {
        // Switch to windowed mode if required
        g_prefsManager->SetInt("ScreenWindowed", 1);
        g_prefsManager->SetInt("ScreenWidth", 800);
        g_prefsManager->SetInt("ScreenHeight", 600);

        g_windowManager->DestroyWin();
        delete g_renderer;
        g_renderer = g_appCommands->CreateRenderer();
        g_renderer->Initialise();
        getW32EventHandler()->ResetWindowHandle();
        g_resource->FlushOpenGlState();
        g_resource->RegenerateOpenGlState();

        g_prefsManager->Save();

        EclInitialise(800, 600);

        m_parent->SetPosition(g_renderer->ScreenW() / 2 - m_parent->m_w / 2, g_renderer->ScreenH() / 2 - m_parent->m_h / 2);
      }
      g_windowManager->OpenWebsite(m_website);
    }
};

LocationWindow::LocationWindow()
  : SpeciesWindow(LANGUAGEPHRASE("dialog_locationmenu"))
{
  int screenW = g_renderer->ScreenW();
  int screenH = g_renderer->ScreenH();

  SetMenuSize(200, 220);
  SetPosition(screenW / 2.0f - m_w / 2.0f, screenH / 2.0f - m_h / 2.0f);
}

void LocationWindow::Create()
{
  SpeciesWindow::Create();

  int fontSize = GetMenuSize(13);
  int y = GetClientRectY1();
  int border = GetClientRectX1() + 10;
  int buttonH = GetMenuSize(20);
  int buttonW = m_w - border * 2;
  int h = buttonH + border;

  int gap = border;

  GlobalLocation* loc = g_globalWorld->GetLocation(g_locationId);

  if (g_appCommands->HasBoughtGame())
  {
    // Full game menu

    if (loc && !loc->m_missionCompleted)
    {
      auto reset = new ResetLevelButton();
      reset->SetShortProperties(LANGUAGEPHRASE("dialog_resetlocation"), border, y += gap, buttonW, buttonH);
      reset->m_fontSize = fontSize;
      reset->m_centered = true;
      RegisterButton(reset);
      m_buttonOrder.PutData(reset);
      gap = h;
    }

    if (g_gameMode == GameModePrologue)
    {
      auto exit = new GameExitButton();
      exit->SetShortProperties(LANGUAGEPHRASE("dialog_leavedarwinia"), border, y += h, buttonW, buttonH);
      exit->m_fontSize = fontSize;
      exit->m_centered = true;
      RegisterButton(exit);
      m_buttonOrder.PutData(exit);
    }
    else
    {
      auto exitLevel = new ExitLevelButton();
      exitLevel->SetShortProperties(LANGUAGEPHRASE("dialog_leavelocation"), border, y += gap, buttonW, buttonH);
      exitLevel->m_fontSize = fontSize;
      exitLevel->m_centered = true;
      RegisterButton(exitLevel);
      m_buttonOrder.PutData(exitLevel);
    }
  }
  else
  {
    // Demo mode

    auto reset = new ResetLevelButton();
    reset->SetShortProperties(LANGUAGEPHRASE("dialog_resetlocation"), border, y += gap, buttonW, buttonH);
    reset->m_fontSize = fontSize;
    reset->m_centered = true;
    RegisterButton(reset);
    m_buttonOrder.PutData(reset);

    auto buy = new WebsiteButton();
    buy->SetShortProperties(LANGUAGEPHRASE("dialog_buyonline"), border, y += h, buttonW, buttonH);
    buy->m_fontSize = fontSize;
    buy->m_centered = true;

    strcpy(buy->m_website, "http://store.introversion.co.uk");

    RegisterButton(buy);
    m_buttonOrder.PutData(buy);

    auto exitLevel = new ExitLevelButton();
    exitLevel->SetShortProperties(LANGUAGEPHRASE("dialog_leavedarwinia"), border, y += h, buttonW, buttonH);
    exitLevel->m_fontSize = fontSize;
    exitLevel->m_centered = true;
    RegisterButton(exitLevel);
    m_buttonOrder.PutData(exitLevel);
  }

  auto options = new OptionsButton();
  options->SetShortProperties(LANGUAGEPHRASE("dialog_options"), border, y += h, buttonW, buttonH);
  options->m_fontSize = fontSize;
  options->m_centered = true;
  RegisterButton(options);
  m_buttonOrder.PutData(options);

  if (g_appCommands->HasBoughtGame() && g_gameMode == GameModePrologue)
  {
    auto skip = new SkipPrologueWindowButton();
    skip->SetShortProperties(LANGUAGEPHRASE("dialog_skipprologue"), border, y += h, buttonW, buttonH);
    skip->m_fontSize = fontSize;
    skip->m_centered = true;
    RegisterButton(skip);
    m_buttonOrder.PutData(skip);
  }

  auto close = new CloseButton();
  close->SetShortProperties(LANGUAGEPHRASE("dialog_close"), border, m_h - h, buttonW, buttonH);
  close->m_fontSize = fontSize;
  close->m_centered = true;
  RegisterButton(close);
  m_buttonOrder.PutData(close);
}

// ============================================================================

class ResetLocationButton : public SpeciesButton
{
  void MouseUp() override
  {
    EclRemoveWindow(m_parent->m_name);
    EclRemoveWindow(LANGUAGEPHRASE("dialog_locationmenu"));
  }
};

ResetLocationWindow::ResetLocationWindow()
  : SpeciesWindow(LANGUAGEPHRASE("dialog_resetlocation")) { SetMenuSize(300, 200); }

void ResetLocationWindow::Create()
{
  SpeciesWindow::Create();

  int fontSize = GetMenuSize(13);
  int y = GetClientRectY1();
  int border = GetClientRectX1() + 10;
  int buttonH = GetMenuSize(20);
  int buttonW = m_w / 2 - border * 2;
  int h = buttonH + border;

  //int y = m_h - 30;

  auto box = new InvertedBox();
  box->SetProperties("invert", border, y + border, m_w - 20, m_h - 2 * h);
  RegisterButton(box);

  auto close = new CloseButton();
  close->SetShortProperties(LANGUAGEPHRASE("dialog_no"), border, m_h - h, buttonW, buttonH);
  close->m_fontSize = fontSize;
  close->m_centered = true;
  RegisterButton(close);
  m_buttonOrder.PutData(close);

  auto reset = new ResetLocationButton();
  reset->SetShortProperties(LANGUAGEPHRASE("dialog_yes"), m_w - buttonW - border, m_h - h, buttonW, buttonH);
  reset->m_fontSize = fontSize;
  reset->m_centered = true;
  RegisterButton(reset);
  m_buttonOrder.PutData(reset);
}

void ResetLocationWindow::Render(bool _hasFocus)
{
  SpeciesWindow::Render(_hasFocus);

  float y = m_y + 25;
  float h = GetMenuSize(18);

  float fontSize = GetMenuSize(13);

  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
  g_gameFont.DrawText2DCentre(m_x + m_w / 2, y += h, fontSize, LANGUAGEPHRASE("dialog_reset1"));
  g_gameFont.DrawText2DCentre(m_x + m_w / 2, y += h, fontSize, LANGUAGEPHRASE("dialog_reset2"));

  y += h;

  g_gameFont.DrawText2DCentre(m_x + m_w / 2, y += h, fontSize, LANGUAGEPHRASE("dialog_reset3"));
  g_gameFont.DrawText2DCentre(m_x + m_w / 2, y += h, fontSize, LANGUAGEPHRASE("dialog_reset4"));
  g_gameFont.DrawText2DCentre(m_x + m_w / 2, y += h, fontSize, LANGUAGEPHRASE("dialog_reset5"));
}

void MainMenuWindow::Create()
{
  SpeciesWindow::Create();

  int y = GetClientRectY1();
  int border = GetClientRectX1() + 10;
  int buttonH = GetMenuSize(20);
  int buttonW = m_w - border * 2;
  int h = buttonH + border;

  int fontSize = GetMenuSize(13);

  auto profile = new MainMenuUserProfileButton();
  profile->SetShortProperties(LANGUAGEPHRASE("dialog_profile"), border, y += border, buttonW, buttonH);
  profile->m_fontSize = fontSize;
  profile->m_centered = true;
  RegisterButton(profile);
  m_buttonOrder.PutData(profile);

  auto options = new OptionsButton();
  options->SetShortProperties(LANGUAGEPHRASE("dialog_options"), border, y += h, buttonW, buttonH);
  options->m_fontSize = fontSize;
  options->m_centered = true;
  RegisterButton(options);
  m_buttonOrder.PutData(options);

  auto website = new WebsiteButton();
  website->SetShortProperties(LANGUAGEPHRASE("dialog_visitwebsite"), border, y += h, buttonW, buttonH);
  website->m_fontSize = fontSize;
  website->m_centered = true;
  strcpy(website->m_website, "http://www.darwinia.co.uk");
  RegisterButton(website);
  m_buttonOrder.PutData(website);

  auto play = new PlayPrologueWindowButton();
  play->SetShortProperties(LANGUAGEPHRASE("dialog_playprologue"), border, y += h, buttonW, buttonH);
  play->m_fontSize = fontSize;
  play->m_centered = true;
  RegisterButton(play);
  m_buttonOrder.PutData(play);

  auto exit = new GameExitButton();
  exit->SetShortProperties(LANGUAGEPHRASE("dialog_leavedarwinia"), border, y += h, buttonW, buttonH);
  exit->m_fontSize = fontSize;
  exit->m_centered = true;
  RegisterButton(exit);
  m_buttonOrder.PutData(exit);

  auto close = new CloseButton();
  close->SetShortProperties(LANGUAGEPHRASE("dialog_close"), border, m_h - h, buttonW, buttonH);
  close->m_fontSize = fontSize;
  close->m_centered = true;
  RegisterButton(close);
  m_buttonOrder.PutData(close);
}

AboutSpeciesWindow::AboutSpeciesWindow()
  : SpeciesWindow(LANGUAGEPHRASE("about_darwinia")) { SetMenuSize(350, 250); }

void AboutSpeciesWindow::Create()
{
  int y = GetClientRectY1();
  int border = GetClientRectX1() + 10;
  int buttonH = GetMenuSize(20);
  int buttonW = m_w - border * 2;
  int h = buttonH + border;
  int fontSize = GetMenuSize(13);

  auto close = new CloseButton();
  close->SetShortProperties(LANGUAGEPHRASE("dialog_close"), border, m_h - h, buttonW, buttonH);
  close->m_fontSize = fontSize;
  close->m_centered = true;
  RegisterButton(close);
  m_buttonOrder.PutData(close);
}

void AboutSpeciesWindow::Render(bool _hasFocus)
{
  SpeciesWindow::Render(_hasFocus);

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Sprites/Darwinian.bmp"));
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

  float texH = 1.0f;
  float texW = texH * 512.0f / 64.0f;

  glColor4f(0.3f, 1.0f, 0.3f, 1.0f);
  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 1.0f);
  glVertex2f(m_x + m_w / 2 - 25, m_y + 30);
  glTexCoord2f(1.0f, 1.0f);
  glVertex2f(m_x + m_w / 2 + 25, m_y + 30);
  glTexCoord2f(1.0f, 0.0f);
  glVertex2f(m_x + m_w / 2 + 25, m_y + GetMenuSize(80));
  glTexCoord2f(0.0f, 0.0f);
  glVertex2f(m_x + m_w / 2 - 25, m_y + GetMenuSize(80));
  glEnd();

  glDisable(GL_TEXTURE_2D);

  float y = m_y + 100;
  float h = GetMenuSize(18);

  float fontSize = GetMenuSize(13);

  char about[512];
  sprintf(about, "%s %s", LANGUAGEPHRASE("bootloader_credits_4"), LANGUAGEPHRASE("bootloader_credits_5"));

  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
  g_gameFont.DrawText2DCentre(m_x + m_w / 2, y += h, fontSize, "Darwinia v1.5.4");
  g_gameFont.DrawText2DCentre(m_x + m_w / 2, y += 2 * h, fontSize, about);
  g_gameFont.DrawText2DCentre(m_x + m_w / 2, y += h, fontSize, "http://www.introversion.co.uk");
}

SkipPrologueWindow::SkipPrologueWindow()
  : SpeciesWindow(LANGUAGEPHRASE("dialog_skipprologue")) { SetMenuSize(360, 350); }

void SkipPrologueWindow::Create()
{
  int y = GetClientRectY1();
  int border = GetClientRectX1() + 10;
  int buttonH = GetMenuSize(20);
  int buttonW = m_w / 2 - border * 2;
  int h = buttonH + border;
  int fontSize = GetMenuSize(13);

  auto skip = new SkipPrologueButton();
  skip->SetShortProperties(LANGUAGEPHRASE("dialog_skipprologue"), border, m_h - h, buttonW, buttonH);
  skip->m_fontSize = fontSize;
  skip->m_centered = true;
  RegisterButton(skip);
  m_buttonOrder.PutData(skip);

  auto close = new CloseButton();
  close->SetShortProperties(LANGUAGEPHRASE("dialog_close"), border * 2 + buttonW, m_h - h, buttonW, buttonH);
  close->m_fontSize = fontSize;
  close->m_centered = true;
  RegisterButton(close);
  m_buttonOrder.PutData(close);
}

void SkipPrologueWindow::Render(bool _hasFocus)
{
  SpeciesWindow::Render(_hasFocus);

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Campaign.bmp"));
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

  float texH = 1.0f;
  float texW = texH * 512.0f / 64.0f;

  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 1.0f);
  glVertex2f(m_x + 25, m_y + 30);
  glTexCoord2f(1.0f, 1.0f);
  glVertex2f(m_x + m_w - 25, m_y + 30);
  glTexCoord2f(1.0f, 0.0f);
  glVertex2f(m_x + m_w - 25, m_y + GetMenuSize(200));
  glTexCoord2f(0.0f, 0.0f);
  glVertex2f(m_x + 25, m_y + GetMenuSize(200));
  glEnd();

  glDisable(GL_TEXTURE_2D);

  float y = m_y + m_h - GetMenuSize(150);
  float h = GetMenuSize(18);

  float fontSize = GetMenuSize(13);

  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

  std::vector<char*>* wrapped = WordWrapText(LANGUAGEPHRASE("dialog_skip1"), m_w * 1.70f, fontSize, true);
  for (char* line : *wrapped)
    g_gameFont.DrawText2DCentre(m_x + m_w / 2, y += h, fontSize, line);

  // Element 0 is the one allocation the whole list points into, so this one
  // delete[] releases every line. It was a plain `delete` over a `new char[]`,
  // and the emptiness check is not redundant: LList::GetData answered an
  // out-of-range read with a null, so `delete wrapped->GetData(0)` was harmless
  // on an empty list where std::vector would be undefined behaviour.
  if (!wrapped->empty())
    delete[] (*wrapped)[0];
  delete wrapped;
};

PlayPrologueWindow::PlayPrologueWindow()
  : SpeciesWindow(LANGUAGEPHRASE("dialog_playprologue")) { SetMenuSize(350, 350); }

void PlayPrologueWindow::Create()
{
  int y = GetClientRectY1();
  int border = GetClientRectX1() + 10;
  int buttonH = GetMenuSize(20);
  int buttonW = m_w / 2 - border * 2;
  int h = buttonH + border;
  int fontSize = GetMenuSize(13);

  auto play = new PlayPrologueButton();
  play->SetShortProperties(LANGUAGEPHRASE("dialog_playprologue"), border, m_h - h, buttonW, buttonH);
  play->m_fontSize = fontSize;
  play->m_centered = true;
  RegisterButton(play);
  m_buttonOrder.PutData(play);

  auto close = new CloseButton();
  close->SetShortProperties(LANGUAGEPHRASE("dialog_close"), border * 2 + buttonW, m_h - h, buttonW, buttonH);
  close->m_fontSize = fontSize;
  close->m_centered = true;
  RegisterButton(close);
  m_buttonOrder.PutData(close);
}

void PlayPrologueWindow::Render(bool _hasFocus)
{
  SpeciesWindow::Render(_hasFocus);

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Prologue.bmp"));
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

  float texH = 1.0f;
  float texW = texH * 512.0f / 64.0f;

  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 1.0f);
  glVertex2f(m_x + 25, m_y + 30);
  glTexCoord2f(1.0f, 1.0f);
  glVertex2f(m_x + m_w - 25, m_y + 30);
  glTexCoord2f(1.0f, 0.0f);
  glVertex2f(m_x + m_w - 25, m_y + GetMenuSize(200));
  glTexCoord2f(0.0f, 0.0f);
  glVertex2f(m_x + 25, m_y + GetMenuSize(200));
  glEnd();

  glDisable(GL_TEXTURE_2D);

  float y = m_y + m_h - GetMenuSize(150);
  float h = GetMenuSize(18);

  float fontSize = GetMenuSize(13);

  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

  std::vector<char*>* wrapped = WordWrapText(LANGUAGEPHRASE("dialog_prologue1"), m_w * 1.70f, fontSize, true);
  for (char* line : *wrapped)
    g_gameFont.DrawText2DCentre(m_x + m_w / 2, y += h, fontSize, line);

  // Element 0 is the one allocation the whole list points into, so this one
  // delete[] releases every line. It was a plain `delete` over a `new char[]`,
  // and the emptiness check is not redundant: LList::GetData answered an
  // out-of-range read with a null, so `delete wrapped->GetData(0)` was harmless
  // on an empty list where std::vector would be undefined behaviour.
  if (!wrapped->empty())
    delete[] (*wrapped)[0];
  delete wrapped;
};
