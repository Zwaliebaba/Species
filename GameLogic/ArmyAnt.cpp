#include "pch.h"
#include "SoundSources.h"
#include "Resource.h"
#include "Shape.h"
#include "MathUtils.h"
#include "DebugRender.h"
#include "Profiler.h"
#include "Preferences.h"

#include "SoundSystem.h"

#include "Location.h"
#include "Unit.h"
#include "Explosion.h"
#include "ParticleSystem.h"
#include "EntityGrid.h"

#include "ArmyAnt.h"
#include "AntHill.h"
#include "Citizen.h"
#include "WorldPointers.h"


ArmyAnt::ArmyAnt()
  : Entity(),
    m_orders(NoOrders),
    m_spiritId(-1),
    m_targetFound(false)
{
  m_type = TypeArmyAnt;

  m_shapes[0] = g_resource->GetShape("ArmyAnt.shp");
  m_shapes[1] = g_resource->GetShape("ArmyAnt2.shp");
  m_shapes[2] = g_resource->GetShape("ArmyAnt3.shp");

  m_shape = m_shapes[0];
  m_carryMarker = m_shape->m_rootFragment->LookupMarker("MarkerCarry");
}


void ArmyAnt::Begin()
{
  Entity::Begin();

  m_onGround = true;

  m_scale = 1.0f + syncsfrand(0.9f);

  float speed = m_stats[StatSpeed];
  speed *= (1.0f + syncsfrand(0.4f));
  if (speed < 0)
    speed = 0;
  if (speed > 255)
    speed = 255;
  m_stats[StatSpeed] = (unsigned char)speed;
}


void ArmyAnt::ChangeHealth(int _amount)
{
  bool dead = m_dead;

  Entity::ChangeHealth(_amount);

  if (m_dead && !dead)
  {
    //
    // We just died

    g_explosionManager.AddExplosion(m_shape, GetScaledLevelMatrix());


    //
    // Drop any spirits we are carrying

    if (m_spiritId != -1)
    {
      if (g_location->m_spirits.ValidIndex(m_spiritId))
      {
        Spirit* spirit = g_location->m_spirits.GetPointer(m_spiritId);
        if (spirit && spirit->m_state == Spirit::StateAttached)
        {
          spirit->CollectorDrops();
          spirit->m_vel = m_vel;
        }
      }
      m_spiritId = -1;
    }
  }
}


// The ant's shape is drawn and exploded at m_scale, and both sites scaled the
// three basis rows while leaving the position row alone. Stated once.
static DirectX::XMFLOAT4X4 ScaleAntBasis(DirectX::FXMMATRIX _basis, float _scale)
{
  DirectX::XMMATRIX mat = _basis;
  DirectX::XMVECTOR const scale = DirectX::XMVectorReplicate(_scale);
  mat.r[0] = DirectX::XMVectorMultiply(mat.r[0], scale);
  mat.r[1] = DirectX::XMVectorMultiply(mat.r[1], scale);
  mat.r[2] = DirectX::XMVectorMultiply(mat.r[2], scale);

  DirectX::XMFLOAT4X4 result;
  DirectX::XMStoreFloat4x4(&result, mat);
  return result;
}

// The death explosion levels the ant against the world up; Render uses the
// same world up. Both go through ScaleAntBasis above.
DirectX::XMFLOAT4X4 ArmyAnt::GetScaledLevelMatrix() const
{
  return ScaleAntBasis(BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)), m_scale);
}

bool ArmyAnt::Advance(Unit* _unit)
{
  bool amIDead = Entity::Advance(_unit);

  if (!m_onGround)
    AdvanceInAir(_unit);

  if (!amIDead && !m_dead && m_onGround)
  {
    switch (m_orders)
    {
    case NoOrders:
      OrderReturnToBase();
      break;
    case ScoutArea:
      amIDead = AdvanceScoutArea();
      break;
    case CollectSpirit:
      amIDead = AdvanceCollectSpirit();
      break;
    case CollectEntity:
      amIDead = AdvanceCollectEntity();
      break;
    case AttackEnemy:
      amIDead = AdvanceAttackEnemy();
      break;
    case ReturnToBase:
      amIDead = AdvanceReturnToBase();
      break;
    case BaseDestroyed:
      amIDead = AdvanceBaseDestroyed();
      break;
    }

    if (!m_targetFound)
      m_targetFound = SearchForTargets();

    //
    // Keep attached spirits attached to us

    if (g_location->m_spirits.ValidIndex(m_spiritId))
    {
      Spirit* spirit = g_location->m_spirits.GetPointer(m_spiritId);
      if (spirit && spirit->m_state == Spirit::StateAttached)
      {
        DirectX::XMFLOAT3 carryPos, carryVel;
        GetCarryMarker(carryPos, carryVel);
        spirit->m_pos = carryPos;
        spirit->m_vel = carryVel;
      }
    }
  }


  //
  // Move the legs on our model

  int currentIndex = -1;
  for (int i = 0; i < 3; ++i)
  {
    if (m_shape == m_shapes[i])
    {
      currentIndex = i;
      break;
    }
  }

  int newIndex = -1;
  while (true)
  {
    newIndex = speciesRandom() % 3;
    if (newIndex != currentIndex)
      break;
  }

  m_shape = m_shapes[newIndex];

  return amIDead;
}


