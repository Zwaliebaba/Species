#include "pch.h"
#include "GlVertex.h"
#include "SoundSources.h"
#include "Resource.h"
#include "Matrix34.h"
#include "Shape.h"
#include "MathUtils.h"
#include "DebugRender.h"
#include "TextRenderer.h"
#include "HiResTime.h"

#include "EntityGrid.h"
#include "Explosion.h"
#include "ProtocolLimits.h"
#include "Location.h"
#include "Unit.h"
#include "GameTime.h"
#include "ParticleSystem.h"

#include "SoundSystem.h"

#include "SoulDestroyer.h"
#include "WorldPointers.h"

Shape* SoulDestroyer::s_shapeHead = nullptr;
Shape* SoulDestroyer::s_shapeTail = nullptr;
ShapeMarker* SoulDestroyer::s_tailMarker = nullptr;


SoulDestroyer::SoulDestroyer()
  : Entity(),
    m_panic(0.0f),
    m_retargetTimer(0.0f)
{
  m_type = TypeSoulDestroyer;

  if (!s_shapeTail || !s_shapeHead)
  {
    s_shapeTail = g_resource->GetShape("SoulDestroyerTail.shp");
    s_shapeHead = g_resource->GetShape("SoulDestroyerHead.shp");

    s_tailMarker = s_shapeHead->m_rootFragment->LookupMarker("MarkerTail");
  }

  m_shape = s_shapeHead;

  float range = 10.0f;
  for (int i = 0; i < SOULDESTROYER_MAXSPIRITS; ++i)
  {
    // The three syncsfrand calls stay in this order: they advance the RNG.
    m_spiritPosition[i] = DirectX::XMFLOAT3(syncsfrand(range), syncsfrand(range), syncsfrand(range));
  }

  m_routeTriggerDistance = 50.0f;
}


// The tail segments are drawn, exploded and shadowed from the same pair of
// history points, and every site scaled the three basis rows while leaving the
// position row alone.
static DirectX::XMFLOAT4X4 ScaledTailBasis(DirectX::FXMVECTOR _front, DirectX::FXMVECTOR _up, DirectX::FXMVECTOR _position, float _scale)
{
  DirectX::XMMATRIX mat = BasisFromFrontAndUp(_front, _up, _position);
  DirectX::XMVECTOR const scale = DirectX::XMVectorReplicate(_scale);
  mat.r[0] = DirectX::XMVectorMultiply(mat.r[0], scale);
  mat.r[1] = DirectX::XMVectorMultiply(mat.r[1], scale);
  mat.r[2] = DirectX::XMVectorMultiply(mat.r[2], scale);

  DirectX::XMFLOAT4X4 result;
  DirectX::XMStoreFloat4x4(&result, mat);
  return result;
}

void SoulDestroyer::Begin()
{
  Entity::Begin();

  m_up = g_upVector;
  m_pos.y = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);

  // This line to make it a bit easier to hit them with missiles
  m_radius *= 1.5f;
}


void SoulDestroyer::ChangeHealth(int _amount)
{
  if (!m_dead && _amount < 0)
  {
    Entity::ChangeHealth(_amount);

    float fractionDead = 1.0f - (float)m_stats[StatHealth] / (float)EntityBlueprint::GetStat(TypeSoulDestroyer, StatHealth);
    fractionDead = std::max(fractionDead, 0.0f);
    fractionDead = std::min(fractionDead, 1.0f);
    if (m_dead)
      fractionDead = 1.0f;

    Panic(2.0f + syncfrand(2.0f));

    DirectX::XMFLOAT4X4 transform;
    DirectX::XMStoreFloat4x4(&transform,
                             BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::XMLoadFloat3(&m_up), DirectX::XMLoadFloat3(&m_pos)));
    g_explosionManager.AddExplosion(m_shape, transform, fractionDead);

    if (m_dead)
    {
      // We just died
      for (int i = 1; i < static_cast<int>(m_positionHistory.size()); i += 1)
      {
        DirectX::XMVECTOR const p1 = DirectX::XMLoadFloat3(&m_positionHistory[i]);
        DirectX::XMVECTOR const p2 = DirectX::XMLoadFloat3(&m_positionHistory[i - 1]);
        DirectX::XMVECTOR const step = DirectX::XMVectorSubtract(p2, p1);

        DirectX::XMVECTOR pos = DirectX::XMVectorAdd(p1, step);
        DirectX::XMVECTOR const front = DirectX::XMVector3Normalize(step);
        DirectX::XMVECTOR const right = DirectX::XMVector3Cross(front, DirectX::g_XMIdentityR1);
        DirectX::XMVECTOR const up = DirectX::XMVector3Cross(right, front);

        float scale = 1.0f - ((float)i / (float)static_cast<int>(m_positionHistory.size()));
        scale *= 1.5f;
        if (i == static_cast<int>(m_positionHistory.size()) - 1)
          scale = 0.8f;
        scale = std::max(scale, 0.5f);

        g_explosionManager.AddExplosion(m_shape, ScaledTailBasis(front, up, pos, scale), 1.0f);
      }
    }
  }
}


