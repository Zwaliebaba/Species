#include "pch.h"
#include "GlVertex.h"
#include "SoundSources.h"
#include "Resource.h"
#include "Shape.h"
#include "MathUtils.h"
#include "DebugRender.h"
#include "HiResTime.h"
#include "TextRenderer.h"
#include "Preferences.h"
#include "Profiler.h"

#include "Input.h"
#include "InputTypes.h"

#include "Officer.h"
#include "Teleport.h"

#include "Location.h"
#include "Team.h"
#include "GameTime.h"
#include "ParticleSystem.h"
#include "Explosion.h"
#include "ObstructionGrid.h"
#include "EntityGrid.h"
#include "GlobalWorld.h"

#include "SoundSystem.h"
#include "WorldPointers.h"


Officer::Officer()
  : Entity(),
    m_demoted(false),
    m_ordersBuildingId(-1),
    m_wayPointTeleportId(-1),
    m_shield(0),
    m_absorb(false),
    m_absorbTimer(2.0f)
{
  m_type = TypeOfficer;
  m_state = StateIdle;
  m_orders = OrderNone;

  m_shape = g_resource->GetShape("Citizen.shp");
  ASSERT_TEXT(m_shape, "Shape not found : officer.shp");

  m_flagMarker = m_shape->m_rootFragment->LookupMarker("MarkerFlag");

  DirectX::XMFLOAT4X4 const identity(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
  m_centrePos = m_shape->CalculateCentre(identity);
  m_radius = m_shape->CalculateRadius(identity, m_centrePos);
}


Officer::~Officer()
{
  if (m_id.GetTeamId() != 255)
  {
    Team* team = &g_location->m_teams[m_id.GetTeamId()];
    team->UnRegisterSpecial(m_id);
  }
}


void Officer::Begin()
{
  Entity::Begin();

  m_wayPoint = m_pos;

  m_flag.SetPosition(m_pos);
  m_flag.SetOrientation(m_front, DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f));
  m_flag.SetSize(20.0f);
  m_flag.Initialise();

  if (m_id.GetTeamId() != 255)
  {
    Team* team = &g_location->m_teams[m_id.GetTeamId()];
    team->RegisterSpecial(m_id);
  }
}


void Officer::ChangeHealth(int amount)
{
  bool dead = m_dead;

  if (amount < 0 && m_shield > 0)
  {
    int shieldLoss = std::min(m_shield * 10, -amount);
    for (int i = 0; i < shieldLoss / 10.0f; ++i)
    {
      // Both draws stay, in order, one per iteration.
      float const velX = syncsfrand(40.0f);
      float const velZ = syncsfrand(40.0f);
      DirectX::XMFLOAT3 const vel(velX, 0.0f, velZ);
      g_location->SpawnSpirit(m_pos, vel, 0, WorldObjectId());
    }
    m_shield -= shieldLoss / 10.0f;
    amount += shieldLoss;
  }

  Entity::ChangeHealth(amount);

  if (!dead && m_dead)
  {
    // We just died
    DirectX::XMFLOAT4X4 transform;
    DirectX::XMStoreFloat4x4(&transform,
                             BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));
    g_explosionManager.AddExplosion(m_shape, transform);
  }
}


void Officer::Render(float _predictionTime)
{
  RenderPixelEffect(_predictionTime);
  RenderShield(_predictionTime);

  if (m_enabled)
  {
    RenderFlag(_predictionTime);
  }
}