bool ArmyAnt::AdvanceScoutArea()
{
  bool arrived = AdvanceToTargetPosition();
  if (arrived)
  {
    Entity* targetEntity = g_location->GetEntity(m_targetId);
    if (targetEntity)
    {
      if (targetEntity->m_type == Entity::TypeCitizen)
      {
        m_orders = CollectEntity;
      }
      else
      {
        m_orders = AttackEnemy;
      }
    }
    else
    {
      OrderReturnToBase();
    }
  }
  return false;
}


bool ArmyAnt::AdvanceCollectSpirit()
{
  Spirit* s = nullptr;
  if (g_location->m_spirits.ValidIndex(m_spiritId))
  {
    s = g_location->m_spirits.GetPointer(m_spiritId);
  }

  if (!s || s->m_state == Spirit::StateDeath || s->m_state == Spirit::StateAttached || s->m_state == Spirit::StateInEgg)
  {
    m_spiritId = -1;
    m_targetFound = false;
    OrderReturnToBase();
    return false;
  }

  m_wayPoint = s->m_pos;
  m_wayPoint.y = g_location->m_landscape.m_heightMap->GetValue(m_wayPoint.x, m_wayPoint.z);
  bool arrived = AdvanceToTargetPosition();
  if (arrived)
  {
    s->CollectorArrives();
    OrderReturnToBase();
  }

  return false;
}


bool ArmyAnt::AdvanceCollectEntity()
{
  //
  // Is our entity still here

  Entity* targetEntity = g_location->GetEntity(m_targetId);
  if (!targetEntity || targetEntity->m_dead)
  {
    m_targetId.SetInvalid();
    m_targetFound = false;
    OrderReturnToBase();
    return false;
  }


  //
  // Make sure he is still capturable

  Citizen* targetCitizen = (Citizen*)targetEntity;
  if (targetCitizen->m_state == Citizen::StateCapturedByAnt)
  {
    m_targetId.SetInvalid();
    m_targetFound = false;
    OrderReturnToBase();
    return false;
  }


  //
  // Go after him

  m_wayPoint = targetEntity->m_pos;
  bool arrived = AdvanceToTargetPosition();
  if (arrived)
  {
    targetCitizen->AntCapture(m_id);
    OrderReturnToBase();
  }

  return false;
}


bool ArmyAnt::AdvanceAttackEnemy()
{
  //
  // Is our entity still here

  Entity* targetEntity = g_location->GetEntity(m_targetId);
  if (!targetEntity || targetEntity->m_dead)
  {
    m_targetId.SetInvalid();
    m_targetFound = false;
    OrderReturnToBase();
    return false;
  }


  //
  // Go after him

  m_wayPoint = targetEntity->m_pos;

  if (targetEntity->m_type == TypeEngineer)
  {
    m_wayPoint.y = g_location->m_landscape.m_heightMap->GetValue(m_wayPoint.x, m_wayPoint.z);
  }

  bool arrived = AdvanceToTargetPosition();
  if (arrived)
  {
    targetEntity->ChangeHealth(-1);
    for (int i = 0; i < 3; ++i)
    {
      // The three syncsfrand calls stay in this order: they advance the RNG.
      DirectX::XMFLOAT3 const flashVel(syncsfrand(15.0f), syncsfrand(15.0f) + 15.0f, syncsfrand(15.0f));
      g_particleSystem->CreateParticle(m_pos, flashVel, Particle::TypeMuzzleFlash);
    }
    g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "Attack");
  }

  return false;
}


