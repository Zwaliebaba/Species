#include "pch.h"
#include "Globals.h"

#include <float.h>

#include "HiResTime.h"
#include "MathUtils.h"
#include "PersistingDebugRender.h"
#include "Profiler.h"
#include "Resource.h"
#include "Shape.h"

#include "EntityLeg.h"
#include "Tripod.h"

#include "EntityGrid.h"
#include "Explosion.h"
#include "Location.h"
#include "GameTime.h"
#include "WorldPointers.h"


#define FOOT_MOVE_THRESHOLD 1.0f // Lower means feet are lifted when less distant from their ideal pos, and thus smaller steps are taken
#define IDEAL_LEG_SLOPE 0.2f
#define LEG_LIFT 3.0f
#define LEG_SWING_DURATION 0.5f
#define LOOK_AHEAD_COEF 1.2f

#define ATTACK_HOVER_HEIGHT 18.0f
#define STATIONARY_HOVER_HEIGHT 30.0f
#define HOVER_LOWER_WITH_SPEED_FACTOR 0.2f

#define NAVIGATION_SEARCH_RADIUS 1000.0f
#define ATTACK_SEARCH_RADIUS 200.0f

#define ATTACK_DURATION 7.0f


//*****************************************************************************
// Class TripodNavData
//*****************************************************************************

TripodNavData::TripodNavData()
{
  m_directions[0].x = 0.5f;
  m_directions[0].y = 0.86603f;
  m_directions[1].x = 1.0f;
  m_directions[1].y = 0.0f;
  m_directions[2].x = 0.5f;
  m_directions[2].y = -0.86603f;
  m_directions[3].x = -0.5f;
  m_directions[3].y = -0.86603f;
  m_directions[4].x = -1.0f;
  m_directions[4].y = 0.0f;
  m_directions[5].x = -0.5f;
  m_directions[5].y = 0.86603f;
}


//*****************************************************************************
// Class Tripod
//*****************************************************************************

Tripod::Tripod()
  : m_mode(ModeWalking),
    m_nextLegToMove(0),
    m_speed(0.0f),
    m_targetHoverHeight(27.0f),
    m_bodyVel(0, 0, 0),
    m_up(g_upVector)
{
  m_shape = g_resource->GetShape("Tripod.shp");
  m_modeStartTime = 0.0f;

  // Initialise legs
  for (int i = 0; i < 3; ++i)
  {
    char markerName[] = "MarkerLegX";
    markerName[strlen(markerName) - 1] = '0' + i;
    m_legs[i] = new EntityLeg(i, this, "TripodLegPart.shp", "TripodLegPart.shp", markerName);
    m_legs[i]->m_legLift = LEG_LIFT;
    m_legs[i]->m_idealLegSlope = IDEAL_LEG_SLOPE;
    m_legs[i]->m_legSwingDuration = LEG_SWING_DURATION;
    m_legs[i]->m_lookAheadCoef = LOOK_AHEAD_COEF;
  }
}


Tripod::~Tripod()
{
  for (int i = 0; i < 3; ++i)
  {
    delete m_legs[i];
  }
}


void Tripod::Begin()
{
  Entity::Begin();

  for (int i = 0; i < 3; ++i)
  {
    m_legs[i]->LiftFoot(m_targetHoverHeight);
    m_legs[i]->PlantFoot();
  }

  // Vector2's Vector3 constructor took x and z; XMFLOAT2 has no such thing.
  m_navData.m_targetPos = DirectX::XMFLOAT2(m_pos.x, m_pos.z);
  m_navData.m_dir = -1;
}


void Tripod::ChangeHealth(int _amount)
{
  if (m_mode == ModeAttacking)
  {
    bool dead = m_dead;

    Entity::ChangeHealth(_amount);

    if (m_dead && !dead)
    {
      // We just died
      DirectX::XMFLOAT4X4 transform;
      DirectX::XMStoreFloat4x4(&transform,
                               BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::XMLoadFloat3(&m_up), DirectX::XMLoadFloat3(&m_pos)));
      g_explosionManager.AddExplosion(m_shape, transform);
    }
  }
}


