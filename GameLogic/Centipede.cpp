#include "pch.h"
#include "SoundSources.h"
#include "Resource.h"
#include "Matrix34.h"
#include "Shape.h"
#include "MathUtils.h"
#include "DebugRender.h"
#include "TextRenderer.h"
#include "Profiler.h"

#include "EntityGrid.h"
#include "Explosion.h"
#include "ProtocolLimits.h"
#include "Location.h"
#include "Team.h"
#include "Unit.h"
#include "GameTime.h"

#include "SoundSystem.h"

#include "Centipede.h"
#include "WorldPointers.h"

Shape* Centipede::s_shapeBody = nullptr;
Shape* Centipede::s_shapeHead = nullptr;


Centipede::Centipede()
  : Entity(),
    m_size(1.0f),
    m_linked(false),
    m_panic(0.0f),
    m_numSpiritsEaten(0),
    m_lastAdvance(0.0f)
{
  m_type = TypeCentipede;

  if (!s_shapeBody)
  {
    s_shapeBody = g_resource->GetShape("Centipede.shp");
    s_shapeHead = g_resource->GetShape("CentipedeHead.shp");
  }

  m_shape = s_shapeBody;
}


void Centipede::Begin()
{
  Entity::Begin();
  m_onGround = true;

  if (!m_next.IsValid())
  {
    //
    // Link every centipede in this unit into one long centipede

    Team* myTeam = &g_location->m_teams[m_id.GetTeamId()];
    Unit* myUnit = nullptr;
    if (myTeam->m_units.ValidIndex(m_id.GetUnitId()))
    {
      myUnit = myTeam->m_units[m_id.GetUnitId()];
    }

    if (myUnit)
    {
      float size = 0.2f * pow(1.1f, myUnit->m_entities.Size());
      size = std::min(size, 10.0f);

      Centipede* prev = nullptr;

      for (int i = 0; i < myUnit->m_entities.Size(); ++i)
      {
        if (myUnit->m_entities.ValidIndex(i))
        {
          Centipede* centipede = (Centipede*)myUnit->m_entities[i];
          centipede->m_size = size;
          size /= 1.1f;
          if (prev)
          {
            prev->m_prev = centipede->m_id;
            centipede->m_next = prev->m_id;
          }
          prev = centipede;
        }
      }
    }
  }

  int health = m_stats[StatHealth];
  health *= m_size * 2;
  if (health < 0)
    health = 0;
  if (health > 255)
    health = 255;
  m_stats[StatHealth] = health;

  m_radius = m_size * 10.0f;
}


void Centipede::ChangeHealth(int _amount)
{
  float maxHealth = EntityBlueprint::GetStat(TypeCentipede, StatHealth);
  maxHealth *= m_size * 2;
  if (maxHealth < 0)
    maxHealth = 0;
  if (maxHealth > 255)
    maxHealth = 255;


  bool dead = m_dead;
  int oldHealthBand = 3 * (m_stats[StatHealth] / maxHealth);
  Entity::ChangeHealth(_amount);
  int newHealthBand = 3 * (m_stats[StatHealth] / maxHealth);

  if (newHealthBand < oldHealthBand)
  {
    // We just took some bad damage
    Panic(2.0f + syncfrand(2.0f));
  }

  if (m_dead && !dead)
  {
    // We just died
    g_explosionManager.AddExplosion(m_shape, GetScaledLevelMatrix(DirectX::XMLoadFloat3(&m_pos)));

    Centipede* next = (Centipede*)g_location->GetEntitySafe(m_next, TypeCentipede);
    if (next)
      next->m_prev.SetInvalid();

    m_next.SetInvalid();
    m_prev.SetInvalid();
  }
}


void Centipede::Panic(float _time)
{
  if (m_panic <= 0.0f)
  {
    g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "Panic");
  }

  m_panic = std::max(_time, m_panic);

  if (m_next.IsValid())
  {
    //
    // We're not the head, so pass on towards the head
    WorldObject* wobj = g_location->GetEntity(m_next);
    Centipede* centipede = (Centipede*)wobj;
    centipede->Panic(_time);
  }
}


