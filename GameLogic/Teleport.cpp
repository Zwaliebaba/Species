#include "pch.h"
#include "GlVertex.h"

#include <math.h>

#include "Debug.h"
#include "Shape.h"
#include "MathUtils.h"
#include "DebugRender.h"

#include "ProtocolLimits.h"
#include "Location.h"
#include "Team.h"
#include "Unit.h"
#include "TaskManager.h"
#include "GlobalWorld.h"
#include "RoutingSystem.h"

#include "Teleport.h"
#include "InsertionSquad.h"
#include "WorldPointers.h"

std::vector<TeleportMap> Teleport::m_teleportMap;


// *** Constructor
Teleport::Teleport()
  : Building(),
    m_timeSync(0.0f),
    m_sendPeriod(1.0f),
    m_entrance(nullptr)
{
}


// *** Initialise
void Teleport::SetShape(Shape* _shape)
{
  Building::SetShape(_shape);

  m_entrance = m_shape->m_rootFragment->LookupMarker("MarkerTeleportEntrance");
}


// *** Advance
bool Teleport::Advance()
{
  m_timeSync -= SERVER_ADVANCE_PERIOD;
  if (m_timeSync < 0.0f)
    m_timeSync = 0.0f;

  //
  // Advance people who are in transit

  for (int i = 0; i < static_cast<int>(m_inTransit.size()); ++i)
  {
    WorldObjectId* id = &m_inTransit[i];
    WorldObject* obj = g_location->GetEntity(*id);
    Entity* ent = (Entity*)obj;
    if (ent)
    {
      bool removeMe = UpdateEntityInTransit(ent);
      if (removeMe)
      {
        m_inTransit.erase(m_inTransit.begin() + i);
        --i;
      }
    }
    else
    {
      m_inTransit.erase(m_inTransit.begin() + i);
      --i;
    }
  }


  //
  // If a unit no longer exists, remove it from our teleport map
  // to prevent confusion (since unit ids are re-used)

  for (int i = 0; i < static_cast<int>(m_teleportMap.size()); ++i)
  {
    TeleportMap* map = &m_teleportMap[i];
    WorldObjectId unitId(map->m_teamId, map->m_fromUnitId, -1, -1);
    Unit* unit = g_location->GetUnit(unitId);
    if (!unit)
    {
      m_teleportMap.erase(m_teleportMap.begin() + i);
      --i;
    }
  }

  return Building::Advance();
}


bool Teleport::IsInView()
{
  DirectX::XMFLOAT3 const startPointStore = GetStartPoint();
  DirectX::XMFLOAT3 const endPointStore = GetEndPoint();
  DirectX::XMVECTOR const startPoint = DirectX::XMLoadFloat3(&startPointStore);
  DirectX::XMVECTOR const endPoint = DirectX::XMLoadFloat3(&endPointStore);

  DirectX::XMFLOAT3 midPoint;
  DirectX::XMStoreFloat3(&midPoint, DirectX::XMVectorScale(DirectX::XMVectorAdd(startPoint, endPoint), 0.5f));
  float radius = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(startPoint, endPoint))) / 2.0f;
  radius += m_radius;

  if (g_camera->SphereInViewFrustum(midPoint, radius))
  {
    return true;
  }

  return Building::IsInView();
}


void Teleport::RenderAlphas(float predictionTime)
{
  //    Vector3 pos, front;
  //    GetEntrance( pos, front );
  //    RenderSphere( pos, 3.0f, RGBAColour(255,0,0) );
  //    RenderSphere( pos, 15.0f, RGBAColour(255,0,0) );

  // RenderHitCheck();

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glDisable(GL_CULL_FACE);
  glDepthMask(false);

  for (int i = 0; i < static_cast<int>(m_inTransit.size()); ++i)
  {
    WorldObjectId* id = &m_inTransit[i];
    WorldObject* obj = g_location->GetEntity(*id);
    if (obj)
    {
      Entity* ent = (Entity*)obj;
      DirectX::XMFLOAT3 pos;
      DirectX::XMStoreFloat3(&pos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&obj->m_vel), DirectX::XMVectorReplicate(predictionTime),
                                                                DirectX::XMLoadFloat3(&obj->m_pos)));
      RenderSpirit(pos, ent->m_id.GetTeamId());
    }
  }

  glDepthMask(true);
  glEnable(GL_CULL_FACE);
  glDisable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  Building::RenderAlphas(predictionTime);
}


