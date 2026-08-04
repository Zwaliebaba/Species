#include "pch.h"
#include "GlVertex.h"
#include "SoundSources.h"

#include <math.h>

#include "MathUtils.h"
#include "Resource.h"
#include "NeuronMath.h"
#include "Shape.h"
#include "Debug.h"
#include "TextRenderer.h"
#include "DebugRender.h"
#include "LanguageTable.h"

#include "Input.h"
#include "InputTypes.h"

#include "Explosion.h"
#include "ProtocolLimits.h"
#include "GlobalWorld.h"
#include "Location.h"
#include "ParticleSystem.h"
#include "ObstructionGrid.h"
#include "GameTime.h"

#include "SoundSystem.h"

#include "Incubator.h"
#include "ControlTower.h"
#include "Engineer.h"
#include "Bridge.h"
#include "ResearchItem.h"
#include "WorldPointers.h"


Engineer::Engineer()
  : Entity(),
    m_hoverHeight(15.0f),
    m_state(StateIdle),
    m_retargetTimer(0.0f),
    m_spiritId(-1),
    m_positionId(-1),
    m_bridgeId(-1)
{
  m_idleRotateRate = syncsfrand(0.5f);

  m_shape = g_resource->GetShape("Engineer.shp");

  DirectX::XMFLOAT4X4 mat;
  DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::g_XMIdentityR2, DirectX::g_XMIdentityR1, DirectX::XMVectorZero()));
  m_centrePos = m_shape->CalculateCentre(mat);
  m_radius = m_shape->CalculateRadius(mat, m_centrePos);
}


void Engineer::Begin()
{
  Entity::Begin();
  m_wayPoint = m_pos;
  m_targetPos = m_wayPoint;
}


void Engineer::SetWaypoint(DirectX::XMFLOAT3 const& _wayPoint)
{
  m_retargetTimer = 0.0f;
  m_wayPoint = _wayPoint;
  m_state = StateToWaypoint;
}


int Engineer::GetNumSpirits() { return static_cast<int>(m_spirits.size()); }


int Engineer::GetMaxSpirits()
{
  int engineerLevel = g_globalWorld->m_research->CurrentLevel(GlobalResearch::TypeEngineer);
  switch (engineerLevel)
  {
  case 0:
    return 0;
  case 1:
    return 10;
  case 2:
    return 15;
  case 3:
    return 25;
  case 4:
    return 30;
  }

  return 0;
}


bool Engineer::SearchForSpirits()
{
  if (static_cast<int>(m_spirits.size()) < GetMaxSpirits())
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

        if (theDist <= ENGINEER_SEARCHRANGE && theDist < closest && (s->m_state == Spirit::StateBirth || s->m_state == Spirit::StateFloating))
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
      return true;
    }
  }

  return false;
}


bool Engineer::SearchForControlTowers()
{
  int buildingIndex = -1;
  float closest = 99999.9f;

  for (int i = 0; i < g_location->m_buildings.Size(); ++i)
  {
    if (g_location->m_buildings.ValidIndex(i))
    {
      Building* building = g_location->m_buildings[i];
      if (building->m_type == Building::TypeControlTower)
      {
        ControlTower* ct = (ControlTower*)building;
        // ControlTower converts in T16, so its out-parameters are still legacy.
        DirectX::XMFLOAT3 pos{0.0f, 0.0f, 0.0f};
        DirectX::XMFLOAT3 front{0.0f, 0.0f, 0.0f};
        if ((ct->m_id.GetTeamId() != m_id.GetTeamId() || ct->m_ownership < 100.0f) && ct->GetAvailablePosition(AsLegacy(pos), AsLegacy(front)) != -1)
        {
          float theDist = DirectX::XMVectorGetX(
            DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&building->m_pos), DirectX::XMLoadFloat3(&m_pos))));
          if (theDist <= ENGINEER_SEARCHRANGE && theDist < closest)
          {
            buildingIndex = i;
            closest = theDist;
          }
        }
      }
    }
  }

  if (buildingIndex != -1)
  {
    Building* building = g_location->m_buildings[buildingIndex];
    m_buildingId = building->m_id.GetUniqueId();
    m_state = StateToControlTower;
    return true;
  }

  return false;
}


bool Engineer::SearchForBridges()
{
  int buildingIndex = -1;
  float closest = 99999.9f;

  for (int i = 0; i < g_location->m_buildings.Size(); ++i)
  {
    if (g_location->m_buildings.ValidIndex(i))
    {
      Building* building = g_location->m_buildings[i];
      if (building->m_type == Building::TypeBridge)
      {
        Bridge* bridge = (Bridge*)building;
        // Bridge converts in T17; same seam.
        DirectX::XMFLOAT3 pos{0.0f, 0.0f, 0.0f};
        DirectX::XMFLOAT3 front{0.0f, 0.0f, 0.0f};
        float theDist = DirectX::XMVectorGetX(
          DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&building->m_pos), DirectX::XMLoadFloat3(&m_pos))));
        if (bridge->GetAvailablePosition(AsLegacy(pos), AsLegacy(front)) && bridge->m_status < 100.0f && theDist <= ENGINEER_SEARCHRANGE &&
            theDist < closest)
        {
          buildingIndex = i;
          closest = theDist;
        }
      }
    }
  }

  if (buildingIndex != -1)
  {
    Building* building = g_location->m_buildings[buildingIndex];
    m_buildingId = building->m_id.GetUniqueId();
    m_state = StateToBridge;
    return true;
  }

  return false;
}


