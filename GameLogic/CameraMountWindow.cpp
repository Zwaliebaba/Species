#include "pch.h"

#include <stdio.h>
#include <string.h>

#include "Random.h"
#include "LanguageTable.h"

#include "CameraMountWindow.h"
#include "CameraAnimWindow.h"
#include "InputField.h"

#include "LevelFile.h"
#include "Location.h"
#include "WorldPointers.h"


#ifdef LOCATION_EDITOR


//*****************************************************************************
// Buttons for CameraMountEditWindow
//*****************************************************************************

class NewMountButton : public SpeciesButton
{
  public:
    void MouseUp()
    {
      CameraMount* mount = new CameraMount();
      mount->m_pos = g_camera->GetPos();
      mount->m_front = g_camera->GetFront();
      mount->m_up = g_camera->GetUp();
      mount->m_name = std::format("blah{}", speciesRandom());

      g_location->m_levelFile->m_cameraMounts.push_back(mount);

      EclWindow* parent = m_parent;
      parent->Remove();
      parent->Create();
    }
};


class GotoMountButton : public SpeciesButton
{
  public:
    // Aliases the mount's own name rather than copying it, exactly as the
    // char* did: InputField edits that string in place, and the lookup below
    // has to keep matching the mount after a rename.
    std::string* m_mountName;

    GotoMountButton(std::string* _mountName)
      : m_mountName(_mountName)
    {
    }

    void MouseUp()
    {
      for (int i = 0; i < static_cast<int>(g_location->m_levelFile->m_cameraMounts.size()); ++i)
      {
        CameraMount* mount = g_location->m_levelFile->m_cameraMounts[i];
        if (stricmp(mount->m_name.c_str(), m_mountName->c_str()) == 0)
        {
          g_camera->SetTarget(mount->m_pos, mount->m_front, mount->m_up);
          g_camera->CutToTarget();
          return;
        }
      }

      DEBUG_ASSERT(false);
    }
};


class DeleteMountButton : public SpeciesButton
{
  public:
    // Aliases the mount's own name rather than copying it, exactly as the
    // char* did: InputField edits that string in place, and the lookup below
    // has to keep matching the mount after a rename.
    std::string* m_mountName;

    DeleteMountButton(std::string* _mountName)
      : m_mountName(_mountName)
    {
    }

    void MouseUp()
    {
      for (int i = 0; i < static_cast<int>(g_location->m_levelFile->m_cameraMounts.size()); ++i)
      {
        CameraMount* mount = g_location->m_levelFile->m_cameraMounts[i];
        if (stricmp(mount->m_name.c_str(), m_mountName->c_str()) == 0)
        {
          g_location->m_levelFile->m_cameraMounts.erase(g_location->m_levelFile->m_cameraMounts.begin() + i);
          delete mount;

          EclWindow* parent = m_parent;
          parent->Remove();
          parent->Create();

          return;
        }
      }

      DEBUG_ASSERT(false);
    }
};


class UpdateMountButton : public SpeciesButton
{
  public:
    // Aliases the mount's own name rather than copying it, exactly as the
    // char* did: InputField edits that string in place, and the lookup below
    // has to keep matching the mount after a rename.
    std::string* m_mountName;

    UpdateMountButton(std::string* _mountName)
      : m_mountName(_mountName)
    {
    }

    void MouseUp()
    {
      for (int i = 0; i < static_cast<int>(g_location->m_levelFile->m_cameraMounts.size()); ++i)
      {
        CameraMount* mount = g_location->m_levelFile->m_cameraMounts[i];
        if (stricmp(mount->m_name.c_str(), m_mountName->c_str()) == 0)
        {
          mount->m_pos = g_camera->GetPos();
          mount->m_front = g_camera->GetFront();
          mount->m_up = g_camera->GetUp();

          return;
        }
      }

      DEBUG_ASSERT(false);
    }
};


// ****************************************************************************
// Class CameraMountEditWindow
// ****************************************************************************

CameraMountEditWindow::CameraMountEditWindow(char const* name)
  : SpeciesWindow(name)
{
}


CameraMountEditWindow::~CameraMountEditWindow()
{
  EclRemoveWindow(LANGUAGEPHRASE("editor_cameraanims"));
  EclRemoveWindow(LANGUAGEPHRASE("editor_cameraanim"));
  g_locationEditor->RequestMode(LocationEditorAccess::ModeNone);
}


void CameraMountEditWindow::Create()
{
  SpeciesWindow::Create();

  int height = 5;
  int pitch = 17;

  NewMountButton* generate = new NewMountButton();
  generate->SetShortProperties(LANGUAGEPHRASE("editor_createnewmount"), 10, height += pitch);
  RegisterButton(generate);

  height += 10;

  for (int i = 0; i < static_cast<int>(g_location->m_levelFile->m_cameraMounts.size()); ++i)
  {
    CameraMount* mount = g_location->m_levelFile->m_cameraMounts[i];

    // The button label was formatted into a char[64] from a label plus a name
    // of up to 63 characters, so a long mount name ran off the end of it
    // before ever reaching the 256-byte button name. CopyInto bounds it at the
    // destination, which is what the copy always meant.
    InputField* button = new InputField();
    button->SetShortProperties(LANGUAGEPHRASE("dialog_name"), 10, height += pitch, 150);
    CopyInto(button->m_name, std::format("{}:{}", LANGUAGEPHRASE("dialog_name"), mount->m_name));
    button->RegisterString(&mount->m_name);
    RegisterButton(button);

    SpeciesButton* delButton = new DeleteMountButton(&mount->m_name);
    delButton->SetShortProperties(LANGUAGEPHRASE("editor_del"), 170, height);
    CopyInto(delButton->m_name, std::format("{}:{}", LANGUAGEPHRASE("dialog_delete"), mount->m_name));
    RegisterButton(delButton);

    SpeciesButton* gotoButton = new GotoMountButton(&mount->m_name);
    gotoButton->SetShortProperties(LANGUAGEPHRASE("editor_goto"), 210, height);
    CopyInto(gotoButton->m_name, std::format("{}:{}", LANGUAGEPHRASE("editor_goto"), mount->m_name));
    RegisterButton(gotoButton);

    SpeciesButton* updateButton = new UpdateMountButton(&mount->m_name);
    updateButton->SetShortProperties(LANGUAGEPHRASE("editor_update"), 256, height);
    CopyInto(updateButton->m_name, std::format("{}:{}", LANGUAGEPHRASE("editor_update"), mount->m_name));
    RegisterButton(updateButton);
  }
}


#endif // LOCATION_EDITOR
