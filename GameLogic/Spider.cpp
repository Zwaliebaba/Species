#include "pch.h"
#include "SoundSources.h"

#include <float.h>

#include "DebugRender.h"
#include "MathUtils.h"
#include "PersistingDebugRender.h"
#include "Profiler.h"
#include "Resource.h"
#include "Shape.h"
#include "TextRenderer.h"

#include "EntityLeg.h"
#include "Spider.h"

#include "SoundSystem.h"

#include "EntityGrid.h"
#include "Explosion.h"
#include "Location.h"
#include "GameTime.h"
#include "ParticleSystem.h"
#include "WorldPointers.h"


// #define FOOT_MOVE_THRESHOLD	        5.0f	// Lower means feet are lifted when less distant from their ideal pos, and thus smaller steps are taken
// #define FOOT_EMERGENCY_THRESHOLD	12.0f
// #define FOOT_DAMAGE_RADIUS			20.0f	// Size of region damaged by foot falls
// #define FOOT_DAMAGE_STRENGTH		10.0f
//
// #define HOVER_HEIGHT				3.0f
//
// #define TURN_RATE					0.18f
// #define NORMAL_SPEED				300.0f
// #define ATTACK_SPEED				90.0f
//
// #define MAX_PATH_HEIGHT				100.0f
//
// #define ATTACK_SEARCH_MAX_RADIUS	250.0f
// #define ATTACK_SEARCH_MIN_RADIUS	100.0f
// #define ATTACK_PREAMBLE				5.0f	// Minimum time between choosing a target and starting to charge (most of this time will normally be spent
// turning)

#define FOOT_MOVE_THRESHOLD 5.0f // Lower means feet are lifted when less distant from their ideal pos, and thus smaller steps are taken
#define FOOT_EMERGENCY_THRESHOLD 12.0f
#define FOOT_DAMAGE_RADIUS 20.0f // Size of region damaged by foot falls
#define FOOT_DAMAGE_STRENGTH 10.0f

#define HOVER_HEIGHT 3.0f

#define TURN_RATE 0.1f
#define NORMAL_SPEED 300.0f
#define ATTACK_SPEED 90.0f

#define MAX_PATH_HEIGHT 100.0f

#define ATTACK_SEARCH_MAX_RADIUS 250.0f
#define ATTACK_SEARCH_MIN_RADIUS 100.0f
#define ATTACK_PREAMBLE 5.0f // Minimum time between choosing a target and starting to charge (most of this time will normally be spent turning)

#define SPIRIT_MAXSEARCHRANGE 100.0f
#define SPIRIT_MINSEARCHRANGE 30.0f


//*****************************************************************************
// Class Spider
//*****************************************************************************

