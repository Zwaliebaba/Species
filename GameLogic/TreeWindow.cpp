#include "pch.h"

#include <time.h>

#include "MathUtils.h"
#include "Preferences.h"
#include "LanguageTable.h"

#include "TreeWindow.h"

#include "Tree.h"

#include "Location.h"
#include "InputField.h"
#include "GlobalWorld.h"
#include "LevelFile.h"
#include "WorldPointers.h"


#ifdef LOCATION_EDITOR

class TreeButton : public SpeciesButton
{
  public:
    enum
    {
      TypeGenerate,
      TypeRandomise,
      TypeClone
    };
    int m_type;

    void MouseUp()
    {
      TreeWindow* tw = (TreeWindow*)m_parent;
      Building* building = g_location->GetBuilding(tw->m_selectionId);
      DEBUG_ASSERT(building && building->m_type == Building::TypeTree);
      Tree* tree = (Tree*)building;

      switch (m_type)
      {
      case TypeGenerate:
        tree->Generate();
        break;

      case TypeRandomise:
        tree->m_seed = (int)frand(99999);
        tree->Generate();
        break;

      case TypeClone:
        Vector3 rayStart;
        Vector3 rayDir;
        g_camera->GetClickRay(g_renderer->ScreenW() / 2, g_renderer->ScreenH() / 2, &rayStart, &rayDir);
        Vector3 _pos;
        g_location->m_landscape.RayHit(rayStart, rayDir, &_pos);

        Building* newBuilding = Building::CreateBuilding(Building::TypeTree);
        newBuilding->Initialise(building);
        newBuilding->SetDetail(g_prefsManager->GetInt("RenderBuildingDetail", 1));
        newBuilding->m_id.SetUniqueId(g_globalWorld->GenerateBuildingId());
        g_location->m_levelFile->m_buildings.push_back(newBuilding);

        speciesSeedRandom(time(nullptr));
        Tree* newTree = (Tree*)newBuilding;
        newTree->m_pos = _pos;
        newTree->m_seed = (int)frand(99999);
        newTree->m_height = tree->m_height * (1.0f + sfrand(0.3f));
        newTree->Generate();
        break;
      }
    }
};


TreeWindow::TreeWindow(char const* _name)
  : SpeciesWindow(_name),
    m_selectionId(-1)
{
}

void TreeWindow::Create()
{
  SpeciesWindow::Create();

  m_selectionId = g_locationEditor->GetSelectionId();
  Building* building = g_location->GetBuilding(m_selectionId);
  DEBUG_ASSERT(building && building->m_type == Building::TypeTree);
  Tree* tree = (Tree*)building;

  int y = 25;
  int h = 18;

  TreeButton* generate = new TreeButton();
  generate->m_type = TreeButton::TypeGenerate;
  generate->SetShortProperties(LANGUAGEPHRASE("editor_generate"), 10, y, m_w - 20);
  RegisterButton(generate);

  TreeButton* randomise = new TreeButton();
  randomise->m_type = TreeButton::TypeRandomise;
  randomise->SetShortProperties(LANGUAGEPHRASE("editor_randomise"), 10, y += h, m_w - 20);
  RegisterButton(randomise);

  TreeButton* clone = new TreeButton();
  clone->m_type = TreeButton::TypeClone;
  clone->SetShortProperties(LANGUAGEPHRASE("editor_clonesimilar"), 10, y += h, m_w - 20);
  RegisterButton(clone);

  CreateColourControl(LANGUAGEPHRASE("editor_branchcolour"), &tree->m_branchColour, y += h, nullptr, 10, m_w - 7);
  CreateColourControl(LANGUAGEPHRASE("editor_leafcolour"), &tree->m_leafColour, y += h, nullptr, 10, m_w - 7);

  CreateValueControl(LANGUAGEPHRASE("editor_height"), &tree->m_height, y += h, 1.0f, 1.0f, 1000.0f);
  CreateValueControl(LANGUAGEPHRASE("editor_budsize"), &tree->m_budsize, y += h, 0.05f, 0.0f, 50.0f, generate);
  CreateValueControl(LANGUAGEPHRASE("editor_pushup"), &tree->m_pushUp, y += h, 0.01f, 0.0f, 5.0f, generate);
  CreateValueControl(LANGUAGEPHRASE("editor_pushout"), &tree->m_pushOut, y += h, 0.01f, 0.0f, 5.0f, generate);
  CreateValueControl(LANGUAGEPHRASE("editor_iterations"), &tree->m_iterations, y += h, 1, 1, 10, generate);
  CreateValueControl(LANGUAGEPHRASE("editor_seed"), &tree->m_seed, y += h, 1, 0, 99999, generate);
  CreateValueControl(LANGUAGEPHRASE("editor_leafdrop"), &tree->m_leafDropRate, y += h, 1, 0, 50);
}

void TreeWindow::Update()
{
  if (!g_locationEditor)
  {
    EclRemoveWindow(m_name);
    return;
  }

  if (g_locationEditor->GetSelectionId() != m_selectionId)
  {
    EclRemoveWindow(m_name);
    return;
  }

  Building* building = g_location->GetBuilding(m_selectionId);
  if (!building || building->m_type != Building::TypeTree)
  {
    EclRemoveWindow(m_name);
  }
}

#endif // LOCATION_EDITOR
