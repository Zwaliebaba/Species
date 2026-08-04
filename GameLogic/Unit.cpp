#include "pch.h"
#include "Globals.h"

#include <math.h>

#include "Debug.h"
#include "Resource.h"
#include "MathUtils.h"
#include "Shape.h"
#include "DebugRender.h"
#include "HiResTime.h"
#include "Profiler.h"

#include "EntityGrid.h"
#include "LevelFile.h"
#include "Location.h"
#include "RoutingSystem.h"
#include "SoundSystem.h"
#include "Team.h"
#include "Unit.h"

#include "WorldObject.h"
#include "LaserTrooper.h"
#include "WorldPointers.h"

Unit::Unit(int troopType, int teamId, int unitId, int numEntities, DirectX::XMFLOAT3 const& _pos)
  : m_troopType(troopType),
    m_teamId(teamId),
    m_unitId(unitId),
    m_radius(0.0f),
    m_centrePos(_pos),
    m_vel(0.0f, 0.0f, 0.0f),
    m_accumulatedCentre(0.0f, 0.0f, 0.0f),
    m_accumulatedRadiusSquared(0.0f),
    m_numAccumulated(0),
    m_wayPoint(0.0f, 0.0f, 0.0f),
    m_routeId(-1),
    m_routeWayPointId(-1),
    m_targetDir(1.0f, 0.0f, 0.0f),
    m_attackAccumulator(0.0f)
{
  m_entitiesWalker.SetTotalNumSlices(NUM_SLICES_PER_FRAME);
  m_entities.SetStepSize(100);
  m_entities.SetSize(numEntities);
}

Unit::~Unit()
{
  Team* myTeam = &g_location->m_teams[m_teamId];
  if (myTeam->m_currentUnitId == m_unitId)
  {
    myTeam->m_currentUnitId = -1;
  }
}

void Unit::Begin() {}

Entity* Unit::NewEntity(int* _index)
{
  Entity* entity = Entity::NewEntity(m_troopType);
  *_index = m_entities.PutData(entity);
  return entity;
}

int Unit::AddEntity(Entity* _entity) { return m_entities.PutData(_entity); }

// Removes an entity from the unit's array of entities. Also removes the
// entity from the EntityGrid and deletes the entity.
// _posX and _posZ specify the position that the entity last registered
// with the EntityGrid.
void Unit::RemoveEntity(int _index, float _posX, float _posZ)
{
  if (m_entities.ValidIndex(_index))
  {
    Entity* entity = m_entities[_index];

    WorldObjectId myId(m_teamId, m_unitId, _index, entity->m_id.GetUniqueId());

    g_location->m_entityGrid->RemoveObject(myId, _posX, _posZ, entity->m_radius);

    m_entities.MarkNotUsed(_index);
    delete entity;
  }
}

void Unit::AdvanceEntities(int _slice)
{
  int startIndex, endIndex;
  m_entitiesWalker.GetNextSliceBounds(_slice, m_entities.Size(), &startIndex, &endIndex);

  for (int i = startIndex; i <= endIndex; i++)
  {
    if (m_entities.ValidIndex(i))
    {
      Entity* s = m_entities[i];

      if (s->m_enabled)
      {
        DirectX::XMFLOAT3 const oldPos(s->m_pos);

        START_PROFILE(g_profiler, Entity::GetTypeName(s->m_type));
        bool amIdead = s->Advance(this);
        END_PROFILE(g_profiler, Entity::GetTypeName(s->m_type));

        if (amIdead)
        {
          RemoveEntity(i, oldPos.x, oldPos.z);
        }
        else
        {
          WorldObjectId myId(m_teamId, m_unitId, i, s->m_id.GetUniqueId());
          g_location->m_entityGrid->UpdateObject(myId, oldPos.x, oldPos.z, s->m_pos.x, s->m_pos.z, s->m_radius);
        }
      }
    }
  }
}


bool Unit::IsInView() { return (g_camera->SphereInViewFrustum(m_centrePos, m_radius)); }