Spider::Spider()
  : m_state(StateIdle),
    m_nextLegMoveTime(-1.0f),
    m_speed(0.0f),
    m_targetHoverHeight(HOVER_HEIGHT),
    m_up(g_upVector),
    m_retargetTimer(0.0f),
    m_spiritId(-1),
    m_eggLay(nullptr)
{
  m_stats[StatHealth] = 200;

  m_shape = g_resource->GetShape("Spider.shp");
  m_eggLay = m_shape->m_rootFragment->LookupMarker("MarkerEggLay");

  m_parameters[0].m_legLift = 3.0f;
  m_parameters[0].m_idealLegSlope = 2.6f;
  m_parameters[0].m_legSwingDuration = 0.4f;
  m_parameters[0].m_delayBetweenLifts = 0.11f;
  m_parameters[0].m_lookAheadCoef = 0.75f;
  m_parameters[0].m_idealSpeed = 10.0f;

  m_parameters[1].m_legLift = 3.0f;
  m_parameters[1].m_idealLegSlope = 2.6f;
  m_parameters[1].m_legSwingDuration = 0.25f;
  m_parameters[1].m_delayBetweenLifts = 0.06f;
  m_parameters[1].m_lookAheadCoef = 0.3f;
  m_parameters[1].m_idealSpeed = 30.0f;

  m_parameters[2].m_legLift = 3.0f;
  m_parameters[2].m_idealLegSlope = 2.6f;
  m_parameters[2].m_legSwingDuration = 0.25f;
  m_parameters[2].m_delayBetweenLifts = 0.04f;
  m_parameters[2].m_lookAheadCoef = 0.2f;
  m_parameters[2].m_idealSpeed = 50.0f;

  // Initialise legs
  for (int i = 0; i < SPIDER_NUM_LEGS; ++i)
  {
    char markerName[] = "MarkerLegX";
    markerName[strlen(markerName) - 1] = '0' + i;
    m_legs[i] = new EntityLeg(i, this, "SpiderLegUpper.shp", "SpiderLegLower.shp", markerName);
    m_legs[i]->m_legLift = m_parameters[2].m_legLift;
    m_legs[i]->m_idealLegSlope = m_parameters[2].m_idealLegSlope;
    m_legs[i]->m_legSwingDuration = m_parameters[2].m_legSwingDuration;
    m_legs[i]->m_lookAheadCoef = m_parameters[2].m_lookAheadCoef;
  }
  m_delayBetweenLifts = m_parameters[2].m_delayBetweenLifts;
}


Spider::~Spider()
{
  for (int i = 0; i < SPIDER_NUM_LEGS; ++i)
  {
    delete m_legs[i];
  }
}


void Spider::Begin()
{
  Entity::Begin();

  for (int i = 0; i < SPIDER_NUM_LEGS; ++i)
  {
    m_legs[i]->LiftFoot(m_targetHoverHeight);
    m_legs[i]->PlantFoot();
  }

  m_targetPos = m_pos;
}


void Spider::ChangeHealth(int _amount)
{
  bool dead = m_dead;

  if (!m_dead && _amount < -1)
  {
    Entity::ChangeHealth(_amount);

    float fractionDead = 1.0f - (float)m_stats[StatHealth] / (float)EntityBlueprint::GetStat(TypeSpider, StatHealth);
    fractionDead = std::max(fractionDead, 0.0f);
    fractionDead = std::min(fractionDead, 1.0f);
    DirectX::XMFLOAT4X4 transform;
    DirectX::XMStoreFloat4x4(&transform,
                             BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::XMLoadFloat3(&m_up), DirectX::XMLoadFloat3(&m_pos)));
    g_explosionManager.AddExplosion(m_shape, transform, fractionDead);
  }
}


int Spider::CalcWhichFootToMove()
{
  if (g_gameTime < m_nextLegMoveTime)
    return -1;

  float bestScore = 0.0f;
  float bestFoot = -1;

  for (int i = 0; i < SPIDER_NUM_LEGS; ++i)
  {
    if (m_legs[i]->m_foot.m_state == EntityFoot::FootState::OnGround)
    {
      float score = m_legs[i]->CalcFootsDesireToMove(m_targetHoverHeight);
      if (score > FOOT_EMERGENCY_THRESHOLD)
      {
        m_legs[i]->LiftFoot(m_targetHoverHeight);
      }
      else if (score > bestScore)
      {
        bestScore = score;
        bestFoot = i;
      }
    }
  }

  if (bestScore > FOOT_MOVE_THRESHOLD)
  {
    m_nextLegMoveTime = g_gameTime + m_delayBetweenLifts;
    return bestFoot;
  }

  return -1;
}


