#include "pch.h"
#include "SoundSources.h"

#include "DebugRender.h"
#include "MathUtils.h"
#include "Profiler.h"
#include "Resource.h"
#include "Shape.h"
#include "HiResTime.h"

#include "Location.h"
#include "Team.h"
#include "EntityGrid.h"
#include "GameTime.h"
#include "Unit.h"
#include "ParticleSystem.h"
#include "TaskManager.h"
#include "RoutingSystem.h"
#include "GlobalWorld.h"
#include "LevelFile.h"
#include "ObstructionGrid.h"

#include "SoundSystem.h"

#include "Citizen.h"
#include "Officer.h"
#include "Teleport.h"
#include "ArmyAnt.h"
#include "Armour.h"
#include "Ai.h"
#include "LaserFence.h"
#include "GodDish.h"
#include "Rocket.h"
#include "WorldPointers.h"

Citizen::Citizen()
  : Entity(),
    m_state(StateIdle),
    m_retargetTimer(0.0f),
    m_spiritId(-1),
    m_buildingId(-1),
    m_portId(-1),
    m_controllerId(-1),
    m_wayPointId(-1),
    m_teleportRequired(false),
    m_ordersBuildingId(-1),
    m_ordersSet(false),
    m_promoted(false),
    m_scared(true),
    m_shadowBuildingId(-1),
    m_threatRange(CITIZEN_SEARCHRANGE_THREATS),
    m_grenadeTimer(0.0f),
    m_officerTimer(0.0f)
{
  SetType(TypeCitizen);
  m_grenadeTimer = syncfrand(5.0f);
}


void Citizen::Begin()
{
  Entity::Begin();
  m_onGround = true;
  m_wayPoint = m_pos;
  m_centrePos = DirectX::XMFLOAT3(0.0f, 2.0f, 0.0f);
  m_radius = 4.0f;
}


void Citizen::ChangeHealth(int _amount)
{
  if (m_state == StateInsideArmour)
  {
    // We are invincible in here
    return;
  }

  bool dead = m_dead;

  Entity::ChangeHealth(_amount);

  if (!dead && m_dead)
  {
    // We just died
  }
}


bool Citizen::SearchForNewTask()
{
  //
  //            switch( m_state )                   // Deliberate fall through here - check all states above our priority
  //            {
  //                case StateIdle:                 SearchForRandomPosition();
  //                                                SearchForSpirits();
  //                case StateWorshipSpirit:        SearchForOfficers();
  //                                                SearchForArmour();
  //                case StateApproachingArmour:
  //                case StateFollowingOrders:
  //                case StateFollowingOfficer:     SearchForPorts();
  //                case StateApproachingPort:
  //                case StateOperatingPort:
  //                case StateWatchingGodDish:
  //                case StateCombat:               SearchForThreats();
  //                //case StateUnderControl:
  //                //case StateCapturedByAnt:
  //                //case StateInsideArmour
  //            }

  bool newTargetFound = false;

  switch (m_state)
  {
  case StateIdle:
    if (!newTargetFound)
      newTargetFound = SearchForThreats();
    if (!newTargetFound)
      newTargetFound = SearchForPorts();
    if (!newTargetFound)
      newTargetFound = SearchForArmour();
    if (!newTargetFound)
      newTargetFound = SearchForOfficers();
    if (!newTargetFound)
      newTargetFound = SearchForSpirits();
    if (!newTargetFound)
      newTargetFound = SearchForRandomPosition();
    break;

  case StateWorshipSpirit:
  case StateWatchingSpectacle:
    if (!newTargetFound)
      newTargetFound = SearchForThreats();
    if (!newTargetFound)
      newTargetFound = SearchForPorts();
    if (!newTargetFound)
      newTargetFound = SearchForArmour();
    if (!newTargetFound)
      newTargetFound = SearchForOfficers();
    break;

  case StateApproachingArmour:
  case StateFollowingOrders:
  case StateFollowingOfficer:
    if (!newTargetFound)
      newTargetFound = SearchForThreats();
    if (!newTargetFound)
      newTargetFound = SearchForPorts();
    break;

  case StateApproachingPort:
  case StateOperatingPort:
  case StateCombat:
    if (!newTargetFound)
      newTargetFound = SearchForThreats();
    break;

  case StateBoardingRocket:
  case StateOnFire:
    break;
  }


  return newTargetFound;
}


bool Citizen::Advance(Unit* _unit)
{
  if (m_promoted)
  {
    return true;
  }

  bool amIDead = Entity::Advance(_unit);

  if (!amIDead && !m_dead && m_onGround && m_inWater == -1.0f)
  {
    //
    // Has something higher priority come along?

    m_retargetTimer -= SERVER_ADVANCE_PERIOD;
    if (m_retargetTimer <= 0.0)
    {
      bool newTaskFound = SearchForNewTask();

      if (m_state == StateIdle)
      {
        bool victoryDance = BeginVictoryDance();
      }

      m_retargetTimer = 1.0f + syncfrand(1.0f);
    }

    //
    // Do what we're supposed to do

    switch (m_state)
    {
    case StateIdle:
      amIDead = AdvanceIdle();
      break;
    case StateApproachingPort:
      amIDead = AdvanceApproachingPort();
      break;
    case StateOperatingPort:
      amIDead = AdvanceOperatingPort();
      break;
    case StateApproachingArmour:
      amIDead = AdvanceApproachingArmour();
      break;
    case StateInsideArmour:
      amIDead = AdvanceInsideArmour();
      break;
    case StateWorshipSpirit:
      amIDead = AdvanceWorshipSpirit();
      break;
    case StateUnderControl:
      amIDead = AdvanceUnderControl();
      break;
    case StateFollowingOrders:
      amIDead = AdvanceFollowingOrders();
      break;
    case StateFollowingOfficer:
      amIDead = AdvanceFollowingOfficer();
      break;
    case StateCombat:
      amIDead = AdvanceCombat();
      break;
    case StateCapturedByAnt:
      amIDead = AdvanceCapturedByAnt();
      break;
    case StateWatchingSpectacle:
      amIDead = AdvanceWatchingSpectacle();
      break;
    case StateBoardingRocket:
      amIDead = AdvanceBoardingRocket();
      break;
    case StateAttackingBuilding:
      amIDead = AdvanceAttackingBuilding();
      break;
    }
  }

  if (m_boxKiteId.IsValid() && (m_state != StateWorshipSpirit || m_dead))
  {
    BoxKite* boxKite = (BoxKite*)g_location->GetEffect(m_boxKiteId);
    boxKite->Release();
    m_boxKiteId.SetInvalid();
  }


  if (!m_onGround)
    AdvanceInAir(_unit);

  if (m_state == StateOnFire && !amIDead && !m_dead)
  {
    amIDead = AdvanceOnFire();
  }

  if (m_pos.y < 0.0f && m_inWater == -1.0f && m_state != StateInsideArmour)
    m_inWater = syncfrand(3.0f);

  if (m_dead && m_onGround)
  {
    DirectX::XMVECTOR const velocity = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), 0.9f);
    DirectX::XMStoreFloat3(&m_vel, velocity);
    DirectX::XMStoreFloat3(&m_pos,
                           DirectX::XMVectorMultiplyAdd(velocity, DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD), DirectX::XMLoadFloat3(&m_pos)));
  }

  return amIDead;
}


bool Citizen::AdvanceIdle()
{
  if (m_onGround)
  {
    AdvanceToTargetPosition();
  }

  return false;
}


bool Citizen::AdvanceWatchingSpectacle()
{
  Building* building = g_location->GetBuilding(m_buildingId);
  if (!building || (building->m_type != Building::TypeGodDish && building->m_type != Building::TypeEscapeRocket))
  {
    m_state = StateIdle;
    return false;
  }


  //
  // Face the spectacle

  float amountToTurn = SERVER_ADVANCE_PERIOD * 4.0f;
  DirectX::XMVECTOR const targetPos =
    DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&building->m_centrePos),
                         DirectX::XMVectorSet(sinf(g_gameTime) * 30.0f, cosf(g_gameTime) * 20.0f, sinf(g_gameTime) * 25.0f, 0.0f));

  DirectX::XMFLOAT3 targetDir;
  DirectX::XMStoreFloat3(&targetDir, DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(targetPos, DirectX::XMLoadFloat3(&m_pos))));


  //
  // Is it a god dish?

  if (building->m_type == Building::TypeGodDish)
  {
    GodDish* dish = (GodDish*)building;
    if (!dish->m_activated && dish->m_timer < 1.0f)
    {
      m_state = StateIdle;
      return false;
    }

    m_front = targetDir;
    m_vel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
  }


  //
  // Is it an escape rocket?

  if (building->m_type == Building::TypeEscapeRocket)
  {
    EscapeRocket* rocket = (EscapeRocket*)building;
    if (!rocket->IsSpectacle())
    {
      m_state = StateIdle;
      return false;
    }

    DirectX::XMVECTOR const buildingPos = DirectX::XMLoadFloat3(&building->m_pos);
    float distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(buildingPos, DirectX::XMLoadFloat3(&m_pos))));
    if (distance < 200.0f)
    {
      // The syncfrand draw stays inside the branch, exactly as before.
      float const pushOut = 200 + syncfrand(100.0f);
      DirectX::XMVECTOR const moveVector =
        DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), buildingPos)), pushOut);
      DirectX::XMStoreFloat3(&m_wayPoint, DirectX::XMVectorAdd(buildingPos, moveVector));
    }

    bool arrived = AdvanceToTargetPosition();
    if (arrived)
    {
      m_front = targetDir;
      m_vel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
    }
  }

  return false;
}


void Citizen::WatchSpectacle(int _buildingId)
{
  m_buildingId = _buildingId;
  m_state = StateWatchingSpectacle;
}


void Citizen::CastShadow(int _buildingId) { m_shadowBuildingId = _buildingId; }


bool Citizen::AdvanceApproachingArmour()
{
  //
  // Is our armour still alive / within range / open

  Armour* armour = (Armour*)g_location->GetEntity(m_armourId);
  if (!armour || !armour->IsLoading())
  {
    m_state = StateIdle;
    m_armourId.SetInvalid();
    return false;
  }

  // Armour converts in T15, so its out-parameters are still legacy.
  DirectX::XMFLOAT3 exitPos{0.0f, 0.0f, 0.0f};
  DirectX::XMFLOAT3 exitDir{0.0f, 0.0f, 0.0f};
  armour->GetEntrance(AsLegacy(exitPos), AsLegacy(exitDir));

  float distance =
    DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&exitPos), DirectX::XMLoadFloat3(&m_pos))));
  if (distance > CITIZEN_SEARCHRANGE_ARMOUR)
  {
    m_state = StateIdle;
    m_armourId.SetInvalid();
    return false;
  }


  //
  // Walk towards the armour until we are there

  m_wayPoint = exitPos;
  m_wayPoint.y = g_location->m_landscape.m_heightMap->GetValue(m_wayPoint.x, m_wayPoint.z);

  bool arrived = AdvanceToTargetPosition();
  if (arrived || distance < 20.0f)
  {
    armour->AddPassenger();
    m_state = StateInsideArmour;
  }

  return false;
}