int Tripod::CalcWhichFootToMove()
{
  bool allOnGround = m_legs[0]->m_foot.m_state == EntityFoot::FootState::OnGround && m_legs[1]->m_foot.m_state == EntityFoot::FootState::OnGround &&
                     m_legs[2]->m_foot.m_state == EntityFoot::FootState::OnGround;

  if (!allOnGround)
    return -1;

  float bestScore = 0.0f;
  float bestFoot = -1;

  for (int i = 0; i < 3; ++i)
  {
    float score = m_legs[i]->CalcFootsDesireToMove(m_targetHoverHeight);
    if (score > bestScore)
    {
      bestScore = score;
      bestFoot = i;
    }
  }

  if (bestScore > FOOT_MOVE_THRESHOLD)
  {
    return bestFoot;
  }

  return -1;
}


void Tripod::DoFallForTwoLegs()
{
  // Calc point between feet and point under body
  //
  // Braced to zero: RayRayDist leaves an out-parameter untouched when the two
  // rays are parallel, so all three read back whatever they held -- which
  // Vector3's default constructor made the origin.
  DirectX::XMFLOAT3 footToFootStore{0.0f, 0.0f, 0.0f};
  DirectX::XMFLOAT3 betweenFeetStore{0.0f, 0.0f, 0.0f};
  DirectX::XMFLOAT3 underBodyStore{0.0f, 0.0f, 0.0f};
  // g_upVector, which is (0,1,0).
  DirectX::XMFLOAT3 const worldUp(0.0f, 1.0f, 0.0f);
  if (m_legs[0]->m_foot.m_state != EntityFoot::FootState::OnGround)
  {
    DirectX::XMStoreFloat3(
      &footToFootStore, DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_legs[1]->m_foot.m_pos), DirectX::XMLoadFloat3(&m_legs[2]->m_foot.m_pos)));
    RayRayDist(m_legs[1]->m_foot.m_pos, footToFootStore, m_pos, worldUp, &betweenFeetStore, &underBodyStore);
  }
  else if (m_legs[1]->m_foot.m_state != EntityFoot::FootState::OnGround)
  {
    DirectX::XMStoreFloat3(
      &footToFootStore, DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_legs[2]->m_foot.m_pos), DirectX::XMLoadFloat3(&m_legs[0]->m_foot.m_pos)));
    RayRayDist(m_legs[0]->m_foot.m_pos, footToFootStore, m_pos, worldUp, &betweenFeetStore, &underBodyStore);
  }
  else
  {
    DirectX::XMStoreFloat3(
      &footToFootStore, DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_legs[0]->m_foot.m_pos), DirectX::XMLoadFloat3(&m_legs[1]->m_foot.m_pos)));
    RayRayDist(m_legs[0]->m_foot.m_pos, footToFootStore, m_pos, worldUp, &betweenFeetStore, &underBodyStore);
  }

  // Calc moment due to centre of gravity and the contact point on the ground not being vertically aligned
  DirectX::XMVECTOR const betweenFeet = DirectX::XMLoadFloat3(&betweenFeetStore);
  DirectX::XMVECTOR const lever = DirectX::XMVectorSubtract(betweenFeet, DirectX::XMLoadFloat3(&underBodyStore));
  float momentPerUnitMass = DirectX::XMVectorGetX(DirectX::XMVector3Length(lever)) * GRAVITY;

  // Calc force due to moment
  DirectX::XMVECTOR const feetToBody = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), betweenFeet);
  float feetToBodyLen = DirectX::XMVectorGetX(DirectX::XMVector3Length(feetToBody));
  DirectX::XMVECTOR const footToFootDir = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&footToFootStore));

  // SetLength on the cross of two roughly perpendicular unit vectors, so it is
  // not zero-length here; this takes the native normalise.
  DirectX::XMVECTOR const forcePerUnitMass = DirectX::XMVectorScale(
    DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&m_up), footToFootDir)), momentPerUnitMass / feetToBodyLen);

  // Calc change in body vel due to force
  DirectX::XMVECTOR bodyVel =
    DirectX::XMVectorMultiplyAdd(forcePerUnitMass, DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD), DirectX::XMLoadFloat3(&m_bodyVel));
  DirectX::XMStoreFloat3(&m_bodyVel, bodyVel);

  // Calc change in body angle due to change in position (it is rotating around pointBetweenFeet)
  float rotation = DirectX::XMVectorGetX(DirectX::XMVector3Length(bodyVel)) / feetToBodyLen; // Small angle approximation

  // RotateAround's angle is the vector's magnitude, with its own 1e-8 guard.
  DirectX::XMVECTOR const axis = DirectX::XMVectorScale(footToFootDir, -rotation * 0.1f);
  float const axisLengthSquared = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(axis));
  if (axisLengthSquared >= 1e-8f)
  {
    float const spin = sqrtf(axisLengthSquared);
    DirectX::XMStoreFloat3(&m_up, DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&m_up),
                                                              DirectX::XMMatrixRotationAxis(DirectX::XMVectorScale(axis, 1.0f / spin), spin)));
  }
  //		m_front.RotateAround(axis);

  DirectX::XMStoreFloat3(&m_pos,
                         DirectX::XMVectorMultiplyAdd(bodyVel, DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD), DirectX::XMLoadFloat3(&m_pos)));

  // SetLength back onto the leg length. feetToBodyLen is a divisor above, so it
  // is non-zero by the time we get here.
  DirectX::XMVECTOR const newFeetToBody = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), betweenFeet);
  DirectX::XMStoreFloat3(
    &m_pos, DirectX::XMVectorMultiplyAdd(DirectX::XMVector3Normalize(newFeetToBody), DirectX::XMVectorReplicate(feetToBodyLen), betweenFeet));
}


