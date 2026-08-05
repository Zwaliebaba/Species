#include "pch.h"
#include "Resource.h"
#include "LanguageTable.h"

#include "InputField.h"
#include "GlobalWorldEditorWindow.h"
#include "MessageDialog.h"

#include "GlobalWorld.h"
#include "LevelFile.h"
#include "WorldPointers.h"
#include "AppState.h"


static std::string s_locationName = "NewLevel";


class SetModeButton : public SpeciesButton
{
  public:
    int m_mode;
    void MouseUp() { g_globalWorld->m_editorMode = m_mode; }

    void Render(int realX, int realY, bool highlighted, bool clicked)
    {
      if (g_globalWorld->m_editorMode == m_mode)
      {
        SpeciesButton::Render(realX, realY, true, clicked);
      }
      else
      {
        SpeciesButton::Render(realX, realY, highlighted, clicked);
      }
    }
};


class NewLocationButton : public SpeciesButton
{
    void MouseUp()
    {
      //
      // If a MOD hasn't been set, don't allow this to happen
      // as it will try to save into darwinia/data/levels, which is clearly wrong
      // for the end user (but allow it for us)

#ifndef TARGET_DEBUG
      if (!g_resource->IsModLoaded())
      {
        EclRegisterWindow(std::make_unique<MessageDialog>(LANGUAGEPHRASE("dialog_newlocationfail1"), LANGUAGEPHRASE("dialog_newlocationfail2")),
                          m_parent);
        return;
      }
#endif

      //
      // Create the map and mission files

      LevelFile levelFile;
      CopyInto(levelFile.m_mapFilename, std::format("Map{}.txt", s_locationName));
      CopyInto(levelFile.m_missionFilename, std::format("Mission{}.txt", s_locationName));
      strlwr(levelFile.m_mapFilename);
      strlwr(levelFile.m_missionFilename);

      levelFile.Save();

      //
      // Create new global location

      GlobalLocation* loc = new GlobalLocation();
      CopyInto(loc->m_mapFilename, std::format("Map{}.txt", s_locationName));
      CopyInto(loc->m_missionFilename, std::format("Mission{}.txt", s_locationName));
      strlwr(loc->m_mapFilename);
      strlwr(loc->m_missionFilename);
      CopyInto(loc->m_name, s_locationName);
      loc->m_available = true;
      loc->m_pos = DirectX::XMFLOAT3(-96.25f, -274.02f, 75.16f);
      g_globalWorld->AddLocation(loc);

      //
      // Save game.txt and locations.txt

      g_globalWorld->SaveGame("Game.txt");
      g_globalWorld->SaveLocations("Locations.txt");
    }
};


class SaveLocationsButton : public SpeciesButton
{
    void MouseUp()
    {
      //
      // If a MOD hasn't been set, don't allow this to happen
      // as it will try to save into darwinia/data/levels, which is clearly wrong
      // for the end user (but allow it for us)

#ifndef TARGET_DEBUG
      if (!g_resource->IsModLoaded())
      {
        EclRegisterWindow(std::make_unique<MessageDialog>(LANGUAGEPHRASE("dialog_savelocationsfail1"), LANGUAGEPHRASE("dialog_savelocationsfail2")),
                          m_parent);
        return;
      }
#endif

      //
      // It's ok to save

      g_globalWorld->SaveLocations("Locations.txt");
    }
};


GlobalWorldEditorWindow::GlobalWorldEditorWindow()
  : SpeciesWindow(LANGUAGEPHRASE("editor_globalworldeditor"))
{
  SetPosition(20, 20);
  SetSize(200, 100);
}


void GlobalWorldEditorWindow::Create()
{
  SpeciesWindow::Create();

  int y = 20;
  int h = 18;

  SetModeButton* setEdit = new SetModeButton();
  setEdit->m_mode = 0;
  setEdit->SetShortProperties(LANGUAGEPHRASE("editor_editlocations"), 10, y += h, m_w - 20);
  RegisterButton(setEdit);

  SetModeButton* setMove = new SetModeButton();
  setMove->m_mode = 1;
  setMove->SetShortProperties(LANGUAGEPHRASE("editor_movelocations"), 10, y += h, m_w - 20);
  RegisterButton(setMove);

  y += h;

  NewLocationButton* newLoc = new NewLocationButton();
  newLoc->SetShortProperties(LANGUAGEPHRASE("editor_createnewlocation"), 10, y += h, m_w - 20);
  RegisterButton(newLoc);

  CreateValueControl(LANGUAGEPHRASE("dialog_name"), &s_locationName, y += h, 0, 0, 0, nullptr, 10, m_w - 20);

  y += h;

  SaveLocationsButton* save = new SaveLocationsButton();
  save->SetShortProperties(LANGUAGEPHRASE("editor_savelocations"), 10, y += h, m_w - 20);
  RegisterButton(save);
}


void GlobalWorldEditorWindow::Update()
{
  if (!g_editing || g_location)
  {
    EclRemoveWindow(m_name);
  }
}