void Spider::StompFoot(DirectX::XMFLOAT3 const& _pos)
{
  for (int p = 0; p < 3; ++p)
  {
    // The three RNG calls stay in this order.
    DirectX::XMFLOAT3 const vel(syncsfrand(20.0f), 5.0f + syncfrand(5.0f), syncsfrand(20.0f));

    g_particleSystem->CreateParticle(_pos, vel, Particle::TypeMuzzleFlash, 10.0f);
  }


  //
  // Damage everyone nearby

  int numFound;
  WorldObjectId* ids = g_location->m_entityGrid->GetEnemies(_pos.x, _pos.z, FOOT_DAMAGE_RADIUS, &numFound, m_id.GetTeamId());
  for (int i = 0; i < numFound; ++i)
  {
    WorldObjectId id = ids[i];
    WorldObject* obj = g_location->GetEntity(id);
    Entity* entity = (Entity*)obj;

    float distance =
      DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&entity->m_pos), DirectX::XMLoadFloat3(&_pos))));
    float fraction = (FOOT_DAMAGE_RADIUS - distance) / FOOT_DAMAGE_RADIUS;
    fraction *= (1.0f + syncfrand(0.3f));

    entity->ChangeHealth(FOOT_DAMAGE_STRENGTH * fraction * -1.0f);

    DirectX::XMFLOAT3 push;
    DirectX::XMStoreFloat3(&push, DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&entity->m_pos),
                                                                                                               DirectX::XMLoadFloat3(&_pos))),
                                                         fraction));

    if (entity->m_onGround)
    {
      push.y = fraction * FOOT_DAMAGE_STRENGTH * 0.3f;
    }
    else
    {
      push.y = -1;
    }
    DirectX::XMStoreFloat3(&entity->m_vel, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&entity->m_vel), DirectX::XMLoadFloat3(&push)));
    entity->m_onGround = false;
  }
}


void Spider::UpdateLegsPouncing()
{
  float fractionComplete = (g_gameTime - m_pounceStartTime) * 2.0f;
  for (int i = 0; i < SPIDER_NUM_LEGS; ++i)
  {
    m_legs[i]->AdvanceSpiderPounce(fractionComplete);
  }
}


void Spider::UpdateLegs()
{
  {
    int bestFootToMove = CalcWhichFootToMove();
    if (bestFootToMove != -1)
    {
      m_legs[bestFootToMove]->LiftFoot(m_targetHoverHeight);
    }
    bestFootToMove = CalcWhichFootToMove();
    if (bestFootToMove != -1)
    {
      m_legs[bestFootToMove]->LiftFoot(m_targetHoverHeight);
    }
  }

  // Work out which set of parameters to use
  int stage = 0;
  for (int i = 1; i < 3; ++i)
  {
    float diffBest = fabs(DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&m_vel))) - m_parameters[stage].m_idealSpeed);
    float diffCurrent = fabs(DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&m_vel))) - m_parameters[i].m_idealSpeed);
    if (diffCurrent < diffBest)
    {
      stage = i;
    }
  }

  for (int i = 0; i < SPIDER_NUM_LEGS; ++i)
  {
    // Pass the current parameters through to the legs
    m_legs[i]->m_legLift = m_parameters[stage].m_legLift;
    m_legs[i]->m_idealLegSlope = m_parameters[stage].m_idealLegSlope * 2.5f;
    m_legs[i]->m_legSwingDuration = m_parameters[stage].m_legSwingDuration;
    m_legs[i]->m_lookAheadCoef = m_parameters[stage].m_lookAheadCoef;

    bool footPlanted = m_legs[i]->Advance();
    //		if (m_attacking && footPlanted)
    //		{
    //			Vector3 const &legPos = m_legs[i]->m_foot.m_pos;
    //			StompFoot(legPos);
    //		}

    if (footPlanted)
      g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "FootFall");
  }

  m_delayBetweenLifts = m_parameters[stage].m_delayBetweenLifts;
}