void Officer::RenderSpirit(DirectX::XMFLOAT3 const& _pos)
{
  DirectX::XMFLOAT3 pos = _pos;

  DirectX::XMFLOAT3 const cameraUp = g_camera->GetUp();
  DirectX::XMFLOAT3 const cameraRight = g_camera->GetRight();
  DirectX::XMVECTOR const camUpAxis = DirectX::XMLoadFloat3(&cameraUp);
  DirectX::XMVECTOR const camRightAxis = DirectX::XMLoadFloat3(&cameraRight);

  int innerAlpha = 255;
  int outerAlpha = 40;
  int glowAlpha = 20;

  float spiritInnerSize = 0.5f;
  float spiritOuterSize = 1.5f;
  float spiritGlowSize = 10;

  float size = spiritInnerSize;
  glColor4ub(100, 250, 100, innerAlpha);

  glDisable(GL_TEXTURE_2D);

  glBegin(GL_QUADS);
  EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camUpAxis, DirectX::XMVectorReplicate(size), DirectX::XMLoadFloat3(&pos)));
  EmitVertex(DirectX::XMVectorMultiplyAdd(camRightAxis, DirectX::XMVectorReplicate(size), DirectX::XMLoadFloat3(&pos)));
  EmitVertex(DirectX::XMVectorMultiplyAdd(camUpAxis, DirectX::XMVectorReplicate(size), DirectX::XMLoadFloat3(&pos)));
  EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camRightAxis, DirectX::XMVectorReplicate(size), DirectX::XMLoadFloat3(&pos)));
  glEnd();

  size = spiritOuterSize;
  glColor4ub(100, 250, 100, outerAlpha);

  glBegin(GL_QUADS);
  EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camUpAxis, DirectX::XMVectorReplicate(size), DirectX::XMLoadFloat3(&pos)));
  EmitVertex(DirectX::XMVectorMultiplyAdd(camRightAxis, DirectX::XMVectorReplicate(size), DirectX::XMLoadFloat3(&pos)));
  EmitVertex(DirectX::XMVectorMultiplyAdd(camUpAxis, DirectX::XMVectorReplicate(size), DirectX::XMLoadFloat3(&pos)));
  EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camRightAxis, DirectX::XMVectorReplicate(size), DirectX::XMLoadFloat3(&pos)));
  glEnd();

  size = spiritGlowSize;
  glColor4ub(100, 250, 100, glowAlpha);

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Glow.bmp"));
  glBegin(GL_QUADS);
  glTexCoord2i(0, 0);
  EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camUpAxis, DirectX::XMVectorReplicate(size), DirectX::XMLoadFloat3(&pos)));
  glTexCoord2i(1, 0);
  EmitVertex(DirectX::XMVectorMultiplyAdd(camRightAxis, DirectX::XMVectorReplicate(size), DirectX::XMLoadFloat3(&pos)));
  glTexCoord2i(1, 1);
  EmitVertex(DirectX::XMVectorMultiplyAdd(camUpAxis, DirectX::XMVectorReplicate(size), DirectX::XMLoadFloat3(&pos)));
  glTexCoord2i(0, 1);
  EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camRightAxis, DirectX::XMVectorReplicate(size), DirectX::XMLoadFloat3(&pos)));
  glEnd();
  glDisable(GL_TEXTURE_2D);
}


void Officer::RenderShield(float _predictionTime)
{
  float timeIndex = g_gameTime + m_id.GetUniqueId() * 20;

  glDisable(GL_CULL_FACE);
  glDepthMask(false);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);

  DirectX::XMFLOAT3 predictedPos;
  DirectX::XMStoreFloat3(&predictedPos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(_predictionTime),
                                                                     DirectX::XMLoadFloat3(&m_pos)));
  predictedPos.y += 10.0f;

  for (int i = 0; i < m_shield; ++i)
  {
    DirectX::XMFLOAT3 spiritPos = predictedPos;
    spiritPos.x += sinf(timeIndex * 1.8f + i * 2.0f) * 10.0f;
    spiritPos.y += cosf(timeIndex * 2.1f + i * 1.2f) * 6.0f;
    spiritPos.z += sinf(timeIndex * 2.4f + i * 1.4f) * 10.0f;

    RenderSpirit(spiritPos);
  }

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_BLEND);
  glDepthMask(true);
  glEnable(GL_CULL_FACE);
}


