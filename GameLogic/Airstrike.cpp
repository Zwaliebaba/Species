#include "pch.h"
#include "SoundSources.h"
#include "Resource.h"
#include "DebugRender.h"
#include "Shape.h"
#include "HiResTime.h"

#include "Airstrike.h"

#include "Explosion.h"
#include "Location.h"

#include "SoundSystem.h"
#include "WorldPointers.h"


AirstrikeUnit::AirstrikeUnit(int teamId, int unitId, int numEntities, DirectX::XMFLOAT3 const& _pos)
  : Unit(Entity::TypeSpaceInvader, teamId, unitId, numEntities, _pos),
    m_numInvaders(numEntities),
    m_state(StateApproaching),
    m_speed(0.0f),
    m_effectId(-1)
{
  m_attackPosition = _pos;
  m_attackPosition.y = g_location->m_landscape.m_heightMap->GetValue(m_attackPosition.x, m_attackPosition.z) + 70.0f;

  m_speed = EntityBlueprint::GetStat(Entity::TypeSpaceInvader, Entity::StatSpeed) * 10.0f;
}


void AirstrikeUnit::Begin()
{
  float inset = 100.0f;
  float startHeight = 500.0f;
  float landSizeX = g_location->m_landscape.GetWorldSizeX();
  float landSizeZ = g_location->m_landscape.GetWorldSizeZ();

  std::vector<DirectX::XMFLOAT3> startPositions;
  startPositions.push_back(DirectX::XMFLOAT3(inset, startHeight, inset));
  startPositions.push_back(DirectX::XMFLOAT3(inset, startHeight, landSizeZ - inset));
  startPositions.push_back(DirectX::XMFLOAT3(landSizeX - inset, startHeight, landSizeZ - inset));
  startPositions.push_back(DirectX::XMFLOAT3(landSizeX - inset, startHeight, inset));

  int enterIndex = -1;
  int exitIndex = -1;
  float nearest = 99999.9f;

  //
  // Choose the nearest corner as the exit position

  for (int i = 0; i < static_cast<int>(startPositions.size()); ++i)
  {
    DirectX::XMFLOAT3 thisPos = startPositions[i];
    float thisDist = DirectX::XMVectorGetX(
      DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&thisPos), DirectX::XMLoadFloat3(&m_attackPosition))));
    if (thisDist < nearest)
    {
      nearest = thisDist;
      exitIndex = i;
    }
  }

  //
  // Choose the next nearest corner as the entry position

  nearest = 99999.9f;
  for (int i = 0; i < static_cast<int>(startPositions.size()); ++i)
  {
    if (i != exitIndex)
    {
      DirectX::XMFLOAT3 thisPos = startPositions[i];
      float thisDist = DirectX::XMVectorGetX(
        DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&thisPos), DirectX::XMLoadFloat3(&m_attackPosition))));
      if (thisDist < nearest)
      {
        nearest = thisDist;
        enterIndex = i;
      }
    }
  }


  //
  // Set up our flight path

  m_enterPosition = startPositions[enterIndex];
  m_exitPosition = startPositions[exitIndex];

  DirectX::XMFLOAT3 const centre(landSizeX / 2.0f, 0.0f, landSizeZ / 2.0f);
  DirectX::XMFLOAT3 front;
  DirectX::XMStoreFloat3(
    &front, DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&centre), DirectX::XMLoadFloat3(&m_enterPosition))));

  m_wayPoint = m_enterPosition;
  m_front = front;
  m_up = g_upVector;

  //
  // Spawn our space invaders

  g_location->SpawnEntities(m_enterPosition, m_teamId, m_unitId, Entity::TypeSpaceInvader, m_numInvaders, front, 0.0f);
}