// Tests that the line from m_pos to _dest doesn't go above a certain height
float Spider::IsPathOK(DirectX::XMFLOAT3 const& _dest)
{
  DirectX::XMVECTOR const toDestFull = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&_dest), DirectX::XMLoadFloat3(&m_pos));
  float distToDest = DirectX::XMVectorGetX(DirectX::XMVector3Length(toDestFull));
  float const sampleSeperation = 8.0f;

  // SetLength. distToDest can be zero if the destination is our own position,
  // and the loop below then does not execute at all -- but the legacy fallback
  // still ran, leaving (8, 0, 0), so it is reproduced to keep the step finite.
  DirectX::XMVECTOR const toDest = NearlyEquals(distToDest, 0.0f) ? DirectX::XMVectorSet(sampleSeperation, 0.0f, 0.0f, 0.0f)
                                                                  : DirectX::XMVectorScale(toDestFull, sampleSeperation / distToDest);

  DirectX::XMVECTOR position = DirectX::XMLoadFloat3(&m_pos);
  for (float i = 0.0f; i < distToDest; i += sampleSeperation)
  {
    DirectX::XMFLOAT3 pos;
    DirectX::XMStoreFloat3(&pos, position);
    float height = g_location->m_landscape.m_heightMap->GetValue(pos.x, pos.z);
    if (height > MAX_PATH_HEIGHT || height < 0.1f /*Sea level*/)
    {
      float rv = i / distToDest;
      return rv;
    }
    position = DirectX::XMVectorAdd(position, toDest);
  }

  return 1.0f;
}


void Spider::DetectCollisions()
{
  DirectX::XMFLOAT3 pos;
  DirectX::XMStoreFloat3(&pos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&m_vel)));
  int numFound;
  WorldObjectId* neighbours = g_location->m_entityGrid->GetNeighbours(pos.x, pos.z, 22.0f, &numFound);

  // Vector3's default constructor zeroed this and XMFLOAT3's does not; it is
  // accumulated into below, so the zero is load-bearing.
  DirectX::XMVECTOR escapeVector = DirectX::XMVectorZero();
  bool collisionDetected = false;

  for (int i = 0; i < numFound; ++i)
  {
    Entity* ent = g_location->GetEntity(neighbours[i]);
    if (ent->m_type == Entity::TypeSpider && ent->m_id != m_id)
    {
      Entity* entity = g_location->GetEntity(neighbours[speciesRandom() % numFound]);
      DEBUG_ASSERT(entity);
      DirectX::XMVECTOR const toNeighbour = DirectX::XMVector3Normalize(
        DirectX::XMVectorSetY(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&entity->m_pos)), 0.0f));
      escapeVector = DirectX::XMVectorAdd(escapeVector, toNeighbour);
      collisionDetected = true;
    }
  }

  if (collisionDetected)
  {
    DirectX::XMStoreFloat3(&m_targetPos,
                           DirectX::XMVectorMultiplyAdd(escapeVector, DirectX::XMVectorReplicate(40.0f), DirectX::XMLoadFloat3(&m_pos)));
  }
}


bool Spider::FaceTarget()
{
  DirectX::XMVECTOR const toTarget =
    DirectX::XMVectorSetY(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_targetPos), DirectX::XMLoadFloat3(&m_pos)), 0.0f);
  float toTargetMag = DirectX::XMVectorGetX(DirectX::XMVector3Length(toTarget));
  DirectX::XMVECTOR const toTargetNormalised = DirectX::XMVectorScale(toTarget, 1.0f / toTargetMag);

  DirectX::XMVECTOR front = DirectX::XMLoadFloat3(&m_front);
  float dotProd = DirectX::XMVectorGetX(DirectX::XMVector3Dot(toTargetNormalised, front));
  if (dotProd < 0.999f)
  {
    // SetLength then RotateAround, and the zero-length fallback is LOAD-BEARING
    // as it is in GunTurret and Armour: facing exactly away makes the cross
    // product zero, and the fallback's rotation about world X is what breaks
    // the deadlock.
    float const turnRate = dotProd < 0.9f ? TURN_RATE : TURN_RATE / 2.0f;
    DirectX::XMVECTOR const rotationAxis = DirectX::XMVector3Cross(front, toTargetNormalised);
    float const axisLength = DirectX::XMVectorGetX(DirectX::XMVector3Length(rotationAxis));
    DirectX::XMVECTOR const rotation =
      NearlyEquals(axisLength, 0.0f) ? DirectX::XMVectorSet(turnRate, 0.0f, 0.0f, 0.0f) : DirectX::XMVectorScale(rotationAxis, turnRate / axisLength);

    // RotateAround's angle is the vector's magnitude, with the same 1e-8 guard.
    float const rotationLengthSquared = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(rotation));
    if (rotationLengthSquared >= 1e-8f)
    {
      float const spin = sqrtf(rotationLengthSquared);
      front = DirectX::XMVector3Transform(front, DirectX::XMMatrixRotationAxis(DirectX::XMVectorScale(rotation, 1.0f / spin), spin));
    }
    DirectX::XMStoreFloat3(&m_front, DirectX::XMVector3Normalize(front));
    m_vel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

    return false;
  }

  return true;
}


