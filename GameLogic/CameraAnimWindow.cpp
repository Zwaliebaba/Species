#include "pch.h"

#include <format>

#include "StringUtils.h"

#include <stdio.h>
#include <string.h>

#include "Random.h"
#include "LanguageTable.h"

#include "CameraAnimWindow.h"
#include "CameraMountWindow.h"
#include "DropDownMenu.h"
#include "InputField.h"

#include "LevelFile.h"
#include "Location.h"
#include "GameTime.h"
#include "WorldPointers.h"


#ifdef LOCATION_EDITOR


//*****************************************************************************
// Buttons for Main Window
//*****************************************************************************


namespace Species
{
  class NewAnimButton : public SpeciesButton
  {
    public:
      void MouseUp()
      {
        CameraAnimation* anim = new CameraAnimation;
        anim->m_name = std::format("CamAnim{}", speciesRandom() & 0x3ff);
        g_location->m_levelFile->m_cameraAnimations.push_back(std::unique_ptr<CameraAnimation>(anim));

        CameraAnimMainEditWindow* parent = (CameraAnimMainEditWindow*)m_parent;
        parent->RemoveButtons();
        parent->AddButtons();
      }
  };


  class DeleteAnimButton : public SpeciesButton
  {
    public:
      void MouseUp()
      {
        auto* anims = &g_location->m_levelFile->m_cameraAnimations;
        for (int i = 0; i < static_cast<int>(anims->size()); ++i)
        {
          if (StrEqualsIgnoreCase((*anims)[i]->m_name, m_name))
          {
            // The erase destroys it.
            anims->erase(anims->begin() + i);
            break;
          }
        }

        CameraAnimMainEditWindow* parent = (CameraAnimMainEditWindow*)m_parent;
        parent->RemoveButtons();
        parent->AddButtons();
      }
  };


  class SelectAnimButton : public SpeciesButton
  {
    public:
      void MouseUp()
      {
        EclRemoveWindow(LANGUAGEPHRASE("editor_cameraanim"));

        // The button carries the animation's name behind a "select:" prefix,
        // and this used to step past it with pointer arithmetic on the char
        // buffer. m_name is a std::string now.
        std::string const animName = m_name.substr(strlen("select:"));
        int animId = g_location->m_levelFile->GetCameraAnimId(animName.c_str());
        g_locationEditor->SetSelectionId(animId);

        //		EclWindow *secondaryWin = new CameraAnimSecondaryEditWindow(
        //										LANGUAGEPHRASE("editor_cameraanim"),
        //										animId);
        //		EclRegisterWindow(secondaryWin);
      }
  };


  // ****************************************************************************
  // Main Edit Window Class
  // ****************************************************************************

  CameraAnimMainEditWindow::CameraAnimMainEditWindow(char const* name)
    : SpeciesWindow(name)
  {
  }


  CameraAnimMainEditWindow::~CameraAnimMainEditWindow()
  {
    EclRemoveWindow(LANGUAGEPHRASE("editor_cameramounts"));
    EclRemoveWindow(LANGUAGEPHRASE("editor_cameraanim"));
    g_locationEditor->RequestMode(LocationEditorAccess::ModeNone);
  }


  void CameraAnimMainEditWindow::Create()
  {
    SpeciesWindow::Create();
    AddButtons();
  }


  void CameraAnimMainEditWindow::AddButtons()
  {
    int height = 5;
    int pitch = 17;

    NewAnimButton* generate = new NewAnimButton();
    generate->SetShortProperties("Create New Anim", 10, height += pitch);
    RegisterButton(generate);

    height += 10;

    for (auto const& anim : g_location->m_levelFile->m_cameraAnimations)
    {
      // The label was built in a char[64] from a prefix plus a name of up to 63
      // characters, which overran it before ever reaching the button name; then
      // CopyInto bounded it at the 256-byte destination. Neither bound exists
      // now — the button name is a std::string, so the label is simply
      // assigned. strings-modernised T11.
      InputField* button = new InputField();
      button->SetShortProperties("Name:", 10, height += pitch, 150);
      button->m_name = std::format("name:{}", anim->m_name);
      button->RegisterString(&anim->m_name);
      RegisterButton(button);

      // Unlike the camera-mount window's buttons, this one holds a COPY of the
      // name in its own m_name and looks the animation up by it. A rename
      // therefore unbinds it until the window is rebuilt, which is what it did
      // before and is not this task's to change.
      SpeciesButton* delButton = new DeleteAnimButton();
      delButton->SetShortProperties("Del", 170, height);
      delButton->m_name = anim->m_name;
      RegisterButton(delButton);

      SpeciesButton* selectButton = new SelectAnimButton();
      selectButton->SetShortProperties("Select", 210, height);
      selectButton->m_name = std::format("select:{}", anim->m_name);
      RegisterButton(selectButton);
    }
  }