// The centipede is drawn and exploded at m_size, and every site scaled the
// three basis rows while leaving the position row alone.
static DirectX::XMFLOAT4X4 ScaleCentipedeBasis(DirectX::FXMMATRIX _basis, float _size)
{
  DirectX::XMMATRIX mat = _basis;
  DirectX::XMVECTOR const scale = DirectX::XMVectorReplicate(_size);
  mat.r[0] = DirectX::XMVectorMultiply(mat.r[0], scale);
  mat.r[1] = DirectX::XMVectorMultiply(mat.r[1], scale);
  mat.r[2] = DirectX::XMVectorMultiply(mat.r[2], scale);

  DirectX::XMFLOAT4X4 result;
  DirectX::XMStoreFloat4x4(&result, mat);
  return result;
}

DirectX::XMFLOAT4X4 Centipede::GetScaledLevelMatrix(DirectX::FXMVECTOR _position) const
{
  return ScaleCentipedeBasis(BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, _position), m_size);
}

bool Centipede::Advance(Unit* _unit)
{
  ASSERT_TEXT(_unit, "Centipedes must be created in a unit");

  if (m_dead)
    return AdvanceDead(_unit);

  m_onGround = true;
  m_lastAdvance = g_gameTime;

  bool recordPositionHistory = false;

  if (m_next.IsValid())
  {
    //
    // We are trailing, so just follow the leader

    m_shape = s_shapeBody;

    Centipede* centipede = (Centipede*)g_location->GetEntitySafe(m_next, TypeCentipede);
    if (centipede && !centipede->m_dead)
    {
      if (centipede->m_linked)
        recordPositionHistory = true;
      m_linked = centipede->m_linked && (static_cast<int>(m_positionHistory.size()) >= 2);
      DirectX::XMFLOAT3 trailPos, trailVel;
      int numSteps = 1;
      if (centipede->m_id.GetIndex() > m_id.GetIndex())
        numSteps = 0;
      bool success = centipede->GetTrailPosition(trailPos, trailVel, numSteps);
      if (success)
      {
        m_pos = trailPos;
        m_vel = trailVel;
        m_front = m_vel;
        DirectX::XMStoreFloat3(&m_front, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&m_front)));
        m_panic = centipede->m_panic;
      }
    }
    else
    {
      m_next.SetInvalid();
    }
  }
  else
  {
    //
    // We are a leader, so look for enemies

    EatSpirits();
    m_shape = s_shapeHead;
    m_linked = true;
    recordPositionHistory = true;

    if (m_panic > 0.0f)
    {
      m_targetEntity.SetInvalid();
      if (syncfrand(10.0f) < 5.0f)
      {
        SearchForRetreatPosition();
      }
      m_panic -= SERVER_ADVANCE_PERIOD;
    }
    else if (m_targetEntity.IsValid())
    {
      WorldObject* target = g_location->GetEntity(m_targetEntity);
      if (target)
      {
        m_targetPos = target->m_pos;
        m_targetPos.y = g_location->m_landscape.m_heightMap->GetValue(m_targetPos.x, m_targetPos.z);
      }
      else
      {
        m_targetEntity.SetInvalid();
      }
    }

    bool arrived = AdvanceToTargetPosition();
    // Was `m_targetPos == g_zeroVector`, which is Vector3::operator== -- a
    // PER-COMPONENT NearlyEquals at 1e-6, not an exact comparison.
    if (arrived || DirectX::XMVector3NearEqual(DirectX::XMLoadFloat3(&m_targetPos), DirectX::XMVectorZero(), DirectX::XMVectorReplicate(1e-6f)))
    {
      bool found = false;
      if (!found)
        found = SearchForTargetEnemy();
      if (!found)
        found = SearchForSpirits();
      if (!found)
        found = SearchForRandomPosition();
    }
  }


  //
  // Make sure we are roughly the right size

  float maxHealth = EntityBlueprint::GetStat(TypeCentipede, StatHealth);
  maxHealth *= m_size * 2;
  if (maxHealth < 0)
    maxHealth = 0;
  if (maxHealth > 255)
    maxHealth = 255;
  float healthFraction = (float)m_stats[StatHealth] / maxHealth;

  float timeIndex = g_gameTime + m_id.GetUniqueId() * 10;
  m_renderDamaged = (frand(0.75f) * (1.0f - fabs(sinf(timeIndex)) * 0.8f) > healthFraction);

  float targetSize = 0.0f;
  if (!m_prev.IsValid())
  {
    targetSize = 0.2f;
  }
  else
  {
    Centipede* prev = (Centipede*)g_location->GetEntitySafe(m_prev, TypeCentipede);
    targetSize = prev->m_size * 1.1f;
    targetSize = std::min(targetSize, 1.0f);
  }

  if (fabs(targetSize - m_size) > 0.01f)
  {
    m_size = m_size * 0.9f + targetSize * 0.1f;
    float maxHealth = EntityBlueprint::GetStat(TypeCentipede, StatHealth);
    maxHealth *= m_size * 2;
    if (maxHealth < 0)
      maxHealth = 0;
    if (maxHealth > 255)
      maxHealth = 255;
    float newHealth = maxHealth * healthFraction;
    newHealth = std::max(newHealth, 0.0f);
    newHealth = std::min(newHealth, 255.0f);
    m_stats[StatHealth] = newHealth;
  }

  if (recordPositionHistory)
  {
    RecordHistoryPosition();
  }

  if (_unit)
  {
    _unit->UpdateEntityPosition(m_pos, m_radius);
  }


  Attack(m_pos);


  return false;
}


