#include "pch.h"
#include "GlVertex.h"
#include "SoundSources.h"

#include "BinaryStreamReaders.h"
#include "Bitmap.h"
#include "DebugRender.h"
#include "Debug.h"
#include "MathUtils.h"
#include "Profiler.h"
#include "Resource.h"
#include "TextRenderer.h"
#include "Preferences.h"

#include "ProtocolLimits.h"
#include "Location.h"
#include "EntityGrid.h"
#include "ParticleSystem.h"
#include "Team.h"
#include "SoundSystem.h"

#include "Virii.h"
#include "Egg.h"
#include "WorldPointers.h"


ViriiUnit::ViriiUnit(int teamId, int unitId, int numEntities, DirectX::XMFLOAT3 const& _pos)
  : Unit(Entity::TypeVirii, teamId, unitId, numEntities, _pos),
    m_enemiesFound(false),
    m_cameraClose(false)
{
}


bool ViriiUnit::Advance(int _slice)
{
  float searchRadius = m_radius + VIRII_MAXSEARCHRANGE;

  m_enemiesFound = g_location->m_entityGrid->AreEnemiesPresent(m_centrePos.x, m_centrePos.z, searchRadius, m_teamId);

  return Unit::Advance(_slice);
}


void ViriiUnit::Render(float _predictionTime)
{
  //
  // Render Red Virii shapes

  float nearPlaneStart = g_renderer->GetNearPlane();
  g_camera->SetupProjectionMatrix(nearPlaneStart * 1.05f, g_renderer->GetFarPlane());

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Sprites/viriifull.bmp"));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glDepthMask(false);
  glDisable(GL_CULL_FACE);
  glBegin(GL_QUADS);

  //
  // Render high detail?

  int entityDetail = g_prefsManager->GetInt("RenderEntityDetail");
  int viriiDetail = 1;
  if (entityDetail == 3)
  {
    viriiDetail = 4;
  }
  else
  {
    // Camera's accessors are still legacy -- Species belongs to T22.
    DirectX::XMFLOAT3 const cameraPos = g_camera->GetPos();
    float rangeToCam = DirectX::XMVectorGetX(
      DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_centrePos), DirectX::XMLoadFloat3(&cameraPos))));
    if (entityDetail == 1 && rangeToCam > 1000.0f)
      viriiDetail = 2;
    else if (entityDetail == 2 && rangeToCam > 1000.0f)
      viriiDetail = 3;
    else if (entityDetail == 2 && rangeToCam > 500.0f)
      viriiDetail = 2;
    // else if ( entityDetail == 3 && rangeToCam > 1000.0f )        viriiDetail = 4;
    // else if ( entityDetail == 3 && rangeToCam > 600.0f )         viriiDetail = 3;
    // else if ( entityDetail == 3 && rangeToCam > 300.0f )         viriiDetail = 2;
  }

  //
  // Render all the entities that are up-to-date with server advances

  int lastUpdated = m_entitiesWalker.GetLastUpdated();
  for (int i = 0; i <= lastUpdated; i++)
  {
    if (m_entities.ValidIndex(i))
    {
      Virii* virii = (Virii*)m_entities[i];
      virii->Render(_predictionTime, m_teamId, viriiDetail);
    }
  }

  //
  // Render all the entities that are one step out-of-date with server advances

  int size = m_entities.Size();
  _predictionTime += SERVER_ADVANCE_PERIOD;
  for (int i = lastUpdated + 1; i < size; i++)
  {
    if (m_entities.ValidIndex(i))
    {
      Virii* virii = (Virii*)m_entities[i];
      virii->Render(_predictionTime, m_teamId, viriiDetail);
    }
  }

  glEnd();
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);
  glEnable(GL_CULL_FACE);
  glDepthMask(true);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

  g_camera->SetupProjectionMatrix(nearPlaneStart, g_renderer->GetFarPlane());
}


Virii::Virii()
  : Entity(),
    m_retargetTimer(-1),
    m_state(StateIdle),
    m_spiritId(-1),
    m_hoverHeight(0.1f),
    m_historyTimer(0.0f),
    m_prevPosTimer(0.0f)
{
  m_retargetTimer = syncfrand(2.0f);
}


// m_positionHistory owns its elements now, so the vector's destructor is enough.
Virii::~Virii() = default;