WorldObjectId Tripod::FindEntityToAttack()
{
  START_PROFILE(g_profiler, "FindEntityToA");

  WorldObjectId id;

  int numFound;
  WorldObjectId* enemies = g_location->m_entityGrid->GetEnemies(m_pos.x, m_pos.z, ATTACK_SEARCH_RADIUS, &numFound, m_id.GetTeamId());

  int nearest = -1;
  float nearestDistSqrd = FLT_MAX;
  for (int i = 0; i < numFound; ++i)
  {
    Entity* entity = g_location->GetEntity(enemies[i]);
    float deltaX = entity->m_pos.x - m_pos.x;
    float deltaY = entity->m_pos.z - m_pos.z;
    float distSqrd = deltaX * deltaX + deltaY * deltaY;
    if (distSqrd < nearestDistSqrd)
    {
      nearestDistSqrd = distSqrd;
      nearest = i;
    }
  }

  if (nearest != -1)
  {
    id = enemies[nearest];
  }

  END_PROFILE(g_profiler, "FindEntityToA");

  return id;
}


DirectX::XMFLOAT2 Tripod::ChooseDestination()
{
  START_PROFILE(g_profiler, "ChooseDest");

  int numFound;
  WorldObjectId* enemies = g_location->m_entityGrid->GetEnemies(m_pos.x, m_pos.z, NAVIGATION_SEARCH_RADIUS, &numFound, m_id.GetTeamId());
  // Vector2's Vector3 constructor took x and z; XMFLOAT2 has no such thing, so
  // the flattening is written out.
  DirectX::XMFLOAT2 pos;

  if (numFound == 0)
  {
    // The two syncsfrand calls stay in this order: they advance the RNG.
    pos = DirectX::XMFLOAT2(m_pos.x, m_pos.z);
    pos.x += syncsfrand(300.0f);
    pos.y += syncsfrand(300.0f);
  }
  else
  {
    int i = syncrand() % numFound;
    Entity* targetEnemy = g_location->GetEntity(enemies[i]);
    pos = DirectX::XMFLOAT2(targetEnemy->m_pos.x, targetEnemy->m_pos.z);
    pos.x += syncsfrand(100.0f);
    pos.y += syncsfrand(100.0f);
  }

  END_PROFILE(g_profiler, "ChooseDest");

  return pos;
}