bool AirstrikeUnit::AdvanceToTargetPosition(DirectX::XMFLOAT3 _targetPos)
{
  DirectX::XMVECTOR const wayPoint = DirectX::XMLoadFloat3(&m_wayPoint);
  DirectX::XMVECTOR const targetFront = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&_targetPos), wayPoint));

  float amountToTurn = SERVER_ADVANCE_PERIOD;
  DirectX::XMVECTOR const actualDir = DirectX::XMVector3Normalize(DirectX::XMVectorAdd(
    DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_front), 1.0f - amountToTurn), DirectX::XMVectorScale(targetFront, amountToTurn)));
  DirectX::XMStoreFloat3(&m_front, actualDir);

  // operator^ was the cross product.
  DirectX::XMVECTOR const upAxis = DirectX::g_XMIdentityR1;
  DirectX::XMVECTOR const right = DirectX::XMVector3Cross(actualDir, upAxis);
  DirectX::XMStoreFloat3(&m_up, DirectX::XMVector3Cross(right, actualDir));

  float distToTarget = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_attackPosition), wayPoint)));
  float desiredSpeed = EntityBlueprint::GetStat(Entity::TypeSpaceInvader, Entity::StatSpeed);
  if ((m_state == StateApproaching && distToTarget > 600.0f) || (m_state == StateLeaving && distToTarget > 100.0f))
  {
    desiredSpeed *= 10.0f;
  }
  m_speed = m_speed * (1.0f - amountToTurn) + desiredSpeed * amountToTurn;

  DirectX::XMStoreFloat3(&m_wayPoint, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_front),
                                                                   DirectX::XMVectorReplicate(m_speed * SERVER_ADVANCE_PERIOD), wayPoint));

  float newDistance = DirectX::XMVectorGetX(
    DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&_targetPos), DirectX::XMLoadFloat3(&m_wayPoint))));
  return (newDistance < 10.0f);
}


bool AirstrikeUnit::Advance(int _slice)
{
  //
  // Has our target marker moved?

  if (g_location->m_effects.ValidIndex(m_effectId))
  {
    WorldObject* targetMarker = g_location->m_effects[m_effectId];
    m_attackPosition = targetMarker->m_pos;
    m_attackPosition.y = g_location->m_landscape.m_heightMap->GetValue(m_attackPosition.x, m_attackPosition.z) + 70.0f;
  }

  switch (m_state)
  {
  case StateApproaching:
  {
    bool amIThere = AdvanceToTargetPosition(m_attackPosition);
    if (amIThere)
    {
      m_state = StateLeaving;
    }
    break;
  }

  case StateLeaving:
  {
    bool amIThere = AdvanceToTargetPosition(m_exitPosition);
    break;
  }
  };

  return Unit::Advance(_slice);
}


bool AirstrikeUnit::IsInView() { return true; }


void AirstrikeUnit::Render(float _predictionTime)
{
  // #ifdef DEBUG_RENDER_ENABLED
  //     glDisable( GL_TEXTURE_2D );
  //     Vector3 height(0,50,0);
  //     RenderArrow     ( m_enterPosition, m_attackPosition, 10.0f, RGBAColour(255,50,50,255) );
  //     RenderArrow     ( m_attackPosition, m_exitPosition, 10.0f, RGBAColour(255,50,50,255) );
  //
  //     Vector3 right = m_front ^ m_up;
  //     RenderSphere    ( m_wayPoint, 5.0f, RGBAColour(255,255,255,255) );
  //     RenderArrow     ( m_wayPoint, m_wayPoint + m_front * 50.0f, 10.0f, RGBAColour(255,255,255,255) );
  //     RenderArrow     ( m_wayPoint, m_wayPoint + m_up * 30.0f, 6.0f, RGBAColour(50,50,255,255) );
  //     RenderArrow     ( m_wayPoint, m_wayPoint + right * 30.0f, 6.0f, RGBAColour(50,255,50,255) );
  // #endif

  Unit::Render(_predictionTime);
}


SpaceInvader::SpaceInvader()
  : Entity(),
    m_armed(true)
{
  m_shape = g_resource->GetShape("SpaceInvader.shp");
  m_bombShape = g_resource->GetShape("Throwable.shp");
}