bool Virii::Advance(Unit* _unit)
{
  m_prevPosTimer -= SERVER_ADVANCE_PERIOD;

  if (m_prevPosTimer <= 0.0f)
  {
    m_prevPos = m_pos;
    m_prevPosTimer = 5.0f;
  }

  //
  // Take damage if we're in the water
  // Actually, don't
  // if( m_pos.y < 0.0f ) ChangeHealth( -1 );

  bool amIDead = Entity::Advance(_unit);

  if (m_dead)
  {
    m_vel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

    if (m_spiritId != -1)
    {
      if (g_location->m_spirits.ValidIndex(m_spiritId))
      {
        Spirit* spirit = g_location->m_spirits.GetPointer(m_spiritId);
        if (spirit && spirit->m_state == Spirit::StateAttached)
        {
          spirit->CollectorDrops();
        }
      }
      m_spiritId = -1;
    }
  }

  if (!m_onGround)
  {
    m_vel.y += -10.0f * SERVER_ADVANCE_PERIOD;
    DirectX::XMStoreFloat3(&m_pos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD),
                                                                DirectX::XMLoadFloat3(&m_pos)));

    float groundLevel = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z) + 2.0f;
    if (m_pos.y <= groundLevel)
    {
      m_onGround = true;
      m_vel = g_zeroVector;
    }
  }

  if (!m_dead)
  {
    switch (m_state)
    {
    case StateIdle:
      amIDead = AdvanceIdle();
      break;
    case StateAttacking:
      amIDead = AdvanceAttacking();
      break;
    case StateToSpirit:
      amIDead = AdvanceToSpirit();
      break;
    case StateToEgg:
      amIDead = AdvanceToEgg();
      break;
    }

    if (g_location->m_spirits.ValidIndex(m_spiritId))
    {
      Spirit* spirit = g_location->m_spirits.GetPointer(m_spiritId);
      if (spirit && spirit->m_state == Spirit::StateAttached)
      {
        spirit->m_pos = m_pos;
        spirit->m_pos.y += 2.0f;
        spirit->m_vel = m_vel;
      }
    }

    // Next 5 lines by Andrew. Purpose - to prevent virii grinding themselves against
    // the side of obstructions.
    //        Vector3 newPos(PushFromObstructions( m_pos ));
    //		if (newPos.x != m_pos.x || newPos.z != m_pos.z)
    //		{
    //			m_pos = newPos;
    //			if (m_retargetTimer > 0.05f) m_retargetTimer = 0.1f;
    //		}
  }

  if (static_cast<int>(m_positionHistory.size()) > 0)
  {
    bool recorded = false;

    if (m_state != StateIdle)
    {
      m_historyTimer -= SERVER_ADVANCE_PERIOD;
      if (m_historyTimer <= 0.0f)
      {
        m_historyTimer = 0.5f;
        RecordHistoryPosition(true);
        recorded = true;
      }
    }

    if (!recorded)
    {
      float lastRecordedHeight = m_positionHistory[0]->m_pos.y;
      if (fabs(m_pos.y - lastRecordedHeight) > 3.0f)
      {
        RecordHistoryPosition(false);
      }
    }
  }
  else
  {
    RecordHistoryPosition(true);
  }

  float worldSizeX = g_location->m_landscape.GetWorldSizeX();
  float worldSizeZ = g_location->m_landscape.GetWorldSizeZ();
  if (m_pos.x < 0.0f)
    m_pos.x = 0.0f;
  if (m_pos.z < 0.0f)
    m_pos.z = 0.0f;
  if (m_pos.x >= worldSizeX)
    m_pos.x = worldSizeX;
  if (m_pos.z >= worldSizeZ)
    m_pos.z = worldSizeZ;

  return amIDead;
}


void Virii::RecordHistoryPosition(bool _required)
{
  START_PROFILE(g_profiler, "RecordHistory");

  // SurfaceMap2D<Vector3> still returns a Vector3 -- Landscape belongs to T18.
  DirectX::XMFLOAT3 const landNormal = g_location->m_landscape.m_normalMap->GetValue(m_pos.x, m_pos.z);

  // Braced to zero: Vector3's default constructor did it and this stays zero
  // when the history is empty.
  DirectX::XMFLOAT3 prevPos{0.0f, 0.0f, 0.0f};
  if (static_cast<int>(m_positionHistory.size()) > 0)
    prevPos = m_positionHistory[0]->m_pos;

  DirectX::XMVECTOR const ourPos = DirectX::XMLoadFloat3(&m_pos);

  auto history = std::make_unique<ViriiHistory>();
  history->m_pos = m_pos;
  DirectX::XMStoreFloat3(&history->m_right,
                         DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::XMVectorSubtract(ourPos, DirectX::XMLoadFloat3(&prevPos)),
                                                                             DirectX::XMLoadFloat3(&landNormal))));
  history->m_distance = 0.0f;
  history->m_required = _required;

  if (static_cast<int>(m_positionHistory.size()) > 0)
  {
    DirectX::XMVECTOR const step = DirectX::XMVectorSubtract(ourPos, DirectX::XMLoadFloat3(&m_positionHistory[0]->m_pos));
    history->m_distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(step));

    // SetLength. m_distance is exactly zero when the worm has not moved since
    // the last recorded point, and the legacy fallback then left (10, 0, 0);
    // reproduced rather than storing QNaN into the glow geometry.
    DirectX::XMStoreFloat3(&history->m_glowDiff, NearlyEquals(history->m_distance, 0.0f) ? DirectX::XMVectorSet(10.0f, 0.0f, 0.0f, 0.0f)
                                                                                         : DirectX::XMVectorScale(step, 10.0f / history->m_distance));
  }


  m_positionHistory.insert(m_positionHistory.begin(), std::move(history));

  float totalDistance = 0.0f;
  int removeFrom = -1;
  int entityDetail = g_prefsManager->GetInt("RenderEntityDetail", 1);
  float tailLength = 175.0f; // - (entityDetail-1) * 50.0f;
  for (int i = 0; i < static_cast<int>(m_positionHistory.size()); ++i)
  {
    ViriiHistory* history = m_positionHistory[i].get();
    totalDistance += history->m_distance;

    if (totalDistance > tailLength)
    {
      removeFrom = i;
      break;
    }
  }

  if (removeFrom != -1)
  {
    while (ValidIndex(m_positionHistory, removeFrom))
    {
      // erase destroys the unique_ptr, so the element frees inside the same
      // statement the explicit delete used to follow.
      m_positionHistory.erase(m_positionHistory.begin() + removeFrom);
    }
  }

  END_PROFILE(g_profiler, "RecordHistory");
}


