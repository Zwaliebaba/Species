#include "pch.h"
#include "Debug.h"
#include "Vector3.h"
#include "TextRenderer.h"
#include "MathUtils.h"
#include "Preferences.h"
#include "LanguageTable.h"

#include "BuildingsWindow.h"
#include "DropDownMenu.h"
#include "InputField.h"

#include "Factory.h"
#include "TrunkPort.h"
#include "LaserFence.h"
#include "AntHill.h"
#include "SafeArea.h"
#include "Mine.h"
#include "Generator.h"
#include "ResearchItem.h"
#include "Triffid.h"
#include "BlueprintStore.h"
#include "Ai.h"
#include "SpawnPoint.h"
#include "ScriptTrigger.h"
#include "StaticShape.h"
#include "Incubator.h"
#include "Rocket.h"
#include "GenericHub.h"
#include "Switch.h"

#include "GlobalWorld.h"
#include "LevelFile.h"
#include "Location.h"
#include "Team.h"
#include "WorldPointers.h"


#ifdef LOCATION_EDITOR

// ****************************************************************************
// Class ToolButton
// ****************************************************************************

class ToolButton : public SpeciesButton
{
  public:
    int m_toolType;

    ToolButton(int _toolType)
      : m_toolType(_toolType)
    {
    }

    void MouseUp() { g_locationEditor->SetTool(m_toolType); }

    void Render(int realX, int realY, bool highlighted, bool clicked)
    {
      if (g_locationEditor->GetTool() == m_toolType)
      {
        SpeciesButton::Render(realX, realY, highlighted, true);
      }
      else
      {
        SpeciesButton::Render(realX, realY, highlighted, clicked);
      }

      if (m_toolType == LocationEditorAccess::ToolLink)
      {
        Building* b = g_location->GetBuilding(g_locationEditor->GetSelectionId());
        g_editorFont.DrawText2DRight(realX + m_w - 10, realY + 10, 14, "%d", b->GetBuildingLink());
      }
    }
};


// ****************************************************************************
// Class DeleteBuildingButton
// ****************************************************************************

class DeleteBuildingButton : public SpeciesButton
{
  public:
    bool m_safetyCatch;
    DeleteBuildingButton()
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
        g_location->m_levelFile->RemoveBuilding(g_locationEditor->GetSelectionId());
        EclRemoveWindow(LANGUAGEPHRASE("editor_buildingid"));
        g_locationEditor->SetTool(LocationEditorAccess::ToolNone);
        g_locationEditor->SetSelectionId(-1);
      }
    }
};


// ****************************************************************************
// Class TeamButton
// ****************************************************************************

class TeamButton : public SpeciesButton
{
  public:
    int m_teamId;
    TeamButton(int _teamId)
      : m_teamId(_teamId)
    {
      if (m_teamId == -1)
        m_teamId = 255;
    }

    void MouseUp()
    {
      Building* b = g_location->GetBuilding(g_locationEditor->GetSelectionId());
      if (b)
      {
        b->m_id.SetTeamId(m_teamId);
      }
    }