void Officer::RenderFlag(float _predictionTime)
{
  float timeIndex = g_gameTime + m_id.GetUniqueId() * 10;
  float size = 20.0f;

  DirectX::XMVECTOR up = DirectX::g_XMIdentityR1;
  DirectX::XMVECTOR const front =
    DirectX::XMVector3Normalize(DirectX::XMVectorSetY(DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_front), -1.0f), 0.0f));

  if (m_orders != OrderNone)
  {
    // RotateAround's angle is the vector's magnitude, with the same 1e-8 guard.
    DirectX::XMVECTOR const rotation = DirectX::XMVectorScale(front, sinf(timeIndex * 2) * 0.3f);
    float const rotationLengthSquared = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(rotation));
    if (rotationLengthSquared >= 1e-8f)
    {
      float const angle = sqrtf(rotationLengthSquared);
      up = DirectX::XMVector3Transform(up, DirectX::XMMatrixRotationAxis(DirectX::XMVectorScale(rotation, 1.0f / angle), angle));
    }
  }

  DirectX::XMVECTOR const entityUp = DirectX::g_XMIdentityR1;
  DirectX::XMVECTOR const entityRight = DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&m_front), entityUp);
  DirectX::XMVECTOR const entityFront = DirectX::XMVector3Cross(entityUp, entityRight);

  DirectX::XMFLOAT4X4 mat;
  DirectX::XMStoreFloat4x4(
    &mat, BasisFromFrontAndUp(
            entityFront, entityUp,
            DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(_predictionTime), DirectX::XMLoadFloat3(&m_pos))));

  DirectX::XMFLOAT3 const flagPos = m_flagMarker->GetWorldPosition(mat);

  int texId = -1;
  if (m_orders == OrderNone)
    texId = g_resource->GetTexture("Icons/BannerNone.bmp");
  else if (m_orders == OrderGoto)
    texId = g_resource->GetTexture("Icons/BannerGoto.bmp");
  else if (m_orders == OrderFollow && !m_absorb)
    texId = g_resource->GetTexture("Icons/BannerFollow.bmp");
  else if (m_orders == OrderFollow && m_absorb)
    texId = g_resource->GetTexture("Icons/BannerAbsorb.bmp");

  m_flag.SetTexture(texId);
  m_flag.SetPosition(flagPos);

  DirectX::XMFLOAT3 flagFront;
  DirectX::XMFLOAT3 flagUp;
  DirectX::XMStoreFloat3(&flagFront, front);
  DirectX::XMStoreFloat3(&flagUp, up);
  m_flag.SetOrientation(flagFront, flagUp);
  m_flag.SetSize(size);
  m_flag.Render();
}


bool Officer::RenderPixelEffect(float _predictionTime)
{
  if (!m_enabled || m_dead)
    return false;

  //
  // Calculate where we are

  DirectX::XMFLOAT3 predictedPos;
  DirectX::XMStoreFloat3(&predictedPos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(_predictionTime),
                                                                     DirectX::XMLoadFloat3(&m_pos)));
  if (m_onGround && m_inWater == -1)
  {
    predictedPos.y = g_location->m_landscape.m_heightMap->GetValue(predictedPos.x, predictedPos.z);
  }
  DirectX::XMVECTOR const entityUp = DirectX::g_XMIdentityR1;
  DirectX::XMVECTOR const entityRight = DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&m_front), entityUp);
  DirectX::XMVECTOR const entityFront = DirectX::XMVector3Cross(entityUp, entityRight);

  DirectX::XMFLOAT4X4 mat;
  DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(entityFront, entityUp, DirectX::XMLoadFloat3(&predictedPos)));

  //
  // If we are damaged, flicked in and out based on our health

  if (m_renderDamaged)
  {
    float timeIndex = g_gameTime + m_id.GetUniqueId() * 10;
    float thefrand = frand();
    // Matrix34 named its rows r/u/f; XMFLOAT4X4 numbers them, and the
    // convention is row 0 = right, row 1 = up, row 2 = front. Scaling one row
    // squashes the model along that axis, which is the damage flicker.
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


  //
  // Render our shape

  g_renderer->SetObjectLighting();
  glDisable(GL_TEXTURE_2D);
  glShadeModel(GL_SMOOTH);

  m_shape->Render(_predictionTime, mat);

  glShadeModel(GL_FLAT);
  glEnable(GL_TEXTURE_2D);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  g_renderer->UnsetObjectLighting();


  g_renderer->MarkUsedCells(m_shape, mat);

  return true;
}


bool Officer::AdvanceIdle()
{
  if (m_orders != OrderNone)
  {
    m_state = StateGivingOrders;
    return false;
  }

  m_vel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

  return false;
}