bool SoulDestroyer::Advance(Unit* _unit)
{
  if (m_dead)
    return AdvanceDead(_unit);

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
      float distance = DirectX::XMVectorGetX(
        DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&target->m_pos), DirectX::XMLoadFloat3(&m_pos))));
      m_targetPos = target->m_pos;

      if (distance > SOULDESTROYER_MAXSEARCHRANGE)
      {
        m_targetEntity.SetInvalid();
      }
    }
    else
    {
      m_targetEntity.SetInvalid();
    }
  }

  m_retargetTimer -= SERVER_ADVANCE_PERIOD;

  bool arrived = AdvanceToTargetPosition();
  // Was `m_targetPos == g_zeroVector`, which is Vector3::operator== -- a
  // PER-COMPONENT NearlyEquals at 1e-6, not an exact comparison.
  if (arrived || DirectX::XMVector3NearEqual(DirectX::XMLoadFloat3(&m_targetPos), DirectX::XMVectorZero(), DirectX::XMVectorReplicate(1e-6f)) ||
      m_retargetTimer < 0.0f)
  {
    m_retargetTimer = 5.0f;
    bool found = false;
    if (!found && m_panic < 0.1f)
      found = SearchForTargetEnemy();
    if (!found)
      found = SearchForRandomPosition();
  }

  RecordHistoryPosition();

  if (m_panic < 0.1f)
    Attack(m_pos);

  return Entity::Advance(_unit);
}


void SoulDestroyer::Attack(DirectX::XMFLOAT3 const& _pos)
{
  int numFound;
  WorldObjectId* ids = g_location->m_entityGrid->GetEnemies(_pos.x, _pos.z, SOULDESTROYER_DAMAGERANGE, &numFound, m_id.GetTeamId());

  for (int i = 0; i < numFound; ++i)
  {
    WorldObjectId id = ids[i];
    Entity* entity = (Entity*)g_location->GetEntity(id);
    bool killed = false;

    DirectX::XMVECTOR pushVector = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&entity->m_pos), DirectX::XMLoadFloat3(&_pos));
    float distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(pushVector));
    if (distance < SOULDESTROYER_DAMAGERANGE)
    {
      g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "Attack");

      // SetLength, whose zero-length fallback left (len, 0, 0). distance can be
      // exactly zero, so the fallback is kept rather than storing QNaN into an
      // entity's position.
      pushVector = NearlyEquals(distance, 0.0f) ? DirectX::XMVectorSet(SOULDESTROYER_DAMAGERANGE - distance, 0.0f, 0.0f, 0.0f)
                                                : DirectX::XMVectorScale(pushVector, (SOULDESTROYER_DAMAGERANGE - distance) / distance);

      g_location->m_entityGrid->RemoveObject(id, entity->m_pos.x, entity->m_pos.z, entity->m_radius);
      DirectX::XMStoreFloat3(&entity->m_pos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&entity->m_pos), pushVector));
      g_location->m_entityGrid->AddObject(id, entity->m_pos.x, entity->m_pos.z, entity->m_radius);

      bool dead = entity->m_dead;
      entity->ChangeHealth((SOULDESTROYER_DAMAGERANGE - distance) * -50.0f);
      if (!dead && entity->m_dead)
        killed = true;
    }

    if (killed && entity->m_type == TypeCitizen)
    {
      // Eat the spirit
      int spiritIndex = g_location->GetSpirit(id);
      if (spiritIndex != -1)
      {
        g_location->m_spirits.MarkNotUsed(spiritIndex);
        if (m_spirits.NumUsed() < SOULDESTROYER_MAXSPIRITS)
        {
          m_spirits.PutData((float)GetHighResTime());
        }
        else
        {
          int index = syncrand() % SOULDESTROYER_MAXSPIRITS;
          m_spirits.PutData((float)GetHighResTime(), index);
        }
      }


      // Create a zombie
      Zombie* zombie = new Zombie();
      zombie->m_pos = entity->m_pos;
      zombie->m_front = entity->m_front;
      zombie->m_up = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);

      // RotateAround's angle is the vector's magnitude, with its own 1e-8 guard.
      DirectX::XMVECTOR const spinAxis = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&zombie->m_front), syncsfrand());
      float const spinLengthSquared = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(spinAxis));
      if (spinLengthSquared >= 1e-8f)
      {
        float const spin = sqrtf(spinLengthSquared);
        DirectX::XMStoreFloat3(&zombie->m_up,
                               DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&zombie->m_up),
                                                           DirectX::XMMatrixRotationAxis(DirectX::XMVectorScale(spinAxis, 1.0f / spin), spin)));
      }
      DirectX::XMStoreFloat3(&zombie->m_vel, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), 0.5f));
      zombie->m_vel.y = 20.0f + syncfrand(25.0f);
      int index = g_location->m_effects.PutData(zombie);
      zombie->m_id.Set(id.GetTeamId(), UNIT_EFFECTS, index, -1);
      zombie->m_id.GenerateUniqueId();
    }
  }
}