bool Spider::AdvanceToTarget()
{
  DirectX::XMVECTOR const toTarget =
    DirectX::XMVectorSetY(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_targetPos), DirectX::XMLoadFloat3(&m_pos)), 0.0f);
  float toTargetMag = DirectX::XMVectorGetX(DirectX::XMVector3Length(toTarget));


  //
  // Have we reached our target

  if (toTargetMag < 5.0f)
  {
    return true;
  }

  bool facingTarget = FaceTarget();

  DirectX::XMVECTOR const toTargetNormalised = DirectX::XMVectorScale(toTarget, 1.0f / toTargetMag);
  float dotProd = DirectX::XMVectorGetX(DirectX::XMVector3Dot(toTargetNormalised, DirectX::XMLoadFloat3(&m_front)));

  if (dotProd > 0.5f)
  {
    // SetLength on m_front, which FaceTarget keeps normalised, so never zero.
    float const speed = toTargetMag < 30.0f ? toTargetMag : 30.0f;
    DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&m_front)), speed));
    DirectX::XMStoreFloat3(&m_pos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD),
                                                                DirectX::XMLoadFloat3(&m_pos)));
  }

  DetectCollisions();

  return false;
}


bool Spider::AdvanceIdle()
{
  //
  // Time to do something new?

  m_retargetTimer -= SERVER_ADVANCE_PERIOD;
  if (m_retargetTimer <= 0.0f)
  {
    m_retargetTimer = 5.0f;
    bool foundNewTarget = false;
    if (!foundNewTarget)
      foundNewTarget = SearchForEnemies();
    if (!foundNewTarget)
      foundNewTarget = SearchForSpirits();
    if (!foundNewTarget)
      foundNewTarget = SearchForRandomPos();

    if (foundNewTarget)
      return false;
  }


  //
  // Just wander around a bit

  bool arrived = AdvanceToTarget();
  if (arrived)
    m_vel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

  return false;
}


bool Spider::AdvanceAttack()
{
  m_targetPos = m_pounceTarget;
  bool facingTarget = FaceTarget();

  if (facingTarget)
  {
    float distance = DirectX::XMVectorGetX(
      DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pounceTarget), DirectX::XMLoadFloat3(&m_pos))));

    if (distance > 150.0f)
    {
      AdvanceToTarget();
    }
    else
    {
      float force = sqrtf(distance) * 24.0f;
      // if( force > 130.0f ) force = 130.0f;

      DirectX::XMFLOAT3 up;
      DirectX::XMStoreFloat3(
        &up, DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pounceTarget), DirectX::XMLoadFloat3(&m_pos))));
      up.y = 0.5f;
      DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&up)), force));

      m_onGround = false;
      m_retargetTimer = 3.0f;
      m_state = StatePouncing;
      m_pounceStartTime = g_gameTime;

      DirectX::XMVECTOR const forwards = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pounceTarget), DirectX::XMLoadFloat3(&m_pos));
      for (int i = 0; i < SPIDER_NUM_LEGS; ++i)
      {
        m_legs[i]->m_foot.m_state = EntityFoot::FootState::Pouncing;
        DirectX::XMStoreFloat3(&m_legs[i]->m_foot.m_bodyToFoot,
                               DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&m_legs[i]->m_foot.m_pos)));
        m_legs[i]->m_foot.m_lastGroundPos = m_legs[i]->m_foot.m_pos;
        DirectX::XMStoreFloat3(&m_legs[i]->m_foot.m_targetPos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_legs[i]->m_foot.m_pos), forwards));
      }

      g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "Attack");
      g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "Pounce");
    }
  }

  return false;
}