bool Virii::AdvanceToTargetPos(DirectX::XMFLOAT3 const& _pos)
{
  START_PROFILE(g_profiler, "AdvanceToTargetPos");

  DirectX::XMFLOAT3 const oldPos = m_pos;

  DirectX::XMVECTOR const toTarget = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&_pos), DirectX::XMLoadFloat3(&m_pos));
  DirectX::XMVECTOR const direction = DirectX::XMVector3Normalize(toTarget);
  DirectX::XMStoreFloat3(&m_front, direction);
  DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(direction, m_stats[StatSpeed]));

  // The arrival test below uses the FLATTENED distance, but m_vel and m_front
  // came from the full one. Preserved as written.
  DirectX::XMVECTOR const distance = DirectX::XMVectorSetY(toTarget, 0.0f);

  DirectX::XMFLOAT3 nextPos;
  DirectX::XMStoreFloat3(&nextPos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD),
                                                                DirectX::XMLoadFloat3(&m_pos)));
  nextPos.y = g_location->m_landscape.m_heightMap->GetValue(nextPos.x, nextPos.z) + m_hoverHeight;
  float currentHeight = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);
  float nextHeight = g_location->m_landscape.m_heightMap->GetValue(nextPos.x, nextPos.z);

  //
  // Slow us down if we're going up hill
  // Speed up if going down hill

  float factor = 1.0f - (currentHeight - nextHeight) / -5.0f;
  if (factor < 0.1f)
    factor = 0.1f;
  if (factor > 2.0f)
    factor = 2.0f;
  DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), factor));
  DirectX::XMStoreFloat3(&nextPos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD),
                                                                DirectX::XMLoadFloat3(&m_pos)));
  nextPos.y = g_location->m_landscape.m_heightMap->GetValue(nextPos.x, nextPos.z) + m_hoverHeight;


  //
  // Are we there?

  bool arrived = false;
  if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(distance)) <
      DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(DirectX::XMLoadFloat3(&m_vel))) * SERVER_ADVANCE_PERIOD * SERVER_ADVANCE_PERIOD)
  {
    nextPos = _pos;
    arrived = true;
  }

  m_pos = nextPos;
  DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&oldPos)),
                                                        1.0f / SERVER_ADVANCE_PERIOD));

  END_PROFILE(g_profiler, "AdvanceToTargetPos");

  return arrived;
}


bool Virii::AdvanceIdle()
{
  START_PROFILE(g_profiler, "AdvanceIdle");

  m_retargetTimer -= SERVER_ADVANCE_PERIOD;
  bool foundTarget = false;

  if (m_retargetTimer <= 0.0f)
  {
    m_retargetTimer = 1.0f;

    if (g_location->m_spirits.ValidIndex(m_spiritId))
    {
      foundTarget = SearchForEggs();
    }

    if (!foundTarget)
      foundTarget = SearchForEnemies();
    if (!foundTarget)
      foundTarget = SearchForSpirits();
    if (!foundTarget)
      foundTarget = SearchForIdleDirection();
  }

  if (m_state == StateIdle)
  {
    AdvanceToTargetPos(m_wayPoint);
  }

  END_PROFILE(g_profiler, "AdvanceIdle");
  return false;
}


bool Virii::AdvanceAttacking()
{
  START_PROFILE(g_profiler, "AdvanceAttacking");

  WorldObject* enemy = g_location->GetEntity(m_enemyId);
  Entity* entity = (Entity*)enemy;

  if (!entity || entity->m_dead)
  {
    m_state = StateIdle;
    END_PROFILE(g_profiler, "AdvanceAttacking");
    return false;
  }

  //    int enemySpiritId = g_location->GetSpirit( m_enemyId );
  //    if( enemySpiritId != -1 && entity->m_dead )
  //    {
  //        // Our enemy has died and dropped a spirit
  //        // If we can see an egg, then go for it
  //        if( FindNearbyEgg( enemySpiritId, VIRII_MAXSEARCHRANGE ).IsValid() )
  //        {
  //            m_enemyId.SetInvalid();
  //            m_spiritId = enemySpiritId;
  //            m_state = StateToSpirit;
  //        }
  //        return false;
  //    }

  bool arrived = AdvanceToTargetPos(entity->m_pos);
  if (arrived)
  {
    entity->ChangeHealth(-20);
    for (int i = 0; i < 3; ++i)
    {
      // The three syncsfrand calls stay in this order: they advance the RNG.
      DirectX::XMFLOAT3 const flashVel(syncsfrand(15.0f), syncsfrand(15.0f) + 15.0f, syncsfrand(15.0f));
      g_particleSystem->CreateParticle(m_pos, flashVel, Particle::TypeMuzzleFlash);
    }
    g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "Attack");
    SearchForEnemies();
  }

  END_PROFILE(g_profiler, "AdvanceAttacking");
  return false;
}