bool Citizen::AdvanceInsideArmour()
{
  //
  // Is our armour still alive

  Armour* armour = (Armour*)g_location->GetEntity(m_armourId);
  if (!armour || armour->m_dead)
  {
    m_state = StateIdle;
    m_armourId.SetInvalid();

    DirectX::XMFLOAT3 const pos = m_pos;
    float radius = syncfrand(20.0f);
    float theta = syncfrand(M_PI * 2);
    m_pos.x += radius * sinf(theta);
    m_pos.z += radius * cosf(theta);
    DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&pos), DirectX::XMLoadFloat3(&m_pos)));
    // SetLength on the zero-length delta this can produce used to leave
    // (len,0,0); native normalise gives zero.
    float const kickback = syncfrand(50.0f);
    DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&m_vel)), kickback));

    ChangeHealth(-500);

    return false;
  }


  m_pos = armour->m_pos;
  m_vel = armour->m_vel;
  m_inWater = -1.0f;


  //
  // Is our armour unloading
  // Only get out if we are over ground, not water

  if (armour->IsUnloading())
  {
    // Armour converts in T15; same seam.
    DirectX::XMFLOAT3 exitPos{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 exitDir{0.0f, 0.0f, 0.0f};
    armour->GetEntrance(AsLegacy(exitPos), AsLegacy(exitDir));
    float landHeight = g_location->m_landscape.m_heightMap->GetValue(exitPos.x, exitPos.z);
    if (landHeight > 0.0f)
    {
      // JUMP!
      armour->RemovePassenger();
      float const exitSpin = syncsfrand(M_PI * 0.5f);
      DirectX::XMVECTOR const spunExit = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&exitDir), DirectX::XMMatrixRotationY(exitSpin));
      DirectX::XMStoreFloat3(&exitDir, spunExit);
      DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(spunExit, 10.0f));
      m_vel.y = 5.0f + syncfrand(10.0f);
      m_pos = exitPos;
      m_onGround = false;
      m_state = StateIdle;
      m_armourId.SetInvalid();
    }
  }

  return false;
}


bool Citizen::AdvanceCapturedByAnt()
{
  ArmyAnt* ant = (ArmyAnt*)g_location->GetEntity(m_threatId);
  if (!ant || ant->m_dead)
  {
    m_state = StateIdle;
    m_threatId.SetInvalid();
    return false;
  }

  // ArmyAnt converts in T15; same seam.
  DirectX::XMFLOAT3 carryPos{0.0f, 0.0f, 0.0f};
  DirectX::XMFLOAT3 carryVel{0.0f, 0.0f, 0.0f};
  ant->GetCarryMarker(AsLegacy(carryPos), AsLegacy(carryVel));

  m_pos = carryPos;
  m_vel = carryVel;

  return false;
}


bool Citizen::AdvanceCombat()
{
  START_PROFILE(g_profiler, "AdvanceCombat");

  //
  // Does our threat still exist?

  WorldObject* threat = g_location->GetWorldObject(m_threatId);
  bool isEntity = threat && threat->m_id.GetUnitId() != UNIT_EFFECTS;
  Entity* entity = (isEntity ? (Entity*)threat : nullptr);

  if (!threat || (entity && entity->m_dead))
  {
    m_state = StateIdle;
    m_retargetTimer = 0.0;
    g_soundSystem->StopAllSounds(m_id, "Citizen SeenThreat");
    END_PROFILE(g_profiler, "AdvanceCombat");
    return false;
  }


  //
  // If this is a gun turret, look to see if we are out of
  // the plausable line of fire

  if (!isEntity && threat && threat->m_type == EffectGunTurretTarget)
  {
    float distance = DirectX::XMVectorGetX(
      DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&threat->m_pos), DirectX::XMLoadFloat3(&m_pos))));
    if (distance > CITIZEN_SEARCHRANGE_TURRETS)
    {
      m_state = StateIdle;
      m_retargetTimer = 0.0;
      g_soundSystem->StopAllSounds(m_id, "Citizen SeenThreat");
      END_PROFILE(g_profiler, "AdvanceCombat");
      return false;
    }
  }

  //
  // Move away from our threat if we're an ordinary Citizen
  // Move towards our threat if we're a Soldier Citizen

  bool soldier = m_id.GetTeamId() == 1 || g_globalWorld->m_research->CurrentLevel(GlobalResearch::TypeCitizen) > 2;

  if (soldier && !m_scared)
  {
    //        bool arrived = AdvanceToTargetPosition();
    //        if( arrived )
    //        {
    /*
                Vector3 targetVector = ( threat->m_pos - m_pos );
                float angle = syncsfrand( M_PI * 0.5f );
                targetVector.RotateAroundY( angle );
                float distance = targetVector.Mag();
                float ourDesiredRange = 20.0f + syncfrand(20.0f);
                targetVector.SetLength( distance - ourDesiredRange );
                m_wayPoint = m_pos + targetVector;
                m_wayPoint = PushFromObstructions( m_wayPoint );
                m_wayPoint.y = g_location->m_landscape.m_heightMap->GetValue( m_wayPoint.x, m_wayPoint.z );
    */
    float distance = DirectX::XMVectorGetX(
      DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&threat->m_pos))));
    if (distance < CITIZEN_FEARRANGE / 2.0f)
    {
      DirectX::XMVECTOR const away = DirectX::XMVectorScale(
        DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&threat->m_pos))), 30.0f);
      float angle = syncsfrand(M_PI * 0.5f);
      DirectX::XMVECTOR const moveAwayVector = DirectX::XMVector3Transform(away, DirectX::XMMatrixRotationY(angle));
      DirectX::XMStoreFloat3(&m_wayPoint, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), moveAwayVector));
    }
    else
    {
      DirectX::XMVECTOR toThreat = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&threat->m_pos), DirectX::XMLoadFloat3(&m_pos));
      float angle = syncsfrand(M_PI * 0.5f);
      toThreat = DirectX::XMVector3Transform(toThreat, DirectX::XMMatrixRotationY(angle));

      float distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(toThreat));
      float ourDesiredRange = 20.0f + syncfrand(20.0f);
      DirectX::XMVECTOR const targetVector = DirectX::XMVectorScale(DirectX::XMVector3Normalize(toThreat), distance - ourDesiredRange);
      DirectX::XMStoreFloat3(&m_wayPoint, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), targetVector));
    }
    m_wayPoint = PushFromObstructions(m_wayPoint);
    m_wayPoint.y = g_location->m_landscape.m_heightMap->GetValue(m_wayPoint.x, m_wayPoint.z);
    AdvanceToTargetPosition();
  }
  else
  {
    float distance = DirectX::XMVectorGetX(
      DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&threat->m_pos))));
    if (distance > CITIZEN_FEARRANGE)
    {
      m_scared = false;
      m_threatId.SetInvalid();
    }

    DirectX::XMVECTOR const away = DirectX::XMVectorScale(
      DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&threat->m_pos))), 30.0f);
    float angle = syncsfrand(M_PI * 0.5f);
    DirectX::XMVECTOR const moveAwayVector = DirectX::XMVector3Transform(away, DirectX::XMMatrixRotationY(angle));
    DirectX::XMStoreFloat3(&m_wayPoint, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), moveAwayVector));
    m_wayPoint = PushFromObstructions(m_wayPoint);
    m_wayPoint.y = g_location->m_landscape.m_heightMap->GetValue(m_wayPoint.x, m_wayPoint.z);
    AdvanceToTargetPosition();
  }


  if (isEntity)
  {
    //
    // Shoot at our enemy

    if (soldier && (syncrand() % 10) == 0)
    {
      Attack(DirectX::XMFLOAT3(threat->m_pos.x, threat->m_pos.y + 2.0f, threat->m_pos.z));
    }


    //
    // Throw grenades if we have a good opportunity
    // ie lots of enemies near our target, and not many friends
    // Or if the enemy is the sort that responds well to grenades
    // NEVER throw grenades if there are people from team 2 nearby (ie the player's squad, officers, engineers)
    // NEVER throw grenades if the target area is too steep - Citizens just can't fucking aim on cliffs

    bool hasGrenade = m_id.GetTeamId() == 1 || g_globalWorld->m_research->CurrentLevel(GlobalResearch::TypeCitizen) > 3;
    if (hasGrenade)
    {
      START_PROFILE(g_profiler, "ThrowGrenade");
      m_grenadeTimer -= SERVER_ADVANCE_PERIOD;
      if (m_grenadeTimer <= 0.0f)
      {
        m_grenadeTimer = 12.0f + syncfrand(8.0f);
        float distanceToTarget = DirectX::XMVectorGetX(
          DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&threat->m_pos), DirectX::XMLoadFloat3(&m_pos))));
        if (distanceToTarget > 75.0f)
        {
          bool includeTeams[] = {false, false, true, false, false, false, false, false};
          int numPlayers = g_location->m_entityGrid->GetNumNeighbours(threat->m_pos.x, threat->m_pos.z, 50.0f, includeTeams);
          if (numPlayers == 0)
          {
            bool throwGrenade = false;
            // The landscape converts in T18, so its normal map is still legacy.
            DirectX::XMFLOAT3 const ourLandNormal = g_location->m_landscape.m_normalMap->GetValue(m_pos.x, m_pos.z);
            DirectX::XMFLOAT3 const targetLandNormal = g_location->m_landscape.m_normalMap->GetValue(threat->m_pos.x, threat->m_pos.z);

            if (ourLandNormal.y > 0.7f && targetLandNormal.y > 0.7f)
            {
              bool grenadeRequired = entity->m_type == TypeSporeGenerator || entity->m_type == TypeSpider || entity->m_type == TypeTriffidEgg ||
                                     entity->m_type == TypeInsertionSquadie || entity->m_type == TypeArmour;

              if (grenadeRequired)
              {
                float targetHeight = entity->m_pos.y - g_location->m_landscape.m_heightMap->GetValue(entity->m_pos.x, entity->m_pos.z);
                if (targetHeight < 40.0f)
                  throwGrenade = true;
              }
              else
              {
                int numFriends = g_location->m_entityGrid->GetNumFriends(threat->m_pos.x, threat->m_pos.z, 50.0f, m_id.GetTeamId());
                int numEnemies = g_location->m_entityGrid->GetNumEnemies(threat->m_pos.x, threat->m_pos.z, 50.0f, m_id.GetTeamId());
                if (numEnemies > 5 && numFriends < 2)
                  throwGrenade = true;
              }
            }

            if (throwGrenade)
            {
              g_location->ThrowWeapon(m_pos, threat->m_pos, EffectThrowableGrenade, m_id.GetTeamId());
            }
          }
        }
      }
      END_PROFILE(g_profiler, "ThrowGrenade");
    }
  }

  END_PROFILE(g_profiler, "AdvanceCombat");
  return false;
}


