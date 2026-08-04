#include "pch.h"
#include "SoundSources.h"
#include "HiResTime.h"

#include "limits.h"

#include "Bitmap.h"
#include "Debug.h"
#include "MathUtils.h"
#include "Resource.h"
#include "Shape.h"
#include "DebugRender.h"
#include "TextRenderer.h"
#include "RgbColour.h"

#include "Input.h"
#include "InputTypes.h"

#include "Explosion.h"
#include "Location.h"
#include "Team.h"
#include "TaskManager.h"
#include "RoutingSystem.h"
#include "ParticleSystem.h"
#include "EntityGrid.h"
#include "ObstructionGrid.h"
#include "GameTime.h"

#include "GlobalWorld.h"

#include "SoundSystem.h"

#include "InsertionSquad.h"
#include "Teleport.h"
#include "WorldPointers.h"


unsigned int HistoricWayPoint::s_lastId = 0;


//*****************************************************************************
// Class InsertionSquad
//*****************************************************************************

InsertionSquad::InsertionSquad(int teamId, int unitId, int numEntities, DirectX::XMFLOAT3 const& _pos)
  : Unit(Entity::TypeInsertionSquadie, teamId, unitId, numEntities, _pos),
    m_weaponType(GlobalResearch::TypeGrenade),
    m_controllerId(-1),
    m_teleportId(-1)
{
  SetWayPoint(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));
  SetWayPoint(_pos);
}


InsertionSquad::~InsertionSquad() { EmptyAndDelete(m_positionHistory); }


Entity* InsertionSquad::GetPointMan()
{
  for (int i = 0; i < m_entities.Size(); ++i)
  {
    if (m_entities.ValidIndex(i))
    {
      Entity* entity = m_entities.GetData(i);
      if (entity->m_formationIndex == 0)
      {
        return entity;
      }
    }
  }

  return nullptr;
}

/*Entity *InsertionSquad::GetFirstValidUnit()
{
  for (int i = 0; i < m_entities.Size(); ++i)
  {
    if (m_entities.ValidIndex(i))
    {
      Entity *entity = m_entities.GetData(i);
      if ( !entity->m_dead &&
         entity->m_enabled )
      {
        return entity;
      }
    }
  }
  return nullptr;
}
*/

void InsertionSquad::SetWayPoint(DirectX::XMFLOAT3 const& _pos)
{
  m_wayPoint = _pos;

  Entity* pointMan = GetPointMan();

  // If we found the point man add his position to the position history
  // otherwise add the position that was passed in as an argument
  HistoricWayPoint* newWayPoint;
  if (pointMan && pointMan->m_enabled)
  {
    newWayPoint = new HistoricWayPoint(pointMan->m_pos);
  }
  else
  {
    newWayPoint = new HistoricWayPoint(_pos);
  }
  m_positionHistory.insert(m_positionHistory.begin(), newWayPoint);


  // If this squad is using a Controller, update the Route
  // that the Controller points to
  Task* controller = g_taskManager->GetTask(m_controllerId);
  if (controller)
  {
    // LevelFile's WayPoint converts in T18, so GetPos is still legacy.
    DirectX::XMFLOAT3 const lastAddedPos = controller->m_route->m_wayPoints[static_cast<int>(controller->m_route->m_wayPoints.size()) - 1]->GetPos();
    float distance = DirectX::XMVectorGetX(
      DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&lastAddedPos), DirectX::XMLoadFloat3(&newWayPoint->m_pos))));
    if (distance > 20.0f)
    {
      controller->m_route->AddWayPoint(newWayPoint->m_pos);
    }
  }


  //
  // If we clicked near a teleport, tell the unit to go into it
  m_teleportId = -1;
  std::vector<int> const* nearbyBuildings = g_location->m_obstructionGrid->GetBuildings(_pos.x, _pos.z);
  for (int buildingId : *nearbyBuildings)
  {
    Building* building = g_location->GetBuilding(buildingId);
    if (building->m_type == Building::TypeRadarDish || building->m_type == Building::TypeBridge)
    {
      Teleport* teleport = (Teleport*)building;
      float distance = DirectX::XMVectorGetX(
        DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&building->m_pos), DirectX::XMLoadFloat3(&_pos))));
      if (distance < 5.0f && teleport->Connected())
      {
        m_teleportId = building->m_id.GetUniqueId();
        // Teleport converts in T17, so its out-parameters are still legacy.
        DirectX::XMFLOAT3 entrancePos{0.0f, 0.0f, 0.0f};
        DirectX::XMFLOAT3 entranceFront{0.0f, 0.0f, 0.0f};
        teleport->GetEntrance(AsLegacy(entrancePos), AsLegacy(entranceFront));
        m_wayPoint = entrancePos;
        break;
      }
    }
  }
}