void SoulDestroyer::Panic(float _time)
{
  if (m_panic <= 0.0f)
  {
    g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "Panic");
  }

  m_panic = std::max(_time, m_panic);
}


bool SoulDestroyer::SearchForRetreatPosition()
{
  WorldObjectId targetId = g_location->m_entityGrid->GetBestEnemy(m_pos.x, m_pos.z, 0.0f, SOULDESTROYER_MAXSEARCHRANGE, m_id.GetTeamId());

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
    m_targetPos.y = std::min(m_targetPos.y, 300.0f);
    return true;
  }

  return false;
}


bool SoulDestroyer::SearchForTargetEnemy()
{
  if (m_routeId != -1)
  {
    // dont attack citizens if on a pre-set route
    return false;
  }
  // If we are too close to the ground, we MUST take off
  float landHeight = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);
  if (fabs(m_pos.y - landHeight) < 50.0f)
  {
    m_targetEntity.SetInvalid();
    return false;
  }

  // If we are too high up we don't see any enemies
  //    if( m_pos.y > landHeight + 100.0f )
  //    {
  //        m_targetEntity.SetInvalid();
  //        return false;
  //    }

  float maxRange = SOULDESTROYER_MAXSEARCHRANGE;
  float minRange = SOULDESTROYER_MINSEARCHRANGE;

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


bool SoulDestroyer::SearchForRandomPosition()
{
  DirectX::XMVECTOR const toSpawnPoint =
    DirectX::XMVectorSetY(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&m_spawnPoint)), 0.0f);
  float distToSpawnPoint = DirectX::XMVectorGetX(DirectX::XMVector3Length(toSpawnPoint));
  float chanceOfReturn = (distToSpawnPoint / m_roamRange);
  if (chanceOfReturn >= 1.0f || syncfrand(1.0f) <= chanceOfReturn)
  {
    // We have strayed too far from our spawn point
    // So head back there now
    DirectX::XMFLOAT3 targetPos = m_spawnPoint;
    targetPos.y = g_location->m_landscape.m_heightMap->GetValue(targetPos.x, targetPos.z);
    targetPos.y += 100.0f + syncsfrand(100.0f);

    // SetLength, unreachable at zero length: this branch needs the destroyer to
    // have strayed from its spawn point.
    DirectX::XMVECTOR const returnVector = DirectX::XMVectorScale(
      DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&targetPos), DirectX::XMLoadFloat3(&m_pos))), 160.0f);
    DirectX::XMStoreFloat3(&m_targetPos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), returnVector));
  }
  else
  {
    float distance = 160.0f;
    float angle = syncsfrand(2.0f * M_PI);

    m_targetPos = DirectX::XMFLOAT3(m_pos.x + sinf(angle) * distance, m_pos.y, m_pos.z + cosf(angle) * distance);
    m_targetPos.y = g_location->m_landscape.m_heightMap->GetValue(m_targetPos.x, m_targetPos.z);
    m_targetPos.y += (100.0f + syncsfrand(100.0f));
  }

  return true;
}