bool Officer::AdvanceToWaypoint()
{
  bool arrived = AdvanceToTargetPosition();
  if (arrived)
  {
    m_state = StateIdle;
  }

  return false;
}


bool Officer::AdvanceToTargetPosition()
{
  //
  // Work out where we want to be next

  DirectX::XMVECTOR const toWayPoint = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_wayPoint), DirectX::XMLoadFloat3(&m_pos));
  if (DirectX::XMVectorGetX(DirectX::XMVector3Length(toWayPoint)) > 5.0f)
  {
    float speed = m_stats[StatSpeed];
    if (m_state == StateIdle)
      speed *= 0.2f;

    float amountToTurn = SERVER_ADVANCE_PERIOD * 3.0f;
    DirectX::XMVECTOR const targetDir = DirectX::XMVector3Normalize(toWayPoint);
    DirectX::XMVECTOR const actualDir = DirectX::XMVector3Normalize(DirectX::XMVectorMultiplyAdd(
      targetDir, DirectX::XMVectorReplicate(amountToTurn), DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_front), 1.0f - amountToTurn)));

    DirectX::XMFLOAT3 const oldPos = m_pos;
    DirectX::XMFLOAT3 newPos;
    DirectX::XMStoreFloat3(
      &newPos, DirectX::XMVectorMultiplyAdd(actualDir, DirectX::XMVectorReplicate(speed * SERVER_ADVANCE_PERIOD), DirectX::XMLoadFloat3(&m_pos)));


    //
    // Slow us down if we're going up hill
    // Speed up if going down hill

    float currentHeight = g_location->m_landscape.m_heightMap->GetValue(oldPos.x, oldPos.z);
    float nextHeight = g_location->m_landscape.m_heightMap->GetValue(newPos.x, newPos.z);
    float factor = 1.0f - (currentHeight - nextHeight) / -3.0f;
    if (factor < 0.1f)
      factor = 0.1f;
    if (factor > 2.0f)
      factor = 2.0f;
    DirectX::XMStoreFloat3(&newPos, DirectX::XMVectorMultiplyAdd(actualDir, DirectX::XMVectorReplicate(speed * factor * SERVER_ADVANCE_PERIOD),
                                                                 DirectX::XMLoadFloat3(&m_pos)));
    newPos = PushFromObstructions(newPos);

    m_pos = newPos;

    DirectX::XMVECTOR const travelled = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&oldPos));
    DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(travelled, 1.0f / SERVER_ADVANCE_PERIOD));
    DirectX::XMStoreFloat3(&m_front, DirectX::XMVector3Normalize(travelled));
  }
  else
  {
    m_vel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
    return true;
  }

  return false;
}

bool Officer::AdvanceGivingOrders()
{
  if (m_orders == OrderGoto)
  {
    DirectX::XMStoreFloat3(
      &m_front, DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_orderPosition), DirectX::XMLoadFloat3(&m_pos))));
  }
  m_vel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
  return false;
}


bool Officer::SearchForRandomPosition()
{
  float distance = 30.0f;
  float angle = syncsfrand(2.0f * M_PI);

  DirectX::XMStoreFloat3(&m_wayPoint, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos),
                                                           DirectX::XMVectorSet(sinf(angle) * distance, 0.0f, cosf(angle) * distance, 0.0f)));

  m_wayPoint = PushFromObstructions(m_wayPoint);
  m_wayPoint.y = g_location->m_landscape.m_heightMap->GetValue(m_wayPoint.x, m_wayPoint.z);

  return true;
}