void Unit::Render(float _predictionTime)
{
  // Render all the entities that are up-to-date with server advances
  int lastUpdated = m_entitiesWalker.GetLastUpdated();
  for (int i = 0; i <= lastUpdated; i++)
  {
    if (m_entities.ValidIndex(i))
    {
      Entity* entity = m_entities[i];
      entity->Render(_predictionTime);
    }
  }

  // Render all the entities that are one step out-of-date with server advances
  int size = m_entities.Size();
  _predictionTime += SERVER_ADVANCE_PERIOD;
  for (int i = lastUpdated + 1; i < size; i++)
  {
    if (m_entities.ValidIndex(i))
    {
      Entity* entity = m_entities[i];
      entity->Render(_predictionTime);
    }
  }

  glEnable(GL_CULL_FACE);
}

bool Unit::Advance(int _slice)
{
  //
  // Maintain our centre and radius values

  DirectX::XMVECTOR const oldPos = DirectX::XMLoadFloat3(&m_centrePos);
  DirectX::XMVECTOR centre = DirectX::XMLoadFloat3(&m_accumulatedCentre);
  if (m_numAccumulated != 0)
    centre = DirectX::XMVectorScale(centre, 1.0f / (float)m_numAccumulated);
  DirectX::XMStoreFloat3(&m_centrePos, centre);
  DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(DirectX::XMVectorSubtract(centre, oldPos), 1.0f / SERVER_ADVANCE_PERIOD));
  m_radius = sqrtf(m_accumulatedRadiusSquared);

  if (m_entities.NumUsed() == 0)
  {
    m_radius = 0.0f;
    return true;
  }

  m_accumulatedCentre = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
  m_accumulatedRadiusSquared = 0.0f;
  m_numAccumulated = 0;

  if (m_routeId != -1)
  {
    FollowRoute();
  }

  bool waypointChanged = false;
  if (waypointChanged)
  {
    RecalculateOffsets();
  }

  float leadDistance = 1.0f;

  if (m_troopType == Entity::TypeLaserTroop)
  {
    for (int i = 0; i < m_entities.Size(); i++)
    {
      if (m_entities.ValidIndex(i))
      {
        LaserTrooper* l = (LaserTrooper*)m_entities[i];

        if (DirectX::XMVectorGetX(DirectX::XMVector3Length(
              DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&l->m_pos), DirectX::XMLoadFloat3(&l->m_targetPos)))) < leadDistance / 5.0f)
        {
          DirectX::XMFLOAT3 pos = l->m_pos;
          //                    Vector3 targetPos = m_wayPoint;
          //                    targetPos += GetFormationOffset( FormationRectangle, l->m_unitIndex );
          //                    targetPos = l->PushFromObstructions( targetPos );
          //                    //targetPos = l->PushFromEachOther( targetPos );
          //                    l->m_unitTargetPos = targetPos;

          DirectX::XMFLOAT3 const targetPos = l->m_unitTargetPos;

          DirectX::XMVECTOR const toTarget = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&targetPos), DirectX::XMLoadFloat3(&pos));
          DirectX::XMVECTOR const desiredDirection = DirectX::XMVector3Normalize(toTarget);
          float distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(toTarget));
          float amountToMove = leadDistance;
          if (amountToMove > distance)
            amountToMove = distance;
          DirectX::XMStoreFloat3(
            &pos, DirectX::XMVectorMultiplyAdd(desiredDirection, DirectX::XMVectorReplicate(amountToMove), DirectX::XMLoadFloat3(&pos)));
          pos.y = g_location->m_landscape.m_heightMap->GetValue(pos.x, pos.z);
          pos = l->PushFromObstructions(pos);
          // pos = l->PushFromEachOther( pos );

          l->m_targetPos = pos;
        }
      }
    }
  }

  return false;
}

int Unit::NumEntities() { return m_entities.NumUsed(); }


int Unit::NumAliveEntities()
{
  int result = 0;

  for (int i = 0; i < m_entities.Size(); ++i)
  {
    if (m_entities.ValidIndex(i))
    {
      Entity* entity = m_entities[i];
      if (!entity->m_dead)
        ++result;
    }
  }

  return result;
}


