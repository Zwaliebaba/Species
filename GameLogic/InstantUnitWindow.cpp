#include "pch.h"

#include <stdio.h>
#include <string.h>

#include "Debug.h"
#include "LanguageTable.h"

#include "InputField.h"
#include "InstantUnitWindow.h"

#include "LevelFile.h"
#include "Location.h"
#include "Team.h"

#include "Entity.h"
#include "WorldPointers.h"

#ifdef LOCATION_EDITOR


// ****************************************************************************
// Class EditButton
// ****************************************************************************

class EditButton : public SpeciesButton
{
  public:
    EditButton() {}

    void MouseUp()
    {
      if (stricmp(m_name, LANGUAGEPHRASE("editor_move")) == 0)
      {
        g_locationEditor->SetTool(LocationEditorAccess::ToolMove);
      }
    }
};


// ****************************************************************************
// Class TeamButton1
// ****************************************************************************

class TeamButton1 : public SpeciesButton
{
  public:
    int m_teamId;
    TeamButton1(int _teamId)
      : m_teamId(_teamId)
    {
      if (m_teamId == -1)
        m_teamId = -1;
    }

    void MouseUp()
    {
      InstantUnit* iu = g_location->m_levelFile->GetInstantUnit(g_locationEditor->GetSelectionId());
      if (iu)
      {
        iu->m_teamId = m_teamId;
      }
    }

    void Render(int realX, int realY, bool highlighted, bool clicked)
    {
      InstantUnit* iu = g_location->m_levelFile->GetInstantUnit(g_locationEditor->GetSelectionId());
      if (iu)
      {
        if (iu->m_teamId == m_teamId)
        {
          SpeciesButton::Render(realX, realY, true, clicked);
        }
        else
        {
          SpeciesButton::Render(realX, realY, highlighted, clicked);
        }
      }

      if (m_teamId == 255)
      {
        glColor3ub(100, 100, 100);
      }
      else
      {
        RGBAColour col = g_location->m_teams[m_teamId].m_colour;
        glColor3ubv(col.GetData());
      }

      glBegin(GL_QUADS);
      glVertex2i(realX + 30, realY + 4);
      glVertex2i(realX + 40, realY + 4);
      glVertex2i(realX + 40, realY + 12);
      glVertex2i(realX + 30, realY + 12);
      glEnd();
    }
};


// ****************************************************************************
// Class DeleteInstantUnitButton
// ****************************************************************************

class DeleteInstantUnitButton : public SpeciesButton
{
  public:
    bool m_safetyCatch;
    DeleteInstantUnitButton()
      : m_safetyCatch(true)
    {
    }

    void MouseUp()
    {
      if (m_safetyCatch)
      {
        SetCaption(LANGUAGEPHRASE("editor_deletenow"));
        m_safetyCatch = false;
      }
      else
      {
        std::vector<InstantUnit*>& units = g_location->m_levelFile->m_instantUnits;
        const int selectionId = g_locationEditor->GetSelectionId();
        if (selectionId >= 0 && selectionId < static_cast<int>(units.size()))
        {
          delete units[selectionId];
          units.erase(units.begin() + selectionId);
        }
        EclRemoveWindow(LANGUAGEPHRASE("editor_instantuniteditor"));
        g_locationEditor->SetTool(LocationEditorAccess::ToolNone);
        g_locationEditor->SetSelectionId(-1);
      }
    }
};


// ****************************************************************************
// Class InstantUnitEditWindow
// ****************************************************************************

InstantUnitEditWindow::InstantUnitEditWindow(char const* name)
  : SpeciesWindow(name)
{
}


InstantUnitEditWindow::~InstantUnitEditWindow() { g_locationEditor->SetSelectionId(-1); }


void InstantUnitEditWindow::Create()
{
  SpeciesWindow::Create();

  int y = 4;
  int buttonPitch = 18;

  EditButton* mb = new EditButton();
  mb->SetShortProperties(LANGUAGEPHRASE("editor_move"), 10, y += buttonPitch, m_w - 20);
  RegisterButton(mb);

  DeleteInstantUnitButton* db = new DeleteInstantUnitButton();
  db->SetShortProperties(LANGUAGEPHRASE("editor_delete"), 10, y += buttonPitch, m_w - 20);
  RegisterButton(db);

  y += buttonPitch;

  for (int i = 0; i < 3; ++i)
  {
    char name[64];
    int w = m_w / 3 - 8;
    sprintf(name, "T%d", i);
    TeamButton1* tb = new TeamButton1(i);
    tb->SetShortProperties(name, 10 + (i * w) + (i * 2), y, w);
    RegisterButton(tb);
  }

  y += 7;

  InstantUnit* iu = g_location->m_levelFile->GetInstantUnit(g_locationEditor->GetSelectionId());
  CreateValueControl(LANGUAGEPHRASE("editor_numentities"), &iu->m_number, y += buttonPitch, 1, 1, 1000);
  CreateValueControl(LANGUAGEPHRASE("editor_spread"), &iu->m_spread, y += buttonPitch, 1.0f, 0.0f, 1000.0f);
  CreateValueControl(LANGUAGEPHRASE("editor_inunit"), &iu->m_inAUnit, y += buttonPitch, 1, 0, 1);

  y += 7;
}