void Officer::Absorb()
{
  int numFound;
  WorldObjectId* ids = g_location->m_entityGrid->GetFriends(m_pos.x, m_pos.z, OFFICER_ABSORBRANGE, &numFound, m_id.GetTeamId());

  WorldObjectId nearestId;
  float nearestDistance = 99999.9f;

  for (int i = 0; i < numFound; ++i)
  {
    WorldObjectId id = ids[i];
    Entity* entity = g_location->GetEntity(id);
    if (entity && entity->m_type == Entity::TypeCitizen && !entity->m_dead)
    {
      float distance = DirectX::XMVectorGetX(
        DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&entity->m_pos), DirectX::XMLoadFloat3(&m_pos))));
      if (distance < nearestDistance)
      {
        nearestDistance = distance;
        nearestId = id;
      }
    }
  }

  if (nearestId.IsValid())
  {
    m_absorbTimer -= SERVER_ADVANCE_PERIOD;
    if (m_absorbTimer < 0.0f)
    {
      Entity* entity = g_location->GetEntity(nearestId);

      g_location->m_entityGrid->RemoveObject(nearestId, entity->m_pos.x, entity->m_pos.z, entity->m_radius);
      g_location->m_teams[nearestId.GetTeamId()].m_others.MarkNotUsed(nearestId.GetIndex());
      ++m_shield;
      m_absorbTimer = 1.0f;
    }
  }
}


bool Officer::Advance(Unit* _unit)
{
  if (!m_onGround)
    AdvanceInAir(_unit);
  bool amIDead = Entity::Advance(_unit);
  if (m_inWater != -1.0f)
    AdvanceInWater(_unit);

  if (m_onGround && !m_dead)
    m_pos.y = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);


  //
  // Advance in whatever state we are in

  if (!amIDead && m_onGround && m_inWater == -1.0f)
  {
    switch (m_state)
    {
    case StateIdle:
      amIDead = AdvanceIdle();
      break;
    case StateToWaypoint:
      amIDead = AdvanceToWaypoint();
      break;
    case StateGivingOrders:
      amIDead = AdvanceGivingOrders();
      break;
    }
  }

  if (m_dead)
  {
    m_vel.y -= 20.0f;
    m_pos.y += m_vel.y * SERVER_ADVANCE_PERIOD;
  }


  //
  // If we are giving orders, render them

  if (m_orders == OrderGoto)
  {
    if (syncfrand() < 0.05f)
    {
      OfficerOrders* orders = new OfficerOrders();
      orders->m_pos = DirectX::XMFLOAT3(m_pos.x, m_pos.y + 2.0f, m_pos.z);
      orders->m_wayPoint = m_orderPosition;
      int index = g_location->m_effects.PutData(orders);
      orders->m_id.Set(m_id.GetTeamId(), UNIT_EFFECTS, index, -1);
      orders->m_id.GenerateUniqueId();
    }
  }

  //
  // If we are absorbing, look around for Citizens

  if (m_absorb)
    Absorb();


  //
  // Attack anything nearby with our "shield"

  if (m_shield > 0)
  {
    WorldObjectId id = g_location->m_entityGrid->GetBestEnemy(m_pos.x, m_pos.z, 0.0f, OFFICER_ATTACKRANGE, m_id.GetTeamId());
    if (id.IsValid())
    {
      Entity* entity = g_location->GetEntity(id);
      entity->ChangeHealth(-10);
      m_shield--;

      DirectX::XMFLOAT3 themToUs;
      DirectX::XMStoreFloat3(&themToUs, DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&entity->m_pos)));
      g_location->SpawnSpirit(m_pos, themToUs, 0, WorldObjectId());
    }
  }


  //
  // Use teleports.  Remember which teleport we entered,
  // As there may be people following us

  if (m_wayPointTeleportId != -1)
  {
    int teleportId = EnterTeleports(m_wayPointTeleportId);
    if (teleportId != -1)
    {
      m_ordersBuildingId = teleportId;
      Teleport* teleport = (Teleport*)g_location->GetBuilding(teleportId);
      DirectX::XMFLOAT3 exitPos{0.0f, 0.0f, 0.0f};
      DirectX::XMFLOAT3 exitFront{0.0f, 0.0f, 0.0f};
      bool exitFound = teleport->GetExit(exitPos, exitFront);
      if (exitFound)
        DirectX::XMStoreFloat3(&m_wayPoint, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&exitFront), DirectX::XMVectorReplicate(30.0f),
                                                                         DirectX::XMLoadFloat3(&exitPos)));
      if (m_orders == OrderGoto)
        m_orders = OrderNone;
      m_wayPointTeleportId = -1;
    }
  }

  return amIDead || m_demoted;
}