  void CameraAnimMainEditWindow::RemoveButtons()
  {
    while (m_buttons.size())
    {
      RemoveButton(m_buttons[0]->m_name);
    }
  }


  //*****************************************************************************
  // Buttons for Secondary Window
  //*****************************************************************************


  class NewNodeButton : public SpeciesButton
  {
    public:
      void MouseUp()
      {
        CameraAnimSecondaryEditWindow* parent = (CameraAnimSecondaryEditWindow*)m_parent;
        parent->m_newNodeArmed = !parent->m_newNodeArmed;
      }

      void Render(int x, int y, bool _hasFocus, bool _clicked)
      {
        CameraAnimSecondaryEditWindow* parent = (CameraAnimSecondaryEditWindow*)m_parent;
        int halfSecond = (int)(g_gameTime * 2.0f) & 1;
        if (parent->m_newNodeArmed && halfSecond)
        {
          SpeciesButton::Render(x, y, true, false);
        }
        else
        {
          SpeciesButton::Render(x, y, false, false);
        }
      }
  };


  class CamBeforeMountButton : public SpeciesButton
  {
    public:
      void MouseUp()
      {
        CameraAnimSecondaryEditWindow* parent = (CameraAnimSecondaryEditWindow*)m_parent;
        if (parent->m_newNodeArmed)
        {
          CameraAnimation* anim = g_location->m_levelFile->GetCameraAnim(parent->m_animId);
          DEBUG_ASSERT(anim);

          auto node = std::make_unique<CamAnimNode>();
          node->m_mountName = strdup(MAGIC_MOUNT_NAME_START_POS);

          anim->m_nodes.push_back(std::move(node));

          parent->m_newNodeArmed = false;
          parent->RemoveButtons();
          parent->AddButtons();
        }
      }
  };


  class StartPreviewButton : public SpeciesButton
  {
    public:
      void MouseUp()
      {
        CameraAnimSecondaryEditWindow* parent = (CameraAnimSecondaryEditWindow*)m_parent;
        CameraAnimation* anim = g_location->m_levelFile->GetCameraAnim(parent->m_animId);
        g_camera->PlayAnimation(anim);
      }
  };


  class StopPreviewButton : public SpeciesButton
  {
    public:
      void MouseUp() { g_camera->StopAnimation(); }
  };


  class SelectMountButton : public SpeciesButton
  {
    public:
      void Render(int x, int y, bool _hasFocus, bool _clicked) { SpeciesButton::Render(x, y, false, false); }
  };


  class DeleteNodeButton : public SpeciesButton
  {
    public:
      void MouseUp()
      {
        CameraAnimSecondaryEditWindow* parent = (CameraAnimSecondaryEditWindow*)m_parent;
        CameraAnimation* anim = g_location->m_levelFile->GetCameraAnim(parent->m_animId);
        DEBUG_ASSERT(anim);

        // Seven characters past the "delete:" prefix, as before.
        std::string const mountName = m_name.substr(7);
        for (int i = 0; i < static_cast<int>(anim->m_nodes.size()); ++i)
        {
          if (stricmp(anim->m_nodes[i]->m_mountName, mountName.c_str()) == 0)
          {
            anim->m_nodes.erase(anim->m_nodes.begin() + i);
            break;
          }
        }

        parent->RemoveButtons();
        parent->AddButtons();
      }
  };