void Unit::Attack(DirectX::XMFLOAT3 pos, bool _withGrenade)
{
  //
  // Deal with grenades

  if (_withGrenade)
  {
    float nearest = 9999.9f;
    Entity* nearestEnt = nullptr;

    //
    // Find the entity nearest to the target that has a grenade

    for (int i = 0; i < m_entities.Size(); ++i)
    {
      if (m_entities.ValidIndex(i))
      {
        Entity* ent = m_entities[i];
        if (!ent->m_dead)
        {
          float distance = DirectX::XMVectorGetX(
            DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&ent->m_pos), DirectX::XMLoadFloat3(&pos))));
          if (distance < nearest)
          {
            nearest = distance;
            nearestEnt = ent;
          }
        }
      }
    }

    if (nearestEnt)
    {
      g_location->ThrowWeapon(nearestEnt->m_pos, pos, WorldObject::EffectThrowableGrenade, m_teamId);
    }
  }


  //
  // Build a list of entities that can attack now

  std::vector<int> canAttack;
  for (int i = 0; i < m_entities.Size(); ++i)
  {
    if (m_entities.ValidIndex(i))
    {
      Entity* ent = m_entities[i];
      if (ent->m_enabled && !ent->m_dead && ent->m_reloading == 0.0f)
      {
        canAttack.push_back(i);
      }
    }
  }


  if (!canAttack.empty())
  {
    //
    // Decide the maximum number of entities
    // that can attack now without pauses appearing in fire rate

    float reloadTime = EntityBlueprint::GetStat(m_troopType, Entity::StatRate);
    float timeToWait = (float)reloadTime / (float)NumEntities();
    m_attackAccumulator += ((float)SERVER_ADVANCE_PERIOD / timeToWait);


    //
    // Pick guys randomly to attack

    while (!canAttack.empty() && m_attackAccumulator >= 1.0f)
    {
      m_attackAccumulator -= 1.0f;
      int randomIndex = syncfrand(static_cast<int>(canAttack.size()));
      int entityIndex = canAttack[randomIndex];
      canAttack.erase(canAttack.begin() + randomIndex);
      Entity* ent = m_entities[entityIndex];
      ent->Attack(pos);
    }
  }
}


void Unit::UpdateEntityPosition(DirectX::XMFLOAT3 pos, float _radius)
{
  DirectX::XMStoreFloat3(&m_accumulatedCentre, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_accumulatedCentre), DirectX::XMLoadFloat3(&pos)));
  ++m_numAccumulated;

  float distanceFromCentre =
    DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&pos), DirectX::XMLoadFloat3(&m_centrePos))));
  distanceFromCentre += _radius;
  float distanceFromCentreSquared = distanceFromCentre * distanceFromCentre;

  if (distanceFromCentreSquared > m_accumulatedRadiusSquared)
  {
    m_accumulatedRadiusSquared = distanceFromCentreSquared;
  }
}


DirectX::XMFLOAT3 Unit::GetWayPoint() { return m_wayPoint; }


void Unit::SetWayPoint(DirectX::XMFLOAT3 const& _pos) { m_wayPoint = _pos; }


DirectX::XMFLOAT3 Unit::GetFormationOffset(int _formation, int _index)
{
  static float* s_offsets = nullptr;
  int const numOffsets = 100;
  float const spacedOut = 4.0f;

  if (_index == -1)
  {
    return DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
  }

  // Generate some noise in our formation
  if (!s_offsets)
  {
    s_offsets = new float[numOffsets];
    for (int i = 0; i < numOffsets; i++)
    {
      s_offsets[i] = syncfrand(spacedOut / 4.0f);
    }
  }

  switch (_formation)
  {
  case FormationRectangle:
  {
    int rowLen = sqrtf(NumEntities());

    float x = _index % rowLen;
    x -= rowLen / 2.0f;
    x *= spacedOut;

    float z = _index / rowLen;
    z -= rowLen / 2.0f;
    z *= spacedOut;

    x += s_offsets[_index % numOffsets];
    z += s_offsets[_index % numOffsets];

    return DirectX::XMFLOAT3(z, 0.0f, x);
  }

  case FormationAirstrike:
  {
    int rowLen = 4;

    float x = _index % rowLen;
    x -= rowLen / 2.0f;
    x *= spacedOut * 6.0f;

    float y = _index / rowLen;
    y -= rowLen / 2.0f;
    y *= spacedOut * 3.0f;

    float z = _index / rowLen;
    z -= rowLen / 2.0f;
    z *= spacedOut * 6.0f;

    x += s_offsets[_index % numOffsets];
    y += s_offsets[_index % numOffsets];
    z += s_offsets[_index % numOffsets];

    return DirectX::XMFLOAT3(x, -y, z);
  }
  }

  return DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
}