void Officer::SetWaypoint(DirectX::XMFLOAT3 const& _wayPoint)
{
  m_wayPoint = _wayPoint;
  m_state = StateToWaypoint;

  //
  // If we clicked near a teleport, tell the officer to go into it
  m_wayPointTeleportId = -1;
  std::vector<int> const* nearbyBuildings = g_location->m_obstructionGrid->GetBuildings(_wayPoint.x, _wayPoint.z);
  for (int buildingId : *nearbyBuildings)
  {
    Building* building = g_location->GetBuilding(buildingId);
    if (building->m_type == Building::TypeRadarDish || building->m_type == Building::TypeBridge)
    {
      float distance = DirectX::XMVectorGetX(
        DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&building->m_pos), DirectX::XMLoadFloat3(&_wayPoint))));
      Teleport* teleport = (Teleport*)building;
      if (distance < 5.0f && teleport->Connected())
      {
        m_wayPointTeleportId = building->m_id.GetUniqueId();
        DirectX::XMFLOAT3 entrancePos{0.0f, 0.0f, 0.0f};
        DirectX::XMFLOAT3 entranceFront{0.0f, 0.0f, 0.0f};
        teleport->GetEntrance(entrancePos, entranceFront);
        m_wayPoint = entrancePos;
        break;
      }
    }
  }
}


void Officer::ListSoundEvents(std::vector<const char*>* _list)
{
  Entity::ListSoundEvents(_list);

  _list->push_back("SetOrderNone");
  _list->push_back("SetOrderGoto");
  _list->push_back("SetOrderFollow");
  _list->push_back("SetOrderAbsorb");
}


void Officer::CancelOrderSounds()
{
  g_soundSystem->StopAllSounds(m_id, "Officer SetOrderNone");
  g_soundSystem->StopAllSounds(m_id, "Officer SetOrderGoto");
  g_soundSystem->StopAllSounds(m_id, "Officer SetOrderFollow");
  g_soundSystem->StopAllSounds(m_id, "Officer SetOrderAbsorb");
}


void Officer::SetOrders(DirectX::XMFLOAT3 const& _orders)
{
  static float lastOrderSet = 0.0f;

  if (!g_location->IsWalkable(m_pos, _orders))
  {
  }
  else
  {
    float distanceToOrders =
      DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&_orders), DirectX::XMLoadFloat3(&m_pos))));

    if (distanceToOrders > 20.0f)
    {
      m_orderPosition = _orders;
      m_orders = OrderGoto;
      m_absorb = false;

      //
      // If there is a teleport nearby,
      // assume he wants us to go in it

      bool foundTeleport = false;

      m_ordersBuildingId = -1;
      std::vector<int> const* nearbyBuildings = g_location->m_obstructionGrid->GetBuildings(m_orderPosition.x, m_orderPosition.z);
      for (int buildingId : *nearbyBuildings)
      {
        Building* building = g_location->GetBuilding(buildingId);
        if (building->m_type == Building::TypeRadarDish || building->m_type == Building::TypeBridge)
        {
          float distance = DirectX::XMVectorGetX(
            DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&building->m_pos), DirectX::XMLoadFloat3(&_orders))));
          if (distance < 5.0f)
          {
            Teleport* teleport = (Teleport*)building;
            m_ordersBuildingId = building->m_id.GetUniqueId();
            DirectX::XMFLOAT3 entrancePos{0.0f, 0.0f, 0.0f};
            DirectX::XMFLOAT3 entranceFront{0.0f, 0.0f, 0.0f};
            teleport->GetEntrance(entrancePos, entranceFront);
            m_orderPosition = entrancePos;
            foundTeleport = true;
            break;
          }
        }
      }

      if (!foundTeleport)
      {
        m_orderPosition = PushFromObstructions(m_orderPosition);
      }


      //
      // Create the line using particles immediately,
      // so the player can see what he's just done

      float timeNow = GetHighResTime();
      if (timeNow > lastOrderSet + 1.0f)
      {
        lastOrderSet = timeNow;
        OfficerOrders orders;
        orders.m_pos = m_pos;
        orders.m_wayPoint = m_orderPosition;
        while (true)
        {
          if (orders.m_arrivedTimer < 0.0f)
          {
            g_particleSystem->CreateParticle(orders.m_pos, DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), Particle::TypeMuzzleFlash, 50.0f);
            g_particleSystem->CreateParticle(orders.m_pos, DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), Particle::TypeMuzzleFlash, 40.0f);
          }
          if (orders.Advance())
            break;
        }

        CancelOrderSounds();
        g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "SetOrderGoto");
      }
    }
    else
    {
      float timeNow = GetHighResTime();
      int researchLevel = g_globalWorld->m_research->CurrentLevel(GlobalResearch::TypeOfficer);

      if (timeNow > lastOrderSet + 0.3f)
      {
        lastOrderSet = timeNow;

        switch (researchLevel)
        {
        case 0:
        case 1:
        case 2:
          m_orders = OrderNone;
          break;

        case 3:
          if (m_orders == OrderNone)
            m_orders = OrderFollow;
          else if (m_orders == OrderFollow)
            m_orders = OrderNone;
          else if (m_orders == OrderGoto)
            m_orders = OrderNone;
          break;

        case 4:
          if (m_orders == OrderNone)
            m_orders = OrderFollow;
          else if (m_orders == OrderFollow && !m_absorb)
          {
            m_absorb = true;
            m_absorbTimer = 2.0f;
          }
          else if (m_orders == OrderFollow && m_absorb)
          {
            m_orders = OrderNone;
            m_absorb = false;
          }
          else if (m_orders == OrderGoto)
            m_orders = OrderNone;
          break;
        }

        CancelOrderSounds();

        switch (m_orders)
        {
        case OrderNone:
          g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "SetOrderNone");
          break;
        case OrderGoto:
          g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "SetOrderGoto");
          break;
        case OrderFollow:
          if (m_absorb)
            g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "SetOrderAbsorb");
          else
            g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "SetOrderFollow");
          break;
        }

        m_buildingId = -1;
      }
    }
  }
}