void Centipede::Attack(DirectX::XMFLOAT3 const& _pos)
{
  int numFound;
  WorldObjectId* ids = g_location->m_entityGrid->GetEnemies(_pos.x, _pos.z, m_radius, &numFound, m_id.GetTeamId());

  for (int i = 0; i < numFound; ++i)
  {
    WorldObjectId id = ids[i];
    Entity* entity = (Entity*)g_location->GetEntity(id);
    DirectX::XMVECTOR pushVector = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&entity->m_pos), DirectX::XMLoadFloat3(&_pos));
    float distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(pushVector));
    if (distance < m_radius)
    {
      g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "Attack");

      // SetLength, whose zero-length fallback left (len, 0, 0). distance can be
      // exactly zero -- an entity standing on the centipede's own centre -- and
      // the native normalise would push it to QNaN, so the fallback is kept.
      pushVector = NearlyEquals(distance, 0.0f) ? DirectX::XMVectorSet(m_radius - distance, 0.0f, 0.0f, 0.0f)
                                                : DirectX::XMVectorScale(pushVector, (m_radius - distance) / distance);

      g_location->m_entityGrid->RemoveObject(id, entity->m_pos.x, entity->m_pos.z, entity->m_radius);
      DirectX::XMStoreFloat3(&entity->m_pos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&entity->m_pos), pushVector));
      g_location->m_entityGrid->AddObject(id, entity->m_pos.x, entity->m_pos.z, entity->m_radius);

      entity->ChangeHealth((m_radius - distance) * -10.0f);
    }
  }
}