void SoulDestroyer::RecordHistoryPosition()
{
  DirectX::XMFLOAT4X4 mat;
  DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::XMLoadFloat3(&m_up), DirectX::XMLoadFloat3(&m_pos)));

  // ShapeMarker::GetWorldMatrix still returns Matrix34 -- T10's seam.
  DirectX::XMFLOAT3 const tailPos = s_tailMarker->GetWorldMatrix(mat).pos;
  m_positionHistory.insert(m_positionHistory.begin(), tailPos);

  // int maxHistorys = 11;
  int maxHistorys = m_roamRange / 30.0f;
  maxHistorys = std::max(9, maxHistorys);
  maxHistorys = std::min(25, maxHistorys);

  for (int i = maxHistorys; i < static_cast<int>(m_positionHistory.size()); ++i)
  {
    m_positionHistory.erase(m_positionHistory.begin() + i);
  }
}


bool SoulDestroyer::GetTrailPosition(DirectX::XMFLOAT3& _pos, DirectX::XMFLOAT3& _vel)
{
  if (static_cast<int>(m_positionHistory.size()) < 2)
    return false;

  DirectX::XMVECTOR const pos1 = DirectX::XMLoadFloat3(&m_positionHistory[1]);
  DirectX::XMVECTOR const pos2 = DirectX::XMLoadFloat3(&m_positionHistory[0]);
  DirectX::XMVECTOR const step = DirectX::XMVectorSubtract(pos2, pos1);

  DirectX::XMStoreFloat3(&_pos, DirectX::XMVectorAdd(pos1, step));
  DirectX::XMStoreFloat3(&_vel, DirectX::XMVectorScale(step, 1.0f / SERVER_ADVANCE_PERIOD));

  return true;
}


bool SoulDestroyer::AdvanceToTargetPosition()
{
  float amountToTurn = SERVER_ADVANCE_PERIOD * 2.0f;
  DirectX::XMVECTOR const ourPos = DirectX::XMLoadFloat3(&m_pos);
  DirectX::XMVECTOR targetDir = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_targetPos), ourPos));
  if (!m_targetEntity.IsValid())
  {
    // RotateAround's angle is the vector's magnitude, with its own 1e-8 guard.
    DirectX::XMVECTOR const right1 = DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&m_front), DirectX::XMLoadFloat3(&m_up));
    DirectX::XMVECTOR const rotation = DirectX::XMVectorScale(right1, sinf(g_gameTime * 6.0f) * 1.5f);
    float const rotationLengthSquared = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(rotation));
    if (rotationLengthSquared >= 1e-8f)
    {
      float const spin = sqrtf(rotationLengthSquared);
      targetDir = DirectX::XMVector3Transform(targetDir, DirectX::XMMatrixRotationAxis(DirectX::XMVectorScale(rotation, 1.0f / spin), spin));
    }
  }

  // Look ahead to see if we're about to hit the ground
  DirectX::XMFLOAT3 forwardPos;
  DirectX::XMStoreFloat3(&forwardPos, DirectX::XMVectorMultiplyAdd(targetDir, DirectX::XMVectorReplicate(50.0f), ourPos));
  float landHeight = g_location->m_landscape.m_heightMap->GetValue(forwardPos.x, forwardPos.z);
  if (forwardPos.y <= landHeight)
  {
    targetDir = DirectX::g_XMIdentityR1;
  }

  DirectX::XMVECTOR const actualDir = DirectX::XMVector3Normalize(DirectX::XMVectorLerp(DirectX::XMLoadFloat3(&m_front), targetDir, amountToTurn));
  float speed = m_stats[StatSpeed];
  speed = 130.0f;

  DirectX::XMFLOAT3 const oldPos = m_pos;
  DirectX::XMFLOAT3 newPos;
  DirectX::XMStoreFloat3(&newPos, DirectX::XMVectorMultiplyAdd(actualDir, DirectX::XMVectorReplicate(speed * SERVER_ADVANCE_PERIOD), ourPos));
  landHeight = g_location->m_landscape.m_heightMap->GetValue(newPos.x, newPos.z);
  // newPos.y = max( newPos.y, landHeight );

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

  DirectX::XMVECTOR const right = DirectX::XMVector3Cross(actualDir, DirectX::g_XMIdentityR1);
  DirectX::XMStoreFloat3(&m_up, DirectX::XMVector3Cross(right, actualDir));

  return DirectX::XMVectorGetX(
           DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&m_targetPos)))) < 40.0f;
}