bool Virii::AdvanceToSpirit()
{
  START_PROFILE(g_profiler, "AdvanceToSpirit");

  Spirit* s = nullptr;
  if (g_location->m_spirits.ValidIndex(m_spiritId))
  {
    s = g_location->m_spirits.GetPointer(m_spiritId);
  }

  if (!s || s->m_state == Spirit::StateDeath || s->m_state == Spirit::StateAttached || s->m_state == Spirit::StateInEgg)
  {
    m_spiritId = -1;
    m_state = StateIdle;
    END_PROFILE(g_profiler, "AdvanceToSpirit");
    return false;
  }

  Spirit* spirit = g_location->m_spirits.GetPointer(m_spiritId);
  DirectX::XMFLOAT3 targetPos = spirit->m_pos;
  targetPos.y = g_location->m_landscape.m_heightMap->GetValue(targetPos.x, targetPos.z);
  bool arrived = AdvanceToTargetPos(targetPos);
  if (arrived)
  {
    spirit->CollectorArrives();
    m_state = StateToEgg;
  }

  END_PROFILE(g_profiler, "AdvanceToSpirit");
  return false;
}


bool Virii::AdvanceToEgg()
{
  START_PROFILE(g_profiler, "AdvanceToEgg");
  Egg* theEgg = (Egg*)g_location->GetEntitySafe(m_eggId, Entity::TypeEgg);

  if (!theEgg || theEgg->m_state == Egg::StateFertilised)
  {
    bool found = SearchForEggs();
    if (!found)
    {
      // We can't find any eggs, so go into holding pattern
      if (g_location->m_spirits.ValidIndex(m_spiritId))
      {
        Spirit* spirit = g_location->m_spirits.GetPointer(m_spiritId);
        if (spirit->m_state == Spirit::StateAttached)
        {
          spirit->CollectorDrops();
        }
        m_spiritId = -1;
      }
      m_state = StateIdle;
      END_PROFILE(g_profiler, "AdvanceToEgg");
      return false;
    }
  }

  if (!g_location->m_spirits.ValidIndex(m_spiritId))
  {
    m_spiritId = -1;
    m_state = StateIdle;
    END_PROFILE(g_profiler, "AdvanceToEgg");
    return false;
  }

  //
  // At this point we MUST have found an egg, otherwise we'd have returned by now

  theEgg = (Egg*)g_location->GetEntitySafe(m_eggId, Entity::TypeEgg);
  DEBUG_ASSERT(theEgg);

  bool arrived = AdvanceToTargetPos(theEgg->m_pos);

  if (arrived)
  {
    theEgg->Fertilise(m_spiritId);
    m_spiritId = -1;
    m_eggId.SetInvalid();
  }

  END_PROFILE(g_profiler, "AdvanceToEgg");
  return false;
}


bool Virii::SearchForEnemies()
{
  START_PROFILE(g_profiler, "SearchForEnemies");

  ViriiUnit* unit = (ViriiUnit*)g_location->GetUnit(m_id);
  if (unit && !unit->m_enemiesFound)
  {
    END_PROFILE(g_profiler, "SearchForEnemies");
    return false;
  }

  WorldObjectId bestEnemyId = g_location->m_entityGrid->GetBestEnemy(m_pos.x, m_pos.z, VIRII_MINSEARCHRANGE, VIRII_MAXSEARCHRANGE, m_id.GetTeamId());
  Entity* enemy = g_location->GetEntity(bestEnemyId);

  if (enemy && !enemy->m_dead)
  {
    m_enemyId = bestEnemyId;
    m_state = StateAttacking;
    END_PROFILE(g_profiler, "SearchForEnemies");
    return true;
  }

  END_PROFILE(g_profiler, "SearchForEnemies");
  return false;
}


bool Virii::SearchForSpirits()
{
  START_PROFILE(g_profiler, "SearchForSpirits");

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

      if (theDist <= VIRII_MAXSEARCHRANGE && theDist < closest && s->NumNearbyEggs() > 0 &&
          (s->m_state == Spirit::StateBirth || s->m_state == Spirit::StateFloating))
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
    m_state = StateToSpirit;
    END_PROFILE(g_profiler, "SearchForSpirits");
    return true;
  }

  END_PROFILE(g_profiler, "SearchForSpirits");
  return false;
}