void Centipede::EatSpirits()
{
  //
  // Are we already too big to eat spirits?

  int size = g_location->GetUnit(m_id)->NumAliveEntities();
  if (size > CENTIPEDE_MAXSIZE)
    return;

  std::vector<int> m_eaten;

  //
  // Find all spirits that we could potentially eat

  for (int i = 0; i < g_location->m_spirits.Size(); ++i)
  {
    if (g_location->m_spirits.ValidIndex(i))
    {
      Spirit* spirit = g_location->m_spirits.GetPointer(i);

      if (spirit->m_state == Spirit::StateFloating)
      {
        DirectX::XMVECTOR const theVector =
          DirectX::XMVectorSetY(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&spirit->m_pos), DirectX::XMLoadFloat3(&m_pos)), 0.0f);
        if (DirectX::XMVectorGetX(DirectX::XMVector3Length(theVector)) < CENTIPEDE_SPIRITEATRANGE)
        {
          m_eaten.push_back(i);
        }
      }
    }
  }


  //
  // Swallow all spirits

  float eatChance = m_size / 2.0f;

  for (int i = 0; i < static_cast<int>(m_eaten.size()); ++i)
  {
    if (syncfrand(1.0f) < eatChance)
    {
      int eatenIndex = m_eaten[i];
      g_location->m_spirits.MarkNotUsed(eatenIndex);
      ++m_numSpiritsEaten;
      break;
    }
  }


  //
  // Now try to grow

  if (m_numSpiritsEaten >= CENTIPEDE_NUMSPIRITSTOREGROW)
  {
    //
    // Find the tail centipede

    Centipede* tail = this;
    while (true)
    {
      Centipede* centipede = (Centipede*)g_location->GetEntitySafe(tail->m_prev, TypeCentipede);
      if (!centipede)
        break;
      tail = centipede;
    }


    //
    // Add one segment for every 3 spirits

    Team* myTeam = &g_location->m_teams[m_id.GetTeamId()];
    Unit* myUnit = myTeam->m_units[m_id.GetUnitId()];

    while (m_numSpiritsEaten >= CENTIPEDE_NUMSPIRITSTOREGROW)
    {
      int index;
      Centipede* centipede = (Centipede*)myUnit->NewEntity(&index);
      centipede->SetType(TypeCentipede);
      centipede->m_id.SetTeamId(m_id.GetTeamId());
      centipede->m_id.SetUnitId(m_id.GetUnitId());
      centipede->m_id.SetIndex(index);
      centipede->m_next = tail->m_id;
      centipede->m_prev.SetInvalid();
      tail->m_prev = centipede->m_id;

      centipede->m_pos = m_spawnPoint;
      centipede->m_size = tail->m_size;
      centipede->m_size = std::max(0.2f, centipede->m_size);
      centipede->m_spawnPoint = m_spawnPoint;
      centipede->m_roamRange = m_roamRange;
      centipede->Begin();

      g_location->m_entityGrid->AddObject(centipede->m_id, centipede->m_pos.x, centipede->m_pos.z, centipede->m_radius);
      g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "Grow");

      tail = centipede;
      m_numSpiritsEaten -= CENTIPEDE_NUMSPIRITSTOREGROW;
    }
  }
}


bool Centipede::SearchForRetreatPosition()
{
  float maxRange = CENTIPEDE_MAXSEARCHRANGE * m_size;

  int numFound;
  WorldObjectId* ids = g_location->m_entityGrid->GetEnemies(m_pos.x, m_pos.z, maxRange, &numFound, m_id.GetTeamId());

  WorldObjectId targetId;
  float bestDistance = 99999.9f;

  for (int i = 0; i < numFound; ++i)
  {
    WorldObjectId id = ids[i];
    WorldObject* entity = g_location->GetEntity(id);
    float distance = DirectX::XMVectorGetX(
      DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&entity->m_pos), DirectX::XMLoadFloat3(&m_pos))));
    if (distance < bestDistance)
    {
      bestDistance = distance;
      targetId = id;
    }
  }


  if (targetId.IsValid())
  {
    WorldObject* obj = g_location->GetEntity(targetId);
    DEBUG_ASSERT(obj);

    float distance = 50.0f;
    DirectX::XMVECTOR retreatVector =
      DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&obj->m_pos)));
    float angle = syncsfrand(M_PI * 1.0f);
    retreatVector = DirectX::XMVector3TransformNormal(retreatVector, DirectX::XMMatrixRotationY(angle));
    DirectX::XMStoreFloat3(&m_targetPos,
                           DirectX::XMVectorMultiplyAdd(retreatVector, DirectX::XMVectorReplicate(distance), DirectX::XMLoadFloat3(&m_pos)));
    m_targetPos = PushFromObstructions(m_targetPos);
    m_targetPos.y = g_location->m_landscape.m_heightMap->GetValue(m_targetPos.x, m_targetPos.z);
    return true;
  }

  return false;
}