// Returns a point on the path that the squad are walking along. The point
// man is the leader of the squad. He always heads towards m_wayPoint.
// When the user clicks to create a new m_wayPoint, the current pos of the
// leader is added to the list of historic wayPoints.
DirectX::XMFLOAT3 InsertionSquad::GetTargetPos(float _distFromPointMan)
{
  if (_distFromPointMan <= 0.0f)
  {
    return m_wayPoint;
  }

  if (m_teleportId != -1)
  {
    return m_wayPoint;
  }

  Entity* pointMan = GetPointMan();
  if (!pointMan)
    return m_wayPoint;

  // The first section of the route is the line from the point man to
  // the most recent HistoricWayPoint.

  DirectX::XMFLOAT3 lineEnd = pointMan->m_pos;

  int i = 0;
  while (i < static_cast<int>(m_positionHistory.size()) && _distFromPointMan > 0.0f)
  {
    DirectX::XMFLOAT3 const lineStart = lineEnd;
    lineEnd = m_positionHistory[i]->m_pos;

    DirectX::XMVECTOR const delta = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&lineEnd), DirectX::XMLoadFloat3(&lineStart));
    float magDelta = DirectX::XMVectorGetX(DirectX::XMVector3Length(delta));
    if (magDelta > _distFromPointMan)
    {
      // SetLength then add. A zero-length segment used to give (len,0,0);
      // native normalise gives zero, so the point stays at lineStart.
      DirectX::XMFLOAT3 result;
      DirectX::XMStoreFloat3(&result, DirectX::XMVectorMultiplyAdd(DirectX::XMVector3Normalize(delta), DirectX::XMVectorReplicate(_distFromPointMan),
                                                                   DirectX::XMLoadFloat3(&lineStart)));
      return result;
    }
    else
    {
      _distFromPointMan -= magDelta;
    }
    i++;
  }

  return lineEnd;
}


void InsertionSquad::SetWeaponType(int _weaponType) { m_weaponType = _weaponType; }