WorldObjectId Virii::FindNearbyEgg(int _spiritId, float _autoAccept)
{
  if (!g_location->m_spirits.ValidIndex(_spiritId))
  {
    return WorldObjectId();
  }

  Spirit* spirit = g_location->m_spirits.GetPointer(_spiritId);
  if (!spirit)
    return WorldObjectId();

  WorldObjectId* m_nearbyEggs = spirit->GetNearbyEggs();
  int numNearbyEggs = spirit->NumNearbyEggs();

  WorldObjectId eggId;
  float closest = 999999.9f;

  for (int i = 0; i < numNearbyEggs; ++i)
  {
    WorldObjectId thisEggId = m_nearbyEggs[i];
    Egg* egg = (Egg*)g_location->GetEntitySafe(thisEggId, Entity::TypeEgg);

    if (egg && egg->m_state == Egg::StateDormant)
    {
      float theDist = DirectX::XMVectorGetX(
        DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&egg->m_pos), DirectX::XMLoadFloat3(&spirit->m_pos))));
      if (theDist <= _autoAccept)
      {
        return thisEggId;
      }
      else if (theDist < closest)
      {
        eggId = thisEggId;
        closest = theDist;
      }
    }
  }

  return eggId;
}


WorldObjectId Virii::FindNearbyEgg(DirectX::XMFLOAT3 const& _pos)
{
  int numFound;
  WorldObjectId* ids = g_location->m_entityGrid->GetFriends(_pos.x, _pos.z, VIRII_MAXSEARCHRANGE, &numFound, m_id.GetTeamId());

  //
  // Build a list of candidates within range

  std::vector<WorldObjectId> eggIds;

  for (int i = 0; i < numFound; ++i)
  {
    WorldObjectId id = ids[i];
    Egg* egg = (Egg*)g_location->GetEntitySafe(id, Entity::TypeEgg);

    if (egg && egg->m_state == Egg::StateDormant && egg->m_onGround)
    {
      eggIds.push_back(id);
    }
  }


  //
  // Chose one randomly

  if (!eggIds.empty())
  {
    int chosenIndex = syncfrand(static_cast<int>(eggIds.size()));
    WorldObjectId* eggId = &eggIds[chosenIndex];
    return *eggId;
  }
  else
  {
    return WorldObjectId();
  }
}


bool Virii::SearchForEggs()
{
  START_PROFILE(g_profiler, "SearchForEggs");

  WorldObjectId eggId = FindNearbyEgg(m_pos);

  if (eggId.IsValid())
  {
    m_eggId = eggId;
    m_state = StateToEgg;
    END_PROFILE(g_profiler, "SearchForEggs");
    return true;
  }

  END_PROFILE(g_profiler, "SearchFprEggs");
  return false;
}


bool Virii::SearchForIdleDirection()
{
  START_PROFILE(g_profiler, "SearchForIdleDir");

  float distToSpawnPoint =
    DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&m_spawnPoint))));
  float chanceOfReturn = (distToSpawnPoint / m_roamRange);
  if (chanceOfReturn < 0.75f)
    chanceOfReturn = 0.0f;
  if (chanceOfReturn >= 1.0f || syncfrand(1.0f) <= chanceOfReturn)
  {
    // We have strayed too far from our spawn point
    // So head back there now
    DirectX::XMFLOAT3 newDirection;
    DirectX::XMStoreFloat3(
      &newDirection, DirectX::XMVectorSetY(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_spawnPoint), DirectX::XMLoadFloat3(&m_pos)), 0.0f));
    if (newDirection.x < -0.5f)
      newDirection.x = -1.0f;
    else if (newDirection.x > 0.5f)
      newDirection.x = 1.0f;
    else
      newDirection.x = 0.0f;
    if (newDirection.z < -0.5f)
      newDirection.z = -1.0f;
    else if (newDirection.z > 0.5f)
      newDirection.z = 1.0f;
    else
      newDirection.z = 0.0f;
    // SetLength on a vector snapped to one of the eight compass directions
    // above -- it is (0, 0, 0) when the worm is already on its spawn point, and
    // the legacy fallback then left (speed, 0, 0). Reproduced.
    float const directionLength = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&newDirection)));
    DirectX::XMVECTOR const step = NearlyEquals(directionLength, 0.0f)
                                     ? DirectX::XMVectorSet(m_stats[StatSpeed], 0.0f, 0.0f, 0.0f)
                                     : DirectX::XMVectorScale(DirectX::XMLoadFloat3(&newDirection), m_stats[StatSpeed] / directionLength);

    DirectX::XMFLOAT3 nextPos;
    DirectX::XMStoreFloat3(&nextPos, DirectX::XMVectorMultiplyAdd(step, DirectX::XMVectorReplicate(m_retargetTimer), DirectX::XMLoadFloat3(&m_pos)));
    nextPos.y = g_location->m_landscape.m_heightMap->GetValue(nextPos.x, nextPos.z);
    m_wayPoint = nextPos;
    m_state = StateIdle;
    RecordHistoryPosition(true);
    g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "ChangeDirection");
    END_PROFILE(g_profiler, "SearchForIdleDir");
    return true;
  }
  else
  {
    int attempts = 0;
    while (attempts < 3)
    {
      ++attempts;

      int x = (((int)syncfrand(3.0f)) - 1);
      int z = (((int)syncfrand(3.0f)) - 1);
      // x and z are each -1, 0 or 1, so (0,0,0) is a real outcome and the
      // legacy Normalise left (0, 0, 1) for it. That fallback IS reproduced
      // here, unlike the migration's default, because it is the difference
      // between the worm picking a direction and picking QNaN.
      DirectX::XMFLOAT3 const rawVel((float)x, 0.0f, (float)z);
      float const rawLength = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&rawVel)));
      DirectX::XMVECTOR const newVel = rawLength > 0.0f ? DirectX::XMVectorScale(DirectX::XMLoadFloat3(&rawVel), m_stats[StatSpeed] / rawLength)
                                                        : DirectX::XMVectorSet(0.0f, 0.0f, m_stats[StatSpeed], 0.0f);

      DirectX::XMFLOAT3 nextPos;
      DirectX::XMStoreFloat3(&nextPos,
                             DirectX::XMVectorMultiplyAdd(newVel, DirectX::XMVectorReplicate(m_retargetTimer), DirectX::XMLoadFloat3(&m_pos)));
      nextPos.y = g_location->m_landscape.m_heightMap->GetValue(nextPos.x, nextPos.z);
      if (nextPos.y > 0.0f)
      {
        m_wayPoint = nextPos;
        m_state = StateIdle;
        RecordHistoryPosition(true);
        g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "ChangeDirection");
        END_PROFILE(g_profiler, "SearchForIdleDir");
        return true;
      }
    }
  }

  END_PROFILE(g_profiler, "SearchForIdleDir");
  return false;
}