bool Engineer::SearchForResearchItems()
{
  float closest = 99999.9f;
  int buildingIndex = -1;

  for (int i = 0; i < g_location->m_buildings.Size(); ++i)
  {
    if (g_location->m_buildings.ValidIndex(i))
    {
      Building* building = g_location->m_buildings[i];
      if (building->m_type == Building::TypeResearchItem)
      {
        ResearchItem* item = (ResearchItem*)building;
        float theDist = DirectX::XMVectorGetX(
          DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&building->m_pos), DirectX::XMLoadFloat3(&m_pos))));
        if (item->NeedsReprogram() && theDist <= ENGINEER_SEARCHRANGE && theDist < closest)
        {
          buildingIndex = i;
          closest = theDist;
        }
      }
    }
  }

  if (buildingIndex != -1)
  {
    Building* building = g_location->m_buildings[buildingIndex];
    m_buildingId = building->m_id.GetUniqueId();
    // SetLength(35) on a zero-length delta left (35,0,0); native normalise
    // gives zero, so an engineer standing exactly on a building targets the
    // building itself rather than a point 35 units along +x.
    DirectX::XMVECTOR const buildingPos = DirectX::XMLoadFloat3(&building->m_pos);
    DirectX::XMVECTOR const usToThem =
      DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(buildingPos, DirectX::XMLoadFloat3(&m_pos))), 35.0f);
    DirectX::XMStoreFloat3(&m_targetPos, DirectX::XMVectorSubtract(buildingPos, usToThem));
    m_targetPos.y = g_location->m_landscape.m_heightMap->GetValue(m_targetPos.x, m_targetPos.z);
    m_targetPos.y += m_hoverHeight;
    m_state = StateToResearchItem;
    return true;
  }

  return false;
}


void Engineer::ChangeHealth(int amount)
{
  if (!m_dead)
  {
    int healthBandBefore = int(m_stats[StatHealth] / 50.0f);

    if (amount < 0)
    {
      g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "LoseHealth");
    }

    if (m_stats[StatHealth] + amount < 0)
    {
      m_stats[StatHealth] = 0;
      m_dead = true;
      g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "Die");
    }
    else if (m_stats[StatHealth] + amount > 255)
    {
      m_stats[StatHealth] = 255;
    }
    else
    {
      m_stats[StatHealth] += amount;
      g_particleSystem->CreateParticle(m_pos, DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), Particle::TypeMuzzleFlash);
    }

    int healthBandAfter = int(m_stats[StatHealth] / 50.0f);

    float fractionDead = 1.0f - m_stats[StatHealth] / (float)EntityBlueprint::GetStat(TypeEngineer, StatHealth);
    fractionDead = std::max(fractionDead, 0.0f);
    fractionDead = std::min(fractionDead, 1.0f);

    if (fractionDead == 1.0f || healthBandAfter < healthBandBefore)
    {
      DirectX::XMFLOAT4X4 transform;
      DirectX::XMStoreFloat4x4(&transform,
                               BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));
      g_explosionManager.AddExplosion(m_shape, transform, fractionDead);
    }
  }
}


