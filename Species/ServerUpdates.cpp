#include "pch.h"

#include "Debug.h"
#include "Vector3.h"

#include "NetworkUpdate.h"
#include "ServerToClientLetter.h"

#include "App.h"
#include "Location.h"
#include "TaskManager.h"
#include "Team.h"

// Engineer.h came with this code and is deliberately not carried over: Engineer
// is named nowhere in the body, and everything the body does need arrives
// through Building.h and the three concrete building types below.
#include "Building.h"
#include "Factory.h"
#include "LaserFence.h"
#include "RadarDish.h"
#include "WorldObject.h"

#include "ServerUpdates.h"
#include "WorldPointers.h"

// The switch below is the inherited body verbatim — same cases, same order, same
// conditions. It is reachable from the client's advance loop, so changing what it
// does changes what the simulation computes. See CODING_STANDARDS.md, Determinism.

void ProcessServerUpdates(ServerToClientLetter* _letter)
{
  DEBUG_ASSERT(_letter->m_type == ServerToClientLetter::LetterType::Update);

  for (int i = 0; i < static_cast<int>(_letter->m_updates.size()); ++i)
  {
    NetworkUpdate* update = _letter->m_updates[i];

    switch (update->m_type)
    {
    case NetworkUpdate::UpdateType::Alive:
      g_location->UpdateTeam(update->m_teamId, update->m_teamControls);
      break;

    case NetworkUpdate::UpdateType::Pause:
      g_paused = !g_paused;
      break;

    case NetworkUpdate::UpdateType::SelectUnit:
      g_location->m_teams[update->m_teamId].SelectUnit(update->m_unitId, update->m_entityId, update->m_buildingId);
      g_taskManager->SelectTask(WorldObjectId(update->m_teamId, update->m_unitId, update->m_entityId, -1));
      break;

    case NetworkUpdate::UpdateType::CreateUnit:
    {
      Building* building = g_location->GetBuilding(update->m_buildingId);
      if (building && building->m_type == Building::TypeFactory)
      {
        Factory* factory = (Factory*)building;
        factory->RequestUnit(update->m_entityType, update->m_numTroops);
      }
      else
      {
        DEBUG_ASSERT(update->GetWorldPos() != g_zeroVector);
        int unitId;
        // The returned Unit* was assigned to an unused local in the original.
        // Dropping the variable keeps the call and its side effects.
        g_location->m_teams[update->m_teamId].NewUnit(update->m_entityType, update->m_numTroops, &unitId, update->GetWorldPos());
        g_location->SpawnEntities(update->GetWorldPos(), update->m_teamId, unitId, update->m_entityType, update->m_numTroops, g_zeroVector,
                                         update->m_numTroops * 2);
      }
      break;
    }

    case NetworkUpdate::UpdateType::AimBuilding:
    {
      Building* building = g_location->GetBuilding(update->m_buildingId);
      if (building && building->m_id.GetTeamId() == update->m_teamId && building->m_type == Building::TypeRadarDish)
      {
        RadarDish* radarDish = (RadarDish*)building;
        radarDish->Aim(update->GetWorldPos());
      }
      break;
    }

    case NetworkUpdate::UpdateType::ToggleLaserFence:
    {
      Building* building = g_location->GetBuilding(update->m_buildingId);
      if (building && building->m_type == Building::TypeLaserFence)
      {
        LaserFence* laserfence = (LaserFence*)building;
        laserfence->Toggle();
      }
      break;
    }

    case NetworkUpdate::UpdateType::RunProgram:
    {
      g_taskManager->RunTask(update->m_program);
      break;
    }

    case NetworkUpdate::UpdateType::TargetProgram:
    {
      int programId = update->m_program;
      g_taskManager->TargetTask(programId, update->GetWorldPos());
    }
    }
  }
}