bool Citizen::AdvanceWorshipSpirit()
{
  START_PROFILE(g_profiler, "AdvanceWorship");

  //
  // Check our spirit is still there and valid

  if (!g_location->m_spirits.ValidIndex(m_spiritId))
  {
    m_state = StateIdle;
    m_retargetTimer = 3.0f;
    END_PROFILE(g_profiler, "AdvanceWorship");
    return false;
  }

  Spirit* spirit = g_location->m_spirits.GetPointer(m_spiritId);
  if (spirit->m_state != Spirit::StateBirth && spirit->m_state != Spirit::StateFloating)
  {
    m_state = StateIdle;
    m_retargetTimer = 3.0f;

    if (m_boxKiteId.IsValid())
    {
      BoxKite* boxKite = (BoxKite*)g_location->GetEffect(m_boxKiteId);
      boxKite->Release();
      m_boxKiteId.SetInvalid();
    }

    END_PROFILE(g_profiler, "AdvanceWorship");
    return false;
  }


  //
  // Move to within range of our spirit

  DirectX::XMVECTOR const toSpirit = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&spirit->m_pos), DirectX::XMLoadFloat3(&m_pos));
  float distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(toSpirit));
  float ourDesiredRange = 20 + (m_id.GetUniqueId() % 20);
  DirectX::XMVECTOR const targetVector = DirectX::XMVectorScale(DirectX::XMVector3Normalize(toSpirit), distance - ourDesiredRange);

  DirectX::XMFLOAT3 newWaypoint;
  DirectX::XMStoreFloat3(&newWaypoint, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), targetVector));
  newWaypoint = PushFromObstructions(newWaypoint);
  newWaypoint.y = g_location->m_landscape.m_heightMap->GetValue(newWaypoint.x, newWaypoint.z);
  if (DirectX::XMVectorGetX(
        DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&newWaypoint), DirectX::XMLoadFloat3(&m_wayPoint)))) > 10.0f)
  {
    m_wayPoint = newWaypoint;
  }

  bool areWeThere = AdvanceToTargetPosition();
  if (areWeThere)
  {
    DirectX::XMStoreFloat3(
      &m_front, DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&spirit->m_pos), DirectX::XMLoadFloat3(&m_pos))));
  }


  //
  // Possibly spawn a boxkite to guide the spirit to heaven

  if (areWeThere && !m_boxKiteId.IsValid())
  {
    bool existingKiteFound = false;

    for (int i = 0; i < g_location->m_effects.Size(); ++i)
    {
      if (g_location->m_effects.ValidIndex(i))
      {
        WorldObject* obj = g_location->m_effects[i];
        if (obj->m_id.GetUnitId() == UNIT_EFFECTS && obj->m_type == EffectBoxKite)
        {
          float distanceSqd = DirectX::XMVectorGetX(
            DirectX::XMVector3LengthSq(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&obj->m_pos), DirectX::XMLoadFloat3(&m_pos))));
          if (distanceSqd < 2500.0f)
          {
            existingKiteFound = true;
            break;
          }
        }
      }
    }

    if (!existingKiteFound)
    {
      BoxKite* boxKite = new BoxKite();
      DirectX::XMStoreFloat3(&boxKite->m_pos,
                             DirectX::XMVectorAdd(DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_front), DirectX::XMVectorReplicate(2.0f),
                                                                               DirectX::XMLoadFloat3(&m_pos)),
                                                  DirectX::XMVectorScale(DirectX::g_XMIdentityR1, 5.0f)));
      boxKite->m_front = m_front;
      int index = g_location->m_effects.PutData(boxKite);
      boxKite->m_id.Set(m_id.GetTeamId(), UNIT_EFFECTS, index, -1);
      boxKite->m_id.GenerateUniqueId();
      m_boxKiteId = boxKite->m_id;
    }
  }


  END_PROFILE(g_profiler, "AdvanceWorship");
  return false;
}


bool Citizen::AdvanceApproachingPort()
{
  //
  // Check the port is still available

  Building* building = g_location->GetBuilding(m_buildingId);
  if (!building)
  {
    m_state = StateIdle;
    return false;
  }

  WorldObjectId occupant = building->GetPortOccupant(m_portId);
  bool otherOccupantFound = occupant.IsValid() && !(building->GetPortOccupant(m_portId) == m_id);
  if (otherOccupantFound)
  {
    m_state = StateIdle;
    return false;
  }


  //
  // Move to within range of the port

  building->OperatePort(m_portId, m_id.GetTeamId());
  bool areWeThere = AdvanceToTargetPosition();
  if (areWeThere)
  {
    // Building converts in T16, so its out-parameters are still legacy.
    DirectX::XMFLOAT3 portPos{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 portFront{0.0f, 0.0f, 0.0f};
    building->GetPortPosition(m_portId, AsLegacy(portPos), AsLegacy(portFront));
    m_front = portFront;
    m_vel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_state = StateOperatingPort;
  }

  return false;
}


bool Citizen::AdvanceOperatingPort()
{
  //
  // Check the port is still available
  Building* building = g_location->GetBuilding(m_buildingId);
  if (!building)
  {
    m_state = StateIdle;
    return false;
  }


  if (building->GetPortOccupant(m_portId) != m_id)
  {
    m_state = StateIdle;
    return false;
  }

  return false;
}


bool Citizen::AdvanceUnderControl()
{
  //
  // Try to lookup our controller

  Task* task = g_taskManager->GetTask(m_controllerId);
  Unit* controller = nullptr;
  if (task)
    controller = g_location->GetUnit(task->m_objId);

  if (!task || !controller)
  {
    m_state = StateIdle;
    int numFlashes = 5 + speciesRandom() % 5;
    for (int i = 0; i < numFlashes; ++i)
    {
      // Three unsynchronised draws per flash, order and count unchanged.
      float const velX = sfrand(5.0f);
      float const velY = frand(15.0f);
      float const velZ = sfrand(5.0f);
      DirectX::XMFLOAT3 const vel(velX, velY, velZ);
      g_particleSystem->CreateParticle(m_pos, vel, Particle::TypeControlFlash);
    }
    g_soundSystem->StopAllSounds(m_id, "Citizen TakenControl");
    g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "EscapedControl");
    return false;
  }


  //
  // Follow the route owned by our controller

  bool arrived = AdvanceToTargetPosition();

  if (m_teleportRequired)
  {
    bool teleported = (EnterTeleports() != -1);
    if (teleported)
    {
      m_teleportRequired = false;
      arrived = true;
    }
  }

  if (arrived)
  {
    float positionError = 0.0f;

    if (m_teleportRequired)
    {
      //
      // We are trying to wriggle our way into a teleport that is very nearby
      // Just wander a bit until we are picked up
      WayPoint* wp = task->m_route->GetWayPoint(m_wayPointId);
      positionError = 10.0f;
      m_wayPoint = wp->GetPos();
    }
    else
    {
      WayPoint* wp = task->m_route->GetWayPoint(m_wayPointId + 1);
      if (wp)
      {
        //
        // Our next waypoint is available
        // So head there immediately
        ++m_wayPointId;
        m_wayPoint = wp->GetPos();
        positionError = 40.0f;
        if (wp->m_type == WayPoint::TypeBuilding)
          m_teleportRequired = true;
      }
      else
      {
        //
        // There are no more waypoints
        // So just head directly for the squad that is controlling us
        m_wayPoint = controller->m_centrePos;
        positionError = 70.0f;
      }
    }

    //
    // Add some randomness to our waypoint

    float radius = syncfrand(positionError);
    float theta = syncfrand(M_PI * 2);
    m_wayPoint.x += radius * sinf(theta);
    m_wayPoint.z += radius * cosf(theta);

    m_wayPoint = PushFromObstructions(m_wayPoint);
    m_wayPoint.y = g_location->m_landscape.m_heightMap->GetValue(m_wayPoint.x, m_wayPoint.z);
  }

  return false;
}


bool Citizen::AdvanceFollowingOrders()
{
  bool arrived = AdvanceToTargetPosition();

  if (m_ordersBuildingId != -1)
  {
    bool teleported = (EnterTeleports(m_ordersBuildingId) != -1);
    if (teleported)
    {
      m_ordersBuildingId = -1;
      arrived = true;
    }
  }

  if (arrived)
  {
    if (m_ordersBuildingId != -1)
    {
      // We have arrived but are trying to enter a teleport
      // Just wiggle around until we are given entry
      Building* building = g_location->GetBuilding(m_ordersBuildingId);
      if (building)
      {
        Teleport* teleport = (Teleport*)building;
        if (!teleport->Connected())
        {
          m_ordersBuildingId = -1;
        }
        else
        {
          // Teleport converts in T17; its out-parameters are still legacy.
          DirectX::XMFLOAT3 entrancePos{0.0f, 0.0f, 0.0f};
          DirectX::XMFLOAT3 entranceFront{0.0f, 0.0f, 0.0f};
          teleport->GetEntrance(AsLegacy(entrancePos), AsLegacy(entranceFront));
          m_wayPoint = entrancePos;
          float radius = syncfrand(10.0f);
          float theta = syncfrand(M_PI * 2);
          m_wayPoint.x += radius * sinf(theta);
          m_wayPoint.z += radius * cosf(theta);
          m_wayPoint = PushFromObstructions(m_wayPoint);
          m_wayPoint.y = g_location->m_landscape.m_heightMap->GetValue(m_wayPoint.x, m_wayPoint.z);
        }
      }
      else
      {
        m_ordersBuildingId = -1;
      }
    }
    else
    {
      m_state = StateIdle;
      m_retargetTimer = 0.0f;
      m_ordersBuildingId = -1;
      m_ordersSet = false;
    }
  }

  return false;
}


bool Citizen::AdvanceFollowingOfficer()
{
  //
  // Look up our officer

  Officer* officer = (Officer*)g_location->GetEntitySafe(m_officerId, TypeOfficer);
  if (!officer || officer->m_dead || officer->m_orders != Officer::OrderFollow)
  {
    m_officerId.SetInvalid();
    m_state = StateIdle;
    return false;
  }


  //
  // Every few seconds, see if our officer is still around
  // Retarget him in case he moved a lot

  m_officerTimer -= SERVER_ADVANCE_PERIOD;
  if (m_officerTimer <= 0.0f)
  {
    m_officerTimer = 5.0f;
    bool walkable = g_location->IsWalkable(m_pos, officer->m_pos);
    if (!walkable)
    {
      //
      // If our officer stepped into a teleport, try to follow
      if (officer->m_ordersBuildingId != -1)
      {
        m_ordersBuildingId = officer->m_ordersBuildingId;
        Building* building = g_location->GetBuilding(m_ordersBuildingId);
        DEBUG_ASSERT(building);
        Teleport* teleport = (Teleport*)building;
        if (!teleport->Connected())
        {
          // Our officer went in but its no longer connected
          m_officerId.SetInvalid();
          m_state = StateIdle;
          return false;
        }
        else
        {
          // Teleport converts in T17; its out-parameters are still legacy.
          DirectX::XMFLOAT3 entrancePos{0.0f, 0.0f, 0.0f};
          DirectX::XMFLOAT3 entranceFront{0.0f, 0.0f, 0.0f};
          teleport->GetEntrance(AsLegacy(entrancePos), AsLegacy(entranceFront));
          m_wayPoint = entrancePos;
          m_wayPoint = PushFromObstructions(m_wayPoint);
          m_wayPoint.y = g_location->m_landscape.m_heightMap->GetValue(m_wayPoint.x, m_wayPoint.z);
        }
      }
      else
      {
        m_officerId.SetInvalid();
        m_state = StateIdle;
        return false;
      }
    }
    else
    {
      m_wayPoint = officer->m_pos;
      float positionError = 40.0f;
      float radius = syncfrand(positionError);
      float theta = syncfrand(M_PI * 2);
      m_wayPoint.x += radius * sinf(theta);
      m_wayPoint.z += radius * cosf(theta);
      m_wayPoint.y = g_location->m_landscape.m_heightMap->GetValue(m_wayPoint.x, m_wayPoint.z);
    }
  }


  //
  // Head straight for him

  bool arrived = AdvanceToTargetPosition();

  if (m_ordersBuildingId != -1)
  {
    bool teleported = (EnterTeleports(m_ordersBuildingId) != -1);
    if (teleported)
    {
      m_ordersBuildingId = -1;
      arrived = true;
    }
  }

  if (arrived)
  {
    float positionError = 0.0f;
    if (m_ordersBuildingId != -1)
    {
      // We have arrived but are trying to enter a teleport
      // Just wiggle around until we are given entry
      Building* building = g_location->GetBuilding(m_ordersBuildingId);
      DEBUG_ASSERT(building);
      Teleport* teleport = (Teleport*)building;
      if (!teleport->Connected())
      {
        m_ordersBuildingId = -1;
      }
      else
      {
        // Teleport converts in T17; its out-parameters are still legacy.
        DirectX::XMFLOAT3 entrancePos{0.0f, 0.0f, 0.0f};
        DirectX::XMFLOAT3 entranceFront{0.0f, 0.0f, 0.0f};
        teleport->GetEntrance(AsLegacy(entrancePos), AsLegacy(entranceFront));
        m_wayPoint = entrancePos;
        positionError = 10.0f;
      }
    }
    else
    {
      m_wayPoint = officer->m_pos;
      positionError = 40.0f;
    }

    float radius = syncfrand(positionError);
    float theta = syncfrand(M_PI * 2);
    m_wayPoint.x += radius * sinf(theta);
    m_wayPoint.z += radius * cosf(theta);
    m_wayPoint = PushFromObstructions(m_wayPoint);
    m_wayPoint.y = g_location->m_landscape.m_heightMap->GetValue(m_wayPoint.x, m_wayPoint.z);
  }

  return false;
}