void Teleport::RenderSpirit(DirectX::XMFLOAT3 const& _pos, int _teamId)
{
  DirectX::XMVECTOR const pos = DirectX::XMLoadFloat3(&_pos);

  // Hoisted out of the eight vertices below, which each called them afresh.
  DirectX::XMFLOAT3 const camUpStore = g_camera->GetUp();
  DirectX::XMFLOAT3 const camRightStore = g_camera->GetRight();
  DirectX::XMVECTOR const camUp = DirectX::XMLoadFloat3(&camUpStore);
  DirectX::XMVECTOR const camRight = DirectX::XMLoadFloat3(&camRightStore);

  int innerAlpha = 100;
  int outerAlpha = 50;
  int spiritInnerSize = 2;
  int spiritOuterSize = 6;

  RGBAColour colour;
  if (_teamId >= 0)
    colour = g_location->m_teams[_teamId].m_colour;

  float size = spiritInnerSize;
  glColor4ub(colour.r, colour.g, colour.b, innerAlpha);

  glBegin(GL_QUADS);
  EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camUp, DirectX::XMVectorReplicate(size), pos));
  EmitVertex(DirectX::XMVectorMultiplyAdd(camRight, DirectX::XMVectorReplicate(size), pos));
  EmitVertex(DirectX::XMVectorMultiplyAdd(camUp, DirectX::XMVectorReplicate(size), pos));
  EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camRight, DirectX::XMVectorReplicate(size), pos));
  glEnd();

  size = spiritOuterSize;
  glColor4ub(colour.r, colour.g, colour.b, outerAlpha);
  glBegin(GL_QUADS);
  EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camUp, DirectX::XMVectorReplicate(size), pos));
  EmitVertex(DirectX::XMVectorMultiplyAdd(camRight, DirectX::XMVectorReplicate(size), pos));
  EmitVertex(DirectX::XMVectorMultiplyAdd(camUp, DirectX::XMVectorReplicate(size), pos));
  EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camRight, DirectX::XMVectorReplicate(size), pos));
  glEnd();
}


bool Teleport::Connected() { return false; }


bool Teleport::ReadyToSend() { return (m_timeSync <= 0.0f); }

