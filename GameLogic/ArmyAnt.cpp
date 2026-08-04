#include "pch.h"
#include "SoundSources.h"
#include "Resource.h"
#include "Matrix34.h"
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

    Matrix34 transform(m_front, g_upVector, m_pos);
    transform.f *= m_scale;
    transform.u *= m_scale;
    transform.r *= m_scale;
    g_explosionManager.AddExplosion(m_shape, transform);


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
        Vector3 carryPos, carryVel;
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
      g_particleSystem->CreateParticle(m_pos, Vector3(syncsfrand(15.0f), syncsfrand(15.0f) + 15.0f, syncsfrand(15.0f)), Particle::TypeMuzzleFlash);
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
      float theDist = (AsLegacy(s->m_pos) - AsLegacy(m_pos)).Mag();

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
        float distance = (AsLegacy(building->m_pos) - AsLegacy(m_pos)).Mag();
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
  float distToSpawnPoint = (AsLegacy(m_pos) - AsLegacy(m_spawnPoint)).Mag();
  float chanceOfReturn = (distToSpawnPoint / 400.0f);
  if (chanceOfReturn >= 1.0f || syncfrand(1.0f) <= chanceOfReturn)
  {
    // We have strayed too far from our spawn point
    // So head back there now
    Vector3 returnVector = AsLegacy(m_spawnPoint) - AsLegacy(m_pos);
    returnVector.SetLength(100.0f);
    m_wayPoint = AsLegacy(m_pos) + returnVector;
  }
  else
  {
    float distance = 100.0f;
    float angle = syncsfrand(2.0f * M_PI);

    m_wayPoint = AsLegacy(m_pos) + Vector3(sinf(angle) * distance, 0.0f, cosf(angle) * distance);
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
  Vector3 oldPos = m_pos;

  if (AsLegacy(m_orders) == CollectEntity || AsLegacy(m_orders) == AttackEnemy)
    speed *= 2.0f;

  Vector3 actualDir = (m_wayPoint - AsLegacy(m_pos)).Normalise();
  Vector3 newPos = AsLegacy(m_pos) + actualDir * speed * SERVER_ADVANCE_PERIOD;
  // newPos = PushFromObstructions( newPos );
  newPos.y = g_location->m_landscape.m_heightMap->GetValue(newPos.x, newPos.z);
  Vector3 moved = newPos - oldPos;
  if (moved.Mag() > speed * SERVER_ADVANCE_PERIOD)
    moved.SetLength(speed * SERVER_ADVANCE_PERIOD);
  newPos = AsLegacy(m_pos) + moved;

  m_pos = newPos;
  m_vel = (AsLegacy(m_pos) - oldPos) / SERVER_ADVANCE_PERIOD;
  m_front = (newPos - oldPos).Normalise();
  AsLegacy(m_front).RotateAroundY(syncsfrand(0.2f));

  float distance = (AsLegacy(m_pos) - m_wayPoint).Mag();
  if (distance < AsLegacy(m_vel).Mag() * SERVER_ADVANCE_PERIOD)
  {
    AsLegacy(m_vel).Zero();
    return true;
  }

  return false;
}


void ArmyAnt::GetCarryMarker(Vector3& _pos, Vector3& _vel)
{
  Vector3 groundUp = g_location->m_landscape.m_normalMap->GetValue(m_pos.x, m_pos.z);
  Matrix34 mat(m_front, groundUp, m_pos);
  _pos = m_carryMarker->GetWorldMatrix(mat).pos;
  _vel = m_vel;
}


void ArmyAnt::Render(float _predictionTime)
{
  if (m_dead)
    return;

  Vector3 predictedPos = AsLegacy(m_pos) + AsLegacy(m_vel) * _predictionTime;
  Vector3 predictedUp = g_upVector;

  g_renderer->SetObjectLighting();
  glDisable(GL_TEXTURE_2D);

  Matrix34 mat(m_front, predictedUp, predictedPos);
  mat.u *= m_scale;
  mat.f *= m_scale;
  mat.r *= m_scale;

  m_shape->Render(_predictionTime, mat);

  g_renderer->UnsetObjectLighting();
}