void Citizen::BoardRocket(int _buildingId)
{
  m_state = StateBoardingRocket;
  m_buildingId = _buildingId;
}


bool Citizen::AdvanceBoardingRocket()
{
  //
  // Find our building

  Building* building = g_location->GetBuilding(m_buildingId);
  if (!building || building->m_type != Building::TypeFuelStation)
  {
    m_state = StateIdle;
    return false;
  }

  //
  // Make sure we are still loading Citizens

  FuelStation* station = (FuelStation*)building;
  if (!station->IsLoading())
  {
    m_state = StateIdle;
    return false;
  }


  //
  // Head towards the building

  m_wayPoint = station->GetEntrance();
  m_wayPoint = PushFromObstructions(m_wayPoint);

  bool arrived = AdvanceToTargetPosition();
  if (arrived)
  {
    bool boarded = station->BoardRocket(m_id);
    if (boarded)
      return true;
  }

  return false;
}


void Citizen::AttackBuilding(int _buildingId)
{
  m_state = StateAttackingBuilding;
  m_buildingId = _buildingId;
}


bool Citizen::AdvanceAttackingBuilding()
{
  //
  // Find our building

  Building* building = g_location->GetBuilding(m_buildingId);
  if (!building || building->m_type != Building::TypeEscapeRocket)
  {
    m_state = StateIdle;
    return false;
  }


  //
  // Run towards our building

  DirectX::XMVECTOR const buildingPos = DirectX::XMLoadFloat3(&building->m_pos);
  float distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(buildingPos, DirectX::XMLoadFloat3(&m_pos))));
  float const standOff = 100 + syncfrand(50.0f);
  DirectX::XMVECTOR const moveVector =
    DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), buildingPos)), standOff);
  DirectX::XMStoreFloat3(&m_wayPoint, DirectX::XMVectorAdd(buildingPos, moveVector));
  bool arrived = AdvanceToTargetPosition();


  //
  // Shoot at the building if we are in range

  if (distance < 200.0f)
  {
    DirectX::XMFLOAT3 targetPos = building->m_pos;
    targetPos.y += 50.0f;
    g_location->ThrowWeapon(m_pos, targetPos, WorldObject::EffectThrowableGrenade, 1);
    m_state = StateIdle;
  }

  return false;
}


bool Citizen::SearchForRandomPosition()
{
  START_PROFILE(g_profiler, "SearchRandomPos");

  //
  // Search for a new random position
  // Sometimes just don't bother, so we stand still,
  // pondering the meaning of the world

  if (syncfrand() < 0.7f)
  {
    float distance = 20.0f;
    float angle = syncsfrand(2.0f * M_PI);

    DirectX::XMStoreFloat3(&m_wayPoint, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos),
                                                             DirectX::XMVectorSet(sinf(angle) * distance, 0.0f, cosf(angle) * distance, 0.0f)));

    m_wayPoint = PushFromObstructions(m_wayPoint);
    m_wayPoint.y = g_location->m_landscape.m_heightMap->GetValue(m_wayPoint.x, m_wayPoint.z);
  }

  END_PROFILE(g_profiler, "SearchRandomPos");

  return true;
}


bool Citizen::SearchForArmour()
{
  //
  // Red Citizens don't respond to armour

  if (m_id.GetTeamId() == 1)
    return false;


  START_PROFILE(g_profiler, "SearchArmour");

  //
  // Build a list of nearby armour

  std::vector<WorldObjectId> m_armour;

  Team* team = g_location->GetMyTeam();

  if (team)
  {
    for (int i = 0; i < static_cast<int>(team->m_specials.size()); ++i)
    {
      WorldObjectId id = team->m_specials[i];
      Entity* entity = g_location->GetEntity(id);
      if (entity && !entity->m_dead && entity->m_type == Entity::TypeArmour)
      {
        Armour* armour = (Armour*)entity;
        float range = DirectX::XMVectorGetX(
          DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&armour->m_pos), DirectX::XMLoadFloat3(&m_pos))));
        if (range <= CITIZEN_SEARCHRANGE_ARMOUR && armour->IsLoading())
        {
          m_armour.push_back(id);
        }
      }
    }
  }

  //
  // Select armour randomly

  if (!m_armour.empty())
  {
    int chosenIndex = rand() % static_cast<int>(m_armour.size());
    m_armourId = *&m_armour[chosenIndex];
    m_state = StateApproachingArmour;
  }

  END_PROFILE(g_profiler, "SearchArmour");
  return (!m_armour.empty());
}


bool Citizen::SearchForOfficers()
{
  //
  // Red Citizens don't respond to officers

  if (m_id.GetTeamId() == 1)
    return false;


  START_PROFILE(g_profiler, "SearchOfficers");

  //
  // Do we already have some orders that have yet to be completed?

  if (m_ordersSet)
  {
    float positionError = 20.0f;
    float radius = syncfrand(positionError);
    float theta = syncfrand(M_PI * 2);
    m_wayPoint = m_orders;
    m_wayPoint.x += radius * sinf(theta);
    m_wayPoint.z += radius * cosf(theta);
    m_wayPoint = PushFromObstructions(m_wayPoint);
    m_wayPoint.y = g_location->m_landscape.m_heightMap->GetValue(m_wayPoint.x, m_wayPoint.z);

    m_state = StateFollowingOrders;
    END_PROFILE(g_profiler, "SearchOfficers");
    return true;
  }


  //
  // Build a list of nearby officers with GOTO orders set
  // Also find the nearest officer with FOLLOW orders set

  Team* team = g_location->GetMyTeam();

  if (team)
  {
    std::vector<WorldObjectId> officers;
    float nearest = 99999.9f;
    WorldObjectId nearestId;

    for (int i = 0; i < static_cast<int>(team->m_specials.size()); ++i)
    {
      WorldObjectId id = team->m_specials[i];
      Entity* entity = g_location->GetEntity(id);
      if (entity && !entity->m_dead && entity->m_type == Entity::TypeOfficer)
      {
        Officer* officer = (Officer*)entity;
        float distance = DirectX::XMVectorGetX(
          DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&officer->m_pos), DirectX::XMLoadFloat3(&m_pos))));
        if (distance < CITIZEN_SEARCHRANGE_OFFICERS && officer->m_orders == Officer::OrderGoto)
        {
          officers.push_back(id);
        }
        else if (officer->m_orders == Officer::OrderFollow && distance > 50.0f && distance < nearest)
        {
          nearest = distance;
          nearestId = id;
        }
      }
    }


    //
    // Select a GOTO officer randomly

    if (!officers.empty())
    {
      int chosenOfficer = syncrand() % static_cast<int>(officers.size());
      WorldObjectId officerId = *&officers[chosenOfficer];
      Officer* officer = (Officer*)g_location->GetEntitySafe(officerId, TypeOfficer);
      DEBUG_ASSERT(officer);

      if (g_location->IsWalkable(m_pos, officer->m_orderPosition))
      {
        m_orders = officer->m_orderPosition;
        m_ordersBuildingId = officer->m_ordersBuildingId;
        m_ordersSet = true;

        float positionError = 20.0f;
        float radius = syncfrand(positionError);
        float theta = syncfrand(M_PI * 2);
        m_wayPoint = m_orders;
        m_wayPoint.x += radius * sinf(theta);
        m_wayPoint.z += radius * cosf(theta);

        m_wayPoint = PushFromObstructions(m_wayPoint, false);
        m_wayPoint.y = g_location->m_landscape.m_heightMap->GetValue(m_wayPoint.x, m_wayPoint.z);

        g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "GivenOrders");

        m_state = StateFollowingOrders;
        END_PROFILE(g_profiler, "SearchOfficers");
        return true;
      }
    }


    //
    // If there aren't any officers nearby, look for officers
    // with the FOLLOW order set and head for them

    if (officers.empty() && nearestId.IsValid())
    {
      m_officerId = nearestId;
      Officer* officer = (Officer*)g_location->GetEntitySafe(m_officerId, TypeOfficer);
      if (g_location->IsWalkable(m_pos, officer->m_pos, true))
      {
        m_wayPoint = officer->m_pos;

        float positionError = 40.0f;
        float radius = syncfrand(positionError);
        float theta = syncfrand(M_PI * 2);
        m_wayPoint.x += radius * sinf(theta);
        m_wayPoint.z += radius * cosf(theta);
        m_wayPoint.y = g_location->m_landscape.m_heightMap->GetValue(m_wayPoint.x, m_wayPoint.z);

        m_state = StateFollowingOfficer;
        m_officerTimer = 5.0f;
        END_PROFILE(g_profiler, "SearchOfficers");
        return true;
      }
    }
  }

  END_PROFILE(g_profiler, "SearchOfficers");
  return false;
}


void Citizen::GiveOrders(DirectX::XMFLOAT3 const& _targetPos)
{
  m_orders = _targetPos;
  m_ordersBuildingId = -1;
  m_ordersSet = true;

  //
  // If there is a teleport nearby,
  // assume he wants us to go in it

  bool foundTeleport = false;

  std::vector<int> const* nearbyBuildings = g_location->m_obstructionGrid->GetBuildings(m_orders.x, m_orders.z);
  for (int buildingId : *nearbyBuildings)
  {
    Building* building = g_location->GetBuilding(buildingId);
    if (building->m_type == Building::TypeRadarDish || building->m_type == Building::TypeBridge)
    {
      float distance = DirectX::XMVectorGetX(
        DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&building->m_pos), DirectX::XMLoadFloat3(&m_orders))));
      if (distance < 5)
      {
        Teleport* teleport = (Teleport*)building;
        m_ordersBuildingId = building->m_id.GetUniqueId();
        // Teleport converts in T17; its out-parameters are still legacy.
        DirectX::XMFLOAT3 entrancePos{0.0f, 0.0f, 0.0f};
        DirectX::XMFLOAT3 entranceFront{0.0f, 0.0f, 0.0f};
        teleport->GetEntrance(AsLegacy(entrancePos), AsLegacy(entranceFront));
        m_orders = entrancePos;
        foundTeleport = true;
        break;
      }
    }
  }

  m_wayPoint = m_orders;

  if (!foundTeleport)
  {
    float positionError = 20.0f;
    float radius = syncfrand(positionError);
    float theta = syncfrand(M_PI * 2);
    m_wayPoint.x += radius * sinf(theta);
    m_wayPoint.z += radius * cosf(theta);
  }

  m_wayPoint = PushFromObstructions(m_wayPoint);
  m_wayPoint.y = g_location->m_landscape.m_heightMap->GetValue(m_wayPoint.x, m_wayPoint.z);
  m_wayPoint = PushFromObstructions(m_wayPoint);

  g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "GivenOrders");

  m_state = StateFollowingOrders;
}