bool Spider::AdvancePouncing()
{
  m_vel.y -= 40.0f;
  DirectX::XMStoreFloat3(&m_pos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD),
                                                              DirectX::XMLoadFloat3(&m_pos)));

  float landHeight = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);
  if (m_pos.y < landHeight + 1.0f)
  {
    m_pos.y = landHeight + 1.0f;
    m_vel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_onGround = true;
    m_state = StateIdle;
    m_retargetTimer = 6.0f;

    for (int i = 0; i < SPIDER_NUM_LEGS; ++i)
    {
      m_legs[i]->m_foot.m_state = EntityFoot::FootState::OnGround;
    }

    g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "PounceLand");

    // Squash people
    float squashRange = 40.0f;
    float damage = 100.0f;
    int numFound;
    WorldObjectId* enemies = g_location->m_entityGrid->GetEnemies(m_pos.x, m_pos.z, squashRange, &numFound, m_id.GetTeamId());
    for (int i = 0; i < numFound; ++i)
    {
      WorldObjectId id = enemies[i];
      Entity* entity = g_location->GetEntity(id);

      float distance = DirectX::XMVectorGetX(
        DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&entity->m_pos), DirectX::XMLoadFloat3(&m_pos))));
      float fraction = (squashRange - distance) / squashRange;
      fraction *= (1.0f + syncfrand(0.3f));

      if (distance < 20.0f)
        entity->ChangeHealth(fraction * -damage);

      // push.y is set from the magnitude of the WHOLE vector, which at that
      // point is still m_front -- so the y term depends on m_front's length,
      // not on its y. Preserved as written.
      DirectX::XMFLOAT3 push = m_front;
      push.y = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&push))) * 4.0f;

      float pushLength = fraction * 30.0f;
      pushLength = std::min(20.0f, pushLength);

      // SetLength on a vector whose y is at least 4x m_front's length, so it is
      // never zero here.
      DirectX::XMStoreFloat3(&push, DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&push)), pushLength));

      DirectX::XMStoreFloat3(&entity->m_vel, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&entity->m_vel), DirectX::XMLoadFloat3(&push)));
      entity->m_onGround = false;
    }
  }

  return false;
}


bool Spider::SearchForRandomPos()
{
  for (int i = 0; i < 10; ++i)
  {
    m_targetPos = m_spawnPoint;
    m_targetPos.x += syncsfrand(m_roamRange);
    m_targetPos.z += syncsfrand(m_roamRange);

    if (IsPathOK(m_targetPos) > 0.99f)
    {
      break;
    }
  }

  float height = g_location->m_landscape.m_heightMap->GetValue(m_targetPos.x, m_targetPos.z);
  m_targetPos.y = height;
  return true;
}


bool Spider::SearchForEnemies()
{
  float maxRange = ATTACK_SEARCH_MAX_RADIUS;
  float minRange = ATTACK_SEARCH_MIN_RADIUS;

  WorldObjectId targetId = g_location->m_entityGrid->GetBestEnemy(m_pos.x, m_pos.z, minRange, maxRange, m_id.GetTeamId());

  Entity* entity = g_location->GetEntity(targetId);

  if (entity && !entity->m_dead)
  {
    m_pounceTarget = entity->m_pos;
    m_state = StateAttack;
    return true;
  }
  else
  {
    return false;
  }
}


