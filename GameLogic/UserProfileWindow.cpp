#include "pch.h"
#include "FilesysUtils.h"
#include "TextRenderer.h"
#include "LanguageTable.h"

#include "UserProfileWindow.h"

#include "Server.h"

#include "InputField.h"
#include "WorldPointers.h"
#include "AppState.h"
#include "AppCommands.h"


class LoadUserProfileButton : public SpeciesButton
{
  public:
    char* m_profileName;
    void MouseUp()
    {
      g_appCommands->SetProfileName(m_profileName);
      g_appCommands->LoadProfile();
      EclRemoveWindow(m_parent->m_name);
      EclRemoveWindow(LANGUAGEPHRASE("dialog_mainmenu"));
    }
};


class NewProfileWindowButton : public SpeciesButton
{
  public:
    void MouseUp() { EclRegisterWindow(std::make_unique<NewUserProfileWindow>(), m_parent); }
};


UserProfileWindow::UserProfileWindow()
  : SpeciesWindow(LANGUAGEPHRASE("dialog_profile"))
{
}


void UserProfileWindow::Render(bool hasFocus)
{
  SpeciesWindow::Render(hasFocus);

  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
  g_editorFont.DrawText2DCentre(m_x + m_w / 2, m_y + GetMenuSize(30), GetMenuSize(12), LANGUAGEPHRASE("dialog_currentprofilename"));
  g_editorFont.DrawText2DCentre(m_x + m_w / 2, m_y + GetMenuSize(45), GetMenuSize(16), g_userProfileName.c_str());
}


void UserProfileWindow::Create()
{
  std::string const profileDir = std::format("{}users/*.*", g_appCommands->ProfileDirectory());
  std::vector<char*>* profileList = ListSubDirectoryNames(profileDir.c_str());
  int numProfiles = static_cast<int>(profileList->size());

  int windowH = 150 + numProfiles * 30;
  SetMenuSize(300, windowH);
  SetPosition(g_renderer->ScreenW() / 2 - m_w / 2, g_renderer->ScreenH() / 2 - m_h / 2);

  SpeciesWindow::Create();

  //
  // New profile

  int y = GetMenuSize(50);
  int h = GetMenuSize(30);

  int invertY = y + GetMenuSize(20);

  //
  // Inverted box

  if (numProfiles > 0)
  {
    InvertedBox* box = new InvertedBox();
    box->SetShortProperties("Box", 10, invertY, m_w - 20, m_h - invertY - GetMenuSize(60));
    RegisterButton(box);
  }

  //
  // Load existing profile button

  for (char* profileName : *profileList)
  {
    std::string const caption = std::format("{}: '{}'", LANGUAGEPHRASE("dialog_loadprofile"), profileName);
    LoadUserProfileButton* button = new LoadUserProfileButton();
    button->SetShortProperties(caption.c_str(), 20, y += h, m_w - 40, GetMenuSize(20));
    button->m_profileName = profileName;
    button->m_fontSize = GetMenuSize(11);
    button->m_centered = true;
    RegisterButton(button);
    m_buttonOrder.push_back(button);
  }

  // Only the list. Each name is now owned by the button that took it, and
  // ListSubDirectoryNames strdup'd them, so whoever frees them must use
  // free() rather than delete[].
  delete profileList;


  NewProfileWindowButton* newProfile = new NewProfileWindowButton();
  newProfile->SetShortProperties(LANGUAGEPHRASE("dialog_newprofile"), 10, m_h - GetMenuSize(55), m_w - 20, GetMenuSize(20));
  newProfile->m_fontSize = GetMenuSize(13);
  newProfile->m_centered = true;
  RegisterButton(newProfile);
  m_buttonOrder.push_back(newProfile);

  CloseButton* cancel = new CloseButton();
  cancel->SetShortProperties(LANGUAGEPHRASE("dialog_close"), 10, m_h - GetMenuSize(30), m_w - 20, GetMenuSize(20));
  cancel->m_fontSize = GetMenuSize(13);
  cancel->m_centered = true;
  RegisterButton(cancel);
  m_buttonOrder.push_back(cancel);
}


// ============================================================================


class NewProfileButton : public SpeciesButton
{
    void MouseUp()
    {
      NewUserProfileWindow* parent = (NewUserProfileWindow*)m_parent;
      g_appCommands->SetProfileName(parent->s_profileName.c_str());
      g_appCommands->LoadProfile();
      EclRemoveWindow(m_parent->m_name);
      EclRemoveWindow(LANGUAGEPHRASE("dialog_newprofile"));
      EclRemoveWindow(LANGUAGEPHRASE("dialog_mainmenu"));
    }
};

std::string NewUserProfileWindow::s_profileName = "NewUser";


NewUserProfileWindow::NewUserProfileWindow()
  : SpeciesWindow(LANGUAGEPHRASE("dialog_newprofile"))
{
}


void NewUserProfileWindow::Create()
{
  SetMenuSize(300, 110);
  SetPosition(g_renderer->ScreenW() / 2 - m_w / 2, g_renderer->ScreenH() / 2 - m_h / 2);

  SpeciesWindow::Create();

  InvertedBox* box = new InvertedBox();
  box->SetShortProperties("box", 10, GetMenuSize(30), m_w - 20, GetMenuSize(40));
  RegisterButton(box);

  CreateValueControl(LANGUAGEPHRASE("dialog_name"), &s_profileName, GetMenuSize(40), 0, 0, 0, nullptr, 20, m_w - 40);

  int y = m_h - GetMenuSize(30);

  CloseButton* close = new CloseButton();
  close->SetShortProperties(LANGUAGEPHRASE("dialog_cancel"), 10, y, m_w / 2 - 15, GetMenuSize(20));
  close->m_fontSize = GetMenuSize(12);
  RegisterButton(close);

  NewProfileButton* newProfile = new NewProfileButton();
  newProfile->SetShortProperties(LANGUAGEPHRASE("dialog_create"), close->m_x + close->m_w + 10, y, m_w / 2 - 15, GetMenuSize(20));
  newProfile->m_fontSize = GetMenuSize(12);
  RegisterButton(newProfile);
}
