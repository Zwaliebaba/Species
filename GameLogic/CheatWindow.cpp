#include "pch.h"
#include "MathUtils.h"
#include "Resource.h"

#include "CheatWindow.h"
#include "DebugMenu.h"

#include "GlobalWorld.h"
#include "ProtocolLimits.h"
#include "Location.h"
#include "Unit.h"
#include "Team.h"
#include "TaskManager.h"
#include "WorldPointers.h"


// g_zeroVector, which the five spawn calls below passed as a velocity.
// directxmath-migration T25 retires the legacy constant; this is the same
// (0,0,0) as native storage, named rather than repeated.
static DirectX::XMFLOAT3 const s_noVelocity{0.0f, 0.0f, 0.0f};


#ifdef CHEATMENU_ENABLED

class KillAllEnemiesButton : public SpeciesButton
{
    void MouseUp()
    {
      if (g_location)
      {
        int myTeamId = g_globalWorld->m_myTeamId;
        for (int t = 0; t < NUM_TEAMS; ++t)
        {
          if (!g_location->IsFriend(myTeamId, t))
          {
            Team* team = &g_location->m_teams[t];

            // Kill all UNITS
            for (int u = 0; u < team->m_units.Size(); ++u)
            {
              if (team->m_units.ValidIndex(u))
              {
                Unit* unit = team->m_units[u];
                for (int e = 0; e < unit->m_entities.Size(); ++e)
                {
                  if (unit->m_entities.ValidIndex(e))
                  {
                    Entity* entity = unit->m_entities[e];
                    entity->ChangeHealth(entity->m_stats[Entity::StatHealth] * -2.0f);
                  }
                }
              }
            }

            // Kill all ENTITIES
            for (int o = 0; o < team->m_others.Size(); ++o)
            {
              if (team->m_others.ValidIndex(o))
              {
                Entity* entity = team->m_others[o];
                entity->ChangeHealth(entity->m_stats[Entity::StatHealth] * -2.0f);
              }
            }
          }
        }

        for (int i = 0; i < g_location->m_buildings.Size(); ++i)
        {
          if (g_location->m_buildings.ValidIndex(i))
          {
            Building* building = g_location->m_buildings[i];
            if (building->m_type == Building::TypeAntHill || building->m_type == Building::TypeTriffid)
            {
              building->Damage(-999);
            }
          }
        }
      }
    }
};


class SpawnCitizensButton : public SpeciesButton
{
  public:
    int m_teamId;

    void MouseUp()
    {
      if (g_location)
      {
        DirectX::XMFLOAT3 rayStart{0.0f, 0.0f, 0.0f};
        DirectX::XMFLOAT3 rayDir{0.0f, 0.0f, 0.0f};
        g_camera->GetClickRay(g_renderer->ScreenW() / 2, g_renderer->ScreenH() / 2, &rayStart, &rayDir);
        DirectX::XMFLOAT3 _pos{0.0f, 0.0f, 0.0f};
        g_location->m_landscape.RayHit(rayStart, rayDir, &_pos);

        g_location->SpawnEntities(_pos, m_teamId, -1, Entity::TypeCitizen, 20, s_noVelocity, 30);
      }
    }
};


class SpawnTankButton : public SpeciesButton
{
    void MouseUp()
    {
      if (g_location)
      {
        DirectX::XMFLOAT3 rayStart{0.0f, 0.0f, 0.0f};
        DirectX::XMFLOAT3 rayDir{0.0f, 0.0f, 0.0f};
        g_camera->GetClickRay(g_renderer->ScreenW() / 2, g_renderer->ScreenH() / 2, &rayStart, &rayDir);
        DirectX::XMFLOAT3 _pos{0.0f, 0.0f, 0.0f};
        g_location->m_landscape.RayHit(rayStart, rayDir, &_pos);

        g_location->SpawnEntities(_pos, 2, -1, Entity::TypeArmour, 1, s_noVelocity, 0);
      }
    }
};


class SpawnViriiButton : public SpeciesButton
{
    void MouseUp()
    {
      if (g_location)
      {
        DirectX::XMFLOAT3 rayStart{0.0f, 0.0f, 0.0f};
        DirectX::XMFLOAT3 rayDir{0.0f, 0.0f, 0.0f};
        g_camera->GetClickRay(g_renderer->ScreenW() / 2, g_renderer->ScreenH() / 2, &rayStart, &rayDir);
        DirectX::XMFLOAT3 _pos{0.0f, 0.0f, 0.0f};
        g_location->m_landscape.RayHit(rayStart, rayDir, &_pos);

        g_location->SpawnEntities(_pos, 1, -1, Entity::TypeVirii, 20, s_noVelocity, 0, 1000.0f);
      }
    }
};