bool Centipede::SearchForTargetEnemy()
{
  float maxRange = CENTIPEDE_MAXSEARCHRANGE * m_size;
  float minRange = CENTIPEDE_MINSEARCHRANGE * m_size;

  WorldObjectId targetId = g_location->m_entityGrid->GetBestEnemy(m_pos.x, m_pos.z, minRange, maxRange, m_id.GetTeamId());

  if (targetId.IsValid())
  {
    m_targetEntity = targetId;
    g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "EnemySighted");
    return true;
  }
  else
  {
    m_targetEntity.SetInvalid();
    return false;
  }
}


bool Centipede::SearchForSpirits()
{
  //
  // Are we already too big to eat spirits?

  int size = g_location->GetUnit(m_id)->NumAliveEntities();
  if (size > CENTIPEDE_MAXSIZE)
    return false;

  START_PROFILE(g_profiler, "SearchSpirits");
  Spirit* found = nullptr;
  float nearest = 9999.9f;

  for (int i = 0; i < g_location->m_spirits.Size(); ++i)
  {
    if (g_location->m_spirits.ValidIndex(i))
    {
      Spirit* s = g_location->m_spirits.GetPointer(i);
      float theDist =
        DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&s->m_pos), DirectX::XMLoadFloat3(&m_pos))));

      if (theDist <= CENTIPEDE_MAXSEARCHRANGE && theDist >= CENTIPEDE_MINSEARCHRANGE && theDist < nearest && s->m_state == Spirit::StateFloating)
      {
        found = s;
        nearest = theDist;
      }
    }
  }

  if (found)
  {
    m_targetPos = found->m_pos;
    m_targetPos.y = g_location->m_landscape.m_heightMap->GetValue(m_targetPos.x, m_targetPos.z);
  }

  END_PROFILE(g_profiler, "SearchSpirits");
  return found;
}


bool Centipede::SearchForRandomPosition()
{
  float distToSpawnPoint =
    DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&m_spawnPoint))));
  float chanceOfReturn = (distToSpawnPoint / m_roamRange);
  if (chanceOfReturn >= 1.0f || syncfrand(1.0f) <= chanceOfReturn)
  {
    // We have strayed too far from our spawn point
    // So head back there now
    // SetLength, unreachable at zero length: this branch needs the centipede to
    // have strayed from its spawn point.
    DirectX::XMVECTOR const returnVector = DirectX::XMVectorScale(
      DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_spawnPoint), DirectX::XMLoadFloat3(&m_pos))), 100.0f);
    DirectX::XMStoreFloat3(&m_targetPos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), returnVector));
  }
  else
  {
    float distance = 100.0f;
    float angle = syncsfrand(2.0f * M_PI);

    m_targetPos = DirectX::XMFLOAT3(m_pos.x + sinf(angle) * distance, m_pos.y, m_pos.z + cosf(angle) * distance);
    m_targetPos = PushFromObstructions(m_targetPos);
  }

  m_targetPos.y = g_location->m_landscape.m_heightMap->GetValue(m_targetPos.x, m_targetPos.z);
  return true;
}


void Centipede::RecordHistoryPosition()
{
  m_positionHistory.insert(m_positionHistory.begin(), m_pos);

  int maxHistorys = 3;

  for (int i = maxHistorys; i < static_cast<int>(m_positionHistory.size()); ++i)
  {
    m_positionHistory.erase(m_positionHistory.begin() + i);
  }
}