void Tripod::DoNavigation()
{
  START_PROFILE(g_profiler, "DoNav");

  // If m_dir is -1 that means we aren't trying to go anywhere. We need to
  // wait until we come to rest before we choose a new direction to travel in
  if (m_navData.m_dir == -1)
  {
    float speed = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&m_vel)));
    if (speed > 0.5f)
    {
      END_PROFILE(g_profiler, "DoNav");
      return;
    }
  }

  // Have we reached our destination?
  DirectX::XMFLOAT2 const pos(m_pos.x, m_pos.z);
  DirectX::XMFLOAT2 delta(m_navData.m_targetPos.x - pos.x, m_navData.m_targetPos.y - pos.y);
  float deltaMag = sqrtf(delta.x * delta.x + delta.y * delta.y);
  if (deltaMag < 1.0f)
  {
    m_navData.m_targetPos = ChooseDestination();

    delta = DirectX::XMFLOAT2(m_navData.m_targetPos.x - pos.x, m_navData.m_targetPos.y - pos.y);
    deltaMag = sqrtf(delta.x * delta.x + delta.y * delta.y);
    m_navData.m_dir = -1;
  }

  // Are we currently going in a direction?
  if (m_navData.m_dir == -1)
  {
    // Choose the best direction
    DirectX::XMFLOAT2 const deltaNorm(delta.x / deltaMag, delta.y / deltaMag);
    float largestDot = m_navData.m_directions[0].x * deltaNorm.x + m_navData.m_directions[0].y * deltaNorm.y;
    unsigned int best = 0;
    for (unsigned int i = 1; i < 6; i++)
    {
      float result = m_navData.m_directions[i].x * deltaNorm.x + m_navData.m_directions[i].y * deltaNorm.y;
      if (result > largestDot)
      {
        largestDot = result;
        best = i;
      }
    }
    m_navData.m_dir = best;
  }

  // Should we changed direction?
  // Vector2::operator^ was the 2D cross product, x*b.y - y*b.x. XMFLOAT2 has
  // no operators, so it is written out.
  DirectX::XMFLOAT2 const& heading = m_navData.m_directions[m_navData.m_dir];
  float cross = heading.x * delta.y - heading.y * delta.x;
  float theta = asinf(cross / deltaMag);
  if (theta < -M_PI / 6.0f)
  {
    m_navData.m_dir++;
    m_navData.m_dir = m_navData.m_dir % 6;
  }
  else if (theta > M_PI / 6.0f)
  {
    m_navData.m_dir--;
    m_navData.m_dir = m_navData.m_dir % 6;
  }

  // Apply some acceleration to move us in our chosen direction
  m_vel.x += m_navData.m_directions[m_navData.m_dir].x * 1.0f;
  m_vel.z += m_navData.m_directions[m_navData.m_dir].y * 1.0f;

  END_PROFILE(g_profiler, "DoNav");
}


DirectX::XMFLOAT3 Tripod::CalcAttackUpVector()
{
  DirectX::XMVECTOR const toEnemy =
    DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_attackTarget), DirectX::XMLoadFloat3(&m_pos)));

  // HorizontalAndNormalise: flatten to the XZ plane, then normalise.
  DirectX::XMVECTOR const toEnemyHorizontal = DirectX::XMVector3Normalize(DirectX::XMVectorSetY(toEnemy, 0.0f));

  DirectX::XMFLOAT3 up;
  DirectX::XMStoreFloat3(&up, DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::g_XMIdentityR1, toEnemyHorizontal)));
  return up;
}


void Tripod::AdvanceWalk()
{
  START_PROFILE(g_profiler, "AdvanceWalk");

  // Consider mode switch
  {
    float timeSinceAttack = g_gameTime - m_modeStartTime;
    if (timeSinceAttack > 10.0f + ATTACK_DURATION)
    {
      WorldObjectId targetId = FindEntityToAttack();
      if (targetId.IsValid())
      {
        Entity* target = g_location->GetEntity(targetId);
        m_attackTarget = target->m_pos;
        m_mode = ModePreAttack;
        m_modeStartTime = g_gameTime;
        END_PROFILE(g_profiler, "AdvanceWalk");
        return;
      }
    }
  }


  // Consider choosing a different nav dest
  if (syncrand() % 40 == 1)
  {
    m_navData.m_targetPos = ChooseDestination();
  }


  // Hover
  m_targetHoverHeight = STATIONARY_HOVER_HEIGHT - fabsf(m_speed) * HOVER_LOWER_WITH_SPEED_FACTOR;


  // Navigate
  DoNavigation();


  // Fall
  {
    bool allOnGround = m_legs[0]->m_foot.m_state == EntityFoot::FootState::OnGround && m_legs[1]->m_foot.m_state == EntityFoot::FootState::OnGround &&
                       m_legs[2]->m_foot.m_state == EntityFoot::FootState::OnGround;
    if (!allOnGround)
    {
      DoFallForTwoLegs();
    }
    else
    {
      m_bodyVel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

      // HorizontalAndNormalise: flatten to the XZ plane, then normalise.
      DirectX::XMStoreFloat3(&m_front, DirectX::XMVector3Normalize(DirectX::XMVectorSetY(DirectX::XMLoadFloat3(&m_front), 0.0f)));
      m_up = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
    }
  }

  END_PROFILE(g_profiler, "AdvanceWalk");
}