void InsertionSquad::Attack(DirectX::XMFLOAT3 pos, bool withGrenade)
{
  //
  // Deal with grenades

  if (withGrenade)
  {
    float nearest = 999999.9f;
    Squadie* nearestEnt = nullptr;

    //
    // Find the entity nearest to the target that has a grenade

    for (int i = 0; i < m_entities.Size(); ++i)
    {
      if (m_entities.ValidIndex(i))
      {
        Squadie* ent = (Squadie*)m_entities[i];
        if (!ent->m_dead && ent->m_enabled && ent->HasSecondaryWeapon())
        {
          float distance = DirectX::XMVectorGetX(
            DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&ent->m_pos), DirectX::XMLoadFloat3(&pos))));
          if (distance < nearest)
          {
            nearest = distance;
            nearestEnt = ent;
          }
        }
      }
    }

    if (nearestEnt)
    {
      nearestEnt->FireSecondaryWeapon(pos);
    }
  }


  //
  // Build a list of squadies that can attack now

  std::vector<int> canAttack;
  for (int i = 0; i < m_entities.Size(); ++i)
  {
    if (m_entities.ValidIndex(i))
    {
      Squadie* ent = (Squadie*)m_entities[i];
      if (ent->m_enabled && !ent->m_dead && ent->m_reloading == 0.0f)
      {
        canAttack.push_back(i);
      }
    }
  }


  if (!canAttack.empty())
  {
    //
    // Decide the maximum number of entities
    // that can attack now without pauses appearing in fire rate

    float reloadTime = EntityBlueprint::GetStat(m_troopType, Entity::StatRate);
    float timeToWait = (float)reloadTime / (float)static_cast<int>(canAttack.size());
    m_attackAccumulator += ((float)SERVER_ADVANCE_PERIOD / timeToWait);


    //
    // Pick guys randomly to attack

    while (!canAttack.empty() && m_attackAccumulator >= 1.0f)
    {
      m_attackAccumulator -= 1.0f;
      int randomIndex = syncfrand(static_cast<int>(canAttack.size()));
      int entityIndex = canAttack[randomIndex];
      canAttack.erase(canAttack.begin() + randomIndex);
      Entity* ent = m_entities[entityIndex];
      ent->Attack(pos);
    }
  }
}

void InsertionSquad::DirectControl(TeamControls const& _teamControls)
{
  if (!_teamControls.m_cameraEntityTracking)
  {
    return;
  }

  Entity* point = GetPointMan();
  if (!point || point->m_type != Entity::TypeInsertionSquadie)
  {
    return;
  }
  Squadie* pointMan = (Squadie*)point;

  // `right` and `front` were computed and never read -- every use below is
  // commented out. Dropped rather than converted.

  if (_teamControls.m_directUnitMove)
  {
    DirectX::XMFLOAT3 t = pointMan->m_pos;

    t.x += _teamControls.m_directUnitMoveDx;
    t.z += _teamControls.m_directUnitMoveDy;
    // t+= front * - _teamControls.m_directUnitMoveDy;

    std::vector<int> const* nearbyBuildings = g_location->m_obstructionGrid->GetBuildings(t.x, t.z);
    for (int buildingId : *nearbyBuildings)
    {
      Building* building = g_location->GetBuilding(buildingId);
      if (building->m_type == Building::TypeRadarDish || building->m_type == Building::TypeBridge)
      {
        Teleport* teleport = (Teleport*)building;
        if (teleport->Connected())
        {
          t = building->m_pos;
        }
      }
    }

    SetWayPoint(t);
  }
  else
  {
    if (DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&m_vel))) > 0)
    {
      SetWayPoint(pointMan->m_pos);
    }
  }

  if (_teamControls.m_primaryFireDirected)
  {
    DirectX::XMFLOAT3 t = pointMan->GetSecondaryWeaponTarget();

    Attack(t, _teamControls.m_secondaryFireDirected);
  }
}


//****************************************************************************
// Class Squadie
//****************************************************************************

Squadie::Squadie()
  : Entity(),
    m_justFired(false),
    m_secondaryTimer(0.0f),
    m_retargetTimer(0.0f)
{
  m_shape = g_resource->GetShape("Squad.shp");
  DEBUG_ASSERT(m_shape);

  DirectX::XMFLOAT4X4 const identity(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
  m_centrePos = m_shape->CalculateCentre(identity);
  m_radius = m_shape->CalculateRadius(identity, m_centrePos);

  m_laser = m_shape->m_rootFragment->LookupMarker("MarkerLaser");
  m_brass = m_shape->m_rootFragment->LookupMarker("MarkerBrass");
  m_eye1 = m_shape->m_rootFragment->LookupMarker("MarkerEye1");
  m_eye2 = m_shape->m_rootFragment->LookupMarker("MarkerEye2");
}


void Squadie::Begin() { Entity::Begin(); }


void Squadie::ChangeHealth(int _amount)
{
  bool dead = m_dead;

  if (!m_dead)
  {
    if (_amount < 0)
    {
      g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "LoseHealth");
    }

    if (m_stats[StatHealth] + _amount <= 0)
    {
      m_stats[StatHealth] = 100;
      m_dead = true;
      g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "Die");
    }
    else if (m_stats[StatHealth] + _amount > 255)
    {
      m_stats[StatHealth] = 255;
    }
    else
    {
      m_stats[StatHealth] += _amount;
    }
  }

  if (!dead && m_dead)
  {
    DirectX::XMFLOAT4X4 transform;
    DirectX::XMStoreFloat4x4(&transform,
                             BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));
    g_explosionManager.AddExplosion(m_shape, transform);
  }
}