bool Centipede::GetTrailPosition(DirectX::XMFLOAT3& _pos, DirectX::XMFLOAT3& _vel, int _numSteps)
{
  if (static_cast<int>(m_positionHistory.size()) < 3)
    return false;

  float timeSinceAdvance = g_gameTime - m_lastAdvance;

  DirectX::XMVECTOR const pos1 = DirectX::XMLoadFloat3(&m_positionHistory[_numSteps + 1]);
  DirectX::XMVECTOR const pos2 = DirectX::XMLoadFloat3(&m_positionHistory[_numSteps]);
  DirectX::XMVECTOR const step = DirectX::XMVectorSubtract(pos2, pos1);

  DirectX::XMStoreFloat3(&_pos, DirectX::XMVectorMultiplyAdd(step, DirectX::XMVectorReplicate(1.0f - m_size), pos1));
  DirectX::XMStoreFloat3(&_vel, DirectX::XMVectorScale(step, 1.0f / SERVER_ADVANCE_PERIOD));

  return true;
}


bool Centipede::AdvanceToTargetPosition()
{
  float amountToTurn = SERVER_ADVANCE_PERIOD * 3.0f;
  if (m_next.IsValid())
    amountToTurn *= 1.5f;
  DirectX::XMVECTOR const ourPos = DirectX::XMLoadFloat3(&m_pos);
  DirectX::XMVECTOR const targetDir = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_targetPos), ourPos));
  DirectX::XMVECTOR const actualDir = DirectX::XMVector3Normalize(DirectX::XMVectorLerp(DirectX::XMLoadFloat3(&m_front), targetDir, amountToTurn));
  float speed = m_stats[StatSpeed];

  DirectX::XMFLOAT3 const oldPos = m_pos;
  DirectX::XMFLOAT3 newPos;
  DirectX::XMStoreFloat3(&newPos, DirectX::XMVectorMultiplyAdd(actualDir, DirectX::XMVectorReplicate(speed * SERVER_ADVANCE_PERIOD), ourPos));


  //
  // Slow us down if we're going up hill
  // Speed up if going down hill

  float currentHeight = g_location->m_landscape.m_heightMap->GetValue(oldPos.x, oldPos.z);
  float nextHeight = g_location->m_landscape.m_heightMap->GetValue(newPos.x, newPos.z);
  float factor = 1.0f - (currentHeight - nextHeight) / -10.0f;
  if (factor < 0.6f)
    factor = 0.6f;
  if (factor > 1.0f)
    factor = 1.0f;
  speed *= factor;

  DirectX::XMStoreFloat3(&newPos, DirectX::XMVectorMultiplyAdd(actualDir, DirectX::XMVectorReplicate(speed * SERVER_ADVANCE_PERIOD), ourPos));
  newPos.y = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);

  DirectX::XMVECTOR const old = DirectX::XMLoadFloat3(&oldPos);
  DirectX::XMVECTOR moved = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&newPos), old);
  if (DirectX::XMVectorGetX(DirectX::XMVector3Length(moved)) > speed * SERVER_ADVANCE_PERIOD)
  {
    // SetLength, guarded by the test above, so never zero-length here.
    moved = DirectX::XMVectorScale(DirectX::XMVector3Normalize(moved), speed * SERVER_ADVANCE_PERIOD);
  }

  DirectX::XMStoreFloat3(&m_pos, DirectX::XMVectorAdd(ourPos, moved));
  DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), old), 1.0f / SERVER_ADVANCE_PERIOD));
  DirectX::XMStoreFloat3(&m_front, actualDir);

  if (m_targetPos.y < 0.0f)
  {
    // We're about to go into the water
    return true;
  }

  int nearestBuildingId = g_location->GetBuildingId(m_pos, m_front, 255, 150.0f);
  if (nearestBuildingId != -1)
  {
    // We're on track to run into a building
    return true;
  }


  return DirectX::XMVectorGetX(
           DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&m_targetPos)))) < 20.0f;
}