bool Spider::SearchForSpirits()
{
  START_PROFILE(g_profiler, "SearchSpirits");
  Spirit* found = nullptr;
  int foundIndex = -1;
  float nearest = 9999.9f;

  for (int i = 0; i < g_location->m_spirits.Size(); ++i)
  {
    if (g_location->m_spirits.ValidIndex(i))
    {
      Spirit* s = g_location->m_spirits.GetPointer(i);
      if (s->NumNearbyEggs() < 3 && s->m_pos.y > 10)
      {
        float theDist =
          DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&s->m_pos), DirectX::XMLoadFloat3(&m_pos))));

        if (theDist <= SPIRIT_MAXSEARCHRANGE && theDist >= SPIRIT_MINSEARCHRANGE && theDist < nearest && s->m_state == Spirit::StateFloating)
        {
          found = s;
          foundIndex = i;
          nearest = theDist;
        }
      }
    }
  }

  if (found)
  {
    m_spiritId = foundIndex;
    DirectX::XMVECTOR const usToThem = DirectX::XMVectorScale(
      DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&found->m_pos), DirectX::XMLoadFloat3(&m_pos))), 45.0f);
    DirectX::XMStoreFloat3(&m_targetPos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&found->m_pos), usToThem));
    m_targetPos.y = g_location->m_landscape.m_heightMap->GetValue(m_targetPos.x, m_targetPos.z);
    m_state = StateEggLaying;
  }

  END_PROFILE(g_profiler, "SearchSpirits");
  return found;
}


bool Spider::AdvanceEggLaying()
{
  //
  // Time to look around for enemies

  m_retargetTimer -= SERVER_ADVANCE_PERIOD;
  if (m_retargetTimer <= 0.0f)
  {
    m_retargetTimer = 5.0f;
    bool foundNewTarget = false;
    if (!foundNewTarget)
      foundNewTarget = SearchForEnemies();
    if (foundNewTarget)
      return false;
  }


  //
  // Advance to where we think the egg should go

  bool arrived = AdvanceToTarget();


  //
  // Lay an egg if we are in place

  if (arrived)
  {
    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::XMLoadFloat3(&m_up), DirectX::XMLoadFloat3(&m_pos)));

    // ShapeMarker::GetWorldMatrix still returns Matrix34 -- T10's seam.
    DirectX::XMFLOAT3 const eggLayPos = m_eggLay->GetWorldPosition(mat);

    g_location->SpawnEntities(eggLayPos, m_id.GetTeamId(), -1, TypeEgg, 1, g_zeroVector, 0.0f);

    g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "LayEgg");

    m_spiritId = -1;
    m_state = StateIdle;
  }

  return false;
}


bool Spider::Advance(Unit* _unit)
{
  if (!m_dead)
  {
    //
    // Do our action

    switch (m_state)
    {
    case StateIdle:
      AdvanceIdle();
      break;
    case StateEggLaying:
      AdvanceEggLaying();
      break;
    case StateAttack:
      AdvanceAttack();
      break;
    case StatePouncing:
      AdvancePouncing();
      break;
    }


    //
    // Adjust body height

    if (m_state == StatePouncing)
    {
      UpdateLegsPouncing();
    }
    else
    {
      float targetHeight = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);
      targetHeight += m_targetHoverHeight;
      float factor1 = 1.0f * SERVER_ADVANCE_PERIOD;
      float factor2 = 1.0f - factor1;
      m_pos.y = factor1 * targetHeight + factor2 * m_pos.y;
      UpdateLegs();
    }
  }

  return Entity::Advance(_unit);
}