void SoulDestroyer::ListSoundEvents(std::vector<const char*>* _list)
{
  Entity::ListSoundEvents(_list);

  _list->push_back("EnemySighted");
  _list->push_back("Panic");
}


void SoulDestroyer::RenderShapes(float _predictionTime)
{
  DirectX::XMFLOAT3 predictedPos;
  DirectX::XMStoreFloat3(&predictedPos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(_predictionTime),
                                                                     DirectX::XMLoadFloat3(&m_pos)));

  // front is RECOMPUTED orthogonal to up and both are normalised before the
  // basis is built -- passing m_front here would be a different matrix.
  DirectX::XMVECTOR const rawUp = DirectX::XMLoadFloat3(&m_up);
  DirectX::XMVECTOR const predictedRight = DirectX::XMVector3Cross(rawUp, DirectX::XMLoadFloat3(&m_front));
  DirectX::XMVECTOR const predictedFront = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(predictedRight, rawUp));
  DirectX::XMVECTOR const predictedUp = DirectX::XMVector3Normalize(rawUp);

  DirectX::XMFLOAT4X4 mat;
  DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(predictedFront, predictedUp, DirectX::XMLoadFloat3(&predictedPos)));

  glEnable(GL_NORMALIZE);
  glDisable(GL_TEXTURE_2D);

  g_renderer->SetObjectLighting();

  s_shapeHead->Render(_predictionTime, mat);


  for (int i = 1; i < static_cast<int>(m_positionHistory.size()); i += 1)
  {
    DirectX::XMVECTOR const p1 = DirectX::XMLoadFloat3(&m_positionHistory[i]);
    DirectX::XMVECTOR const p2 = DirectX::XMLoadFloat3(&m_positionHistory[i - 1]);
    DirectX::XMVECTOR const step = DirectX::XMVectorSubtract(p2, p1);

    DirectX::XMVECTOR const front = DirectX::XMVector3Normalize(step);
    DirectX::XMVECTOR const right = DirectX::XMVector3Cross(front, DirectX::g_XMIdentityR1);
    DirectX::XMVECTOR const up = DirectX::XMVector3Cross(right, front);

    DirectX::XMVECTOR const vel = DirectX::XMVectorScale(step, 1.0f / SERVER_ADVANCE_PERIOD);
    DirectX::XMVECTOR const pos = DirectX::XMVectorMultiplyAdd(vel, DirectX::XMVectorReplicate(_predictionTime), DirectX::XMVectorAdd(p1, step));

    float scale = 1.0f - ((float)i / (float)static_cast<int>(m_positionHistory.size()));
    scale *= 1.5f;
    if (i == static_cast<int>(m_positionHistory.size()) - 1)
      scale = 0.8f;
    scale = std::max(scale, 0.5f);

    DirectX::XMFLOAT4X4 const tailMat = ScaledTailBasis(front, up, pos, scale);

    s_shapeTail->Render(_predictionTime, tailMat);
  }

  g_renderer->UnsetObjectLighting();

  glDisable(GL_NORMALIZE);
}