bool Citizen::SearchForSpirits()
{
  // Red citizens don't worship spirits
  if (m_id.GetTeamId() == 1)
    return false;

  START_PROFILE(g_profiler, "SearchSpirits");

  Spirit* found = nullptr;
  int spiritId = -1;
  float closest = CITIZEN_SEARCHRANGE_SPIRITS;

  if (syncrand() % 5 == 0)
  {
    for (int i = 0; i < g_location->m_spirits.Size(); ++i)
    {
      if (g_location->m_spirits.ValidIndex(i))
      {
        Spirit* s = g_location->m_spirits.GetPointer(i);
        float theDist =
          DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&s->m_pos), DirectX::XMLoadFloat3(&m_pos))));

        if (theDist < closest && (s->m_state == Spirit::StateBirth || s->m_state == Spirit::StateFloating))
        {
          found = s;
          spiritId = i;
          closest = theDist;
        }
      }
    }
  }

  if (found)
  {
    m_spiritId = spiritId;
    m_state = StateWorshipSpirit;
  }

  END_PROFILE(g_profiler, "SearchSpirits");
  return found;
}


bool Citizen::SearchForThreats()
{
  START_PROFILE(g_profiler, "SearchThreats");

  //
  // Allow our threat range to creep back up to the max

  float threatRangeChange = SERVER_ADVANCE_PERIOD;
  m_threatRange = (CITIZEN_SEARCHRANGE_THREATS * threatRangeChange) + (m_threatRange * (1.0f - threatRangeChange));


  //
  // If we are running towards a Battle Cannon, this takes
  // priority over everything else

  if (m_state == StateApproachingPort)
  {
    Building* building = g_location->GetBuilding(m_buildingId);
    if (building && building->m_type == Building::TypeGunTurret)
    {
      END_PROFILE(g_profiler, "SearchThreats");
      return false;
    }
  }

  //
  // Search for grenades, airstrikes nearby

  int numEnemies = 0;
  float nearestThreatSqd = FLT_MAX;
  WorldObjectId threatId;
  bool throwableWeaponFound = false;

  float maxGrenadeRangeSqd = pow(CITIZEN_SEARCHRANGE_GRENADES, 2);

  for (int i = 0; i < g_location->m_effects.Size(); ++i)
  {
    if (g_location->m_effects.ValidIndex(i))
    {
      WorldObject* wobj = g_location->m_effects[i];
      if (wobj->m_type == EffectThrowableGrenade || wobj->m_type == EffectThrowableAirstrikeMarker || wobj->m_type == EffectGunTurretTarget ||
          (wobj->m_type == EffectSpamInfection && m_id.GetTeamId() == 0))
      {
        float distanceSqd = DirectX::XMVectorGetX(
          DirectX::XMVector3LengthSq(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&wobj->m_pos), DirectX::XMLoadFloat3(&m_pos))));

        if (distanceSqd < maxGrenadeRangeSqd && distanceSqd < nearestThreatSqd)
        {
          nearestThreatSqd = distanceSqd;
          threatId = wobj->m_id;
          throwableWeaponFound = true;
        }
      }
    }
  }

  //
  // If we found a grenade, run away immediately

  if (throwableWeaponFound)
  {
    m_state = StateCombat;
    m_scared = true;
    if (m_threatId != threatId)
    {
      g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "SeenThreatRunAway");
      m_threatId = threatId;
    }
    END_PROFILE(g_profiler, "SearchThreats");
    return true;
  }


  //
  // No explosives nearby.  Look for bad guys
  // Start with a quick evaluation of the area, by querying any AITarget buildings

  for (int i = 0; i < g_location->m_buildings.Size(); ++i)
  {
    if (g_location->m_buildings.ValidIndex(i))
    {
      Building* building = g_location->m_buildings[i];
      if (building && building->m_type == Building::TypeAITarget)
      {
        float range = DirectX::XMVectorGetX(
          DirectX::XMVector3LengthSq(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&building->m_pos), DirectX::XMLoadFloat3(&m_pos))));
        if (range < 40000.0f) // 200m
        {
          AITarget* target = (AITarget*)building;
          int numEnemiesNearby = target->m_enemyCount[m_id.GetTeamId()];
          if (numEnemiesNearby == 0)
          {
            END_PROFILE(g_profiler, "SearchThreats");
            return false;
          }
        }
      }
    }
  }


  // Count the number of nearby friends and enemies
  // Also find the nearest enemy

  int numFound = 0;
  float searchRange = m_threatRange;
  if (m_state == StateOperatingPort)
  {
    searchRange *= 0.5f;
    Building* building = g_location->GetBuilding(m_buildingId);
    if (building && building->m_type == Building::TypeGunTurret)
    {
      searchRange = 0.0f;
    }
  }

  WorldObjectId* ids = g_location->m_entityGrid->GetEnemies(m_pos.x, m_pos.z, searchRange, &numFound, m_id.GetTeamId());
  bool friendsPresent = g_location->m_entityGrid->AreFriendsPresent(m_pos.x, m_pos.z, searchRange, m_id.GetTeamId());

  for (int i = 0; i < numFound; ++i)
  {
    WorldObjectId id = ids[i];
    Entity* entity = g_location->GetEntity(id);
    bool onFire = entity->m_type == TypeCitizen && ((Citizen*)entity)->IsOnFire();

    if (!entity->m_dead && !onFire && entity->m_type != TypeEgg)
    {
      ++numEnemies;

      float distanceSqd = DirectX::XMVectorGetX(
        DirectX::XMVector3LengthSq(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&entity->m_pos), DirectX::XMLoadFloat3(&m_pos))));
      if (distanceSqd < nearestThreatSqd)
      {
        nearestThreatSqd = distanceSqd;
        threatId = id;
      }
    }
  }


  //
  // Decide what to do with our threat

  Entity* entity = g_location->GetEntity(threatId);

  if (entity && !entity->m_dead)
  {
    m_state = StateCombat;
    bool soldier = m_id.GetTeamId() == 1 || g_globalWorld->m_research->CurrentLevel(GlobalResearch::TypeCitizen) > 2;

    if (!soldier)
      m_scared = true;
    if (soldier)
    {
      m_scared = numEnemies > 5 && !friendsPresent;
    }

    if (entity->m_type == TypeSporeGenerator || entity->m_type == TypeTripod || entity->m_type == TypeSpider || entity->m_type == TypeSoulDestroyer ||
        entity->m_type == TypeTriffidEgg || entity->m_type == TypeInsertionSquadie || entity->m_type == TypeArmour)
    {
      m_scared = true;
    }

    if (m_threatId != threatId)
    {
      if (m_scared)
      {
        g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "SeenThreatRunAway");
      }
      else
      {
        g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "SeenThreatAttack");
      }
      m_threatId = threatId;
    }

    if (m_ordersSet && DirectX::XMVectorGetX(DirectX::XMVector3Length(
                         DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&m_orders)))) < 50.0f)
    {
      // We got near enough
      m_ordersSet = false;
    }

    m_threatRange = sqrtf(nearestThreatSqd);
    END_PROFILE(g_profiler, "SearchThreats");
    return true;
  }
  else
  {
    // There are no nearby threats
    m_threatId.SetInvalid();
    g_soundSystem->StopAllSounds(m_id, "Citizen SeenThreat");

    END_PROFILE(g_profiler, "SearchThreats");
    return false;
  }
}


bool Citizen::SearchForPorts()
{
  START_PROFILE(g_profiler, "SearchPorts");

  //
  // Build a list of available buildings

  std::vector<int> availableBuildings;

  for (int i = 0; i < g_location->m_buildings.Size(); ++i)
  {
    if (g_location->m_buildings.ValidIndex(i))
    {
      Building* building = g_location->m_buildings[i];
      float distanceToBuilding = DirectX::XMVectorGetX(
        DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&building->m_pos), DirectX::XMLoadFloat3(&m_pos))));
      distanceToBuilding -= building->m_radius;
      if (distanceToBuilding < CITIZEN_SEARCHRANGE_PORTS)
      {
        if (building->GetNumPortsOccupied() < building->GetNumPorts())
        {
          availableBuildings.push_back(building->m_id.GetUniqueId());
        }
      }
    }
  }

  if (availableBuildings.empty())
  {
    END_PROFILE(g_profiler, "SearchPorts");
    return false;
  }


  //
  // Select a random building

  int chosenBuildingIndex = syncrand() % static_cast<int>(availableBuildings.size());
  Building* chosenBuilding = g_location->GetBuilding(availableBuildings[chosenBuildingIndex]);
  DEBUG_ASSERT(chosenBuilding);


  //
  // Build a list of available ports;

  std::vector<int> availablePorts;
  for (int p = 0; p < chosenBuilding->GetNumPorts(); ++p)
  {
    if (!chosenBuilding->GetPortOccupant(p).IsValid() && chosenBuilding->GetPortOperatorCount(p, m_id.GetTeamId()) < 20)
    {
      availablePorts.push_back(p);
    }
  }


  //
  // Select a random port

  if (availablePorts.empty())
  {
    END_PROFILE(g_profiler, "SearchPorts");
    return false;
  }

  int randomSelection = syncrand() % static_cast<int>(availablePorts.size());
  m_buildingId = chosenBuilding->m_id.GetUniqueId();
  m_portId = availablePorts[randomSelection];
  m_state = StateApproachingPort;
  // Building converts in T16; same seam.
  DirectX::XMFLOAT3 portPos{0.0f, 0.0f, 0.0f};
  DirectX::XMFLOAT3 portFront{0.0f, 0.0f, 0.0f};
  chosenBuilding->GetPortPosition(m_portId, AsLegacy(portPos), AsLegacy(portFront));
  m_wayPoint = portPos;
  // m_wayPoint.y = g_location->m_landscape.m_heightMap->GetValue( m_wayPoint.x, m_wayPoint.z );

  END_PROFILE(g_profiler, "SearchPorts");
  return true;
}


bool Citizen::BeginVictoryDance()
{
  if (m_onGround && m_id.GetTeamId() == 0 && syncfrand(5.0f) < 1.0f && m_pos.y > 10.0f)
  {
    std::vector<GlobalEventCondition*>* objectivesList = &g_location->m_levelFile->m_primaryObjectives;

    bool victory = true;
    for (GlobalEventCondition* gec : *objectivesList)
    {
      if (!gec->Evaluate())
      {
        victory = false;
        break;
      }
    }


    if (victory)
    {
      // jump!
      m_vel.y += 15.0f + syncfrand(15.0f);
      m_onGround = false;
      g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "VictoryJump");
      return true;
    }
  }

  return false;
}