bool ArmyAnt::AdvanceReturnToBase()
{
  bool arrived = AdvanceToTargetPosition();
  if (arrived)
  {
    Building* building = g_location->GetBuilding(m_buildingId);

    if (!building)
    {
      // Our anthill has been destroyed
      bool newBaseFound = SearchForAntHill();
      if (!newBaseFound)
      {
        m_orders = BaseDestroyed;
      }
      return false;
    }
    else if (building && building->m_type == Building::TypeAntHill)
    {
      AntHill* antHill = (AntHill*)building;
      antHill->m_numAntsInside++;

      // Drop off any spirits we are carrying
      if (g_location->m_spirits.ValidIndex(m_spiritId))
      {
        Spirit* spirit = g_location->m_spirits.GetPointer(m_spiritId);
        if (spirit && spirit->m_state == Spirit::StateAttached)
        {
          antHill->m_numSpiritsInside++;
          g_location->m_spirits.MarkNotUsed(m_spiritId);
        }
      }

      // Any Citizens being carried are now killed
      Entity* entity = g_location->GetEntity(m_targetId);
      if (entity && entity->m_type == Entity::TypeCitizen)
      {
        Citizen* citizen = (Citizen*)entity;
        if (citizen->m_state == Citizen::StateCapturedByAnt)
        {
          citizen->ChangeHealth(citizen->m_stats[Entity::StatHealth] * -2);
        }
      }
    }

    return true;
  }

  return false;
}


bool ArmyAnt::AdvanceBaseDestroyed()
{
  bool arrived = AdvanceToTargetPosition();
  if (arrived)
  {
    SearchForRandomPosition();
  }

  return false;
}


void ArmyAnt::OrderReturnToBase()
{
  Building* building = g_location->GetBuilding(m_buildingId);
  if (building)
  {
    m_wayPoint = building->m_pos;
    m_wayPoint.y = g_location->m_landscape.m_heightMap->GetValue(m_wayPoint.x, m_wayPoint.z);
    m_orders = ReturnToBase;
  }
  else
  {
    bool newBaseFound = SearchForAntHill();
    if (!newBaseFound)
    {
      m_buildingId = -1;
      m_orders = BaseDestroyed;
    }
  }
}


bool ArmyAnt::SearchForTargets()
{
  bool targetFound = false;

  if (!targetFound)
    targetFound = SearchForSpirits();
  if (!targetFound)
    targetFound = SearchForEnemies();

  return targetFound;
}


bool ArmyAnt::SearchForSpirits()
{
  Spirit* found = nullptr;
  int spiritId = -1;
  float closest = 999999.9f;

  for (int i = 0; i < g_location->m_spirits.Size(); ++i)
  {
    if (g_location->m_spirits.ValidIndex(i))
    {
      Spirit* s = g_location->m_spirits.GetPointer(i);
      float theDist =
        DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&s->m_pos), DirectX::XMLoadFloat3(&m_pos))));

      if (theDist <= ARMYANT_SEARCHRANGE && theDist < closest && (s->m_state == Spirit::StateBirth || s->m_state == Spirit::StateFloating))
      {
        found = s;
        spiritId = i;
        closest = theDist;
      }
    }
  }

  if (found)
  {
    m_spiritId = spiritId;
    m_orders = CollectSpirit;
    return true;
  }

  return false;
}


bool ArmyAnt::SearchForEnemies()
{
  WorldObjectId enemyId = g_location->m_entityGrid->GetBestEnemy(m_pos.x, m_pos.z, 0.0f, ARMYANT_SEARCHRANGE, m_id.GetTeamId());
  Entity* enemy = g_location->GetEntity(enemyId);

  if (enemy && !enemy->m_dead && enemy->m_type != Entity::TypeCitizen)
  {
    m_targetId = enemyId;
    m_orders = AttackEnemy;
    return true;
  }

  return false;
}


bool ArmyAnt::SearchForAntHill()
{
  int buildingId = -1;
  float nearest = 500.0f;

  for (int i = 0; i < g_location->m_buildings.Size(); ++i)
  {
    if (g_location->m_buildings.ValidIndex(i))
    {
      Building* building = g_location->m_buildings[i];

      if (building->m_type == Building::TypeAntHill && g_location->IsFriend(building->m_id.GetTeamId(), m_id.GetTeamId()))
      {
        float distance = DirectX::XMVectorGetX(
          DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&building->m_pos), DirectX::XMLoadFloat3(&m_pos))));
        if (distance < nearest)
        {
          buildingId = building->m_id.GetUniqueId();
          nearest = distance;
        }
      }
    }
  }


  if (buildingId != -1)
  {
    m_buildingId = buildingId;
    OrderReturnToBase();
    return true;
  }

  return false;
}