bool Engineer::Advance(Unit* _unit)
{
  bool amIDead = false;

  switch (m_state)
  {
  case StateIdle:
    amIDead = AdvanceIdle();
    break;
  case StateToWaypoint:
    amIDead = AdvanceToWaypoint();
    break;
  case StateToSpirit:
    amIDead = AdvanceToSpirit();
    break;
  case StateToIncubator:
    amIDead = AdvanceToIncubator();
    break;
  case StateToControlTower:
    amIDead = AdvanceToControlTower();
    break;
  case StateReprogramming:
    amIDead = AdvanceReprogramming();
    break;
  case StateToBridge:
    amIDead = AdvanceToBridge();
    break;
  case StateOperatingBridge:
    amIDead = AdvanceOperatingBridge();
    break;
  case StateToResearchItem:
    amIDead = AdvanceToResearchItem();
    break;
  case StateResearching:
    amIDead = AdvanceResearching();
    break;
  };

  //
  // If I am now dead, handle it accordingly

  amIDead = Entity::Advance(_unit);
  if (amIDead)
  {
    // Drop all my spirits
    while (static_cast<int>(m_spirits.size()) > 0)
    {
      int spiritId = m_spirits[0];
      if (g_location->m_spirits.ValidIndex(spiritId))
      {
        Spirit* s = g_location->m_spirits.GetPointer(spiritId);
        s->CollectorDrops();
        m_spirits.erase(m_spirits.begin() + 0);
      }
    }

    // If I was reprogramming something, stop now
    if (m_state == StateReprogramming)
    {
      Building* building = g_location->GetBuilding(m_buildingId);
      ControlTower* ct = (ControlTower*)building;
      if (ct)
      {
        ct->EndReprogram(m_positionId);
        g_soundSystem->StopAllSounds(m_id, "Engineer BeginReprogramming");
        g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "EndReprogramming");
      }
    }

    // If I was researching something, stop now
    if (m_state == StateResearching)
    {
      g_soundSystem->StopAllSounds(m_id, "Engineer BeginReprogramming");
      g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "EndReprogramming");
    }

    // If I was operating a bridge, stop now
    if (m_state == StateOperatingBridge)
    {
      Bridge* bridge = (Bridge*)g_location->GetBuilding(m_buildingId);
      if (bridge && bridge->m_type == Building::TypeBridge)
      {
        bridge->EndOperation();
        if (m_bridgeId == m_buildingId)
          EndBridge();
      }
    }
  }

  //
  // Update position history

  m_positionHistory.insert(m_positionHistory.begin(), new DirectX::XMFLOAT3(m_pos));
  int maxSpirits = GetMaxSpirits();
  while (ValidIndex(m_positionHistory, maxSpirits + 1))
  {
    DirectX::XMFLOAT3* position = m_positionHistory[maxSpirits + 1];
    m_positionHistory.erase(m_positionHistory.begin() + maxSpirits + 1);
    delete position;
  }


  //
  // Update spirits

  for (int i = 0; i < static_cast<int>(m_spirits.size()); ++i)
  {
    int spiritId = m_spirits[i];
    if (g_location->m_spirits.ValidIndex(spiritId))
    {
      Spirit* s = g_location->m_spirits.GetPointer(spiritId);
      if (s && s->m_state == Spirit::StateAttached)
      {
        if (ValidIndex(m_positionHistory, i + 1))
        {
          s->m_pos = *m_positionHistory[i + 1];
          DirectX::XMStoreFloat3(&s->m_vel, DirectX::XMVectorScale(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(m_positionHistory[i]),
                                                                                             DirectX::XMLoadFloat3(m_positionHistory[i + 1])),
                                                                   1.0f / SERVER_ADVANCE_PERIOD));
        }
      }
    }
  }


  //
  // Look to see if we've been pulled away by the player

  Building* building = g_location->GetBuilding(m_buildingId);
  if (building && building->m_type == Building::TypeControlTower && m_positionId != -1 && m_state != StateReprogramming)
  {
    // We've been moved away from reprogramming
    // (eg user specified waypoint)
    ControlTower* ct = (ControlTower*)building;
    ct->EndReprogram(m_positionId);
    m_positionId = -1;
    g_soundSystem->StopAllSounds(m_id, "Engineer BeginReprogramming");
    g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "EndReprogramming");
  }

  if (building && building->m_type == Building::TypeBridge && m_state != StateOperatingBridge)
  {
    // We've been moved away from building bridges
    Bridge* bridge = (Bridge*)building;
    bridge->EndOperation();
  }

  if (building && building->m_type == Building::TypeResearchItem && m_state != StateResearching)
  {
    // We've been moved away from researching a research item
    g_soundSystem->StopAllSounds(m_id, "Engineer BeginReprogramming");
    // g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "EndReprogramming" );
  }


  //
  // If we own a bridge, check to make sure we are still in range of it

  Bridge* bridge = (Bridge*)g_location->GetBuilding(m_bridgeId);
  if (bridge && bridge->m_type == Building::TypeBridge)
  {
    float range = DirectX::XMVectorGetX(
      DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&bridge->m_pos), DirectX::XMLoadFloat3(&m_pos))));
    if (range > 100.0f)
    {
      EndBridge();
    }
  }

  return amIDead;
}


bool Engineer::AdvanceToTargetPos()
{
  DirectX::XMVECTOR const toTarget = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_targetPos), DirectX::XMLoadFloat3(&m_pos));
  DirectX::XMVECTOR const distance = DirectX::XMVectorSetY(toTarget, 0.0f);
  float const distanceLength = DirectX::XMVectorGetX(DirectX::XMVector3Length(distance));

  if (distanceLength > 5.0f)
  {
    DirectX::XMFLOAT3 const oldPos = m_pos;

    float amountToTurn = SERVER_ADVANCE_PERIOD * 3.0f;

    // targetDir is normalised from the UNFLATTENED delta -- the y component
    // was dropped for the distance test only, and reproducing that split
    // matters because the engineer hovers.
    DirectX::XMVECTOR const targetDir = DirectX::XMVector3Normalize(toTarget);
    DirectX::XMVECTOR const actualDir = DirectX::XMVector3Normalize(DirectX::XMVectorMultiplyAdd(
      targetDir, DirectX::XMVectorReplicate(amountToTurn), DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_front), 1.0f - amountToTurn)));

    float desiredSpeed = distanceLength;
    desiredSpeed = std::min(desiredSpeed, static_cast<float>(m_stats[StatSpeed]));
    if (m_state == StateIdle)
      desiredSpeed *= 0.25f;

    DirectX::XMFLOAT3 newPos;
    DirectX::XMStoreFloat3(&newPos, DirectX::XMVectorMultiplyAdd(actualDir, DirectX::XMVectorReplicate(desiredSpeed * SERVER_ADVANCE_PERIOD),
                                                                 DirectX::XMLoadFloat3(&m_pos)));
    newPos = PushFromObstructions(newPos);

    DirectX::XMVECTOR moved = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&newPos), DirectX::XMLoadFloat3(&oldPos));
    if (DirectX::XMVectorGetX(DirectX::XMVector3Length(moved)) > desiredSpeed * SERVER_ADVANCE_PERIOD)
    {
      moved = DirectX::XMVectorScale(DirectX::XMVector3Normalize(moved), desiredSpeed * SERVER_ADVANCE_PERIOD);
    }
    DirectX::XMStoreFloat3(&m_pos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), moved));

    float targetHeight = std::max(g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z), 0.0f) + m_hoverHeight;
    m_pos.y = targetHeight;

    DirectX::XMVECTOR const velocity =
      DirectX::XMVectorScale(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&oldPos)), 1.0f / SERVER_ADVANCE_PERIOD);
    DirectX::XMStoreFloat3(&m_vel, velocity);
    DirectX::XMStoreFloat3(&m_front, DirectX::XMVector3Normalize(velocity));

    return false;
  }
  else
  {
    m_vel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
    // if( m_targetFront != g_zeroVector ) m_front = m_targetFront;
    return true;
  }
}