void SoulDestroyer::RenderShapesForPixelEffect(float _predictionTime)
{
  DirectX::XMFLOAT3 predictedPos;
  DirectX::XMStoreFloat3(&predictedPos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(_predictionTime),
                                                                     DirectX::XMLoadFloat3(&m_pos)));

  // front is RECOMPUTED orthogonal to up, and deliberately NOT normalised
  // here -- unlike the Render path, which normalises both.
  DirectX::XMVECTOR const predictedUp = DirectX::XMLoadFloat3(&m_up);
  DirectX::XMVECTOR const predictedRight = DirectX::XMVector3Cross(predictedUp, DirectX::XMLoadFloat3(&m_front));
  DirectX::XMVECTOR const predictedFront = DirectX::XMVector3Cross(predictedRight, predictedUp);

  DirectX::XMFLOAT4X4 mat;
  DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(predictedFront, predictedUp, DirectX::XMLoadFloat3(&predictedPos)));

  g_renderer->MarkUsedCells(s_shapeHead, mat);

  for (int i = 1; i < static_cast<int>(m_positionHistory.size()); i += 1)
  {
    DirectX::XMVECTOR const p1 = DirectX::XMLoadFloat3(&m_positionHistory[i]);
    DirectX::XMVECTOR const p2 = DirectX::XMLoadFloat3(&m_positionHistory[i - 1]);
    DirectX::XMVECTOR const step = DirectX::XMVectorSubtract(p2, p1);

    DirectX::XMVECTOR const front = DirectX::XMVector3Normalize(step);
    DirectX::XMVECTOR const right = DirectX::XMVector3Cross(front, DirectX::g_XMIdentityR1);
    DirectX::XMVECTOR const up = DirectX::XMVector3Cross(right, front);

    DirectX::XMVECTOR const vel = DirectX::XMVectorScale(step, 1.0f / SERVER_ADVANCE_PERIOD);
    DirectX::XMVECTOR const pos = DirectX::XMVectorMultiplyAdd(vel, DirectX::XMVectorReplicate(_predictionTime), DirectX::XMVectorAdd(p1, step));

    float scale = 1.0f - ((float)i / (float)static_cast<int>(m_positionHistory.size()));
    scale *= 1.5f;
    if (i == static_cast<int>(m_positionHistory.size()) - 1)
      scale = 0.8f;
    scale = std::max(scale, 0.5f);

    DirectX::XMFLOAT4X4 const tailMat = ScaledTailBasis(front, up, pos, scale);

    g_renderer->MarkUsedCells(s_shapeTail, tailMat);
  }
}


void SoulDestroyer::Render(float _predictionTime)
{
  _predictionTime -= SERVER_ADVANCE_PERIOD;

  if (!m_dead)
  {
    glDisable(GL_TEXTURE_2D);

    DirectX::XMFLOAT3 predictedPos;
    DirectX::XMStoreFloat3(&predictedPos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(_predictionTime),
                                                                       DirectX::XMLoadFloat3(&m_pos)));

    // front is RECOMPUTED orthogonal to up, and deliberately NOT normalised
    // here -- unlike the Render path, which normalises both.
    DirectX::XMVECTOR const predictedUp = DirectX::XMLoadFloat3(&m_up);
    DirectX::XMVECTOR const predictedRight = DirectX::XMVector3Cross(predictedUp, DirectX::XMLoadFloat3(&m_front));
    DirectX::XMVECTOR const predictedFront = DirectX::XMVector3Cross(predictedRight, predictedUp);

    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(predictedFront, predictedUp, DirectX::XMLoadFloat3(&predictedPos)));

    //        RenderSphere( m_targetPos, 5.0f );
    //        RenderSphere( predictedPos, 300.0f );
    //        RenderArrow( m_pos, m_pos+predictedFront * 200.0f, 3.0f, RGBAColour(0,255,0) );
    //        RenderArrow( m_pos, m_pos+predictedUp * 100.0f, 2.0f, RGBAColour(255,0,0) );
    //        RenderArrow( m_pos, m_pos+predictedRight * 100.0f, 2.0f, RGBAColour(0,0,255) );

    RenderShapes(_predictionTime);


    //
    // Render shadows

    BeginRenderShadow();
    RenderShadow(predictedPos, 50.0f);

    for (int i = 1; i < static_cast<int>(m_positionHistory.size()); i += 1)
    {
      DirectX::XMVECTOR const p1 = DirectX::XMLoadFloat3(&m_positionHistory[i]);
      DirectX::XMVECTOR const p2 = DirectX::XMLoadFloat3(&m_positionHistory[i - 1]);
      DirectX::XMVECTOR const step = DirectX::XMVectorSubtract(p2, p1);
      DirectX::XMVECTOR const vel = DirectX::XMVectorScale(step, 1.0f / SERVER_ADVANCE_PERIOD);

      DirectX::XMFLOAT3 pos;
      DirectX::XMStoreFloat3(&pos, DirectX::XMVectorMultiplyAdd(vel, DirectX::XMVectorReplicate(_predictionTime), DirectX::XMVectorAdd(p1, step)));

      float scale = 1.0f - ((float)i / (float)static_cast<int>(m_positionHistory.size()));
      scale *= 1.5f;
      if (i == static_cast<int>(m_positionHistory.size()) - 1)
        scale = 0.8f;
      scale = std::max(scale, 0.5f);

      RenderShadow(pos, scale * 20.0f);
    }

    EndRenderShadow();


    //
    // Render our spirits

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glEnable(GL_BLEND);
    glDepthMask(false);

    float timeNow = GetHighResTime();
    for (int i = 0; i < m_spirits.Size(); ++i)
    {
      if (m_spirits.ValidIndex(i))
      {
        float birthTime = m_spirits[i];
        if (timeNow >= birthTime + 60.0f)
        {
          m_spirits.MarkNotUsed(i);
        }
        else
        {
          float alpha = 1.0f - (timeNow - m_spirits[i]) / 60.0f;
          alpha = std::min(alpha, 1.0f);
          alpha = std::max(alpha, 0.0f);
          DirectX::XMFLOAT3 pos;
          DirectX::XMStoreFloat3(
            &pos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(_predictionTime),
                                               DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&m_spiritPosition[i]))));
          RenderSpirit(pos, alpha);
        }
      }
    }

    glDepthMask(true);
    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);


    // glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
    // g_editorFont.DrawText3DCentre( predictedPos+Vector3(0,50,0), 20, "%2.2f", m_panic );
  }
}