// ****************************************************************************
// Class CreateButton
// ****************************************************************************

class CreateButton : public SpeciesButton
{
  public:
    CreateButton() {}

    void MouseUp()
    {
      for (int i = 0; i < Entity::NumEntityTypes; ++i)
      {
        if (stricmp(m_name, Entity::GetTypeNameTranslated(i)) == 0)
        {
          g_locationEditor->SetTool(LocationEditorAccess::ToolMove);

          // Where did we click?
          DirectX::XMFLOAT3 rayStart{0.0f, 0.0f, 0.0f};
          DirectX::XMFLOAT3 rayDir{0.0f, 0.0f, 0.0f};
          Vector3 hitPos;
          g_camera->GetClickRay(g_renderer->ScreenW() / 2, g_renderer->ScreenH() / 2, &rayStart, &rayDir);
          g_location->m_landscape.RayHit(rayStart, rayDir, &hitPos);

          // Make sure that any old edit window is removed
          EclWindow* ew = EclGetWindow(LANGUAGEPHRASE("editor_instantuniteditor"));
          if (ew)
          {
            EclRemoveWindow(ew->m_name);
          }

          // Create the new instant unit
          InstantUnit* iu = new InstantUnit();
          iu->m_number = 40;
          iu->m_posX = hitPos.x;
          iu->m_posZ = hitPos.z;
          iu->m_teamId = 0;
          iu->m_type = i;
          iu->m_inAUnit = false;
          g_locationEditor->SetSelectionId(static_cast<int>(g_location->m_levelFile->m_instantUnits.size()));
          g_location->m_levelFile->m_instantUnits.push_back(iu);

          // Create an edit window for the new instant unit
          EclWindow* cw = EclGetWindow(LANGUAGEPHRASE("editor_instantunits"));
          DEBUG_ASSERT(cw);
          // Not `ew` — that name is already an EclWindow* in this scope, holding
          // the old edit window this block just removed.
          auto owned = std::make_unique<InstantUnitEditWindow>(LANGUAGEPHRASE("editor_instantuniteditor"));
          InstantUnitEditWindow* editWindow = owned.get();
          editWindow->m_w = cw->m_w;
          editWindow->m_h = 160;
          editWindow->m_x = cw->m_x;
          EclRegisterWindow(std::move(owned));
          editWindow->m_y = cw->m_y - editWindow->m_h - 10;
        }
      }
    }
};


// ****************************************************************************
// Class InstantUnitCreateWindow
// ****************************************************************************

InstantUnitCreateWindow::InstantUnitCreateWindow(char const* name)
  : SpeciesWindow(name)
{
}


InstantUnitCreateWindow::~InstantUnitCreateWindow()
{
  g_locationEditor->RequestMode(LocationEditorAccess::ModeNone);
  EclRemoveWindow(LANGUAGEPHRASE("editor_instantuniteditor"));
}


void InstantUnitCreateWindow::Create()
{
  SpeciesWindow::Create();

  int y = 3;
  int const buttonYPitch = 18;
  char const* lowerLimit = " ";
  char const* best;

  for (int i = 0; i < Entity::NumEntityTypes; ++i)
  {
    best = "~~~~"; // All reasonable strings come before this when alphabetically sorted
    for (int j = 0; j < Entity::NumEntityTypes; ++j)
    {
      char const* typeName = Entity::GetTypeNameTranslated(j);
      if (stricmp(typeName, lowerLimit) > 0 && stricmp(typeName, best) < 0)
      {
        best = typeName;
      }
    }
    lowerLimit = best;
    CreateButton* button = new CreateButton();
    button->SetShortProperties(best, 10, y += buttonYPitch, m_w - 20);
    RegisterButton(button);
  }
}

#endif // LOCATION_EDITOR