void Officer::SetNextMode()
{
  static float lastOrderSet = 0.0f;

  float timeNow = GetHighResTime();
  int researchLevel = g_globalWorld->m_research->CurrentLevel(GlobalResearch::TypeOfficer);

  if (timeNow > lastOrderSet + 0.3f)
  {
    lastOrderSet = timeNow;

    switch (researchLevel)
    {
    case 0:
    case 1:
    case 2:
      m_orders = OrderNone;
      break;

    case 3:
      if (m_orders == OrderNone)
        m_orders = OrderFollow;
      else if (m_orders == OrderFollow)
        m_orders = OrderNone;
      else if (m_orders == OrderGoto)
        m_orders = OrderNone;
      break;

    case 4:
      if (m_orders == OrderNone)
        m_orders = OrderFollow;
      else if (m_orders == OrderFollow && !m_absorb)
      {
        m_absorb = true;
        m_absorbTimer = 2.0f;
      }
      else if (m_orders == OrderFollow && m_absorb)
      {
        m_orders = OrderNone;
        m_absorb = false;
      }
      else if (m_orders == OrderGoto)
        m_orders = OrderNone;
      break;
    }

    CancelOrderSounds();

    switch (m_orders)
    {
    case OrderNone:
      g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "SetOrderNone");
      break;
    case OrderGoto:
      g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "SetOrderGoto");
      break;
    case OrderFollow:
      if (m_absorb)
        g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "SetOrderAbsorb");
      else
        g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "SetOrderFollow");
      break;
    }

    m_buildingId = -1;
  }
}

void Officer::SetPreviousMode()
{
  static float lastOrderSet = 0.0f;

  float timeNow = GetHighResTime();
  int researchLevel = g_globalWorld->m_research->CurrentLevel(GlobalResearch::TypeOfficer);

  if (timeNow > lastOrderSet + 0.3f)
  {
    lastOrderSet = timeNow;

    switch (researchLevel)
    {
    case 0:
    case 1:
    case 2:
      m_orders = OrderNone;
      break;

    case 3:
      if (m_orders == OrderNone)
        m_orders = OrderFollow;
      else if (m_orders == OrderFollow)
        m_orders = OrderNone;
      else if (m_orders == OrderGoto)
        m_orders = OrderNone;
      break;

    case 4:
      if (m_orders == OrderNone)
      {
        m_orders = OrderFollow;
        m_absorb = true;
      }
      else if (m_orders == OrderFollow && !m_absorb)
        m_orders = OrderNone;
      else if (m_orders == OrderFollow && m_absorb)
      {
        m_orders = OrderFollow;
        m_absorb = false;
      }
      else if (m_orders == OrderGoto)
        m_orders = OrderNone;
      break;
    }

    CancelOrderSounds();

    switch (m_orders)
    {
    case OrderNone:
      g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "SetOrderNone");
      break;
    case OrderGoto:
      g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "SetOrderGoto");
      break;
    case OrderFollow:
      if (m_absorb)
        g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "SetOrderAbsorb");
      else
        g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "SetOrderFollow");
      break;
    }

    m_buildingId = -1;
  }
}