bool SpaceInvader::Advance(Unit* _unit)
{
  if (m_dead)
    return true;

  AirstrikeUnit* airstrikeUnit = (AirstrikeUnit*)_unit;

  //
  // Move a little

  DirectX::XMFLOAT3 targetPos = _unit->GetWayPoint();

  if (!DirectX::XMVector3Equal(DirectX::XMLoadFloat3(&targetPos), DirectX::XMVectorZero()))
  {
    DirectX::XMFLOAT3 const offset = _unit->GetFormationOffset(Unit::FormationAirstrike, m_formationIndex);
    DirectX::XMVECTOR const target = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&targetPos), DirectX::XMLoadFloat3(&offset));
    DirectX::XMStoreFloat3(&targetPos, target);

    // operator/ multiplied by a precomputed reciprocal.
    DirectX::XMStoreFloat3(&m_vel,
                           DirectX::XMVectorScale(DirectX::XMVectorSubtract(target, DirectX::XMLoadFloat3(&m_pos)), 1.0f / SERVER_ADVANCE_PERIOD));
    m_pos = targetPos;

    m_front = airstrikeUnit->m_front;
  }

  //
  // Drop bombs if near enough

  if (m_armed)
  {
    float distToTarget = DirectX::XMVectorGetX(
      DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&airstrikeUnit->m_attackPosition))));
    if (distToTarget < 90.0f)
    {
      DirectX::XMFLOAT3 dropPos;
      DirectX::XMStoreFloat3(&dropPos, DirectX::XMVectorNegativeMultiplySubtract(DirectX::g_XMIdentityR1, DirectX::XMVectorReplicate(12.0f),
                                                                                 DirectX::XMLoadFloat3(&m_pos)));
      float const speed = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&m_vel)));
      Grenade* weapon = new Grenade(dropPos, m_front, speed);
      weapon->m_type = EffectThrowableAirstrikeBomb;
      weapon->m_life = 1.5f;
      weapon->m_power = 50.0f;
      int index = g_location->m_effects.PutData(weapon);
      weapon->m_id.Set(m_id.GetTeamId(), UNIT_EFFECTS, index, -1);
      weapon->m_id.GenerateUniqueId();
      g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "DropGrenade");
      m_armed = false;
    }
  }


  //
  // Clip to edge of world

  float worldSizeX = g_location->m_landscape.GetWorldSizeX();
  float worldSizeZ = g_location->m_landscape.GetWorldSizeZ();

  if (m_pos.x < 0.0f || m_pos.z < 0.0f || m_pos.x >= worldSizeX || m_pos.z >= worldSizeZ)
  {
    return true;
  }

  return false;
}


void SpaceInvader::ChangeHealth(int _amount)
{
  // Space invaders are now INVINCIBLE

  //    bool dead = m_dead;
  //
  //    Entity::ChangeHealth( _amount );
  //
  //    if( m_dead && !dead )
  //    {
  //        // We just died
  //        Matrix34 transform( m_front, g_upVector, m_pos );
  //        g_explosionManager.AddExplosion( m_shape, transform );
  //    }
}


void SpaceInvader::ListSoundEvents(std::vector<const char*>* _list)
{
  Entity::ListSoundEvents(_list);

  _list->push_back("DropGrenade");
}


void SpaceInvader::Render(float _predictionTime)
{
  DirectX::XMFLOAT3 predictedPos;
  DirectX::XMStoreFloat3(&predictedPos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(_predictionTime),
                                                                     DirectX::XMLoadFloat3(&m_pos)));
  glDisable(GL_TEXTURE_2D);

#ifdef DEBUG_RENDER_ENABLED
  // RenderSphere( m_targetPos, 5.0f );
#endif

  g_renderer->SetObjectLighting();

  // BasisFromFrontAndUp is pinned against the legacy Matrix34(front, up, pos)
  // constructor in NeuronMathTests. Its rows are right, up, front, position in
  // that order, so scaling f, u and r means scaling rows 2, 1 and 0.
  DirectX::XMMATRIX basis = BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&predictedPos));
  basis.r[2] = DirectX::XMVectorScale(basis.r[2], 2.0f);
  basis.r[1] = DirectX::XMVectorScale(basis.r[1], 2.0f);
  basis.r[0] = DirectX::XMVectorScale(basis.r[0], 2.0f);
  DirectX::XMFLOAT4X4 mat;
  DirectX::XMStoreFloat4x4(&mat, basis);
  m_shape->Render(_predictionTime, mat);

  if (m_armed)
  {
    DirectX::XMVECTOR const upAxis = DirectX::g_XMIdentityR1;
    DirectX::XMVECTOR const bombPos =
      DirectX::XMVectorNegativeMultiplySubtract(upAxis, DirectX::XMVectorReplicate(12.0f), DirectX::XMLoadFloat3(&predictedPos));
    DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMVectorNegate(upAxis), DirectX::XMLoadFloat3(&m_front), bombPos));
    m_bombShape->Render(_predictionTime, mat);
  }

  g_renderer->UnsetObjectLighting();
}