void SoulDestroyer::RenderSpirit(DirectX::XMFLOAT3 const& _pos, float _alpha)
{
  DirectX::XMVECTOR const pos = DirectX::XMLoadFloat3(&_pos);

  // Camera's accessors are still legacy -- Species belongs to T22. Hoisted out
  // of the eight vertices below, which each called them afresh.
  DirectX::XMFLOAT3 const camPosStore = g_camera->GetPos();
  DirectX::XMFLOAT3 const camUpStore = g_camera->GetUp();
  DirectX::XMFLOAT3 const camRightStore = g_camera->GetRight();
  DirectX::XMVECTOR const camUp = DirectX::XMLoadFloat3(&camUpStore);
  DirectX::XMVECTOR const camRight = DirectX::XMLoadFloat3(&camRightStore);

  int innerAlpha = 200 * _alpha;
  int outerAlpha = 100 * _alpha;
  int spiritInnerSize = 4 * _alpha;
  int spiritOuterSize = 12 * _alpha;

  RGBAColour colour(100, 50, 50);
  float distToParticle = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&camPosStore), pos)));

  float size = spiritInnerSize / sqrtf(sqrtf(distToParticle));
  glColor4ub(colour.r, colour.g, colour.b, innerAlpha);

  glBegin(GL_QUADS);
  EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camUp, DirectX::XMVectorReplicate(size), pos));
  EmitVertex(DirectX::XMVectorMultiplyAdd(camRight, DirectX::XMVectorReplicate(size), pos));
  EmitVertex(DirectX::XMVectorMultiplyAdd(camUp, DirectX::XMVectorReplicate(size), pos));
  EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camRight, DirectX::XMVectorReplicate(size), pos));
  glEnd();

  size = spiritOuterSize / sqrtf(sqrtf(distToParticle));
  glColor4ub(colour.r, colour.g, colour.b, outerAlpha);
  glBegin(GL_QUADS);
  EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camUp, DirectX::XMVectorReplicate(size), pos));
  EmitVertex(DirectX::XMVectorMultiplyAdd(camRight, DirectX::XMVectorReplicate(size), pos));
  EmitVertex(DirectX::XMVectorMultiplyAdd(camUp, DirectX::XMVectorReplicate(size), pos));
  EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camRight, DirectX::XMVectorReplicate(size), pos));
  glEnd();
}


bool SoulDestroyer::RenderPixelEffect(float _predictionTime)
{
  _predictionTime -= SERVER_ADVANCE_PERIOD;

  if (!m_dead)
  {
    RenderShapes(_predictionTime);
    RenderShapesForPixelEffect(_predictionTime);
    return true;
  }

  return false;
}

void SoulDestroyer::SetWaypoint(DirectX::XMFLOAT3 const _waypoint) { m_targetPos = _waypoint; }


Zombie::Zombie()
  : WorldObject(),
    m_life(0.0f)
{
  m_positionOffset = syncfrand(10.0f);
  m_xaxisRate = syncfrand(2.0f);
  m_yaxisRate = syncfrand(2.0f);
  m_zaxisRate = syncfrand(2.0f);

  m_type = EffectZombie;
}