bool Squadie::Advance(Unit* _theUnit)
{
  // DebugTrace( "Squadie %d, Health: %d, Dead %d\n", m_formationIndex, m_stats[StatHealth], (int)m_dead );
  if (m_secondaryTimer > 0.0f)
  {
    m_secondaryTimer -= SERVER_ADVANCE_PERIOD;
    if (m_secondaryTimer <= 0.0f)
    {
      // Secondary weapon is reloaded
      g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "WeaponReturns");
    }
  }

  InsertionSquad* _unit = (InsertionSquad*)_theUnit;

  DirectX::XMFLOAT3 const oldPos = m_pos;

  if (!m_dead && m_onGround && m_inWater < 0.0f)
  {
    DirectX::XMFLOAT3 const targetPos = _unit->GetTargetPos(GAP_BETWEEN_MEN * m_formationIndex);
    DirectX::XMVECTOR const targetVector =
      DirectX::XMVectorSetY(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&targetPos), DirectX::XMLoadFloat3(&m_pos)), 0.0f);

    float distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(targetVector));

    // If moving towards way point...
    if (distance > 0.01f || _unit->m_teleportId != -1)
    {
      // Was `m_vel = targetVector; m_vel *= speed / distance;` -- a divide by
      // `distance`, which the branch above only guarantees non-zero on the
      // first of its two conditions. Kept as a divide rather than turned into
      // a normalise, so the teleport path still produces the same infinity it
      // always did rather than quietly changing behaviour.
      DirectX::XMVECTOR velocity = DirectX::XMVectorScale(targetVector, m_stats[StatSpeed] / distance);

      //
      // Slow us down if we're going up hill
      // Speed up if going down hill

      DirectX::XMFLOAT3 nextPos;
      DirectX::XMStoreFloat3(
        &nextPos, DirectX::XMVectorMultiplyAdd(velocity, DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD), DirectX::XMLoadFloat3(&m_pos)));
      nextPos.y = g_location->m_landscape.m_heightMap->GetValue(nextPos.x, nextPos.z);
      float currentHeight = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);
      float nextHeight = g_location->m_landscape.m_heightMap->GetValue(nextPos.x, nextPos.z);

      // The landscape converts in T18, so its normal map is still legacy.
      DirectX::XMFLOAT3 const landNormal = g_location->m_landscape.m_normalMap->GetValue(m_pos.x, m_pos.z);

      float factor = 1.0f - (currentHeight - nextHeight) / -3.0f;
      if (factor < 0.2f)
        factor = 0.2f;
      if (factor > 2.0f)
        factor = 2.0f;
      if (landNormal.y < 0.6f && factor < 1.0f)
        factor = 0.0f;
      velocity = DirectX::XMVectorScale(velocity, factor);

      if (distance < m_stats[StatSpeed] * SERVER_ADVANCE_PERIOD)
      {
        velocity = DirectX::XMVectorScale(DirectX::XMVector3Normalize(velocity), distance / SERVER_ADVANCE_PERIOD);
      }
      DirectX::XMStoreFloat3(&m_vel, velocity);
      DirectX::XMStoreFloat3(
        &m_pos, DirectX::XMVectorMultiplyAdd(velocity, DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD), DirectX::XMLoadFloat3(&m_pos)));
      m_pos = PushFromObstructions(m_pos);
      m_pos.y = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z) + 0.1f;
    }

    DirectX::XMVECTOR const front = DirectX::XMLoadFloat3(&m_front);

    Team* team = &g_location->m_teams[m_id.GetTeamId()];
    if (m_id.GetUnitId() == team->m_currentUnitId)
    {
      DirectX::XMVECTOR const toMouse = DirectX::XMVector3Normalize(
        DirectX::XMVectorSetY(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&team->m_currentMousePos), DirectX::XMLoadFloat3(&m_pos)), 0.0f));
      DirectX::XMStoreFloat3(&m_angVel, DirectX::XMVectorScale(DirectX::XMVector3Cross(front, toMouse), 4.0f));
    }
    else
    {
      DirectX::XMVECTOR const velocity = DirectX::XMLoadFloat3(&m_vel);
      if (DirectX::XMVectorGetX(DirectX::XMVector3Length(velocity)) > 0.0f)
      {
        DirectX::XMStoreFloat3(&m_angVel, DirectX::XMVectorScale(DirectX::XMVector3Cross(DirectX::XMVector3Normalize(velocity), front), 4.0f));
      }
      else
      {
        m_angVel = m_front;
      }

      RunAI();

      Entity* enemy = g_location->GetEntity(m_enemyId);
      if (enemy)
      {
        DirectX::XMVECTOR const away =
          DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&enemy->m_pos)));
        DirectX::XMStoreFloat3(&m_angVel, DirectX::XMVectorScale(DirectX::XMVector3Cross(away, front), 4.0f));
      }
    }

    m_angVel.x = 0.0f;
    m_angVel.z = 0.0f;
    float const maxTurnRate = 20.0f; // radians per second
    DirectX::XMVECTOR angularVelocity = DirectX::XMLoadFloat3(&m_angVel);
    if (DirectX::XMVectorGetX(DirectX::XMVector3Length(angularVelocity)) > maxTurnRate)
    {
      angularVelocity = DirectX::XMVectorScale(DirectX::XMVector3Normalize(angularVelocity), maxTurnRate);
      DirectX::XMStoreFloat3(&m_angVel, angularVelocity);
    }

    // RotateAround takes the angle as the vector's MAGNITUDE and does nothing
    // below 1e-8 squared. Both halves reproduced -- without the guard a
    // stationary squaddie would rotate its front by a NaN.
    DirectX::XMVECTOR const rotation = DirectX::XMVectorScale(angularVelocity, SERVER_ADVANCE_PERIOD);
    float const rotationLengthSquared = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(rotation));
    DirectX::XMVECTOR rotatedFront = front;
    if (rotationLengthSquared >= 1e-8f)
    {
      float const angle = sqrtf(rotationLengthSquared);
      rotatedFront = DirectX::XMVector3Transform(front, DirectX::XMMatrixRotationAxis(DirectX::XMVectorScale(rotation, 1.0f / angle), angle));
    }
    DirectX::XMStoreFloat3(&m_front, DirectX::XMVector3Normalize(rotatedFront));

    if (_unit->m_teleportId != -1)
    {
      DirectX::XMStoreFloat3(&m_front, DirectX::XMVector3Normalize(DirectX::XMVectorSetY(
                                         DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&targetPos), DirectX::XMLoadFloat3(&m_pos)), 0.0f)));
    }

    if (m_pos.y <= 0.0f)
    {
      m_inWater = syncfrand(3.0f);
    }
  }

  if (!m_onGround)
    AdvanceInAir(nullptr);

  DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&oldPos)),
                                                        1.0f / SERVER_ADVANCE_PERIOD));

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

  if (_unit->m_teleportId != -1)
  {
    if (EnterTeleports(_unit->m_teleportId) != -1)
    {
      return true;
    }
  }

  bool manDead = Entity::Advance(_unit);
  if (m_dead)
  {
    _unit->RecalculateOffsets();
  }
  return manDead;
}