bool Citizen::AdvanceToTargetPosition()
{
  START_PROFILE(g_profiler, "AdvanceToTargetPos");

  DirectX::XMFLOAT3 const oldPos = m_pos;

  //
  // Are we there yet?

  DirectX::XMVECTOR const vectorRemaining =
    DirectX::XMVectorSetY(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_wayPoint), DirectX::XMLoadFloat3(&m_pos)), 0.0f);
  float distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(vectorRemaining));
  if (distance == 0.0f)
  {
    m_vel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
    END_PROFILE(g_profiler, "AdvanceToTargetPos");
    return true;
  }
  else if (distance < 1.0f)
  {
    m_pos = m_wayPoint;
    DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&oldPos)),
                                                          1.0f / SERVER_ADVANCE_PERIOD));
    END_PROFILE(g_profiler, "AdvanceToTargetPos");
    return false;
  }


  //
  // Work out where we want to be next

  float speed = m_stats[StatSpeed];
  if (m_state == StateIdle || m_state == StateWorshipSpirit)
    speed *= 0.2f;

  float amountToTurn = SERVER_ADVANCE_PERIOD * 4.0f;
  DirectX::XMVECTOR const targetDir =
    DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_wayPoint), DirectX::XMLoadFloat3(&m_pos)));
  DirectX::XMVECTOR const actualDir = DirectX::XMVector3Normalize(DirectX::XMVectorMultiplyAdd(
    targetDir, DirectX::XMVectorReplicate(amountToTurn), DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_front), 1.0f - amountToTurn)));

  DirectX::XMFLOAT3 newPos;
  DirectX::XMStoreFloat3(
    &newPos, DirectX::XMVectorMultiplyAdd(actualDir, DirectX::XMVectorReplicate(speed * SERVER_ADVANCE_PERIOD), DirectX::XMLoadFloat3(&m_pos)));


  //
  // Slow us down if we're going up hill
  // Speed up if going down hill

  float currentHeight = g_location->m_landscape.m_heightMap->GetValue(oldPos.x, oldPos.z);
  float nextHeight = g_location->m_landscape.m_heightMap->GetValue(newPos.x, newPos.z);
  float factor = 1.0f - (currentHeight - nextHeight) / -3.0f;
  if (factor < 0.3f)
    factor = 0.3f;
  if (factor > 2.0f)
    factor = 2.0f;
  speed *= factor;


  //
  // Slow us down if we're near our objective

  if (distance < 10.0f)
  {
    float distanceFactor = distance / 10.0f;
    speed *= distanceFactor;
  }

  DirectX::XMStoreFloat3(
    &newPos, DirectX::XMVectorMultiplyAdd(actualDir, DirectX::XMVectorReplicate(speed * SERVER_ADVANCE_PERIOD), DirectX::XMLoadFloat3(&m_pos)));
  newPos = PushFromObstructions(newPos);
  // newPos = PushFromCliffs( newPos, oldPos );

  DirectX::XMVECTOR moved = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&newPos), DirectX::XMLoadFloat3(&oldPos));
  if (DirectX::XMVectorGetX(DirectX::XMVector3Length(moved)) > speed * SERVER_ADVANCE_PERIOD)
  {
    moved = DirectX::XMVectorScale(DirectX::XMVector3Normalize(moved), speed * SERVER_ADVANCE_PERIOD);
  }
  DirectX::XMStoreFloat3(&m_pos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), moved));

  DirectX::XMVECTOR const travelled = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&oldPos));
  DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(travelled, 1.0f / SERVER_ADVANCE_PERIOD));
  DirectX::XMStoreFloat3(&m_front, DirectX::XMVector3Normalize(travelled));

  END_PROFILE(g_profiler, "AdvanceToTargetPos");
  return false;
}


DirectX::XMFLOAT3 Citizen::PushFromObstructions(DirectX::XMFLOAT3 const& pos, bool killem)
{
  DirectX::XMFLOAT3 result = pos;
  if (m_onGround)
  {
    result.y = g_location->m_landscape.m_heightMap->GetValue(result.x, result.z);
  }

  DirectX::XMFLOAT4X4 transform;
  DirectX::XMStoreFloat4x4(&transform, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&result)));

  //
  // Push from Water


  if (result.y <= 1.0f)
  {
    START_PROFILE(g_profiler, "PushFromWater");

    float pushAngle = syncsfrand(1.0f);
    float distance = 40.0f;
    while (distance < 100.0f)
    {
      float angle = distance * pushAngle * M_PI;
      DirectX::XMVECTOR const offset = DirectX::XMVectorSet(cosf(angle) * distance, 0.0f, sinf(angle) * distance, 0.0f);

      DirectX::XMFLOAT3 newPos;
      DirectX::XMStoreFloat3(&newPos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&result), offset));
      float height = g_location->m_landscape.m_heightMap->GetValue(newPos.x, newPos.z);
      if (height > 1.0f)
      {
        result = newPos;
        result.y = height;
        m_avoidObstruction = result;
        break;
      }
      distance += 5.0f;
    }

    END_PROFILE(g_profiler, "PushFromWater");
  }


  //
  // Push from buildings

  START_PROFILE(g_profiler, "PushFromBuildings");

  std::vector<int> const* buildings = g_location->m_obstructionGrid->GetBuildings(result.x, result.z);

  for (int buildingId : *buildings)
  {
    Building* building = g_location->GetBuilding(buildingId);
    if (building)
    {
      if (building->m_type == Building::TypeLaserFence && ((LaserFence*)building)->IsEnabled())
      {
        float closest = 5.0f + m_id.GetUniqueId() % 10;
        if (building->DoesSphereHit(m_pos, 1.0f) && killem)
        {
          // ChangeHealth( -999 );
          SetFire();
          ((LaserFence*)building)->Electrocute(m_pos);
        }
        else if (building->DoesSphereHit(result, closest))
        {
          LaserFence* nextFence = (LaserFence*)g_location->GetBuilding(((LaserFence*)building)->GetBuildingLink());
          DirectX::XMVECTOR pushForce = DirectX::XMVectorScale(
            DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&building->m_centrePos), DirectX::XMLoadFloat3(&result))),
            1.0f);
          if (nextFence)
          {
            DirectX::XMVECTOR const fenceVector =
              DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&nextFence->m_pos), DirectX::XMLoadFloat3(&building->m_pos));
            DirectX::XMVECTOR const rightAngle = DirectX::XMVector3Cross(fenceVector, DirectX::g_XMIdentityR1);

            // The intermediate SetLength to the cross product's y component is
            // DEAD -- the very next line overwrites the length with 20 -- so
            // only its side effect on direction ever mattered, and a normalise
            // preserves that. Kept as one scale rather than two.
            pushForce = DirectX::XMVectorScale(DirectX::XMVector3Normalize(rightAngle), 20.0f);
          }
          DirectX::XMStoreFloat3(&result, DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&result), pushForce));
          m_avoidObstruction = result;
          m_state = StateIdle;
          DirectX::XMStoreFloat3(&m_wayPoint, DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), pushForce));
          m_ordersSet = false;
        }
      }
      else
      {
        if (building->DoesSphereHit(result, 30.0f))
        {
          // Same load-bearing fallback as Entity::PushFromObstructions: the
          // legacy SetLength left (2,0,0) for a zero-length delta, and without
          // it this loop does not terminate.
          DirectX::XMVECTOR pushForce = DirectX::XMVectorScale(
            DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&building->m_pos), DirectX::XMLoadFloat3(&result))), 2.0f);
          if (DirectX::XMVector3Equal(pushForce, DirectX::XMVectorZero()))
          {
            pushForce = DirectX::XMVectorSet(2.0f, 0.0f, 0.0f, 0.0f);
          }

          while (building->DoesSphereHit(result, 1.0f))
          {
            DirectX::XMStoreFloat3(&result, DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&result), pushForce));
            // result.y = g_location->m_landscape.m_heightMap->GetValue( result.x, result.z );
          }
        }
      }
      break;
    }
  }

  END_PROFILE(g_profiler, "PushFromBuildings");

  //
  // If we already have some avoidance rules,
  // follow them above all else

  // Vector3::operator!= was a per-component NearlyEquals at 1e-6, so this is
  // XMVector3NearEqual rather than XMVector3NotEqual.
  if (!DirectX::XMVector3NearEqual(DirectX::XMLoadFloat3(&m_avoidObstruction), DirectX::XMVectorZero(), DirectX::XMVectorReplicate(1e-6f)))
  {
    float distance = DirectX::XMVectorGetX(
      DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_avoidObstruction), DirectX::XMLoadFloat3(&pos))));
    if (distance < 10.0f)
    {
      m_avoidObstruction = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
    }
    else
    {
      return m_avoidObstruction;
    }
  }

  return result;
}


void Citizen::TakeControl(int _controllerId)
{
  Task* controller = g_taskManager->GetTask(_controllerId);
  if (controller)
  {
    m_controllerId = _controllerId;
    m_wayPointId = controller->m_route->GetIdOfNearestWayPoint(m_pos);
    // LevelFile's WayPoint converts in T18, so GetPos is still legacy.
    m_wayPoint = DirectX::XMFLOAT3(controller->m_route->GetWayPoint(m_wayPointId)->GetPos());
    float const jitterX = syncsfrand(30.0f);
    float const jitterZ = syncsfrand(30.0f);
    DirectX::XMStoreFloat3(&m_wayPoint, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_wayPoint), DirectX::XMVectorSet(jitterX, 0.0f, jitterZ, 0.0f)));
    m_wayPoint = PushFromObstructions(m_wayPoint);
    m_wayPoint.y = g_location->m_landscape.m_heightMap->GetValue(m_wayPoint.x, m_wayPoint.z);
    m_state = StateUnderControl;
    m_ordersSet = false;

    int numFlashes = 5 + speciesRandom() % 5;
    for (int i = 0; i < numFlashes; ++i)
    {
      // Three unsynchronised draws per flash, order and count unchanged.
      float const velX = sfrand(5.0f);
      float const velY = frand(15.0f);
      float const velZ = sfrand(5.0f);
      DirectX::XMFLOAT3 const vel(velX, velY, velZ);
      g_particleSystem->CreateParticle(m_pos, vel, Particle::TypeControlFlash);
    }

    g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "TakenControl");
  }
}


void Citizen::AntCapture(WorldObjectId _antId)
{
  m_threatId = _antId;
  m_state = StateCapturedByAnt;
}


bool Citizen::IsInView() { return g_camera->PosInViewFrustum(m_pos); }


bool Citizen::AdvanceOnFire()
{
  m_wayPoint = m_pos;
  float const wanderX = syncsfrand(100.0f);
  float const wanderZ = syncsfrand(100.0f);
  DirectX::XMStoreFloat3(&m_wayPoint, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_wayPoint), DirectX::XMVectorSet(wanderX, 0.0f, wanderZ, 0.0f)));
  m_wayPoint.y = g_location->m_landscape.m_heightMap->GetValue(m_wayPoint.x, m_wayPoint.z);

  if (m_onGround)
    AdvanceToTargetPosition();

  int numFireParticles = syncrand() % 8;
  for (int i = 0; i < numFireParticles; ++i)
  {
    // Three syncfrand draws per particle, in this order. Hoisted into named
    // locals so the order is on the page rather than inside an expression.
    float const spawnHeight = syncfrand(5);
    DirectX::XMFLOAT3 fireSpawn;
    DirectX::XMStoreFloat3(
      &fireSpawn, DirectX::XMVectorMultiplyAdd(DirectX::g_XMIdentityR1, DirectX::XMVectorReplicate(spawnHeight), DirectX::XMLoadFloat3(&m_pos)));
    // fireSpawn -= m_vel * 0.1f;
    float fireSize = 20 + syncfrand(30.0f);

    float const rise = 3 + syncfrand(3);
    DirectX::XMFLOAT3 fireVel;
    DirectX::XMStoreFloat3(&fireVel, DirectX::XMVectorMultiplyAdd(DirectX::g_XMIdentityR1, DirectX::XMVectorReplicate(rise),
                                                                  DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), 0.3f)));
    int particleType = Particle::TypeCitizenFire;
    if (i > 4)
      particleType = Particle::TypeMissileTrail;
    g_particleSystem->CreateParticle(fireSpawn, fireVel, particleType, fireSize);
  }

  if (syncrand() % 50 == 0)
  {
    g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "OnFire");
  }

  if (!m_dead && syncfrand(10) < 2 && m_onGround)
    ChangeHealth(-2);

  if (m_inWater > 0.0f)
  {
    m_state = StateIdle;
  }

  return false;
}


void Citizen::SetFire() { m_state = StateOnFire; }


bool Citizen::IsOnFire() { return (m_state == StateOnFire); }