void Spider::Render(float _predictionTime)
{
  if (m_dead)
    return;

  glDisable(GL_TEXTURE_2D);


  // RenderArrow(m_pos, m_targetPos, 1.0f);

  if (m_state == StateAttack)
  {
    // RenderArrow(m_pos, m_pounceTarget, 1.0f, RGBAColour(255,0,0) );
  }

  g_renderer->SetObjectLighting();


  //
  // Render body

  DirectX::XMFLOAT3 predictedMovement;
  DirectX::XMStoreFloat3(&predictedMovement, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), _predictionTime));

  DirectX::XMFLOAT3 predictedPos;
  DirectX::XMStoreFloat3(&predictedPos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&predictedMovement)));
  //	predictedPos.y = g_location->m_landscape.m_heightMap->GetValue(predictedPos.x, predictedPos.z) +
  //					 m_targetHoverHeight;

  // SurfaceMap2D<Vector3> still returns a Vector3 -- Landscape belongs to T18.
  DirectX::XMFLOAT3 const landUp = g_location->m_landscape.m_normalMap->GetValue(m_pos.x, m_pos.z);
  DirectX::XMVECTOR const up = DirectX::XMLoadFloat3(&landUp);

  // right comes from m_up, not from the landscape up just fetched. Preserved.
  DirectX::XMVECTOR const right = DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&m_up), DirectX::XMLoadFloat3(&m_front));
  DirectX::XMVECTOR const front = DirectX::XMVector3Cross(right, up);

  DirectX::XMMATRIX basis = BasisFromFrontAndUp(front, up, DirectX::XMLoadFloat3(&predictedPos));

  if (m_renderDamaged)
  {
    float timeIndex = g_gameTime + m_id.GetUniqueId() * 10;

    // r is row 0 and u is row 1, in the row-vector convention.
    if (frand() > 0.5f)
      basis.r[0] = DirectX::XMVectorScale(basis.r[0], 1.0f + sinf(timeIndex) * 0.5f);
    else
      basis.r[1] = DirectX::XMVectorScale(basis.r[1], 1.0f + sinf(timeIndex) * 0.5f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_COLOR);
  }

  DirectX::XMFLOAT4X4 mat;
  DirectX::XMStoreFloat4x4(&mat, basis);

  m_shape->Render(_predictionTime, mat);

  glDisable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  //
  // Render Legs

  for (int i = 0; i < SPIDER_NUM_LEGS; ++i)
  {
    m_legs[i]->Render(_predictionTime, predictedMovement);
  }

  g_renderer->UnsetObjectLighting();
}


bool Spider::RenderPixelEffect(float _predictionTime)
{
  Render(_predictionTime);

  DirectX::XMFLOAT3 predictedMovement;
  DirectX::XMStoreFloat3(&predictedMovement, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), _predictionTime));

  DirectX::XMFLOAT3 predictedPos;
  DirectX::XMStoreFloat3(&predictedPos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&predictedMovement)));
  // SurfaceMap2D<Vector3> still returns a Vector3 -- Landscape belongs to T18.
  DirectX::XMFLOAT3 const landUp = g_location->m_landscape.m_normalMap->GetValue(m_pos.x, m_pos.z);
  DirectX::XMVECTOR const up = DirectX::XMLoadFloat3(&landUp);

  // right comes from m_up, not from the landscape up just fetched. Preserved.
  DirectX::XMVECTOR const right = DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&m_up), DirectX::XMLoadFloat3(&m_front));
  DirectX::XMVECTOR const front = DirectX::XMVector3Cross(right, up);

  DirectX::XMFLOAT4X4 mat;
  DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(front, up, DirectX::XMLoadFloat3(&predictedPos)));
  g_renderer->MarkUsedCells(m_shape, mat);

  for (int i = 0; i < SPIDER_NUM_LEGS; ++i)
  {
    m_legs[i]->RenderPixelEffect(_predictionTime, predictedMovement);
  }

  return true;
}


bool Spider::IsInView()
{
  DirectX::XMFLOAT3 centre;
  DirectX::XMStoreFloat3(&centre, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&m_centrePos)));

  return g_camera->SphereInViewFrustum(centre, m_radius);
}


void Spider::ListSoundEvents(std::vector<const char*>* _list)
{
  Entity::ListSoundEvents(_list);

  _list->push_back("Pounce");
  _list->push_back("PounceLand");
  _list->push_back("FootFall");
  _list->push_back("LayEgg");
}