void Squadie::Render(float _predictionTime)
{
  if (!m_enabled)
    return;


  //
  // Work out our predicted position and orientation

  DirectX::XMFLOAT3 predictedPos;
  DirectX::XMStoreFloat3(&predictedPos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(_predictionTime),
                                                                     DirectX::XMLoadFloat3(&m_pos)));
  if (m_onGround)
  {
    predictedPos.y = g_location->m_landscape.m_heightMap->GetValue(predictedPos.x, predictedPos.z);
  }

  float size = 0.5f;

  DirectX::XMVECTOR const entityFront = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&m_front));
  DirectX::XMVECTOR const entityRight = DirectX::XMVector3Cross(entityFront, DirectX::g_XMIdentityR1);
  DirectX::XMVECTOR const entityUp = DirectX::XMVector3Cross(entityRight, entityFront);

  if (!m_dead)
  {
    //
    // 3d Shape

    g_renderer->SetObjectLighting();
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_COLOR_MATERIAL);
    glDisable(GL_BLEND);

    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(entityFront, entityUp, DirectX::XMLoadFloat3(&predictedPos)));

    //
    // If we are damaged, flicked in and out based on our health

    if (m_renderDamaged)
    {
      float timeIndex = g_gameTime + m_id.GetUniqueId() * 10;
      float thefrand = frand();
      // Matrix34 named its rows r/u/f; XMFLOAT4X4 numbers them -- row 0 is
      // right, row 1 up, row 2 front, per NeuronMath.h.
      DirectX::XMMATRIX squashed = DirectX::XMLoadFloat4x4(&mat);
      if (thefrand > 0.7f)
        squashed.r[2] = DirectX::XMVectorScale(squashed.r[2], 1.0f - sinf(timeIndex) * 0.5f);
      else if (thefrand > 0.4f)
        squashed.r[1] = DirectX::XMVectorScale(squashed.r[1], 1.0f - sinf(timeIndex) * 0.2f);
      else
        squashed.r[0] = DirectX::XMVectorScale(squashed.r[0], 1.0f - sinf(timeIndex) * 0.5f);
      DirectX::XMStoreFloat4x4(&mat, squashed);
      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ONE);
    }


    m_shape->Render(_predictionTime, mat);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_COLOR_MATERIAL);
    glEnable(GL_TEXTURE_2D);
    g_renderer->UnsetObjectLighting();
  }
}


