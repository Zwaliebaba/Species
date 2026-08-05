#include "pch.h"

#include "MathUtils.h"
#include "Resource.h"
#include "Shape.h"
#include "Debug.h"

#include "Explosion.h"
#include "EntityGrid.h"
#include "Location.h"
#include "Team.h"
#include "Unit.h"

#include "Cave.h"
#include "WorldPointers.h"


Cave::Cave()
  : Building(),
    m_troopType(-1),
    m_unitId(-1),
    m_spawnTimer(0.0f),
    m_dead(false)
{
  m_type = TypeCave;
  m_troopType = Entity::TypeVirii;

  SetShape(g_resource->GetShape("Cave.shp"));

  m_spawnPoint = m_shape->m_rootFragment->LookupMarker("MarkerSpawnPoint");
  DEBUG_ASSERT(m_spawnPoint);
}


bool Cave::Advance()
{
  if (m_dead)
    return true;

  if (m_id.GetTeamId() < 0 || m_id.GetTeamId() >= NUM_TEAMS)
    return false;
  if (g_location->m_teams[m_id.GetTeamId()].m_teamType == Team::TeamTypeUnused)
    return false;

  m_spawnTimer -= SERVER_ADVANCE_PERIOD;

  if (m_spawnTimer <= 0.0f)
  {
    //
    // Only spawn if the area is sufficiently empty

    DirectX::XMFLOAT4X4 rootMat;
    DirectX::XMStoreFloat4x4(&rootMat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

    // the marker's position comes back off a legacy row.
    DirectX::XMFLOAT3 spawnPoint = m_spawnPoint->GetWorldPosition(rootMat);
    spawnPoint.y = g_location->m_landscape.m_heightMap->GetValue(spawnPoint.x, spawnPoint.z);

    int numFound;
    WorldObjectId* objs = g_location->m_entityGrid->GetFriends(spawnPoint.x, spawnPoint.z, 100.0f, &numFound, m_id.GetTeamId());
    if (numFound < 30)
    {
      // int numToCreate = int( syncfrand(5.0f) + 3.0f );
      Team* team = &g_location->m_teams[m_id.GetTeamId()];

      // Unit *unit = team->NewUnit( m_troopType, numToCreate, &m_unitId );
      // unit->m_wayPoint = spawnPoint + worldMat.f * 25.0f;
      // unit->m_wayPoint += Vector3( syncsfrand(25.0f), 0.0f, syncsfrand(25.0f) );
      // unit->m_wayPoint.y = g_location->m_landscape.m_heightMap->GetValue( unit->m_wayPoint.x, unit->m_wayPoint.z );

      g_location->SpawnEntities(spawnPoint, m_id.GetTeamId(), m_id.GetUniqueId(), m_troopType, 1, g_zeroVector, 0.0f);
    }

    m_spawnTimer = syncfrand(0.5f);
  }

  return false;
}


void Cave::Damage(float _damage)
{
  if (_damage > 80.0f)
  {
    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));
    g_explosionManager.AddExplosion(m_shape, mat);
    m_dead = true;
  }
}