void Tripod::AdvancePreAttack()
{
  START_PROFILE(g_profiler, "AdvancePreAttack");

  // Exit if we haven't come to a stop yet
  if (DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&m_vel))) > 0.05f)
  {
    END_PROFILE(g_profiler, "AdvancePreAttack");
    return;
  }

  m_targetHoverHeight = ATTACK_HOVER_HEIGHT;

  // See if we have achieved a full crouch yet
  float height = m_pos.y - g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);
  if (height < ATTACK_HOVER_HEIGHT + 0.5f)
  {
    m_mode = ModeAttacking;
    m_modeStartTime = g_gameTime;
  }

  // Blend into attack orientation
  DirectX::XMFLOAT3 const desiredUp = CalcAttackUpVector();
  float factor1 = 0.8f * SERVER_ADVANCE_PERIOD;
  float factor2 = 1.0f - factor1;

  DirectX::XMVECTOR const up = DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&desiredUp), DirectX::XMVectorReplicate(factor1),
                                                            DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_up), factor2));
  DirectX::XMStoreFloat3(&m_up, up);

  DirectX::XMVECTOR const right = DirectX::XMVector3Cross(up, DirectX::XMLoadFloat3(&m_front));
  DirectX::XMStoreFloat3(&m_front, DirectX::XMVector3Normalize(DirectX::XMVector3Cross(right, up)));

  END_PROFILE(g_profiler, "AdvancePreAttack");
}


void Tripod::AdvanceAttack()
{
  START_PROFILE(g_profiler, "AdvanceAttack");


  // Consider mode switch
  float timeInAttack = g_gameTime - m_modeStartTime;
  if (timeInAttack > ATTACK_DURATION)
  {
    m_mode = ModePostAttack;
    m_modeStartTime = g_gameTime;
    END_PROFILE(g_profiler, "AdvanceAttack");
    return;
  }


  //	m_up = CalcAttackUpVector();
  DirectX::XMVECTOR const rightAxis = DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&m_up), DirectX::g_XMIdentityR1);
  //	m_front = m_up ^ right;
  //	m_front.Normalise();

  if (timeInAttack > 1.0f)
  {
    int t2 = (int)(timeInAttack * 10.0f);
    if (t2 & 1)
    {
      DirectX::XMFLOAT3 toEnemy;
      DirectX::XMStoreFloat3(
        &toEnemy, DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_attackTarget), DirectX::XMLoadFloat3(&m_pos))));
      float const speed = 80.0f;

      // SetLength on the cross of m_up and the world up: zero only if the tripod
      // is exactly upright, which the attack crouch makes unreachable, so this
      // takes the native normalise.
      DirectX::XMVECTOR const barrelOffset = DirectX::XMVectorScale(DirectX::XMVector3Normalize(rightAxis), 4.0f);

      // The two syncsfrand calls stay in this order: they advance the RNG, and
      // they perturb toEnemy AFTER the muzzle offset is built.
      toEnemy.x += syncsfrand(0.1f);
      toEnemy.z += syncsfrand(0.1f);

      DirectX::XMVECTOR const aim = DirectX::XMLoadFloat3(&toEnemy);
      DirectX::XMVECTOR const ourPos = DirectX::XMLoadFloat3(&m_pos);

      DirectX::XMFLOAT3 muzzle, shot;
      DirectX::XMStoreFloat3(&shot, DirectX::XMVectorScale(aim, speed));

      DirectX::XMStoreFloat3(&muzzle,
                             DirectX::XMVectorMultiplyAdd(aim, DirectX::XMVectorReplicate(5.0f), DirectX::XMVectorAdd(ourPos, barrelOffset)));
      g_location->FireLaser(muzzle, shot, m_id.GetTeamId());

      DirectX::XMStoreFloat3(&muzzle,
                             DirectX::XMVectorMultiplyAdd(aim, DirectX::XMVectorReplicate(5.0f), DirectX::XMVectorSubtract(ourPos, barrelOffset)));
      g_location->FireLaser(muzzle, shot, m_id.GetTeamId());
    }
  }

  END_PROFILE(g_profiler, "AdvanceAttack");
}