// glVertex3fv wants three contiguous floats; an XMVECTOR is four lanes.
// Same helper, same reason, as T10's EmitVertex in TextRenderer.
static void EmitVertex(DirectX::FXMVECTOR _position)
{
  DirectX::XMFLOAT3 vertex;
  DirectX::XMStoreFloat3(&vertex, _position);
  glVertex3fv(&vertex.x);
}

// Vector3::RotateAround(axis) took the ANGLE FROM THE AXIS VECTOR'S MAGNITUDE
// and did nothing at all below 1e-8 squared. Both halves are load-bearing: the
// guard is what stops a zero axis producing a NaN orientation. Named here
// because this file rotates that way three times.
static DirectX::XMVECTOR XM_CALLCONV RotateAroundScaledAxis(DirectX::FXMVECTOR _v, DirectX::FXMVECTOR _scaledAxis)
{
  float const lengthSquared = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(_scaledAxis));
  if (lengthSquared < 1e-8f)
    return _v;

  float const angle = sqrtf(lengthSquared);
  return DirectX::XMVector3Transform(_v, DirectX::XMMatrixRotationAxis(DirectX::XMVectorScale(_scaledAxis, 1.0f / angle), angle));
}

void Citizen::Render(float _predictionTime, float _highDetail)
{
  if (!m_enabled)
    return;

  if (m_state == StateInsideArmour)
  {
    return;
  }

  RGBAColour colour;
  if (m_id.GetTeamId() >= 0)
    colour = g_location->m_teams[m_id.GetTeamId()].m_colour;

  DirectX::XMFLOAT3 predictedPos;
  DirectX::XMStoreFloat3(&predictedPos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(_predictionTime),
                                                                     DirectX::XMLoadFloat3(&m_pos)));

  DirectX::XMVECTOR entityUp = DirectX::g_XMIdentityR1;

  if (_highDetail > 0.0f)
  {
    if (m_onGround && m_inWater == -1)
    {
      predictedPos.y = g_location->m_landscape.m_heightMap->GetValue(predictedPos.x, predictedPos.z);
    }
    else if (!m_onGround)
    {
      predictedPos.y += 3.0f;
    }

    // The landscape converts in T18, so its normal map is still legacy.
    DirectX::XMFLOAT3 const landNormal = g_location->m_landscape.m_normalMap->GetValue(predictedPos.x, predictedPos.z);
    entityUp = DirectX::XMLoadFloat3(&landNormal);

    if (m_state == StateWorshipSpirit || m_state == StateOperatingPort || m_state == StateWatchingSpectacle)
    {
      entityUp = RotateAroundScaledAxis(entityUp, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_front), sinf(g_gameTime) * 0.3f));
    }
  }

  if (!m_onGround)
  {
    entityUp =
      RotateAroundScaledAxis(entityUp, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_front), g_gameTime * (m_id.GetUniqueId() % 10) * 0.1f));
  }

  DirectX::XMVECTOR entityRight = DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&m_front), entityUp);

  if (m_state == StateCapturedByAnt)
  {
    DirectX::XMVECTOR const capturedRight = entityUp;
    entityUp = DirectX::XMVector3Normalize(DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), -1.0f));
    entityRight = capturedRight;
    predictedPos.y += 4.0f;
  }

  float size = 3.0f;
  size *= (1.0f + 0.03f * ((m_id.GetIndex() * m_id.GetUniqueId()) % 10));
  entityRight *= size;
  entityUp *= size * 2.0f;


  /*
      #ifdef DEBUG_RENDER_ENABLED
          glDisable( GL_TEXTURE_2D );
      //    if( m_avoidObstruction != g_zeroVector )
      //    {
      //        RenderArrow( predictedPos, m_avoidObstruction, 1.0f );
      //    }
          ////    RenderArrow( predictedPos+entityUp*0.5f, predictedPos+entityUp*0.5f+entityFront*5.0f, 5.0f );
          RenderArrow( predictedPos+entityUp*0.5f, m_wayPoint, 1.0f, RGBAColour(255,0,0,255) );

      //    if( m_id.GetUniqueId() % 15 == 0 )
      //    {
      //        RenderSphere( predictedPos, m_threatRange, RGBAColour(255,0,0,255) );
      //    }

      //    if( m_state == StateCombat &&
      //        GetHighResTime() < m_grenadeTimer + 1.0f )
      //    {
      //        RenderArrow( predictedPos, m_grenadeTarget, 1.0f, RGBAColour(255,0,0,255) );
      //        RenderSphere( m_grenadeTarget, 50.0f, RGBAColour(255,0,0,255) );
      //    }
            glEnable( GL_TEXTURE_2D );
            glEnable( GL_BLEND );
      #endif*/


  //
  // Draw our shadow on the landscape

  if (_highDetail > 0.0f && m_shadowBuildingId == -1 && !m_dead)
  {
    int alpha = 150 * _highDetail;
    alpha = std::min(alpha, 255);
    glColor4ub(0, 0, 0, alpha);

    DirectX::XMVECTOR const centre = DirectX::XMLoadFloat3(&predictedPos);
    DirectX::XMVECTOR const forward = DirectX::XMVectorSet(0.0f, 0.0f, size * 2.0f, 0.0f);

    DirectX::XMFLOAT3 pos1;
    DirectX::XMFLOAT3 pos2;
    DirectX::XMFLOAT3 pos3;
    DirectX::XMFLOAT3 pos4;
    DirectX::XMStoreFloat3(&pos1, DirectX::XMVectorSubtract(centre, entityRight));
    DirectX::XMStoreFloat3(&pos2, DirectX::XMVectorAdd(centre, entityRight));
    DirectX::XMStoreFloat3(&pos4, DirectX::XMVectorAdd(DirectX::XMVectorSubtract(centre, entityRight), forward));
    DirectX::XMStoreFloat3(&pos3, DirectX::XMVectorAdd(DirectX::XMVectorAdd(centre, entityRight), forward));

    pos1.y = 0.3f + g_location->m_landscape.m_heightMap->GetValue(pos1.x, pos1.z);
    pos2.y = 0.3f + g_location->m_landscape.m_heightMap->GetValue(pos2.x, pos2.z);
    pos3.y = 0.3f + g_location->m_landscape.m_heightMap->GetValue(pos3.x, pos3.z);
    pos4.y = 0.3f + g_location->m_landscape.m_heightMap->GetValue(pos4.x, pos4.z);

    glDepthMask(false);
    glLineWidth(1.0f);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3fv(&pos1.x);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3fv(&pos2.x);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3fv(&pos3.x);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3fv(&pos4.x);
    glEnd();
    glDepthMask(true);
  }

  if (!m_dead || !m_onGround)
  {
    //
    // Draw our texture

    float maxHealth = EntityBlueprint::GetStat(m_type, StatHealth);
    float health = (float)m_stats[StatHealth] / maxHealth;
    if (health > 1.0f)
      health = 1.0f;
    colour *= 0.3f + 0.7f * health;
    colour.a = 255;

    if (m_dead)
    {
      glEnable(GL_BLEND);
      colour.a = 2.5f * (float)m_stats[StatHealth];
      colour.r *= 0.2f;
      colour.g *= 0.2f;
      colour.b *= 0.2f;
    }

    if (m_state == StateOnFire)
    {
      colour.r *= 0.01f;
      colour.g *= 0.01f;
      colour.b *= 0.01f;
    }

    glColor4ubv(colour.GetData());
    glBegin(GL_QUADS);
    glTexCoord2i(0, 1);
    EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&predictedPos), entityRight), entityUp));
    glTexCoord2i(1, 1);
    EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&predictedPos), entityRight), entityUp));
    glTexCoord2i(1, 0);
    EmitVertex(DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&predictedPos), entityRight));
    glTexCoord2i(0, 0);
    EmitVertex(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&predictedPos), entityRight));
    glEnd();

    // glDisable( GL_BLEND );

    //
    // Draw a blue glow if we are under control

    if (m_state == StateUnderControl)
    {
      glBlendFunc(GL_SRC_ALPHA, GL_ONE);
      glEnable(GL_BLEND);
      glDisable(GL_DEPTH_TEST);
      float scale = 1.1f;
      entityRight = DirectX::XMVectorScale(entityRight, scale);
      entityUp = DirectX::XMVectorScale(entityUp, scale);
      float alpha = fabs(sinf(g_gameTime * 2.0f));
      glColor4f(0.0f, 0.0f, 1.0f, alpha);
      glBegin(GL_QUADS);
      glTexCoord2i(0, 1);
      EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&predictedPos), entityRight), entityUp));
      glTexCoord2i(1, 1);
      EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&predictedPos), entityRight), entityUp));
      glTexCoord2i(1, 0);
      EmitVertex(DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&predictedPos), entityRight));
      glTexCoord2i(0, 0);
      EmitVertex(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&predictedPos), entityRight));
      glEnd();
      glEnable(GL_DEPTH_TEST);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    //
    // Draw shadow if we are watching a God Dish

    if (_highDetail > 0.0f && m_shadowBuildingId != -1)
    {
      Building* building = g_location->GetBuilding(m_shadowBuildingId);
      if (building)
      {
        float alpha = 1.0f;
        if (building->m_type == Building::TypeGodDish)
        {
          GodDish* dish = (GodDish*)building;
          if (!dish->m_activated && dish->m_timer < 1.0f)
          {
            m_shadowBuildingId = -1;
          }
          alpha = dish->m_timer * 0.1f;
        }
        else if (building->m_type == Building::TypeEscapeRocket)
        {
          EscapeRocket* rocket = (EscapeRocket*)building;
          if (!rocket->IsSpectacle())
          {
            m_shadowBuildingId = -1;
          }
          float distance = DirectX::XMVectorGetX(
            DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&building->m_pos), DirectX::XMLoadFloat3(&m_pos))));
          if (distance < 400.0f)
          {
            alpha = 1.0f;
          }
          else if (distance < 700.0f)
          {
            alpha = (700 - distance) / 300.0f;
          }
          else
          {
            alpha = 0.0f;
          }
        }
        alpha = std::min(alpha, 1.0f);
        alpha = std::max(alpha, 0.0f);

        if (alpha > 0.0f)
        {
          DirectX::XMVECTOR const length = DirectX::XMVectorScale(
            DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&predictedPos), DirectX::XMLoadFloat3(&building->m_pos))),
            size * 10);

          // Shadow behind the Citizen (green dudes only)
          if (m_id.GetTeamId() == 0)
          {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_BLEND);
            glColor4f(0.0f, 0.0f, 0.0f, alpha);
            DirectX::XMVECTOR const shadowVector = DirectX::XMVectorScale(DirectX::XMVector3Normalize(length), 0.05f);
            DirectX::XMStoreFloat3(&predictedPos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&predictedPos), shadowVector));
            glBegin(GL_QUADS);
            glTexCoord2i(0, 1);
            EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&predictedPos), entityRight), entityUp));
            glTexCoord2i(1, 1);
            EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&predictedPos), entityRight), entityUp));
            glTexCoord2i(1, 0);
            EmitVertex(DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&predictedPos), entityRight));
            glTexCoord2i(0, 0);
            EmitVertex(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&predictedPos), entityRight));
            glEnd();
          }

          // Shadow on the ground

          DirectX::XMVECTOR const shadowCentre = DirectX::XMLoadFloat3(&predictedPos);

          DirectX::XMFLOAT3 pos1;
          DirectX::XMFLOAT3 pos2;
          DirectX::XMFLOAT3 pos3;
          DirectX::XMFLOAT3 pos4;
          DirectX::XMStoreFloat3(&pos1, DirectX::XMVectorSubtract(shadowCentre, entityRight));
          DirectX::XMStoreFloat3(&pos2, DirectX::XMVectorAdd(shadowCentre, entityRight));
          DirectX::XMStoreFloat3(
            &pos4, DirectX::XMVectorSubtract(DirectX::XMVectorAdd(DirectX::XMVectorSubtract(shadowCentre, entityRight), length), entityRight));
          DirectX::XMStoreFloat3(&pos3,
                                 DirectX::XMVectorAdd(DirectX::XMVectorAdd(DirectX::XMVectorAdd(shadowCentre, entityRight), length), entityRight));

          pos1.y = 0.3f + g_location->m_landscape.m_heightMap->GetValue(pos1.x, pos1.z);
          pos2.y = 0.3f + g_location->m_landscape.m_heightMap->GetValue(pos2.x, pos2.z);
          pos3.y = 0.3f + g_location->m_landscape.m_heightMap->GetValue(pos3.x, pos3.z);
          pos4.y = 0.3f + g_location->m_landscape.m_heightMap->GetValue(pos4.x, pos4.z);

          // glDisable( GL_DEPTH_TEST );
          glShadeModel(GL_SMOOTH);
          glDepthMask(false);
          glBegin(GL_QUADS);
          glColor4f(0.0f, 0.0f, 0.0f, alpha);
          glTexCoord2f(0.0f, 0.0f);
          glVertex3fv(&pos1.x);
          glTexCoord2f(1.0f, 0.0f);
          glVertex3fv(&pos2.x);
          glColor4f(0.0f, 0.0f, 0.0f, 0.1f * alpha);
          glTexCoord2f(1.0f, 1.0f);
          glVertex3fv(&pos3.x);
          glTexCoord2f(0.0f, 1.0f);
          glVertex3fv(&pos4.x);
          glEnd();
          glShadeModel(GL_FLAT);
          glDepthMask(true);
        }
      }
    }
  }


  //
  // Render a santa hat if it's Christmas

  if (m_id.GetTeamId() == 0 && Location::ChristmasModEnabled() == 1)
  {
    if (m_id.GetUniqueId() % 3 == 0)
    {
      int santaHatId = g_resource->GetTexture("Sprites/SantaHat.bmp");
      glBindTexture(GL_TEXTURE_2D, santaHatId);
      glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
      DirectX::XMVECTOR hatPos = DirectX::XMVectorMultiplyAdd(entityUp, DirectX::XMVectorReplicate(0.95f), DirectX::XMLoadFloat3(&predictedPos));
      hatPos = DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_front), DirectX::XMVectorReplicate(0.01f), hatPos);
      DirectX::XMStoreFloat3(&predictedPos, hatPos);

      entityRight = DirectX::XMVectorScale(entityRight, 0.65f);
      entityUp = DirectX::XMVectorScale(entityUp, 0.65f);
      glBegin(GL_QUADS);
      glTexCoord2i(0, 1);
      EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&predictedPos), entityRight), entityUp));
      glTexCoord2i(1, 1);
      EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&predictedPos), entityRight), entityUp));
      glTexCoord2i(1, 0);
      EmitVertex(DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&predictedPos), entityRight));
      glTexCoord2i(0, 0);
      EmitVertex(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&predictedPos), entityRight));
      glEnd();
      glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Sprites/Citizen.bmp"));
    }
  }

  //
  // Render our box kite if we have one

  if (m_boxKiteId.IsValid())
  {
    BoxKite* boxKite = (BoxKite*)g_location->GetEffect(m_boxKiteId);
    if (boxKite)
    {
      boxKite->m_up = entityUp;
      DirectX::XMStoreFloat3(&boxKite->m_up, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&boxKite->m_up)));
      boxKite->m_front = m_front;
      DirectX::XMStoreFloat3(&boxKite->m_front, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&boxKite->m_front)));
      boxKite->m_pos = predictedPos + m_front * 2 + boxKite->m_up * 3;
      boxKite->m_vel = m_vel;
    }
  }


  //
  // If we are dead render us in pieces

  if (m_dead)
  {
    DirectX::XMVECTOR const entityFront = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&m_front));
    entityUp = DirectX::g_XMIdentityR1;
    entityRight = DirectX::XMVector3Cross(entityFront, entityUp);
    entityUp = DirectX::XMVectorScale(entityUp, size * 2.0f);
    entityRight = DirectX::XMVectorScale(DirectX::XMVector3Normalize(entityRight), size);
    unsigned char alpha = (float)m_stats[StatHealth] * 2.55f;

    glColor4ub(0, 0, 0, alpha);

    entityRight = DirectX::XMVectorScale(entityRight, 0.5f);
    entityUp = DirectX::XMVectorScale(entityUp, 0.5f);
    float predictedHealth = m_stats[StatHealth];
    if (m_onGround)
      predictedHealth -= 40 * _predictionTime;
    else
      predictedHealth -= 20 * _predictionTime;
    float landHeight = g_location->m_landscape.m_heightMap->GetValue(predictedPos.x, predictedPos.z);

    for (int i = 0; i < 3; ++i)
    {
      DirectX::XMFLOAT3 fragmentPos = predictedPos;
      if (i == 0)
        fragmentPos.x += 10.0f - predictedHealth / 10.0f;
      if (i == 1)
        fragmentPos.x += 10.0f - predictedHealth / 10.0f;
      if (i == 1)
        fragmentPos.z += 10.0f - predictedHealth / 10.0f;
      if (i == 2)
        fragmentPos.x -= 10.0f - predictedHealth / 10.0f;
      fragmentPos.y += (fragmentPos.y - landHeight) * i * 0.5f;


      float left = 0.0f;
      float right = 1.0f;
      float top = 1.0f;
      float bottom = 0.0f;

      if (i == 0)
      {
        right -= (right - left) / 2;
        bottom -= (bottom - top) / 2;
      }
      if (i == 1)
      {
        left += (right - left) / 2;
        bottom -= (bottom - top) / 2;
      }
      if (i == 2)
      {
        top += (bottom - top) / 2;
        left += (right - left) / 2;
      }

      glBegin(GL_QUADS);
      glTexCoord2f(left, bottom);
      EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&fragmentPos), entityRight), entityUp));
      glTexCoord2f(right, bottom);
      EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&fragmentPos), entityRight), entityUp));
      glTexCoord2f(right, top);
      EmitVertex(DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&fragmentPos), entityRight));
      glTexCoord2f(left, top);
      EmitVertex(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&fragmentPos), entityRight));
      glEnd();
    }
  }
}