bool Squadie::RenderPixelEffect(float _predictionTime)
{
  if (!m_enabled)
    return false;

  Render(_predictionTime);

  //
  // Work out our predicted position and orientation

  DirectX::XMFLOAT3 predictedPos;
  DirectX::XMStoreFloat3(&predictedPos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(_predictionTime),
                                                                     DirectX::XMLoadFloat3(&m_pos)));
  if (m_onGround)
  {
    predictedPos.y = g_location->m_landscape.m_heightMap->GetValue(predictedPos.x, predictedPos.z);
  }

  float size = 0.5f;

  DirectX::XMVECTOR const entityFront = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&m_front));
  DirectX::XMVECTOR const entityRight = DirectX::XMVector3Cross(entityFront, DirectX::g_XMIdentityR1);
  DirectX::XMVECTOR const entityUp = DirectX::XMVector3Cross(entityRight, entityFront);

  DirectX::XMFLOAT4X4 mat;
  DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(entityFront, entityUp, DirectX::XMLoadFloat3(&predictedPos)));

  // RendererAccess still takes a legacy matrix; T12 converts it, behind T22.
  g_renderer->MarkUsedCells(m_shape, mat);

  return true;
}


void Squadie::RunAI()
{
  //
  // Look for a new enemy every now and then

  m_retargetTimer -= SERVER_ADVANCE_PERIOD;

  if (m_retargetTimer <= 0.0f)
  {
    m_enemyId = g_location->m_entityGrid->GetBestEnemy(m_pos.x, m_pos.z, 0.0f, 100.0f, m_id.GetTeamId());
    m_retargetTimer = 1.0f;
  }


  //
  // Fire at the nearest enemy now and then

  Entity* enemy = g_location->GetEntity(m_enemyId);
  ;
  if (enemy)
  {
    float distance =
      DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&enemy->m_pos), DirectX::XMLoadFloat3(&m_pos))));
    if (syncfrand(distance * 0.5f) < 2.0f)
    {
      Attack(enemy->m_pos);
    }
  }
}