bool Zombie::Advance()
{
  m_life += SERVER_ADVANCE_PERIOD;

  if (m_life > 600.0f)
  {
    return true;
  }

  DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), 0.9f));

  //
  // Make me hover around a bit

  m_positionOffset += SERVER_ADVANCE_PERIOD;
  m_xaxisRate += syncsfrand(1.0f);
  m_yaxisRate += syncsfrand(1.0f);
  m_zaxisRate += syncsfrand(1.0f);
  if (m_xaxisRate > 2.0f)
    m_xaxisRate = 2.0f;
  if (m_xaxisRate < 0.0f)
    m_xaxisRate = 0.0f;
  if (m_yaxisRate > 2.0f)
    m_yaxisRate = 2.0f;
  if (m_yaxisRate < 0.0f)
    m_yaxisRate = 0.0f;
  if (m_zaxisRate > 2.0f)
    m_zaxisRate = 2.0f;
  if (m_zaxisRate < 0.0f)
    m_zaxisRate = 0.0f;

  m_hover.x = sinf(m_positionOffset) * m_xaxisRate;
  m_hover.y = sinf(m_positionOffset) * m_yaxisRate;
  m_hover.z = sinf(m_positionOffset) * m_zaxisRate;

  DirectX::XMStoreFloat3(&m_pos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_hover), DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD),
                                                              DirectX::XMLoadFloat3(&m_pos)));
  DirectX::XMStoreFloat3(&m_pos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD),
                                                              DirectX::XMLoadFloat3(&m_pos)));

  return false;
}


void Zombie::Render(float _predictionTime)
{
  DirectX::XMVECTOR const predictedPos = DirectX::XMVectorMultiplyAdd(
    DirectX::XMLoadFloat3(&m_hover), DirectX::XMVectorReplicate(_predictionTime),
    DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(_predictionTime), DirectX::XMLoadFloat3(&m_pos)));

  DirectX::XMVECTOR const predictedUp = DirectX::XMLoadFloat3(&m_up);
  DirectX::XMVECTOR const predictedRight = DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&m_front), predictedUp);

  float size = 5.0f;

  float alpha = 1.0f - (m_life / 10.0f);
  alpha = std::max(0.1f, alpha);
  alpha = std::min(0.7f, alpha);

  float outerAlpha = (0.7f - alpha) * 0.1f;

  float timeRemaining = 600.0f - m_life;
  if (timeRemaining < 100.0f)
  {
    alpha *= timeRemaining * 0.01f;
    outerAlpha *= timeRemaining * 0.01f;
  }

  glDisable(GL_CULL_FACE);
  glColor4f(0.9f, 0.9f, 1.0f, alpha);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Sprites/Ghost.bmp"));

  glBegin(GL_QUADS);
  glTexCoord2i(0, 0);
  EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorSubtract(predictedPos, DirectX::XMVectorScale(predictedRight, size)),
                                       DirectX::XMVectorScale(predictedUp, size)));
  glTexCoord2i(1, 0);
  EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorAdd(predictedPos, DirectX::XMVectorScale(predictedRight, size)),
                                       DirectX::XMVectorScale(predictedUp, size)));
  glTexCoord2i(1, 1);
  EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(predictedPos, DirectX::XMVectorScale(predictedRight, size)),
                                  DirectX::XMVectorScale(predictedUp, size)));
  glTexCoord2i(0, 1);
  EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorSubtract(predictedPos, DirectX::XMVectorScale(predictedRight, size)),
                                  DirectX::XMVectorScale(predictedUp, size)));
  glEnd();

  size *= 2.0f;
  glColor4f(0.9f, 0.9f, 1.0f, outerAlpha);
  glBegin(GL_QUADS);
  glTexCoord2i(0, 0);
  EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorSubtract(predictedPos, DirectX::XMVectorScale(predictedRight, size)),
                                       DirectX::XMVectorScale(predictedUp, size)));
  glTexCoord2i(1, 0);
  EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorAdd(predictedPos, DirectX::XMVectorScale(predictedRight, size)),
                                       DirectX::XMVectorScale(predictedUp, size)));
  glTexCoord2i(1, 1);
  EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(predictedPos, DirectX::XMVectorScale(predictedRight, size)),
                                  DirectX::XMVectorScale(predictedUp, size)));
  glTexCoord2i(0, 1);
  EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorSubtract(predictedPos, DirectX::XMVectorScale(predictedRight, size)),
                                  DirectX::XMVectorScale(predictedUp, size)));
  glEnd();

  glDisable(GL_TEXTURE_2D);
  glEnable(GL_CULL_FACE);
}