class SpawnSpiritButton : public SpeciesButton
{
    void MouseUp()
    {
      if (g_location)
      {
        DirectX::XMFLOAT3 rayStart{0.0f, 0.0f, 0.0f};
        DirectX::XMFLOAT3 rayDir{0.0f, 0.0f, 0.0f};
        g_camera->GetClickRay(g_renderer->ScreenW() / 2, g_renderer->ScreenH() / 2, &rayStart, &rayDir);
        DirectX::XMFLOAT3 _pos{0.0f, 0.0f, 0.0f};
        g_location->m_landscape.RayHit(rayStart, rayDir, &_pos);

        for (int i = 0; i < 10; ++i)
        {
          // The RNG call order is not sanctioned to change, so the two
          // syncsfrand calls stay in this order and in this one expression.
          DirectX::XMFLOAT3 spiritPos;
          DirectX::XMStoreFloat3(
            &spiritPos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&_pos), DirectX::XMVectorSet(syncsfrand(20.0f), 0.0f, syncsfrand(20.0f), 0.0f)));
          g_location->SpawnSpirit(spiritPos, s_noVelocity, 2, WorldObjectId());
        }
      }
    }
};


class AllowArbitraryPlacementButton : public SpeciesButton
{
    void MouseUp() { g_taskManager->m_verifyTargetting = false; }
};


class EnableGeneratorAndMineButton : public SpeciesButton
{
    void MouseUp()
    {
      int generatorLocationId = g_globalWorld->GetLocationId("generator");
      int mineLocationId = g_globalWorld->GetLocationId("mine");

      for (auto const& gb : g_globalWorld->m_buildings)
      {
        if (gb && gb->m_locationId == generatorLocationId && gb->m_type == Building::TypeGenerator)
        {
          gb->m_online = true;
        }

        if (gb && gb->m_locationId == mineLocationId && gb->m_type == Building::TypeRefinery)
        {
          gb->m_online = true;
        }
      }

      g_globalWorld->EvaluateEvents();
    }
};


class EnableReceiverAndBufferButton : public SpeciesButton
{
    void MouseUp()
    {
      int receiverLocationId = g_globalWorld->GetLocationId("receiver");
      int bufferLocationId = g_globalWorld->GetLocationId("PatternBuffer");

      for (auto const& gb : g_globalWorld->m_buildings)
      {
        if (gb && gb->m_locationId == receiverLocationId && gb->m_type == Building::TypeSpiritProcessor)
        {
          gb->m_online = true;
        }

        if (gb && gb->m_locationId == bufferLocationId && gb->m_type == Building::TypeBlueprintStore)
        {
          gb->m_online = true;
        }
      }

      g_globalWorld->EvaluateEvents();
    }
};


class OpenAllLocationsButton : public SpeciesButton
{
    void MouseUp()
    {
      for (auto const& loc : g_globalWorld->m_locations)
      {
        loc->m_available = true;
      }
    }
};


class ClearResourcesButton : public SpeciesButton
{
    void MouseUp()
    {
      g_resource->FlushOpenGlState();
      g_resource->RegenerateOpenGlState();
    }
};


class GiveAllResearchButton : public SpeciesButton
{
    void MouseUp()
    {
      for (int i = 0; i < GlobalResearch::NumResearchItems; ++i)
      {
        g_globalWorld->m_research->AddResearch(i);
        // g_globalWorld->m_research->SetCurrentProgress( i, 400 );
        g_globalWorld->m_research->EvaluateLevel(i);
      }
    }
};


class SpawnPortsButton : public SpeciesButton
{
    void MouseUp()
    {
      for (int i = 0; i < g_location->m_buildings.Size(); ++i)
      {
        if (g_location->m_buildings.ValidIndex(i))
        {
          Building* building = g_location->m_buildings[i];
          for (int p = 0; p < building->GetNumPorts(); ++p)
          {
            // Braced to zero, as Vector3's default constructor did:
            // GetPortPosition leaves both untouched on a building with no
            // shape marker for the port it was asked about.
            DirectX::XMFLOAT3 portPos{0.0f, 0.0f, 0.0f};
            DirectX::XMFLOAT3 portFront{0.0f, 0.0f, 0.0f};
            building->GetPortPosition(p, portPos, portFront);

            g_location->SpawnEntities(portPos, 0, -1, Entity::TypeCitizen, 3, s_noVelocity, 30.0f);
          }
        }
      }
    }
};