void Squadie::Attack(DirectX::XMFLOAT3 const& _pos)
{
  if (m_reloading == 0.0f)
  {
    //
    // Fire laser

    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

    // GetWorldMatrix still returns a legacy matrix -- T10's recorded seam.
    DirectX::XMFLOAT3 const fromPos = m_laser->GetWorldMatrix(mat).pos;

    // Both syncsfrand draws stay, in order and unconditional.
    float const spreadX = syncsfrand(7.0f);
    float const spreadZ = syncsfrand(7.0f);
    DirectX::XMVECTOR const toPos = DirectX::XMVectorSet(_pos.x + spreadX, _pos.y, _pos.z + spreadZ, 0.0f);

    DirectX::XMFLOAT3 velocity;
    DirectX::XMStoreFloat3(
      &velocity, DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(toPos, DirectX::XMLoadFloat3(&fromPos))), 200.0f));
    g_location->FireLaser(fromPos, velocity, m_id.GetTeamId());
    m_justFired = true;

    //
    // Create ejected brass particle

    // The one legacy type left in this file, and deliberately: ShapeMarker::
    // GetWorldMatrix still returns Matrix34 -- T10's recorded seam, because 43
    // sites in fourteen GameLogic files read .pos or .f off it. Both are read
    // here. It clears when that signature moves.
    Matrix34 brass = m_brass->GetWorldMatrix(mat);

    // Four syncfrand/syncsfrand draws, in the same order and count as before.
    float const brassSpeed = 5.0f + syncfrand(10.0f);
    float const scatterX = syncsfrand(5.0f);
    float const scatterY = syncsfrand(5.0f);
    float const scatterZ = syncsfrand(5.0f);

    // brass is the legacy Matrix34 above, so its rows are Vector3 and cannot
    // be addressed as XMFLOAT3. Copy-initialising takes the seam's conversion.
    DirectX::XMFLOAT3 const brassFront = brass.f;

    DirectX::XMFLOAT3 particleVel;
    DirectX::XMStoreFloat3(&particleVel, DirectX::XMVectorAdd(DirectX::XMVectorScale(DirectX::XMLoadFloat3(&brassFront), brassSpeed),
                                                              DirectX::XMVectorSet(scatterX, scatterY, scatterZ, 0.0f)));
    g_particleSystem->CreateParticle(brass.pos, particleVel, Particle::TypeBrass);


    //
    //
    m_reloading = m_stats[StatRate];
    g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "Attack");
    g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "FireLaser");
  }
}


bool Squadie::HasSecondaryWeapon() { return (m_secondaryTimer <= 0.0f); }


void Squadie::FireSecondaryWeapon(DirectX::XMFLOAT3 const& _pos)
{
  InsertionSquad* squad = (InsertionSquad*)g_location->GetUnit(m_id);
  DEBUG_ASSERT(squad);

  if (g_globalWorld->m_research->HasResearch(squad->m_weaponType))
  {
    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));
    DirectX::XMFLOAT3 const laserPos = m_laser->GetWorldMatrix(mat).pos;

    switch (squad->m_weaponType)
    {
    case GlobalResearch::TypeGrenade:
      g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "ThrowGrenade");
      g_location->ThrowWeapon(laserPos, _pos, EffectThrowableGrenade, m_id.GetTeamId());
      m_secondaryTimer = 4.0f;
      break;

    case GlobalResearch::TypeAirStrike:
      g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "ThrowAirStrike");
      g_location->ThrowWeapon(laserPos, _pos, EffectThrowableAirstrikeMarker, m_id.GetTeamId());
      m_secondaryTimer = 20.0f;
      break;

    case GlobalResearch::TypeController:
      g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "ThrowController");
      g_location->ThrowWeapon(laserPos, _pos, EffectThrowableControllerGrenade, m_id.GetTeamId());
      m_secondaryTimer = 4.0f;
      break;

    case GlobalResearch::TypeRocket:
      g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "FireRocket");
      g_location->FireRocket(laserPos, _pos, m_id.GetTeamId());
      m_secondaryTimer = 4.0f;
      break;
    }
  }
}