void Tripod::AdvancePostAttack()
{
  m_targetHoverHeight = STATIONARY_HOVER_HEIGHT;

  // Check if we have achieved full walking height yet
  float height = m_pos.y - g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);
  if (height > STATIONARY_HOVER_HEIGHT - 0.5f)
  {
    m_mode = ModeWalking;
    m_modeStartTime = g_gameTime;
  }

  // Blend out of attack orientation
  float factor1 = 0.8f * SERVER_ADVANCE_PERIOD;
  float factor2 = 1.0f - factor1;
  DirectX::XMVECTOR const up = DirectX::XMVectorMultiplyAdd(DirectX::g_XMIdentityR1, DirectX::XMVectorReplicate(factor1),
                                                            DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_up), factor2));
  DirectX::XMStoreFloat3(&m_up, up);

  DirectX::XMVECTOR const right = DirectX::XMVector3Cross(up, DirectX::XMLoadFloat3(&m_front));
  DirectX::XMStoreFloat3(&m_front, DirectX::XMVector3Normalize(DirectX::XMVector3Cross(right, up)));
}


bool Tripod::Advance(Unit* _unit)
{
  //
  // Advance current mode

  switch (m_mode)
  {
  case ModeWalking:
    AdvanceWalk();
    break;
  case ModePreAttack:
    AdvancePreAttack();
    break;
  case ModeAttacking:
    AdvanceAttack();
    break;
  case ModePostAttack:
    AdvancePostAttack();
    break;
  }


  //
  // Rotation

  DirectX::XMStoreFloat3(&m_front,
                         DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&m_front), DirectX::XMMatrixRotationY(g_advanceTime * m_angVel.y)));
  m_angVel.y *= 1.0f - SERVER_ADVANCE_PERIOD * 0.5f;


  //
  // Move

  float factor1 = SERVER_ADVANCE_PERIOD * 1.65f;
  float factor2 = 1.0f - factor1;
  m_speed *= 1.0f - SERVER_ADVANCE_PERIOD * 0.5f;
  // frontHoriNorm was computed and never used; dropped rather than kept as a
  // native no-op.
  DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_front), DirectX::XMVectorReplicate(m_speed * factor1),
                                                              DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), factor2)));
  DirectX::XMStoreFloat3(&m_pos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD),
                                                              DirectX::XMLoadFloat3(&m_pos)));


  //
  // Adjust body height

  {
    float targetHeight = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);
    targetHeight += m_targetHoverHeight;
    float factor1 = 1.0f * SERVER_ADVANCE_PERIOD;
    float factor2 = 1.0f - factor1;
    m_pos.y = factor1 * targetHeight + factor2 * m_pos.y;
  }


  //
  // Update legs

  int bestFootToMove = CalcWhichFootToMove();
  if (bestFootToMove != -1)
  {
    m_legs[bestFootToMove]->LiftFoot(m_targetHoverHeight);
  }

  for (int i = 0; i < 3; ++i)
  {
    m_legs[i]->Advance();
  }


  //
  // Detect death

  if (m_pos.y < g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z))
  {
    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::XMLoadFloat3(&m_up), DirectX::XMLoadFloat3(&m_pos)));
    g_explosionManager.AddExplosion(m_shape, mat);
    return true;
  }

  return m_dead;
}


void Tripod::Render(float _predictionTime)
{
  glDisable(GL_TEXTURE_2D);

  g_renderer->SetObjectLighting();


  //
  // Render body

  DirectX::XMFLOAT3 predictedMovement;
  DirectX::XMStoreFloat3(&predictedMovement, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), _predictionTime));

  DirectX::XMFLOAT3 predictedPos;
  DirectX::XMStoreFloat3(&predictedPos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&predictedMovement)));
  //	predictedPos.y = g_location->m_landscape.m_heightMap->GetValue(predictedPos.x, predictedPos.z) +
  //					 m_targetHoverHeight;

  DirectX::XMFLOAT4X4 mat;
  DirectX::XMStoreFloat4x4(&mat,
                           BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::XMLoadFloat3(&m_up), DirectX::XMLoadFloat3(&predictedPos)));
  m_shape->Render(_predictionTime, mat);


  //
  // Render Legs

  for (int i = 0; i < 3; ++i)
  {
    m_legs[i]->Render(_predictionTime, predictedMovement);
  }

  //	m_s->Render();

  g_renderer->UnsetObjectLighting();
}