#ifdef PROFILER_ENABLED
class ProfilerCreateButton : public SpeciesButton
{
    void MouseUp() { DebugKeyBindings::ProfileButton(); }
};
#endif // PROFILER_ENABLED


void CheatWindow::Create()
{
  SpeciesWindow::Create();

  int y = 25;

  KillAllEnemiesButton* killAllEnemies = new KillAllEnemiesButton();
  killAllEnemies->SetShortProperties("Kill All Enemies", 10, y, m_w - 20);
  RegisterButton(killAllEnemies);

  SpawnCitizensButton* spawnCitizensGreen = new SpawnCitizensButton();
  spawnCitizensGreen->SetShortProperties("Spawn Green", 10, y += 20, (m_w - 25) / 2);
  spawnCitizensGreen->m_teamId = 0;
  RegisterButton(spawnCitizensGreen);

  SpawnCitizensButton* spawnCitizensRed = new SpawnCitizensButton();
  spawnCitizensRed->SetShortProperties("Spawn Red", spawnCitizensGreen->m_x + spawnCitizensGreen->m_w + 5, y, (m_w - 25) / 2);
  spawnCitizensRed->m_teamId = 1;
  RegisterButton(spawnCitizensRed);

  SpawnTankButton* spawnTankButton = new SpawnTankButton();
  spawnTankButton->SetShortProperties("Spawn Armour", 10, y += 20, m_w - 20);
  RegisterButton(spawnTankButton);

  SpawnViriiButton* spawnVirii = new SpawnViriiButton();
  spawnVirii->SetShortProperties("Spawn Virii", 10, y += 20, m_w - 20);
  RegisterButton(spawnVirii);

  SpawnSpiritButton* spawnSpirits = new SpawnSpiritButton();
  spawnSpirits->SetShortProperties("Spawn Spirits", 10, y += 20, m_w - 20);
  RegisterButton(spawnSpirits);

  AllowArbitraryPlacementButton* allowPlacement = new AllowArbitraryPlacementButton();
  allowPlacement->SetShortProperties("Allow Arbitrary Placement", 10, y += 20, m_w - 20);
  RegisterButton(allowPlacement);

  EnableGeneratorAndMineButton* enable = new EnableGeneratorAndMineButton();
  enable->SetShortProperties("Enable Generator and Mine", 10, y += 20, m_w - 20);
  RegisterButton(enable);

  EnableReceiverAndBufferButton* receiver = new EnableReceiverAndBufferButton();
  receiver->SetShortProperties("Enable Receiver and Buffer", 10, y += 20, m_w - 20);
  RegisterButton(receiver);

  OpenAllLocationsButton* openAllLocations = new OpenAllLocationsButton();
  openAllLocations->SetShortProperties("Open All Locations", 10, y += 20, m_w - 20);
  RegisterButton(openAllLocations);

  GiveAllResearchButton* allResearch = new GiveAllResearchButton();
  allResearch->SetShortProperties("Give all research", 10, y += 20, m_w - 20);
  RegisterButton(allResearch);

  ClearResourcesButton* clearResources = new ClearResourcesButton();
  clearResources->SetShortProperties("Clear Resources", 10, y += 20, m_w - 20);
  RegisterButton(clearResources);

  SpawnPortsButton* spawnPorts = new SpawnPortsButton();
  spawnPorts->SetShortProperties("Spawn Ports", 10, y += 20, m_w - 20);
  RegisterButton(spawnPorts);

  y += 20;

#ifdef PROFILER_ENABLED
  ProfilerCreateButton* profiler = new ProfilerCreateButton();
  profiler->SetShortProperties("Profiler", 10, y += 20, m_w - 20);
  RegisterButton(profiler);
#endif // PROFILER_ENABLED
}


#else // CHEATMENU_ENABLED

void CheatWindow::Create() {}

#endif // CHEATMENU_ENABLED

CheatWindow::CheatWindow(char const* _name)
  : SpeciesWindow(_name)
{
}