bool Engineer::SearchForRandomPosition()
{
  float distance = 30.0f;
  float angle = syncsfrand(2.0f * M_PI);

  DirectX::XMStoreFloat3(&m_targetPos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos),
                                                            DirectX::XMVectorSet(sinf(angle) * distance, 0.0f, cosf(angle) * distance, 0.0f)));

  return true;
}


bool Engineer::AdvanceIdle()
{
  m_retargetTimer -= SERVER_ADVANCE_PERIOD;
  if (m_retargetTimer <= 0.0f)
  {
    bool foundNewTarget = false;

    if (!foundNewTarget)
      foundNewTarget = SearchForResearchItems();
    if (!foundNewTarget)
      foundNewTarget = SearchForControlTowers();
    if (!foundNewTarget)
      foundNewTarget = SearchForBridges();
    if (!foundNewTarget)
      foundNewTarget = SearchForSpirits();

    if (foundNewTarget)
      return false;
    m_retargetTimer = ENGINEER_RETARGETTIMER;
  }

  //
  // Nothing to do...do we have some spirits that need dropping off?

  if (static_cast<int>(m_spirits.size()) > 0)
  {
    m_buildingId = -1; // Force the engineer to re-evaluate which is the nearest incubator
    bool incubatorFound = SearchForIncubator();
    if (incubatorFound)
    {
      m_state = StateToIncubator;
      return false;
    }
  }


  //
  // Just go into a holding pattern

  bool arrived = AdvanceToTargetPos();
  if (arrived)
  {
    SearchForRandomPosition();
  }

  return false;
}


bool Engineer::AdvanceToWaypoint()
{
  m_targetPos = m_wayPoint;
  m_targetFront = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

  bool arrived = AdvanceToTargetPos();
  if (arrived)
    m_state = StateIdle;
  return false;
}


bool Engineer::AdvanceToSpirit()
{
  Spirit* s = nullptr;
  if (g_location->m_spirits.ValidIndex(m_spiritId))
  {
    s = g_location->m_spirits.GetPointer(m_spiritId);
  }

  if (!s || s->m_state == Spirit::StateDeath || s->m_state == Spirit::StateAttached)
  {
    // Our spirit died while we were going for it, return to waypoint and continue looking
    m_spiritId = -1;
    m_state = StateToWaypoint;
    return false;
  }

  m_targetPos = s->m_pos;
  m_targetFront = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
  bool arrived = AdvanceToTargetPos();
  if (arrived)
  {
    CollectSpirit(m_spiritId);
    m_spiritId = -1;
    m_state = StateIdle;
  }

  return false;
}


void Engineer::CollectSpirit(int _spiritId)
{
  if (g_location->m_spirits.ValidIndex(_spiritId))
  {
    Spirit* spirit = g_location->m_spirits.GetPointer(_spiritId);

    spirit->CollectorArrives();
    m_spirits.push_back(_spiritId);
  }
}


bool Engineer::SearchForIncubator()
{
  //
  // Source building lost, look for another incubator to bind to
  float nearest = 99999.9f;
  bool found = false;

  for (int i = 0; i < g_location->m_buildings.Size(); ++i)
  {
    if (g_location->m_buildings.ValidIndex(i))
    {
      Building* building = g_location->m_buildings[i];
      if (building->m_type == Building::TypeIncubator && g_location->IsFriend(building->m_id.GetTeamId(), m_id.GetTeamId()))
      {
        float distance = DirectX::XMVectorGetX(
          DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&building->m_pos), DirectX::XMLoadFloat3(&m_pos))));
        int population = ((Incubator*)building)->NumSpiritsInside();
        distance += population * 10;

        if (distance < nearest)
        {
          m_buildingId = building->m_id.GetUniqueId();
          nearest = distance;
          found = true;
        }
      }
    }
  }

  return found;
}


bool Engineer::AdvanceToIncubator()
{
  Incubator* incubator = (Incubator*)g_location->GetBuilding(m_buildingId);

  if (!incubator)
  {
    bool found = SearchForIncubator();
    incubator = (Incubator*)g_location->GetBuilding(m_buildingId);
    if (!incubator)
    {
      // We can't find a friendly incubator, so go into holding pattern
      m_state = StateIdle;
      return false;
    }
  }


  // Incubator converts in T17; its out-parameters are still legacy.
  incubator->GetDockPoint(AsLegacy(m_targetPos), AsLegacy(m_targetFront));
  bool arrived = AdvanceToTargetPos();
  if (arrived)
  {
    m_front = m_targetFront;

    // Arrived at our incubator, drop spirit off here one at a time
    int spiritId = m_spirits[0];
    if (g_location->m_spirits.ValidIndex(spiritId))
    {
      Spirit* s = g_location->m_spirits.GetPointer(spiritId);
      incubator->AddSpirit(s);
      g_location->m_spirits.MarkNotUsed(spiritId);
      m_spirits.erase(m_spirits.begin() + 0);
    }

    if (static_cast<int>(m_spirits.size()) == 0)
    {
      // Return to spirit field
      m_state = StateToWaypoint;
    }
  }

  return false;
}