DirectX::XMFLOAT3 Virii::AdvanceDeadPositionVector(int _index, DirectX::XMFLOAT3 const& _pos, float _time)
{
  float distance = 10.0f * _time;

  // The eight compass offsets the switch used to spell out, in the same order:
  // index 0 is -x and they turn anticlockwise from there.
  static float const offsets[8][2] = {{-1.0f, 0.0f}, {-1.0f, 1.0f}, {0.0f, 1.0f},  {1.0f, 1.0f},
                                      {1.0f, 0.0f},  {1.0f, -1.0f}, {0.0f, -1.0f}, {-1.0f, -1.0f}};
  float const* const offset = offsets[_index % 8];

  return DirectX::XMFLOAT3(_pos.x + offset[0] * distance, _pos.y, _pos.z + offset[1] * distance);
}


bool Virii::AdvanceDead()
{
  for (int i = 0; i < static_cast<int>(m_positionHistory.size()); ++i)
  {
    DirectX::XMFLOAT3* thisPos = &m_positionHistory[i]->m_pos;
    *thisPos = AdvanceDeadPositionVector(i, *thisPos, SERVER_ADVANCE_PERIOD);
  }
  return true;
}


bool Virii::IsInView()
{
  DirectX::XMFLOAT3 const centrePos = m_pos;
  DirectX::XMVECTOR const centre = DirectX::XMLoadFloat3(&centrePos);
  float radiusSqd = 0.0f;

  for (int i = 0; i < static_cast<int>(m_positionHistory.size()); ++i)
  {
    float distance =
      DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_positionHistory[i]->m_pos), centre)));
    if (distance > radiusSqd)
    {
      radiusSqd = distance;
    }
  }

  float radius = sqrtf(radiusSqd);

  // SphereInViewFrustum still takes a Vector3 -- Camera belongs to T22.
  return g_camera->SphereInViewFrustum(centrePos, radius);
}


void Virii::ListSoundEvents(std::vector<const char*>* _list)
{
  Entity::ListSoundEvents(_list);
  _list->push_back("ChangeDirection");
}


void Virii::Render(float predictionTime, int teamId, int _detail)
{
  predictionTime += SERVER_ADVANCE_PERIOD;

  DirectX::XMFLOAT3 predictedPos;
  DirectX::XMStoreFloat3(&predictedPos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(predictionTime),
                                                                     DirectX::XMLoadFloat3(&m_pos)));
  if (m_onGround && _detail == 1)
  {
    predictedPos.y = g_location->m_landscape.m_heightMap->GetValue(predictedPos.x, predictedPos.z) + m_hoverHeight;
  }

  float health = (float)m_stats[StatHealth] / (float)EntityBlueprint::GetStat(m_type, StatHealth);

  RGBAColour wormColour = g_location->m_teams[m_id.GetTeamId()].m_colour * health;
  RGBAColour glowColour(200, 100, 100);
  wormColour.a = 200;
  glowColour.a = 150;

  if (m_dead)
  {
    wormColour.r = wormColour.g = wormColour.b = 250;
    wormColour.a = 100 * m_stats[StatHealth] / 100.0f;

    glowColour.r = glowColour.g = glowColour.b = 250;
    glowColour.a = 250 * m_stats[StatHealth] / 100.0f;
  }

  // SurfaceMap2D<Vector3> still returns a Vector3 -- Landscape belongs to T18.
  DirectX::XMFLOAT3 landNormal(0.0f, 1.0f, 0.0f);
  if (_detail == 1)
    landNormal = g_location->m_landscape.m_normalMap->GetValue(predictedPos.x, predictedPos.z);

  ViriiHistory prevPos;
  prevPos.m_pos = predictedPos;
  DirectX::XMStoreFloat3(&prevPos.m_right,
                         DirectX::XMVector3Cross(DirectX::XMVectorNegate(DirectX::XMLoadFloat3(&m_front)), DirectX::XMLoadFloat3(&landNormal)));

  // Braced to zero: Vector3's default constructor did it and this stays zero
  // when the history is empty.
  DirectX::XMFLOAT3 firstPos{0.0f, 0.0f, 0.0f};
  if (static_cast<int>(m_positionHistory.size()) > 0)
    firstPos = m_positionHistory[0]->m_pos;

  DirectX::XMVECTOR const toFirst = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&predictedPos), DirectX::XMLoadFloat3(&firstPos));
  prevPos.m_distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(toFirst));

  // SetLength, whose zero-length fallback left (10, 0, 0) -- reachable on the
  // first frame, before any history exists.
  DirectX::XMStoreFloat3(&prevPos.m_glowDiff, NearlyEquals(prevPos.m_distance, 0.0f) ? DirectX::XMVectorSet(10.0f, 0.0f, 0.0f, 0.0f)
                                                                                     : DirectX::XMVectorScale(toFirst, 10.0f / prevPos.m_distance));