    void Render(int realX, int realY, bool highlighted, bool clicked)
    {
      Building* b = g_location->GetBuilding(g_locationEditor->GetSelectionId());
      if (b)
      {
        if (b->m_id.GetTeamId() == m_teamId)
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
// Class IsGlobalButton
// ****************************************************************************

class IsGlobalButton : public SpeciesButton
{
  public:
    void Render(int realX, int realY, bool highlighted, bool clicked)
    {
      SpeciesButton::Render(realX, realY, highlighted, clicked);
      Building* b = g_location->GetBuilding(g_locationEditor->GetSelectionId());
      if (b)
      {
        g_editorFont.DrawText2DRight(realX + m_w - 10, realY + 10, DEF_FONT_SIZE, "%d", b->m_isGlobal);
      }
    };

    void MouseUp()
    {
      Building* b = g_location->GetBuilding(g_locationEditor->GetSelectionId());
      if (b)
      {
        b->m_isGlobal = !b->m_isGlobal;
      }
    }
};


// ****************************************************************************
// Class CloneBuildingButton
// ****************************************************************************

class CloneBuildingButton : public SpeciesButton
{
    void MouseUp()
    {
      DirectX::XMFLOAT3 rayStart{0.0f, 0.0f, 0.0f};
      DirectX::XMFLOAT3 rayDir{0.0f, 0.0f, 0.0f};
      g_camera->GetClickRay(g_renderer->ScreenW() / 2, g_renderer->ScreenH() / 2, &rayStart, &rayDir);
      // Landscape::RayHit keeps its Vector3 out-pointer until
      // directxmath-migration T28, and the seam does not reach through a
      // pointer, so &AsLegacy is what writes native storage from it.
      DirectX::XMFLOAT3 _pos{0.0f, 0.0f, 0.0f};
      g_location->m_landscape.RayHit(rayStart, rayDir, &AsLegacy(_pos));

      Building* building = g_location->GetBuilding(g_locationEditor->GetSelectionId());
      DEBUG_ASSERT(building);

      Building* newBuilding = Building::CreateBuilding(building->m_type);
      newBuilding->Initialise(building);
      newBuilding->SetDetail(g_prefsManager->GetInt("RenderBuildingDetail", 1));
      newBuilding->m_id.SetUniqueId(g_globalWorld->GenerateBuildingId());
      newBuilding->m_pos = _pos;
      g_location->m_levelFile->m_buildings.push_back(newBuilding);
    }
};


// ****************************************************************************
// Class BuildingEditWindow
// ****************************************************************************

BuildingEditWindow::BuildingEditWindow(char const* name)
  : SpeciesWindow(name)
{
}


BuildingEditWindow::~BuildingEditWindow() { g_locationEditor->SetSelectionId(-1); }

void BuildingEditWindow::Create()
{
  SpeciesWindow::Create();

  Building* building = g_location->GetBuilding(g_locationEditor->GetSelectionId());
  DEBUG_ASSERT(building);

  int buttonPitch = 18;
  int y = 6;

  ToolButton* mb = new ToolButton(LocationEditorAccess::ToolMove);
  mb->SetShortProperties(LANGUAGEPHRASE("editor_move"), 10, y += buttonPitch, m_w - 20);
  RegisterButton(mb);

  ToolButton* rb = new ToolButton(LocationEditorAccess::ToolRotate);
  rb->SetShortProperties(LANGUAGEPHRASE("editor_rotate"), 10, y += buttonPitch, m_w - 20);
  RegisterButton(rb);

  CloneBuildingButton* clone = new CloneBuildingButton();
  clone->SetShortProperties(LANGUAGEPHRASE("editor_clone"), 10, y += buttonPitch, m_w - 20);
  RegisterButton(clone);

  DeleteBuildingButton* db = new DeleteBuildingButton();
  db->SetShortProperties(LANGUAGEPHRASE("editor_delete"), 10, y += buttonPitch, m_w - 20);
  RegisterButton(db);

  ToolButton* lb = new ToolButton(LocationEditorAccess::ToolLink);
  lb->SetShortProperties(LANGUAGEPHRASE("editor_link"), 10, y += buttonPitch, m_w - 20);
  RegisterButton(lb);

  y += buttonPitch;

  for (int i = -1; i < 3; ++i)
  {
    int w = m_w / 4 - 5;
    std::string const name = std::format("T{}", i);
    TeamButton* tb = new TeamButton(i);
    tb->SetShortProperties(name.c_str(), 61 + (float)i * ((float)w + 1.0f), y, w - 2);
    RegisterButton(tb);
  }

  CreateValueControl(LANGUAGEPHRASE("editor_dynamic"), &building->m_dynamic, y += buttonPitch, 1.0f, 0, 1);

  CreateValueControl(LANGUAGEPHRASE("editor_isglobal"), &building->m_isGlobal, y += buttonPitch, 1.0f, 0, 1);

  if (building->m_type == Building::TypeFactory)
  {
    InputField* intExtra = new InputField();
    intExtra->SetShortProperties(LANGUAGEPHRASE("editor_spirits"), 10, y += buttonPitch, m_w - 20);
    Factory* factory = (Factory*)building;
    intExtra->RegisterInt(&factory->m_initialCapacity);
    RegisterButton(intExtra);
  }
  else if (building->m_type == Building::TypeTrunkPort)
  {
    InputField* inExtra = new InputField();
    inExtra->SetShortProperties(LANGUAGEPHRASE("editor_targetlocation"), 10, y += buttonPitch, m_w - 20);
    TrunkPort* port = (TrunkPort*)building;
    inExtra->RegisterInt(&port->m_targetLocationId);
    RegisterButton(inExtra);
  }
  else if (building->m_type == Building::TypeLaserFence)
  {
    LaserFence* fence = (LaserFence*)building;
    CreateValueControl(LANGUAGEPHRASE("editor_scale"), &fence->m_scale, y += buttonPitch, 0.01f, 0.0f, 100.0f);

    DropDownMenu* menu = new DropDownMenu(true);
    menu->SetShortProperties(LANGUAGEPHRASE("editor_mode"), 10, y += buttonPitch, m_w - 20);
    menu->AddOption(LANGUAGEPHRASE("editor_disabled"));
    menu->AddOption(LANGUAGEPHRASE("editor_enabling"));
    menu->AddOption(LANGUAGEPHRASE("editor_enabled"));
    menu->AddOption(LANGUAGEPHRASE("editor_disabling"));
    menu->AddOption(LANGUAGEPHRASE("editor_neveron"));
    menu->RegisterInt(&fence->m_mode);
    RegisterButton(menu);
  }
  else if (building->m_type == Building::TypeAntHill)
  {
    AntHill* antHill = (AntHill*)building;
    CreateValueControl(LANGUAGEPHRASE("editor_numants"), &antHill->m_numAntsInside, y += buttonPitch, 1, 0, 1000);
  }
  else if (building->m_type == Building::TypeSafeArea)
  {
    SafeArea* safeArea = (SafeArea*)building;
    CreateValueControl(LANGUAGEPHRASE("editor_size"), &safeArea->m_size, y += buttonPitch, 1.0f, 0.0f, 1000.0f);
    CreateValueControl(LANGUAGEPHRASE("editor_capacity"), &safeArea->m_entitiesRequired, y += buttonPitch, 1, 0, 10000);

    DropDownMenu* menu = new DropDownMenu(true);
    menu->SetShortProperties(LANGUAGEPHRASE("editor_entitytype"), 10, y += buttonPitch, m_w - 20);
    for (int i = 0; i < Entity::NumEntityTypes; ++i)
    {
      menu->AddOption(Entity::GetTypeNameTranslated(i), i);
    }
    menu->RegisterInt(&safeArea->m_entityTypeRequired);
    RegisterButton(menu);
  }
  else if (building->m_type == Building::TypeTrackStart)
  {
    TrackStart* trackStart = (TrackStart*)building;
    CreateValueControl(LANGUAGEPHRASE("editor_toggledby"), &trackStart->m_reqBuildingId, y += buttonPitch, 1.0f, -1.0f, 1000.0f);
  }
  else if (building->m_type == Building::TypeTrackEnd)
  {
    TrackEnd* trackEnd = (TrackEnd*)building;
    CreateValueControl(LANGUAGEPHRASE("editor_toggledby"), &trackEnd->m_reqBuildingId, y += buttonPitch, 1.0f, -1.0f, 1000.0f);
  }
  else if (building->m_type == Building::TypePylonStart)
  {
    PylonStart* pylonStart = (PylonStart*)building;
    CreateValueControl(LANGUAGEPHRASE("editor_toggledby"), &pylonStart->m_reqBuildingId, y += buttonPitch, 1.0f, -1.0f, 1000.0f);
  }
  else if (building->m_type == Building::TypeResearchItem)
  {
    DropDownMenu* menu = new DropDownMenu(true);
    menu->SetShortProperties(LANGUAGEPHRASE("editor_research"), 10, y += buttonPitch, m_w - 20);
    for (int i = 0; i < GlobalResearch::NumResearchItems; ++i)
    {
      menu->AddOption(GlobalResearch::GetTypeNameTranslated(i), i);
    }
    menu->RegisterInt(&((ResearchItem*)building)->m_researchType);
    RegisterButton(menu);
    CreateValueControl(LANGUAGEPHRASE("editor_level"), &((ResearchItem*)building)->m_level, y += buttonPitch, 1, 0, 4);
  }
  else if (building->m_type == Building::TypeTriffid)
  {
    Triffid* triffid = (Triffid*)building;
    CreateValueControl(LANGUAGEPHRASE("editor_size"), &triffid->m_size, y += buttonPitch, 0.1f, 0.0f, 50.0f);
    CreateValueControl(LANGUAGEPHRASE("editor_pitch"), &triffid->m_pitch, y += buttonPitch, 0.1f, -M_PI, M_PI);
    CreateValueControl(LANGUAGEPHRASE("editor_force"), &triffid->m_force, y += buttonPitch, 1.0f, 0.0f, 1000.0f);
    CreateValueControl(LANGUAGEPHRASE("editor_variance"), &triffid->m_variance, y += buttonPitch, 0.01f, 0.0f, M_PI);
    CreateValueControl(LANGUAGEPHRASE("editor_reload"), &triffid->m_reloadTime, y += buttonPitch, 1.0f, 0.0f, 1000.0f);

    for (int i = 0; i < Triffid::NumSpawnTypes; ++i)
    {
      CreateValueControl(Triffid::GetSpawnNameTranslated(i), &triffid->m_spawn[i], y += buttonPitch, 1.0f, 0.0f, 1.0f);
    }

    CreateValueControl(LANGUAGEPHRASE("editor_usetrigger"), &triffid->m_useTrigger, y += buttonPitch, 1, 0, 1);
    CreateValueControl(LANGUAGEPHRASE("editor_triggerX"), &triffid->m_triggerLocation.x, y += buttonPitch, 1.0f, -10000.0f, 10000.0f);
    CreateValueControl(LANGUAGEPHRASE("editor_triggerZ"), &triffid->m_triggerLocation.z, y += buttonPitch, 1.0f, -10000.0f, 10000.0f);
    CreateValueControl(LANGUAGEPHRASE("editor_triggerrad"), &triffid->m_triggerRadius, y += buttonPitch, 1.0f, 0.0f, 1000.0f);
  }
  else if (building->m_type == Building::TypeBlueprintRelay)
  {
    BlueprintRelay* relay = (BlueprintRelay*)building;
    CreateValueControl(LANGUAGEPHRASE("editor_altitude"), &relay->m_altitude, y += buttonPitch, 1.0f, 0.0f, 1000.0f);
  }
  else if (building->m_type == Building::TypeBlueprintConsole)
  {
    BlueprintConsole* console = (BlueprintConsole*)building;
    CreateValueControl(LANGUAGEPHRASE("editor_segment"), &console->m_segment, y += buttonPitch, 1, 0, 3);
  }
  else if (building->m_type == Building::TypeAISpawnPoint)
  {
    AISpawnPoint* spawn = (AISpawnPoint*)building;
    DropDownMenu* menu = new DropDownMenu();
    menu->SetShortProperties(LANGUAGEPHRASE("editor_entitytype"), 10, y += buttonPitch, m_w - 20);
    for (int i = 0; i < Entity::NumEntityTypes; ++i)
    {
      menu->AddOption(Entity::GetTypeNameTranslated(i), i);
    }
    menu->RegisterInt(&spawn->m_entityType);
    RegisterButton(menu);

    CreateValueControl(LANGUAGEPHRASE("editor_count"), &spawn->m_count, y += buttonPitch, 1, 0, 1000);
    CreateValueControl(LANGUAGEPHRASE("editor_period"), &spawn->m_period, y += buttonPitch, 1, 0, 1000);
    CreateValueControl(LANGUAGEPHRASE("editor_spawnlimit"), &spawn->m_spawnLimit, y += buttonPitch, 1, 0, 1000);
  }
  else if (building->m_type == Building::TypeSpawnPopulationLock)
  {
    SpawnPopulationLock* lock = (SpawnPopulationLock*)building;

    CreateValueControl(LANGUAGEPHRASE("editor_searchradius"), &lock->m_searchRadius, y += buttonPitch, 1.0f, 0.0f, 10000.0f);
    CreateValueControl(LANGUAGEPHRASE("editor_maxpopulation"), &lock->m_maxPopulation, y += buttonPitch, 1, 0, 10000);
  }
  else if (building->m_type == Building::TypeSpawnLink || building->m_type == Building::TypeSpawnPointMaster ||
           building->m_type == Building::TypeSpawnPoint)
  {
    class ClearLinksButton : public SpeciesButton
    {
      public:
        int m_buildingId;
        void MouseUp()
        {
          SpawnBuilding* building = (SpawnBuilding*)g_location->GetBuilding(m_buildingId);
          building->ClearLinks();
        }
    };

    ClearLinksButton* button = new ClearLinksButton();
    button->SetShortProperties(LANGUAGEPHRASE("editor_clearlinks"), 10, y += buttonPitch, m_w - 20);
    button->m_buildingId = building->m_id.GetUniqueId();
    RegisterButton(button);
  }
  else if (building->m_type == Building::TypeScriptTrigger)
  {
    ScriptTrigger* trigger = (ScriptTrigger*)building;

    CreateValueControl(LANGUAGEPHRASE("editor_range"), &trigger->m_range, y += buttonPitch, 0.5f, 0.0f, 1000.0f);
    CreateValueControl(LANGUAGEPHRASE("editor_script"), &trigger->m_scriptFilename, y += buttonPitch, 0, 0, 0);

    DropDownMenu* menu = new DropDownMenu(true);
    menu->SetShortProperties(LANGUAGEPHRASE("editor_entitytype"), 10, y += buttonPitch, m_w - 20);
    menu->AddOption(LANGUAGEPHRASE("editor_always"), SCRIPTRIGGER_RUNALWAYS);
    menu->AddOption(LANGUAGEPHRASE("editor_never"), SCRIPTRIGGER_RUNNEVER);
    menu->AddOption(LANGUAGEPHRASE("editor_cameraenter"), SCRIPTRIGGER_RUNCAMENTER);
    menu->AddOption(LANGUAGEPHRASE("editor_cameraview"), SCRIPTRIGGER_RUNCAMVIEW);
    for (int i = 0; i < Entity::NumEntityTypes; ++i)
    {
      menu->AddOption(Entity::GetTypeNameTranslated(i), i);
    }
    menu->RegisterInt(&trigger->m_entityType);
    RegisterButton(menu);
  }
  else if (building->m_type == Building::TypeStaticShape)
  {
    StaticShape* staticShape = (StaticShape*)building;

    CreateValueControl(LANGUAGEPHRASE("editor_scale"), &staticShape->m_scale, y += buttonPitch, 0.1f, 0.0f, 100.0f);
    CreateValueControl(LANGUAGEPHRASE("editor_shape"), &staticShape->m_shapeName, y += buttonPitch, 0, 0, 0);
  }
  else if (building->m_type == Building::TypeIncubator)
  {
    CreateValueControl(LANGUAGEPHRASE("editor_spirits"), &((Incubator*)building)->m_numStartingSpirits, y += buttonPitch, 1, 0, 1000);
  }
  else if (building->m_type == Building::TypeEscapeRocket)
  {
    EscapeRocket* rocket = (EscapeRocket*)building;

    CreateValueControl(LANGUAGEPHRASE("editor_fuel"), &rocket->m_fuel, y += buttonPitch, 0.1f, 0.0f, 100.0f);
    CreateValueControl(LANGUAGEPHRASE("editor_passengers"), &rocket->m_passengers, y += buttonPitch, 1, 0, 100);
    CreateValueControl(LANGUAGEPHRASE("editor_spawnport"), &rocket->m_spawnBuildingId, y += buttonPitch, 1, 0, 9999);
  }
  else if (building->m_type == Building::TypeDynamicHub)
  {
    DynamicHub* hub = (DynamicHub*)building;

    CreateValueControl(LANGUAGEPHRASE("editor_shape"), &hub->m_shapeName, y += buttonPitch, 0, 0, 0);
    CreateValueControl(LANGUAGEPHRASE("editor_requiredscore"), &hub->m_requiredScore, y += buttonPitch, 1, 0, 100000);
    CreateValueControl(LANGUAGEPHRASE("editor_minlinks"), &hub->m_minActiveLinks, y += buttonPitch, 1, 0, 100);
  }
  else if (building->m_type == Building::TypeDynamicNode)
  {
    DynamicNode* node = (DynamicNode*)building;

    CreateValueControl(LANGUAGEPHRASE("editor_shape"), &node->m_shapeName, y += buttonPitch, 0, 0, 0);
    CreateValueControl(LANGUAGEPHRASE("editor_pointspersec"), &node->m_scoreValue, y += buttonPitch, 1, 0, 1000);
  }
  else if (building->m_type == Building::TypeFenceSwitch)
  {
    FenceSwitch* fs = (FenceSwitch*)building;

    CreateValueControl(LANGUAGEPHRASE("editor_switchonce"), &fs->m_lockable, y += buttonPitch, 0, 1, 0);
    CreateValueControl(LANGUAGEPHRASE("editor_script"), &fs->m_script, y += buttonPitch, 0, 0, 0);
  }
}


void BuildingEditWindow::Render(bool hasFocus)
{
  SpeciesWindow::Render(hasFocus);

  Building* building = g_location->GetBuilding(g_locationEditor->GetSelectionId());
  DEBUG_ASSERT(building);

  g_editorFont.SetRenderShadow(true);
  glColor4ub(255, 255, 150, 30);
  g_editorFont.DrawText2D(m_x + m_w - 43, m_y + 8, DEF_FONT_SIZE * 1.1f, "%d", building->m_id.GetUniqueId());
  g_editorFont.DrawText2D(m_x + m_w - 43, m_y + 8, DEF_FONT_SIZE * 1.1f, "%d", building->m_id.GetUniqueId());
  g_editorFont.SetRenderShadow(false);
}


// ****************************************************************************
// Class NewBuildingButton
// ****************************************************************************

class NewBuildingButton : public SpeciesButton
{
  public:
    void MouseUp()
    {
      DirectX::XMFLOAT3 rayStart{0.0f, 0.0f, 0.0f};
      DirectX::XMFLOAT3 rayDir{0.0f, 0.0f, 0.0f};
      g_camera->GetClickRay(g_renderer->ScreenW() / 2, g_renderer->ScreenH() / 2, &rayStart, &rayDir);
      // Landscape::RayHit keeps its Vector3 out-pointer until
      // directxmath-migration T28, and the seam does not reach through a
      // pointer, so &AsLegacy is what writes native storage from it.
      DirectX::XMFLOAT3 _pos{0.0f, 0.0f, 0.0f};
      g_location->m_landscape.RayHit(rayStart, rayDir, &AsLegacy(_pos));

      BuildingsCreateWindow* bcw = (BuildingsCreateWindow*)m_parent;
      Building* building = Building::CreateBuilding(bcw->m_buildingType);
      if (building)
      {
        building->m_pos = _pos;
        building->m_id.SetUniqueId(g_globalWorld->GenerateBuildingId());
        g_location->m_levelFile->m_buildings.push_back(building);
      }
    }
};


// ****************************************************************************
// Class BuildingsCreateWindow
// ****************************************************************************

BuildingsCreateWindow::BuildingsCreateWindow(char const* _name)
  : SpeciesWindow(_name),
    m_buildingType(0)
{
}


BuildingsCreateWindow::~BuildingsCreateWindow()
{
  g_locationEditor->RequestMode(LocationEditorAccess::ModeNone);
  EclRemoveWindow(LANGUAGEPHRASE("editor_buildingid"));
}


void BuildingsCreateWindow::Create()
{
  SpeciesWindow::Create();

  int y = 25;
  int ySpacing = 18;

  DropDownMenu* menu = new DropDownMenu(true);
  menu->SetShortProperties(LANGUAGEPHRASE("editor_buildingtype"), 10, 25, m_w - 20);
  menu->RegisterInt(&m_buildingType);
  RegisterButton(menu);
  for (int i = Building::TypeInvalid + 1; i < Building::NumBuildingTypes; ++i)
  {
    menu->AddOption(Building::GetTypeNameTranslated(i), i);
  }

  NewBuildingButton* b = new NewBuildingButton();
  b->SetShortProperties(LANGUAGEPHRASE("editor_createbuilding"), 10, 45, m_w - 20);
  RegisterButton(b);

  //	for (int i = Building::TypeInvalid + 1; i < Building::NumBuildingTypes; ++i)
  //	{
  //	    NewBuildingButton *n = new NewBuildingButton(i);
  //        char *name = Building::GetTypeName( i );
  //        n->SetShortProperties( name, 10, y, m_w - 20 );
  //		RegisterButton( n );
  //		y += ySpacing;
  //	}
}

#endif // LOCATION_EDITOR