bool Engineer::AdvanceToControlTower()
{
  Building* building = g_location->GetBuilding(m_buildingId);
  if (!building)
  {
    m_state = StateIdle;
    return false;
  }

  DEBUG_ASSERT(building->m_type == Building::TypeControlTower);

  ControlTower* ct = (ControlTower*)building;
  // ControlTower converts in T16; same seam.
  int positionId = ct->GetAvailablePosition(AsLegacy(m_targetPos), AsLegacy(m_targetFront));

  if ((ct->m_id.GetTeamId() == m_id.GetTeamId() && ct->m_ownership >= 100.0f) || positionId == -1)
  {
    // Our building is gone or is already captured or is full
    m_buildingId = -1;
    m_positionId = -1;
    m_state = StateIdle;
    return false;
  }

  if (positionId != -1)
  {
    bool arrived = AdvanceToTargetPos();
    if (arrived)
    {
      m_positionId = positionId;
      ct->BeginReprogram(m_positionId);
      g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "BeginReprogramming");
      m_state = StateReprogramming;
    }
  }
  else
  {
    m_state = StateIdle;
  }

  return false;
}


bool Engineer::AdvanceResearching()
{
  //
  // Make sure our research item is still available

  ResearchItem* item = (ResearchItem*)g_location->GetBuilding(m_buildingId);
  if (!item || item->m_type != Building::TypeResearchItem || !item->NeedsReprogram())
  {
    g_soundSystem->StopAllSounds(m_id, "Engineer BeginReprogramming");

    m_buildingId = -1;
    m_state = StateIdle;
    return false;
  }


  //
  // Do some reprogramming

  bool amIDone = item->Reprogram();

  if (amIDone)
  {
    g_soundSystem->StopAllSounds(m_id, "Engineer BeginReprogramming");
    g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "ReprogrammingComplete");
    m_buildingId = -1;
    m_state = StateIdle;
    return false;
  }


  //
  // Spark

  // ResearchItem converts in T16, so its out-parameters are still legacy.
  DirectX::XMFLOAT3 end1{0.0f, 0.0f, 0.0f};
  DirectX::XMFLOAT3 end2{0.0f, 0.0f, 0.0f};
  item->GetEndPositions(AsLegacy(end1), AsLegacy(end2));

  float time = g_gameTime + m_id.GetIndex();
  DirectX::XMVECTOR const span = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&end2), DirectX::XMLoadFloat3(&end1));

  DirectX::XMFLOAT3 toPos;
  DirectX::XMStoreFloat3(
    &toPos, DirectX::XMVectorMultiplyAdd(span, DirectX::XMVectorReplicate(sinf(time) * 0.5f),
                                         DirectX::XMVectorMultiplyAdd(span, DirectX::XMVectorReplicate(0.5f), DirectX::XMLoadFloat3(&end1))));

  for (int i = 0; i < 2; ++i)
  {
    // Three unsynchronised draws per iteration, in order and count unchanged.
    // frand/sfrand are the RENDER stream, not speciesRandom's.
    float const scatterX = sfrand() * 15.0f;
    float const scatterY = frand() * 10.0f;
    float const scatterZ = sfrand() * 15.0f;

    DirectX::XMFLOAT3 particleVel;
    DirectX::XMStoreFloat3(&particleVel, DirectX::XMVectorAdd(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&toPos)),
                                                              DirectX::XMVectorSet(scatterX, scatterY, scatterZ, 0.0f)));
    g_particleSystem->CreateParticle(toPos, particleVel, Particle::TypeBlueSpark);
  }


  //
  // Make us float around a bit while we work

  DirectX::XMStoreFloat3(&m_targetFront,
                         DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&toPos), DirectX::XMLoadFloat3(&m_pos))));
  DirectX::XMStoreFloat3(&m_front,
                         DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_front), DirectX::XMVectorReplicate(1.0f - SERVER_ADVANCE_PERIOD),
                                                      DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_targetFront), SERVER_ADVANCE_PERIOD)));

  DirectX::XMFLOAT3 const oldPos = m_pos;

  DirectX::XMVECTOR const targetFrontAxis = DirectX::XMLoadFloat3(&m_targetFront);
  DirectX::XMVECTOR const rightAngle = DirectX::XMVector3Cross(targetFrontAxis, DirectX::g_XMIdentityR1);
  float scale = 2.0f;

  DirectX::XMVECTOR targetPos = DirectX::XMLoadFloat3(&m_targetPos);
  targetPos =
    DirectX::XMVectorMultiplyAdd(targetFrontAxis, DirectX::XMVectorReplicate(sinf(g_gameTime + m_id.GetUniqueId() * 10.0f) * scale), targetPos);
  targetPos =
    DirectX::XMVectorMultiplyAdd(rightAngle, DirectX::XMVectorReplicate(cosf(g_gameTime + m_id.GetUniqueId() * 10.0f) * scale * 1.5f), targetPos);

  DirectX::XMStoreFloat3(&m_pos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMVectorReplicate(1.0f - SERVER_ADVANCE_PERIOD),
                                                              DirectX::XMVectorScale(targetPos, SERVER_ADVANCE_PERIOD)));
  DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&oldPos)),
                                                        1.0f / SERVER_ADVANCE_PERIOD));

  return false;
}