#define wormWidth 3.0f
#define wormTexW 32.0f / (32.0f + 128.0f)
#define wormTexHeight (10.0f * 2) / 1024.0f

#define glowWidth 12.0f
#define glowTexXpos 32.0f / (32.0f + 128.0f)
#define glowTexH 128.0f / 512.0f

  float wormTexYpos = 0.0f;
  float skippedDistance = 0.0f;

  int lastIndex = static_cast<int>(m_positionHistory.size());
  if (_detail > 1)
  {
    int amountToChop = _detail - 1;
    amountToChop = std::min(amountToChop, 2);
    lastIndex *= (1.0f - 0.25f * amountToChop);
  }

  for (int i = 0; i < lastIndex; i++)
  {
    ViriiHistory* history = m_positionHistory[i].get();

    if (!history->m_required)
    {
      // This worm is a long way away, so we can skip this point in his history
      skippedDistance += history->m_distance;
      continue;
    }

    DirectX::XMVECTOR const pos = DirectX::XMLoadFloat3(&history->m_pos);
    DirectX::XMVECTOR const prevRight = DirectX::XMLoadFloat3(&prevPos.m_right);
    DirectX::XMVECTOR const wormRightAngle = DirectX::XMVectorScale(prevRight, wormWidth);
    DirectX::XMVECTOR const glowRightAngle = DirectX::XMVectorScale(prevRight, glowWidth);
    float distance = prevPos.m_distance + skippedDistance;
    DirectX::XMVECTOR const glowDiff = DirectX::XMLoadFloat3(&prevPos.m_glowDiff);
    DirectX::XMVECTOR const prevPosVec = DirectX::XMLoadFloat3(&prevPos.m_pos);

    skippedDistance = 0.0f;


    //
    // Worm shape

    int newCol = wormColour.a - distance;
    if (newCol < 0)
      newCol = 0;
    wormColour.a = newCol;
    glColor4ubv(wormColour.GetData());

    glTexCoord2f(0.0f, wormTexYpos);
    EmitVertex(DirectX::XMVectorSubtract(prevPosVec, wormRightAngle));
    glTexCoord2f(wormTexW, wormTexYpos);
    EmitVertex(DirectX::XMVectorAdd(prevPosVec, wormRightAngle));
    wormTexYpos += (distance * 6) / 512.0f;
    glTexCoord2f(wormTexW, wormTexYpos);
    EmitVertex(DirectX::XMVectorAdd(pos, wormRightAngle));
    glTexCoord2f(0.0f, wormTexYpos);
    EmitVertex(DirectX::XMVectorSubtract(pos, wormRightAngle));

    //
    // Glow effect

    if (_detail < 4)
    {
      newCol = glowColour.a - distance;
      if (newCol < 0)
        newCol = 0;
      glowColour.a = newCol;
      glColor4ubv(glowColour.GetData());

      glTexCoord2f(glowTexXpos, 0.0f);
      EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorSubtract(prevPosVec, glowRightAngle), glowDiff));
      glTexCoord2f(1.0f, 0.0f);
      EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(prevPosVec, glowRightAngle), glowDiff));

      glTexCoord2f(1.0f, glowTexH);
      EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorAdd(pos, glowRightAngle), glowDiff));
      glTexCoord2f(glowTexXpos, glowTexH);
      EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorSubtract(pos, glowRightAngle), glowDiff));
    }

    prevPos = *history;
  }
}