void Centipede::ListSoundEvents(std::vector<const char*>* _list)
{
  Entity::ListSoundEvents(_list);

  _list->push_back("Panic");
  _list->push_back("EnemySighted");
  _list->push_back("Grow");
}


void Centipede::Render(float _predictionTime)
{
  DirectX::XMFLOAT3 predictedPos;
  DirectX::XMStoreFloat3(&predictedPos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(_predictionTime),
                                                                     DirectX::XMLoadFloat3(&m_pos)));
  predictedPos.y = g_location->m_landscape.m_heightMap->GetValue(predictedPos.x, predictedPos.z);

  float maxHealth = EntityBlueprint::GetStat(TypeCentipede, StatHealth);
  maxHealth *= m_size * 2;
  if (maxHealth < 0)
    maxHealth = 0;
  if (maxHealth > 255)
    maxHealth = 255;

  Shape* shape = m_shape;

  if (!m_dead && m_linked)
  {
    glDisable(GL_TEXTURE_2D);
    // RenderSphere( m_targetPos, 5.0f );

    // SurfaceMap2D<Vector3> still returns a Vector3 -- Landscape belongs to T18.
    DirectX::XMFLOAT3 const landUp = g_location->m_landscape.m_normalMap->GetValue(predictedPos.x, predictedPos.z);
    DirectX::XMVECTOR const predictedUp = DirectX::XMLoadFloat3(&landUp);
    DirectX::XMVECTOR const predictedRight = DirectX::XMVector3Cross(predictedUp, DirectX::XMLoadFloat3(&m_front));
    DirectX::XMVECTOR const predictedFront = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(predictedRight, predictedUp));

    DirectX::XMFLOAT4X4 const mat =
      ScaleCentipedeBasis(BasisFromFrontAndUp(predictedFront, predictedUp, DirectX::XMLoadFloat3(&predictedPos)), m_size);

    g_renderer->SetObjectLighting();
    shape->Render(_predictionTime, mat);
    g_renderer->UnsetObjectLighting();

    glDisable(GL_NORMALIZE);
  }
}


bool Centipede::IsInView()
{
  DirectX::XMFLOAT3 centre;
  DirectX::XMStoreFloat3(&centre, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&m_centrePos)));

  return g_camera->SphereInViewFrustum(centre, m_radius);
}


bool Centipede::RenderPixelEffect(float _predictionTime)
{
  Render(_predictionTime);

  DirectX::XMFLOAT3 predictedPos;
  DirectX::XMStoreFloat3(&predictedPos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(_predictionTime),
                                                                     DirectX::XMLoadFloat3(&m_pos)));
  predictedPos.y = g_location->m_landscape.m_heightMap->GetValue(predictedPos.x, predictedPos.z);

  if (!m_dead && m_linked)
  {
    // SurfaceMap2D<Vector3> still returns a Vector3 -- Landscape belongs to T18.
    DirectX::XMFLOAT3 const landUp = g_location->m_landscape.m_normalMap->GetValue(predictedPos.x, predictedPos.z);
    DirectX::XMVECTOR const predictedUp = DirectX::XMLoadFloat3(&landUp);
    DirectX::XMVECTOR const predictedRight = DirectX::XMVector3Cross(predictedUp, DirectX::XMLoadFloat3(&m_front));

    // NOT normalised, unlike the Render path a few lines up. Preserved as
    // written: BasisFromFrontAndUp leaves the front row alone, so an
    // unnormalised front scales the shape along its own axis, and that is what
    // the cell-marking pass has always seen.
    DirectX::XMVECTOR const predictedFront = DirectX::XMVector3Cross(predictedRight, predictedUp);

    DirectX::XMFLOAT4X4 const mat =
      ScaleCentipedeBasis(BasisFromFrontAndUp(predictedFront, predictedUp, DirectX::XMLoadFloat3(&predictedPos)), m_size);

    g_renderer->MarkUsedCells(m_shape, mat);
  }

  return true;
}