bool Engineer::AdvanceReprogramming()
{
  Building* building = g_location->GetBuilding(m_buildingId);

  if (!building)
  {
    m_state = StateIdle;
    g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "EndReprogramming");
    return false;
  }

  ControlTower* ct = (ControlTower*)building;

  for (int i = 0; i < 10; ++i)
  {
    bool finished = ct->Reprogram(m_id.GetTeamId());
    if (finished)
    {
      g_soundSystem->StopAllSounds(m_id, "Engineer BeginReprogramming");
      g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "ReprogrammingComplete");
      ct->EndReprogram(m_positionId);
      m_buildingId = -1;
      m_positionId = -1;
      m_state = StateIdle;
      return false;
    }
  }


  //
  // Make us float around a bit while we work

  // ControlTower converts in T16, so this out-parameter is still legacy.
  DirectX::XMFLOAT3 consolePos{0.0f, 0.0f, 0.0f};
  ct->GetConsolePosition(m_positionId, AsLegacy(consolePos));

  DirectX::XMVECTOR const targetFront =
    DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&consolePos), DirectX::XMLoadFloat3(&m_pos)));
  DirectX::XMStoreFloat3(&m_front,
                         DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_front), DirectX::XMVectorReplicate(1.0f - SERVER_ADVANCE_PERIOD),
                                                      DirectX::XMVectorScale(targetFront, SERVER_ADVANCE_PERIOD)));

  DirectX::XMFLOAT3 const oldPos = m_pos;

  DirectX::XMVECTOR const targetFrontAxis = DirectX::XMLoadFloat3(&m_targetFront);
  DirectX::XMVECTOR const rightAngle = DirectX::XMVector3Cross(targetFrontAxis, DirectX::g_XMIdentityR1);
  float scale = 2.0f;

  DirectX::XMVECTOR targetPos = DirectX::XMLoadFloat3(&m_targetPos);
  targetPos =
    DirectX::XMVectorMultiplyAdd(targetFrontAxis, DirectX::XMVectorReplicate(sinf(g_gameTime + m_id.GetUniqueId() * 10.0f) * scale), targetPos);
  targetPos =
    DirectX::XMVectorMultiplyAdd(rightAngle, DirectX::XMVectorReplicate(cosf(g_gameTime + m_id.GetUniqueId() * 10.0f) * scale * 1.5f), targetPos);

  DirectX::XMStoreFloat3(&m_pos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMVectorReplicate(1.0f - SERVER_ADVANCE_PERIOD),
                                                              DirectX::XMVectorScale(targetPos, SERVER_ADVANCE_PERIOD)));
  DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&oldPos)),
                                                        1.0f / SERVER_ADVANCE_PERIOD));

  return false;
}


void Engineer::BeginBridge(DirectX::XMFLOAT3 _to)
{
  int engineerLevel = g_globalWorld->m_research->CurrentLevel(GlobalResearch::TypeEngineer);
  if (engineerLevel < 5)
    return;

  //
  // Shut down any existing bridges

  EndBridge();


  //
  // Calculate the size of our bridge

  DirectX::XMVECTOR const bridgeSpan = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&_to), DirectX::XMLoadFloat3(&m_wayPoint));
  int numComponents = int(DirectX::XMVectorGetX(DirectX::XMVector3Length(bridgeSpan)) / 80.0f);
  DirectX::XMVECTOR const componentSpan = DirectX::XMVectorScale(bridgeSpan, 1.0f / (float)numComponents);

  //
  // Create bridge towers

  int linkBuildingId = -1;

  for (int i = numComponents; i >= 0; --i)
  {
    Bridge* component = (Bridge*)Building::CreateBuilding(Building::TypeBridge);
    g_location->m_buildings.PutData(component);
    component->m_id.SetUniqueId(g_globalWorld->GenerateBuildingId());
    component->m_nextBridgeId = linkBuildingId;
    // Both draws stay, in order, one pair per component.
    float const jitterX = syncsfrand(15.0f);
    float const jitterZ = syncsfrand(15.0f);

    DirectX::XMStoreFloat3(&component->m_pos, DirectX::XMVectorAdd(DirectX::XMVectorMultiplyAdd(componentSpan, DirectX::XMVectorReplicate((float)i),
                                                                                                DirectX::XMLoadFloat3(&m_wayPoint)),
                                                                   DirectX::XMVectorSet(jitterX, 0.0f, jitterZ, 0.0f)));
    component->m_pos.y = g_location->m_landscape.m_heightMap->GetValue(component->m_pos.x, component->m_pos.z);

    // The legacy chain was: front = normalise(_to - m_wayPoint), then a
    // quarter turn for the middle components or a half turn for the far end,
    // then right = front x up and front = right x up. Kept whole -- the
    // earlier conversion split the if/else and left an orphaned `else`.
    DirectX::XMVECTOR bridgeFront = DirectX::XMVector3Normalize(bridgeSpan);
    if (i < numComponents && i > 0)
    {
      bridgeFront = DirectX::XMVector3Transform(bridgeFront, DirectX::XMMatrixRotationY(0.5f * M_PI));
    }
    else if (i == numComponents)
    {
      bridgeFront = DirectX::XMVector3Transform(bridgeFront, DirectX::XMMatrixRotationY(M_PI));
    }

    component->m_id.SetTeamId(m_id.GetTeamId());
    linkBuildingId = component->m_id.GetUniqueId();

    DirectX::XMVECTOR const right = DirectX::XMVector3Cross(bridgeFront, DirectX::g_XMIdentityR1);
    DirectX::XMStoreFloat3(&component->m_front, DirectX::XMVector3Cross(right, DirectX::g_XMIdentityR1));

    if (i == numComponents || i == 0)
    {
      component->SetBridgeType(Bridge::BridgeTypeEnd);
    }
    else
    {
      component->SetBridgeType(Bridge::BridgeTypeTower);
    }

    if (i == 0)
      m_bridgeId = component->m_id.GetUniqueId();
  }

  g_location->m_obstructionGrid->CalculateAll();
}