  // ****************************************************************************
  // Secondary Edit Window Class
  // ****************************************************************************

  CameraAnimSecondaryEditWindow::CameraAnimSecondaryEditWindow(char* name, int _animId)
    : SpeciesWindow(name),
      m_animId(_animId),
      m_newNodeArmed(false)
  {
    m_w = 310;
    m_h = 100;
    m_x = g_renderer->ScreenW() - m_w;
    m_y = g_renderer->ScreenH() - m_h;
  }


  CameraAnimSecondaryEditWindow::~CameraAnimSecondaryEditWindow() { g_locationEditor->SetSelectionId(-1); }


  void CameraAnimSecondaryEditWindow::Create()
  {
    SpeciesWindow::Create();
    AddButtons();
  }


  void CameraAnimSecondaryEditWindow::AddButtons()
  {
    NewNodeButton* newLinkButton = new NewNodeButton();
    newLinkButton->SetShortProperties("Add link", 10, 22);
    RegisterButton(newLinkButton);

    CamBeforeMountButton* camBeforeMountButton = new CamBeforeMountButton;
    camBeforeMountButton->SetShortProperties("CamStart", 85, 22);
    RegisterButton(camBeforeMountButton);

    StartPreviewButton* startPreviewButton = new StartPreviewButton;
    startPreviewButton->SetShortProperties("Preview", m_w - 112, 22);
    RegisterButton(startPreviewButton);

    StopPreviewButton* stopPreviewButton = new StopPreviewButton;
    stopPreviewButton->SetShortProperties("Stop", m_w - 46, 22);
    RegisterButton(stopPreviewButton);


    //
    // Create new buttons for each mount

    int height = 49;
    int pitch = 17;
    CameraAnimation* anim = g_location->m_levelFile->GetCameraAnim(m_animId);
    if (anim)
    {
      for (int j = 0; j < static_cast<int>(anim->m_nodes.size()); ++j)
      {
        CamAnimNode* node = anim->m_nodes[j].get();

        int x = 10;

        DropDownMenu* modeBut = new DropDownMenu();
        modeBut->SetShortProperties("Mode", x, height, 60);
        // Options are added in enumerator order and AddOption numbers them from
        // zero, so an option's value IS its Transition. RegisterEnum relies on
        // that, and so did RegisterInt before it.
        for (int i = 0; i < static_cast<int>(Neuron::I(CamAnimNode::Transition::TransitionNumModes)); ++i)
        {
          modeBut->AddOption(CamAnimNode::GetTransitModeName(static_cast<CamAnimNode::Transition>(i)));
        }
        modeBut->RegisterEnum(&node->m_transitionMode);
        x += 70;
        modeBut->m_name = std::format("mode:{}", node->m_mountName);
        RegisterButton(modeBut);

        CreateValueControl(node->m_mountName, &node->m_duration, height, 0.5f, 0.1f, 100.0f, nullptr, x, 90);
        EclButton* b = GetButton(node->m_mountName);
        b->m_name = std::format("duration:{}", node->m_mountName);
        b->m_caption.clear();
        x += 100;

        SpeciesButton* but;

        but = new SelectMountButton();
        but->SetShortProperties(node->m_mountName, x, height, 80);
        x += 90;
        but->m_name = std::format("mount:{}", node->m_mountName);
        RegisterButton(but);

        but = new DeleteNodeButton();
        but->SetShortProperties("Del", x, height, 30);
        but->m_name = std::format("delete:{}", node->m_mountName);
        RegisterButton(but);

        height += pitch;
      }
    }

    if (height > m_h)
    {
      SetSize(m_w, height);
    }
  }


  void CameraAnimSecondaryEditWindow::RemoveButtons()
  {
    for (int i = 0; i < m_buttons.size(); ++i)
    {
      EclButton* but = m_buttons[i];
      if (!StrEqualsIgnoreCase(but->m_name, "Close"))
      {
        RemoveButton(m_buttons[i]->m_name);
        --i;
      }
    }
  }


#endif // LOCATION_EDITOR
} // namespace Species