DirectX::XMFLOAT3 Unit::GetOffset(int _formation, int _index)
{
  DirectX::XMFLOAT3 const formationOffset = GetFormationOffset(_formation, _index);

  DirectX::XMVECTOR const wayPoint = DirectX::XMLoadFloat3(&m_wayPoint);
  DirectX::XMVECTOR const finalPos = DirectX::XMVectorAdd(wayPoint, DirectX::XMLoadFloat3(&formationOffset));

  DirectX::XMFLOAT3 result;
  if (DirectX::XMVectorGetX(DirectX::XMVector3Length(finalPos)) == 0.0f)
  {
    DirectX::XMStoreFloat3(&result, finalPos);
  }
  else
  {
    DirectX::XMStoreFloat3(&result, DirectX::XMVectorSubtract(finalPos, wayPoint));
  }
  return result;
}


void Unit::RecalculateOffsets()
{
  int offset = 0;

  for (int i = 0; i < m_entities.Size(); ++i)
  {
    if (m_entities.ValidIndex(i))
    {
      Entity* ent = m_entities[i];
      if (!ent->m_dead)
      {
        ent->m_formationIndex = offset;
        ++offset;
      }
      else
      {
        ent->m_formationIndex = -1;
      }
    }
  }

  if (m_troopType == Entity::TypeLaserTroop)
  {
    for (int i = 0; i < m_entities.Size(); i++)
    {
      if (m_entities.ValidIndex(i))
      {
        LaserTrooper* l = (LaserTrooper*)m_entities[i];
        DirectX::XMFLOAT3 const offset = GetFormationOffset(FormationRectangle, l->m_id.GetIndex());
        DirectX::XMFLOAT3 targetPos;
        DirectX::XMStoreFloat3(&targetPos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_wayPoint), DirectX::XMLoadFloat3(&offset)));
        targetPos = l->PushFromObstructions(targetPos);
        // targetPos = l->PushFromEachOther( targetPos );
        l->m_unitTargetPos = targetPos;
      }
    }
  }
}

void Unit::FollowRoute()
{
  DEBUG_ASSERT(m_routeId != -1);
  Route* route = g_location->m_levelFile->GetRoute(m_routeId);
  DEBUG_ASSERT(route);

  if (m_routeWayPointId == -1)
  {
    m_routeWayPointId = 0;
  }

  WayPoint* waypoint = route->m_wayPoints[m_routeWayPointId];

  // LevelFile converts in T18, so GetPos still returns a legacy vector.
  m_wayPoint = DirectX::XMFLOAT3(waypoint->GetPos());

  DirectX::XMVECTOR const targetVect = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_wayPoint), DirectX::XMLoadFloat3(&m_centrePos));

  if (waypoint->m_type != WayPoint::TypeBuilding && DirectX::XMVectorGetX(DirectX::XMVector3Length(targetVect)) < 10.0f)
  {
    m_routeWayPointId++;
    if (m_routeWayPointId >= static_cast<int>(route->m_wayPoints.size()))
    {
      m_routeWayPointId = -1;
      m_routeId = -1;
    }
  }

  //
  // If its a building instead of a 3D pos, this unit will never
  // get to the next waypoint.  A new unit is created when the unit
  // enters the teleport, and taht new unit will automatically
  // continue towards the next waypoint instead.
  //
}


Entity* Unit::RayHit(DirectX::XMFLOAT3 const& _rayStart, DirectX::XMFLOAT3 const& _rayDir)
{
  for (unsigned int i = 0; i < m_entities.Size(); ++i)
  {
    if (m_entities.ValidIndex(i))
    {
      if (m_entities[i]->RayHit(_rayStart, _rayDir))
      {
        return m_entities[i];
      }
    }
  }

  return nullptr;
}

void Unit::DirectControl(TeamControls const& _teamControls) {}