void Engineer::EndBridge()
{
  Bridge* bridge = (Bridge*)g_location->GetBuilding(m_bridgeId);

  while (bridge)
  {
    bridge->m_status = -1.0f;
    int nextBuildingId = bridge->m_nextBridgeId;
    bridge = (Bridge*)g_location->GetBuilding(nextBuildingId);
  }

  m_bridgeId = -1;
}


bool Engineer::AdvanceToBridge()
{
  Building* building = g_location->GetBuilding(m_buildingId);
  if (building && building->m_type == Building::TypeBridge)
  {
    Bridge* bridge = (Bridge*)building;
    // Bridge converts in T17; same seam.
    bool positionAvailable = bridge->GetAvailablePosition(AsLegacy(m_targetPos), AsLegacy(m_targetFront));
    if (!positionAvailable)
    {
      m_state = StateIdle;
      m_buildingId = -1;
    }
    else
    {
      bool arrived = AdvanceToTargetPos();
      if (arrived)
      {
        m_state = StateOperatingBridge;
        m_vel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
        bridge->BeginOperation();
      }
    }
  }
  else
  {
    m_state = StateIdle;
  }

  return false;
}


bool Engineer::AdvanceOperatingBridge()
{
  Building* building = g_location->GetBuilding(m_buildingId);
  if (building && building->m_type == Building::TypeBridge)
  {
    Bridge* bridge = (Bridge*)building;
    // Nothing to do really -- `front` was declared and never used.
  }
  else
  {
    m_buildingId = -1;
    m_state = StateIdle;
  }

  return false;
}


bool Engineer::AdvanceToResearchItem()
{
  //
  // Make sure our research item is still available

  ResearchItem* item = (ResearchItem*)g_location->GetBuilding(m_buildingId);
  if (!item || !item->NeedsReprogram())
  {
    m_buildingId = -1;
    m_state = StateIdle;
    return false;
  }


  bool arrived = AdvanceToTargetPos();
  if (arrived)
  {
    g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "BeginReprogramming");
    m_state = StateResearching;
  }

  return false;
}

void Engineer::RenderShape(float predictionTime)
{
  DirectX::XMFLOAT3 predictedPos;
  DirectX::XMStoreFloat3(&predictedPos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(predictionTime),
                                                                     DirectX::XMLoadFloat3(&m_pos)));
  if (m_onGround)
  {
    predictedPos.y = std::max(g_location->m_landscape.m_heightMap->GetValue(predictedPos.x, predictedPos.z), 0.0f /*sea level*/) + m_hoverHeight;
  }

  DirectX::XMFLOAT3 flattenedFront = m_front;
  flattenedFront.y *= 0.5f;

  DirectX::XMVECTOR entityFront = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&flattenedFront));
  DirectX::XMVECTOR const entityRight = DirectX::XMVector3Cross(entityFront, DirectX::g_XMIdentityR1);
  DirectX::XMVECTOR const entityUp = DirectX::XMVector3Cross(entityRight, entityFront);

  g_renderer->SetObjectLighting();

  glEnable(GL_CULL_FACE);
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_COLOR_MATERIAL);
  glDisable(GL_BLEND);

  if (DirectX::XMVectorGetY(entityFront) > 0.5f)
  {
    entityFront = DirectX::g_XMIdentityR0;
  }

  DirectX::XMFLOAT4X4 mat;
  DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(entityFront, entityUp, DirectX::XMLoadFloat3(&predictedPos)));
  m_shape->Render(predictionTime, mat);

  glEnable(GL_BLEND);
  glDisable(GL_COLOR_MATERIAL);
  glEnable(GL_TEXTURE_2D);
  g_renderer->UnsetObjectLighting();
  glEnable(GL_CULL_FACE);

  g_renderer->MarkUsedCells(m_shape, mat);
}