/*
void Virii::RenderLowDetail( float predictionTime, int teamId )
{
    predictionTime += 0.1f;

    Vector3 predictedPos = AsLegacy(m_pos) + AsLegacy(m_vel) * predictionTime;
    if( m_onGround )
  {
    predictedPos.y = g_location->m_landscape.m_heightMap->GetValue( predictedPos.x, predictedPos.z ) + m_hoverHeight;
  }

    float health = (float) m_stats[StatHealth] / (float) EntityBlueprint::GetStat( m_type, StatHealth );
    RGBAColour colour = g_location->m_teams[ m_teamId ].m_colour * health;
    colour.a = 200;
    if( m_dead )
    {
        colour.r = colour.g = colour.b = 50;
        colour.a = 100 * m_stats[StatHealth] / 100.0f;
    }

    glColor4ubv( colour.GetData() );

    Vector3 landNormal = g_location->m_landscape.m_normalMap->GetValue( predictedPos.x, predictedPos.z );

    ViriiHistory prevPos;
    prevPos.m_pos = predictedPos;
    prevPos.m_right = -AsLegacy(m_front) ^ landNormal;
    Vector3 firstPos;
    if( static_cast<int>(m_positionHistory.size()) > 0 ) firstPos = m_positionHistory[0]->m_pos;
    prevPos.m_distance = ( predictedPos - firstPos ).Mag();

    float width = 3.0f;
    float texYpos = 0.0f;
    float texW = 32.0f/(32.0f+128.0f);
    float texHeight = (10.0f * 2)/1024.0f;

    glBegin         ( GL_QUADS );

    for( int i = 0; i < static_cast<int>(m_positionHistory.size()); i += 1 )
    {
        ViriiHistory *history = m_positionHistory[i];
        Vector3 pos = history->m_pos;
        Vector3 rightAngle = prevPos.m_right * width;
        float distance = prevPos.m_distance;

        int newCol = colour.a - distance;
        if( newCol < 0 ) newCol = 0;
        colour.a = newCol;
        glColor4ubv( colour.GetData() );

        glTexCoord2f( 0.0f, texYpos );  glVertex3fv( (prevPos.m_pos - rightAngle).GetData() );
        glTexCoord2f( texW, texYpos );  glVertex3fv( (prevPos.m_pos + rightAngle).GetData() );

        texYpos += (distance*6) / 512.0f;

        glTexCoord2f( texW, texYpos );  glVertex3fv( (pos + rightAngle).GetData() );
        glTexCoord2f( 0.0f, texYpos );  glVertex3fv( (pos - rightAngle).GetData() );

        prevPos = *history;
    }

    glEnd();

}

void Virii::RenderGlow ( float predictionTime, int teamId )
{
    predictionTime += 0.1f;

    Vector3 predictedPos = AsLegacy(m_pos) + AsLegacy(m_vel) * predictionTime;
    if( m_onGround )
  {
    //predictedPos.y = g_location->m_landscape.m_heightMap->GetValue( predictedPos.x, predictedPos.z ) + m_hoverHeight;
  }

    float health = (float) m_stats[StatHealth] / (float) EntityBlueprint::GetStat( m_type, StatHealth );
    //RGBAColour colour = g_location->m_teams[ m_teamId ].m_colour * health;
    RGBAColour colour( 200, 100, 100 );
    colour.a = 100;
    if( m_dead )
    {
        colour.r = colour.g = colour.b = 50;
        colour.a = 250 * m_stats[StatHealth] / 100.0f;
    }

    glColor4ubv( colour.GetData() );

    //Vector3 landNormal = g_location->m_landscape.m_normalMap->GetValue( predictedPos.x, predictedPos.z );
    Vector3 landNormal = g_upVector;

    ViriiHistory prevPos;
    prevPos.m_pos = predictedPos;
    prevPos.m_right = -AsLegacy(m_front) ^ landNormal;
    Vector3 firstPos;
    if( static_cast<int>(m_positionHistory.size()) > 0 ) firstPos = m_positionHistory[0]->m_pos;
    prevPos.m_distance = ( predictedPos - firstPos ).Mag();

    float width = 12.0f;
    float texXpos = 32.0f/(32.0f+128.0f);
    float texH = 128.0f/512.0f;

    glBegin         ( GL_QUADS );

    for( int i = 0; i < static_cast<int>(m_positionHistory.size()); i += 1 )
    {
        ViriiHistory *history = m_positionHistory[i];
        Vector3 pos = history->m_pos;
        Vector3 rightAngle = prevPos.m_right * width;
        float distance = prevPos.m_distance;

        Vector3 diff = (prevPos.m_pos - pos).Normalise();

        int newCol = colour.a - distance;
        if( newCol < 0 ) newCol = 0;
        colour.a = newCol;
        glColor4ubv( colour.GetData() );

        glTexCoord2f( texXpos, 0.0f );      glVertex3fv( (prevPos.m_pos - rightAngle + diff * 10.0f).GetData() );
        glTexCoord2f( 1.0f, 0.0f );         glVertex3fv( (prevPos.m_pos + rightAngle + diff * 10.0f).GetData() );

        glTexCoord2f( 1.0f, texH );         glVertex3fv( (pos + rightAngle - diff * 10.0f).GetData() );
        glTexCoord2f( texXpos, texH );      glVertex3fv( (pos - rightAngle - diff * 10.0f).GetData() );

        prevPos = *history;
    }

    glEnd();

}
*/