void Squadie::ListSoundEvents(std::vector<const char*>* _list)
{
  Entity::ListSoundEvents(_list);

  _list->push_back("FireLaser");
  _list->push_back("ThrowGrenade");
  _list->push_back("FireRocket");
  _list->push_back("ThrowAirStrike");
  _list->push_back("ThrowController");
  _list->push_back("WeaponReturns");
}

DirectX::XMFLOAT3 Squadie::GetCameraFocusPoint()
{
  if (g_inputManager->controlEvent(ControlUnitPrimaryFireDirected /* ControlUnitStartSecondaryFireDirected */) &&
      g_camera->IsInMode(CameraAccess::ModeEntityTrack))
  {
    InputDetails details;
    g_inputManager->controlEvent(ControlUnitPrimaryFireDirected, details);

    DirectX::XMFLOAT3 const t = GetSecondaryWeaponTarget();

    DirectX::XMFLOAT3 midpoint;
    DirectX::XMStoreFloat3(&midpoint, DirectX::XMVectorScale(DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&t), DirectX::XMLoadFloat3(&m_pos)), 0.5f));
    return midpoint;
  }

  return Entity::GetCameraFocusPoint();
}

DirectX::XMFLOAT3 Squadie::GetSecondaryWeaponTarget()
{
  InputDetails details;
  g_inputManager->controlEvent(ControlUnitPrimaryFireDirected, details);

  DirectX::XMFLOAT3 t = m_pos;

  // CameraAccess still returns a legacy vector; T12 converts it, behind T22.
  DirectX::XMFLOAT3 const cameraPos = g_camera->GetPos();

  DirectX::XMVECTOR const front = DirectX::XMVector3Normalize(
    DirectX::XMVectorSetY(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&cameraPos)), 0.0f));
  DirectX::XMVECTOR const right = DirectX::XMVector3Transform(front, DirectX::XMMatrixRotationY(M_PI / 2.0f));

  float force = ThrowableWeapon::GetMaxForce(g_globalWorld->m_research->CurrentLevel(GlobalResearch::TypeGrenade));
  float maxRange = ThrowableWeapon::GetApproxMaxRange(force);

  // float total = abs(details.x) + abs(details.y);

  /*char debugstring[512];
  double r = details.x * details.x + details.y * details.y;
  r = sqrt(r);
  sprintf(debugstring, "x = %d, y = %d r = %f\n", details.x, details.y, r);
  DebugTrace( debugstring );*/

  // This should be dependent on distance of camera from units
  // Further away, rangeFactor = 1.0, closer rangeFactor closer to 0.66
  const float rangeFactor = 0.75;

  float rMod = (rangeFactor * maxRange * float(float(details.x) / 40.0f));
  float fMod = (rangeFactor * maxRange * float(float(details.y) / 40.0f));
  DirectX::XMVECTOR offset = DirectX::XMVectorMultiplyAdd(right, DirectX::XMVectorReplicate(-rMod), DirectX::XMLoadFloat3(&t));
  offset = DirectX::XMVectorMultiplyAdd(front, DirectX::XMVectorReplicate(-fMod), offset);
  DirectX::XMStoreFloat3(&t, offset);
  t.y = g_location->m_landscape.m_heightMap->GetValue(t.x, t.z) + 5.0f;

  return t;
}