void Engineer::Render(float predictionTime)
{
  //
  // Work out our predicted position and orientation

  DirectX::XMFLOAT3 predictedPos;
  DirectX::XMStoreFloat3(&predictedPos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(predictionTime),
                                                                     DirectX::XMLoadFloat3(&m_pos)));
  if (m_onGround)
  {
    predictedPos.y = std::max(g_location->m_landscape.m_heightMap->GetValue(predictedPos.x, predictedPos.z), 0.0f /*sea level*/) + m_hoverHeight;
  }


  if (!m_dead)
  {
    RenderShape(predictionTime);

    BeginRenderShadow();
    RenderShadow(predictedPos, 10.0f);
    EndRenderShadow();
  }

  //
  // If we are reprogramming, draw some control lasers

  if (m_state == StateReprogramming || m_state == StateOperatingBridge || m_state == StateResearching)
  {
    DirectX::XMFLOAT3 const fromPos = m_pos;
    DirectX::XMFLOAT3 toPos{0.0f, 0.0f, 0.0f};

    Building* building = g_location->GetBuilding(m_buildingId);
    if (building)
    {
      if (building->m_type == Building::TypeControlTower)
      {
        ControlTower* tower = (ControlTower*)building;
        // ControlTower converts in T16; its out-parameter is still legacy.
        tower->GetConsolePosition(m_positionId, AsLegacy(toPos));
      }
      else if (building->m_type == Building::TypeBridge)
      {
        // Bridge converts in T17; same seam.
        DirectX::XMFLOAT3 front{0.0f, 0.0f, 0.0f};
        Bridge* bridge = (Bridge*)building;
        bridge->GetAvailablePosition(AsLegacy(toPos), AsLegacy(front));
      }
      else if (building->m_type == Building::TypeResearchItem)
      {
        ResearchItem* item = (ResearchItem*)building;
        // ResearchItem converts in T16; same seam.
        DirectX::XMFLOAT3 end1{0.0f, 0.0f, 0.0f};
        DirectX::XMFLOAT3 end2{0.0f, 0.0f, 0.0f};
        item->GetEndPositions(AsLegacy(end1), AsLegacy(end2));

        float time = g_gameTime + m_id.GetIndex();
        DirectX::XMVECTOR const span = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&end2), DirectX::XMLoadFloat3(&end1));
        DirectX::XMStoreFloat3(
          &toPos, DirectX::XMVectorMultiplyAdd(span, DirectX::XMVectorReplicate(sinf(time) * 0.5f),
                                               DirectX::XMVectorMultiplyAdd(span, DirectX::XMVectorReplicate(0.5f), DirectX::XMLoadFloat3(&end1))));
      }


      DirectX::XMVECTOR const from = DirectX::XMLoadFloat3(&fromPos);
      DirectX::XMVECTOR const to = DirectX::XMLoadFloat3(&toPos);
      DirectX::XMVECTOR const midPoint = DirectX::XMVectorScale(DirectX::XMVectorAdd(from, to), 0.5f);

      // CameraAccess still returns a legacy vector; T12 converts it, behind T22.
      DirectX::XMFLOAT3 const cameraPos = g_camera->GetPos();
      DirectX::XMVECTOR const camToMidPoint = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&cameraPos), midPoint);

      DirectX::XMVECTOR const rightAngle =
        DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMVector3Cross(camToMidPoint, DirectX::XMVectorSubtract(midPoint, to))), 0.5f);

      glColor4f(0.2f, 0.4f, 1.0f, fabs(sinf(g_gameTime * 3.0f)));

      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE);
      glDepthMask(false);
      glEnable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Laser.bmp"));

      glBegin(GL_QUADS);
      glTexCoord2i(0, 0);
      EmitVertex(DirectX::XMVectorSubtract(from, rightAngle));
      glTexCoord2i(0, 1);
      EmitVertex(DirectX::XMVectorAdd(from, rightAngle));
      glTexCoord2i(1, 1);
      EmitVertex(DirectX::XMVectorAdd(to, rightAngle));
      glTexCoord2i(1, 0);
      EmitVertex(DirectX::XMVectorSubtract(to, rightAngle));
      glEnd();

      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDisable(GL_TEXTURE_2D);
      glDepthMask(true);
      glDisable(GL_BLEND);
    }
  }
}


bool Engineer::RenderPixelEffect(float predictionTime)
{
  RenderShape(predictionTime);

  return true;
}


char* Engineer::GetCurrentAction()
{
  switch (m_state)
  {
  case StateIdle:
    return LANGUAGEPHRASE("engineer_idle");
  case StateToWaypoint:
    return LANGUAGEPHRASE("engineer_towaypoint");
  case StateToSpirit:
    return LANGUAGEPHRASE("engineer_tospirit");
  case StateToIncubator:
    return LANGUAGEPHRASE("engineer_toincubator");

  case StateToControlTower:
  case StateReprogramming:
    return LANGUAGEPHRASE("engineer_reprogramming");

  case StateToResearchItem:
  case StateResearching:
    return LANGUAGEPHRASE("engineer_researching");

  case StateToBridge:
  case StateOperatingBridge:
    return LANGUAGEPHRASE("engineer_bridge");
  }

  return LANGUAGEPHRASE("engineer_idle");
}


void Engineer::ListSoundEvents(std::vector<const char*>* _list)
{
  Entity::ListSoundEvents(_list);

  _list->push_back("BeginReprogramming");
  _list->push_back("EndReprogramming");
  _list->push_back("ReprogrammingComplete");
}