void Teleport::EnterTeleport(WorldObjectId _id, bool _relay)
{
  WorldObject* wobj = g_location->GetEntity(_id);
  Entity* entity = (Entity*)wobj;

  if (entity)
  {
    DirectX::XMFLOAT3 oldPos = entity->m_pos;

    if (!_relay)
    {
      Unit* oldUnit = g_location->GetUnit(_id);
      if (oldUnit)
      {
        //
        // Look for the new unit that i'm going to

        int newUnitId = -1;
        for (int i = 0; i < static_cast<int>(m_teleportMap.size()); ++i)
        {
          TeleportMap* map = &m_teleportMap[i];
          if (map->m_teamId == _id.GetTeamId() && map->m_fromUnitId == _id.GetUnitId())
          {
            newUnitId = map->m_toUnitId;
          }
        }

        DEBUG_ASSERT(oldUnit);

        Unit* newUnit = nullptr;

        if (newUnitId != -1)
        {
          if (g_location->m_teams[_id.GetTeamId()].m_units.ValidIndex(newUnitId))
          {
            newUnit = g_location->m_teams[_id.GetTeamId()].m_units[newUnitId];
          }
          else
          {
            newUnitId = -1;
          }
        }

        if (newUnitId == -1)
        {
          //
          // Oh well, i'm the first, so create a new unit
          newUnit = g_location->m_teams[_id.GetTeamId()].NewUnit(oldUnit->m_troopType, oldUnit->m_entities.NumUsed(), &newUnitId, m_pos);

          //
          // Special case :
          // If this was an insertion squad we just broke up,
          // then we must create a Task to represent the new squad

          if (oldUnit->m_troopType == Entity::TypeInsertionSquadie)
          {
            // Shut down the old task
            for (int i = 0; i < static_cast<int>(g_taskManager->m_tasks.size()); ++i)
            {
              Task* task = g_taskManager->m_tasks[i];
              if (task->m_type == GlobalResearch::TypeSquad && task->m_objId == WorldObjectId(oldUnit->m_teamId, oldUnit->m_unitId, -1, -1))
              {
                g_taskManager->m_tasks.erase(g_taskManager->m_tasks.begin() + i);
                delete task;
                break;
              }
            }

            Task* task = new Task();
            task->m_type = GlobalResearch::TypeSquad;
            task->m_state = Task::StateRunning;
            task->m_objId.Set(newUnit->m_teamId, newUnit->m_unitId, -1, -1);
            bool success = g_taskManager->RegisterTask(task);
            if (success)
              g_taskManager->SelectTask(task->m_id);

            ((InsertionSquad*)newUnit)->m_weaponType = ((InsertionSquad*)oldUnit)->m_weaponType;
            ((InsertionSquad*)newUnit)->m_controllerId = ((InsertionSquad*)oldUnit)->m_controllerId;
            ((InsertionSquad*)oldUnit)->m_controllerId = -1;
            Task* controller = g_taskManager->GetTask(((InsertionSquad*)newUnit)->m_controllerId);
            if (controller)
            {
              controller->m_objId = task->m_objId;
              controller->m_route->AddWayPoint(m_id.GetUniqueId());
            }
          }


          if (oldUnit->m_routeId != -1)
          {
            newUnit->m_routeId = oldUnit->m_routeId;
            newUnit->m_routeWayPointId = oldUnit->m_routeWayPointId + 1;
          }
          else
          {
            DirectX::XMFLOAT3 exitPos, exitFront;
            GetExit(exitPos, exitFront);

            // The two syncsfrand calls stay in this order: they advance the
            // synchronised RNG, and the migration may not change the sequence.
            DirectX::XMFLOAT3 const scatter(syncsfrand(35.0f), 0.0f, syncsfrand(35.0f));
            DirectX::XMFLOAT3 newWayPoint;
            DirectX::XMStoreFloat3(
              &newWayPoint, DirectX::XMVectorAdd(DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&exitFront), DirectX::XMVectorReplicate(50.0f),
                                                                              DirectX::XMLoadFloat3(&exitPos)),
                                                 DirectX::XMLoadFloat3(&scatter)));
            newWayPoint.y = g_location->m_landscape.m_heightMap->GetValue(newWayPoint.x, newWayPoint.z);
            newUnit->SetWayPoint(newWayPoint);
          }

          TeleportMap map;
          map.m_teamId = entity->m_id.GetTeamId();
          map.m_fromUnitId = oldUnit->m_unitId;
          map.m_toUnitId = newUnit->m_unitId;
          m_teleportMap.insert(m_teleportMap.begin(), map);
        }

        DEBUG_ASSERT(newUnit);


        // Put me into the new unit

        int newUnitIndex;
        Entity* newEntity = newUnit->NewEntity(&newUnitIndex);
        int newUniqueId = newEntity->m_id.GetUniqueId();
        *newEntity = *entity;
        entity = newEntity;
        entity->m_id.SetUnitId(newUnitId);
        entity->m_id.SetIndex(newUnitIndex);
        entity->m_id.SetUniqueId(newUniqueId);
        entity->m_onGround = false;
        entity->m_enabled = false;
        entity->m_vel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

        newUnit->RecalculateOffsets();
      }
    }

    entity->m_pos = GetStartPoint();
    UpdateEntityInTransit(entity);

    WorldObjectId newId(entity->m_id);
    m_inTransit.push_back(newId);
    m_timeSync = m_sendPeriod;
  }
}

bool Teleport::GetEntrance(DirectX::XMFLOAT3& _pos, DirectX::XMFLOAT3& _front)
{
  DirectX::XMFLOAT4X4 rootMat = GetWorldMatrix();

  DirectX::XMFLOAT4X4 const worldMat = m_entrance->GetWorldMatrix(rootMat);
  _pos = DirectX::XMFLOAT3(worldMat._41, worldMat._42, worldMat._43);
  _front = DirectX::XMFLOAT3(worldMat._31, worldMat._32, worldMat._33);
  return true;
}

bool Teleport::GetExit(DirectX::XMFLOAT3& _pos, DirectX::XMFLOAT3& _front)
{
  DEBUG_ASSERT(false);
  return false;
}

DirectX::XMFLOAT3 Teleport::GetStartPoint()
{
  DEBUG_ASSERT(false);

  // Vector3() zeroed; XMFLOAT3() does not, so this states the zero.
  return DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
}

DirectX::XMFLOAT3 Teleport::GetEndPoint()
{
  DEBUG_ASSERT(false);
  return DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
}

bool Teleport::UpdateEntityInTransit(Entity* _entity)
{
  DEBUG_ASSERT(false);
  return false;
}