void Citizen::ListSoundEvents(std::vector<const char*>* _list)
{
  Entity::ListSoundEvents(_list);

  _list->push_back("SeenThreatAttack");
  _list->push_back("SeenThreatRunAway");
  _list->push_back("TakenControl");
  _list->push_back("EscapedControl");
  _list->push_back("GivenOrders");
  _list->push_back("VictoryJump");
  _list->push_back("OnFire");
}


// ===========================================================================

BoxKite::BoxKite()
  : WorldObject(),
    m_state(StateHeld),
    m_birthTime(0.0f),
    m_deathTime(0.0f)
{
  m_shape = g_resource->GetShape("BoxKite.shp");
  m_birthTime = GetHighResTime();

  m_size = 1.0f + syncsfrand(1.0f);
  m_type = EffectBoxKite;

  m_up = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
}


bool BoxKite::Advance()
{
  if (m_state == StateReleased)
  {
    m_vel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_vel.y = 2.0f + syncfrand(2.0f);

    m_vel.x = sinf(g_gameTime + m_id.GetIndex());
    m_vel.z = sinf(g_gameTime + m_id.GetIndex());

    DirectX::XMStoreFloat3(&m_pos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD),
                                                                DirectX::XMLoadFloat3(&m_pos)));

    float factor1 = SERVER_ADVANCE_PERIOD * 0.1f;
    float factor2 = 1.0f - factor1;
    DirectX::XMStoreFloat3(&m_up, DirectX::XMVectorMultiplyAdd(DirectX::g_XMIdentityR1, DirectX::XMVectorReplicate(factor1),
                                                               DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_up), factor2)));
    m_front.y = m_front.y * factor2;
    DirectX::XMStoreFloat3(&m_front, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&m_front)));

    if (GetHighResTime() > m_deathTime)
    {
      return true;
    }
  }

  m_brightness = 0.5f + syncfrand(0.5f);

  return false;
}


void BoxKite::Release()
{
  m_state = StateReleased;
  m_deathTime = GetHighResTime() + 180.0f;
}


void BoxKite::Render(float _predictionTime)
{
  glDisable(GL_BLEND);
  glDepthMask(true);

  DirectX::XMFLOAT3 predictedPos;
  DirectX::XMStoreFloat3(&predictedPos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(_predictionTime),
                                                                     DirectX::XMLoadFloat3(&m_pos)));

  float scale = (GetHighResTime() - m_birthTime) / 10.0f;
  if (scale > m_size)
    scale = m_size;

  if (m_deathTime != 0.0f && GetHighResTime() > m_deathTime - 20.0f)
  {
    float deathScale = (m_deathTime - GetHighResTime()) / 20.0f;
    if (deathScale < 0.0f)
      deathScale = 0.0f;
    scale = deathScale * m_size;
  }

  g_renderer->SetObjectLighting();
  // The legacy code built the basis and then scaled the three rotation rows,
  // leaving the position row alone -- a uniform scale of the model. Done here
  // on the rows directly, which is the same thing said in the native layout.
  DirectX::XMMATRIX basis = BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::XMLoadFloat3(&m_up), DirectX::XMLoadFloat3(&predictedPos));
  basis.r[0] = DirectX::XMVectorScale(basis.r[0], scale);
  basis.r[1] = DirectX::XMVectorScale(basis.r[1], scale);
  basis.r[2] = DirectX::XMVectorScale(basis.r[2], scale);

  DirectX::XMFLOAT4X4 mat;
  DirectX::XMStoreFloat4x4(&mat, basis);
  m_shape->Render(_predictionTime, mat);
  g_renderer->UnsetObjectLighting();


  //
  // Candle in the middle

  glEnable(GL_BLEND);
  glDepthMask(false);

  // CameraAccess still returns legacy vectors; T12 converts it, behind T22.
  DirectX::XMFLOAT3 const cameraUp = g_camera->GetUp();
  DirectX::XMFLOAT3 const cameraRight = g_camera->GetRight();

  // Not const: the second, smaller quad below scales both down by 0.2.
  DirectX::XMVECTOR camUpScaled = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&cameraUp), 3.0f * scale * m_brightness);
  DirectX::XMVECTOR camRightScaled = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&cameraRight), 3.0f * scale * m_brightness);

  glColor4f(1.0f, 0.75f, 0.2f, m_brightness);

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Starburst.bmp"));

  glBegin(GL_QUADS);
  glTexCoord2i(0, 0);
  EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&predictedPos), camRightScaled), camUpScaled));
  glTexCoord2i(1, 0);
  EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&predictedPos), camRightScaled), camUpScaled));
  glTexCoord2i(1, 1);
  EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&predictedPos), camRightScaled), camUpScaled));
  glTexCoord2i(0, 1);
  EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&predictedPos), camRightScaled), camUpScaled));
  glEnd();

  camUpScaled = DirectX::XMVectorScale(camUpScaled, 0.2f);
  camRightScaled = DirectX::XMVectorScale(camRightScaled, 0.2f);

  glBegin(GL_QUADS);
  glTexCoord2i(0, 0);
  EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&predictedPos), camRightScaled), camUpScaled));
  glTexCoord2i(1, 0);
  EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&predictedPos), camRightScaled), camUpScaled));
  glTexCoord2i(1, 1);
  EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&predictedPos), camRightScaled), camUpScaled));
  glTexCoord2i(0, 1);
  EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&predictedPos), camRightScaled), camUpScaled));
  glEnd();

  glDisable(GL_TEXTURE_2D);
}