char const* Officer::GetOrderType(int _orderType)
{
  static char const* orders[] = {"None", "Goto", "Follow"};

  return orders[_orderType];
}

// ============================================================================

OfficerOrders::OfficerOrders()
  : WorldObject(),
    m_arrivedTimer(-1.0f)
{
  m_type = EffectOfficerOrders;
}


bool OfficerOrders::Advance()
{
  if (m_arrivedTimer >= 0.0f)
  {
    // We are already here, so just fade out
    m_vel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_arrivedTimer += SERVER_ADVANCE_PERIOD;
    if (m_arrivedTimer > 1.0f)
      return true;
  }
  else
  {
    float speed = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&m_vel)));
    speed *= 1.1f;
    speed = std::max(speed, 30.0f);
    speed = std::min(speed, 150.0f);

    DirectX::XMVECTOR const toWaypoint =
      DirectX::XMVectorSetY(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_wayPoint), DirectX::XMLoadFloat3(&m_pos)), 0.0f);
    float distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(toWaypoint));

    DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(DirectX::XMVector3Normalize(toWaypoint), speed));

    DirectX::XMFLOAT3 const oldPos = m_pos;
    DirectX::XMStoreFloat3(&m_pos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD),
                                                                DirectX::XMLoadFloat3(&m_pos)));

    float landHeight = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);
    m_pos.y = landHeight + 2.0f;

    DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&oldPos)),
                                                          1.0f / SERVER_ADVANCE_PERIOD));

    g_particleSystem->CreateParticle(oldPos, DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), Particle::TypeMuzzleFlash, 30.0f);

    if (DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&m_vel))) * SERVER_ADVANCE_PERIOD > distance)
      m_arrivedTimer = 0.0f;
    if (distance < 3.0f)
      m_arrivedTimer = 0.0f;
  }

  return false;
}


void OfficerOrders::Render(float _time)
{
  float size = 15.0f;
  DirectX::XMFLOAT3 predictedPos;
  DirectX::XMStoreFloat3(
    &predictedPos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(_time), DirectX::XMLoadFloat3(&m_pos)));

  float alpha = 0.7f;
  if (m_arrivedTimer >= 0.0f)
  {
    float fraction = 1.0f - (m_arrivedTimer + _time);
    fraction = std::max(fraction, 0.0f);
    fraction = std::min(fraction, 1.0f);
    // alpha = 0.7f * fraction;
    size *= fraction;
  }

  DirectX::XMFLOAT3 const cameraUp = g_camera->GetUp();
  DirectX::XMFLOAT3 const cameraRight = g_camera->GetRight();

  DirectX::XMVECTOR const camUp = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&cameraUp), size);
  DirectX::XMVECTOR const camRight = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&cameraRight), size);

  glColor4f(1.0f, 0.3f, 1.0f, alpha);

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Starburst.bmp"));
  glDisable(GL_DEPTH_TEST);

  glBegin(GL_QUADS);
  glTexCoord2i(0, 0);
  EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&predictedPos), camRight), camUp));
  glTexCoord2i(1, 0);
  EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&predictedPos), camRight), camUp));
  glTexCoord2i(1, 1);
  EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&predictedPos), camRight), camUp));
  glTexCoord2i(0, 1);
  EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&predictedPos), camRight), camUp));
  glEnd();

  glEnable(GL_DEPTH_TEST);
  glDisable(GL_TEXTURE_2D);
}