bool ArmyAnt::SearchForRandomPosition()
{
  float distToSpawnPoint =
    DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&m_spawnPoint))));
  float chanceOfReturn = (distToSpawnPoint / 400.0f);
  if (chanceOfReturn >= 1.0f || syncfrand(1.0f) <= chanceOfReturn)
  {
    // We have strayed too far from our spawn point
    // So head back there now
    // SetLength. distToSpawnPoint is compared against 400 above and this branch
    // is reached when the ant has strayed, so the zero-length case is
    // unreachable and this takes the native normalise.
    DirectX::XMVECTOR const returnVector = DirectX::XMVectorScale(
      DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_spawnPoint), DirectX::XMLoadFloat3(&m_pos))), 100.0f);
    DirectX::XMStoreFloat3(&m_wayPoint, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), returnVector));
  }
  else
  {
    float distance = 100.0f;
    float angle = syncsfrand(2.0f * M_PI);

    m_wayPoint = DirectX::XMFLOAT3(m_pos.x + sinf(angle) * distance, m_pos.y, m_pos.z + cosf(angle) * distance);
    m_wayPoint = PushFromObstructions(m_wayPoint);
  }

  m_wayPoint.y = g_location->m_landscape.m_heightMap->GetValue(m_wayPoint.x, m_wayPoint.z);
  return true;
}


bool ArmyAnt::AdvanceToTargetPosition()
{
  //
  // Work out where we want to be next

  float speed = m_stats[StatSpeed];
  DirectX::XMFLOAT3 const oldPos = m_pos;

  if (m_orders == CollectEntity || m_orders == AttackEnemy)
    speed *= 2.0f;

  DirectX::XMVECTOR const ourPos = DirectX::XMLoadFloat3(&m_pos);
  DirectX::XMVECTOR const actualDir = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_wayPoint), ourPos));

  DirectX::XMFLOAT3 newPos;
  DirectX::XMStoreFloat3(&newPos, DirectX::XMVectorMultiplyAdd(actualDir, DirectX::XMVectorReplicate(speed * SERVER_ADVANCE_PERIOD), ourPos));
  // newPos = PushFromObstructions( newPos );
  newPos.y = g_location->m_landscape.m_heightMap->GetValue(newPos.x, newPos.z);

  DirectX::XMVECTOR const old = DirectX::XMLoadFloat3(&oldPos);
  DirectX::XMVECTOR moved = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&newPos), old);
  if (DirectX::XMVectorGetX(DirectX::XMVector3Length(moved)) > speed * SERVER_ADVANCE_PERIOD)
  {
    // SetLength, guarded by the Mag test above, so it cannot be reached with a
    // zero-length vector.
    moved = DirectX::XMVectorScale(DirectX::XMVector3Normalize(moved), speed * SERVER_ADVANCE_PERIOD);
  }

  DirectX::XMStoreFloat3(&m_pos, DirectX::XMVectorAdd(ourPos, moved));

  DirectX::XMVECTOR const movedFromOld = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), old);
  DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(movedFromOld, 1.0f / SERVER_ADVANCE_PERIOD));

  // m_front used newPos, not the clamped m_pos -- preserved as written.
  DirectX::XMVECTOR const facing = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&newPos), old));
  DirectX::XMStoreFloat3(&m_front, DirectX::XMVector3Transform(facing, DirectX::XMMatrixRotationY(syncsfrand(0.2f))));

  float distance =
    DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&m_wayPoint))));
  if (distance < DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&m_vel))) * SERVER_ADVANCE_PERIOD)
  {
    m_vel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
    return true;
  }

  return false;
}


void ArmyAnt::GetCarryMarker(DirectX::XMFLOAT3& _pos, DirectX::XMFLOAT3& _vel)
{
  // SurfaceMap2D<Vector3> still returns a Vector3 -- Landscape belongs to T18.
  DirectX::XMFLOAT3 const groundUp = g_location->m_landscape.m_normalMap->GetValue(m_pos.x, m_pos.z);

  DirectX::XMFLOAT4X4 mat;
  DirectX::XMStoreFloat4x4(&mat,
                           BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::XMLoadFloat3(&groundUp), DirectX::XMLoadFloat3(&m_pos)));

  _pos = m_carryMarker->GetWorldPosition(mat);
  _vel = m_vel;
}


void ArmyAnt::Render(float _predictionTime)
{
  if (m_dead)
    return;

  DirectX::XMVECTOR const predictedPos =
    DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(_predictionTime), DirectX::XMLoadFloat3(&m_pos));

  g_renderer->SetObjectLighting();
  glDisable(GL_TEXTURE_2D);

  DirectX::XMFLOAT4X4 const mat = ScaleAntBasis(BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, predictedPos), m_scale);

  m_shape->Render(_predictionTime, mat);

  g_renderer->UnsetObjectLighting();
}
