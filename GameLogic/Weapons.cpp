#include "pch.h"
#include "SoundSources.h"

#include <math.h>

#include "MathUtils.h"
#include "Resource.h"
#include "Profiler.h"
#include "GlVertex.h"
#include "Shape.h"
#include "HiResTime.h"
#include "DebugRender.h"

#include "Weapons.h"
#include "InsertionSquad.h"
#include "Airstrike.h"
#include "Citizen.h"
#include "Armour.h"


#include "EntityGrid.h"
#include "ObstructionGrid.h"
#include "Location.h"
#include "ParticleSystem.h"
#include "Team.h"
#include "Unit.h"
#include "GlobalWorld.h"
#include "TaskManager.h"

#include "SoundSystem.h"
#include "SoundLibrary3d.h"
#include "WorldPointers.h"


// ****************************************************************************
//  Class ThrowableWeapon
// ****************************************************************************

ThrowableWeapon::ThrowableWeapon(int _type, DirectX::XMFLOAT3 const& _startPos, DirectX::XMFLOAT3 const& _front, float _force)
  : m_shape(nullptr),
    m_force(1.0f),
    m_numFlashes(0)
{
  m_shape = g_resource->GetShape("Throwable.shp");
  m_pos = _startPos;
  DirectX::XMVECTOR const front = DirectX::XMLoadFloat3(&_front);
  DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(front, _force));

  m_up = _front;
  // operator^ was the cross product. m_front held g_upVector for exactly the one
  // line that read it, so g_XMIdentityR1 stands in for it and the assignment goes.
  DirectX::XMVECTOR const right = DirectX::XMVector3Cross(DirectX::g_XMIdentityR1, front);
  DirectX::XMStoreFloat3(&m_front, DirectX::XMVector3Cross(right, front));

  m_birthTime = GetHighResTime();

  m_type = _type;
}


void ThrowableWeapon::Initialise() { TriggerSoundEvent("Create"); }


void ThrowableWeapon::TriggerSoundEvent(char const* _event)
{
  switch (m_type)
  {
  case EffectThrowableGrenade:
  case EffectThrowableAirstrikeMarker:
  case EffectThrowableControllerGrenade:
    g_soundSystem->TriggerOtherEvent(SoundSourceOf(this), _event, SoundSourceBlueprint::TypeGrenade);
    break;

  case EffectThrowableAirstrikeBomb:
    g_soundSystem->TriggerOtherEvent(SoundSourceOf(this), _event, SoundSourceBlueprint::TypeAirstrikeBomb);
    break;
  }
}


bool ThrowableWeapon::Advance()
{
  if (m_type != EffectThrowableAirstrikeMarker && syncfrand() > 0.2f)
  {
    DirectX::XMFLOAT3 vel;
    DirectX::XMStoreFloat3(&vel, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), 1.0f / 4.0f));
    vel.x += syncsfrand(5.0f);
    vel.y += syncsfrand(5.0f);
    vel.z += syncsfrand(5.0f);

    DirectX::XMFLOAT3 trailPos;
    DirectX::XMStoreFloat3(&trailPos,
                           DirectX::XMVectorNegativeMultiplySubtract(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD),
                                                                     DirectX::XMLoadFloat3(&m_pos)));
    g_particleSystem->CreateParticle(trailPos, vel, Particle::TypeRocketTrail, 40.0f);
  }

  if (m_force > 0.1f)
  {
    m_vel.y -= 9.8f;
    DirectX::XMStoreFloat3(&m_pos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD),
                                                                DirectX::XMLoadFloat3(&m_pos)));

    float landHeight = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);
    if (m_pos.y < landHeight + 1.0f)
    {
      BounceOffLandscape();
      m_force *= 0.75f;
      DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), m_force));
      if (m_pos.y < landHeight + 1.0f)
        m_pos.y = landHeight + 1.0f;
      TriggerSoundEvent("Bounce");
    }
  }

  return false;
}


void ThrowableWeapon::Render(float _predictionTime)
{
  _predictionTime -= SERVER_ADVANCE_PERIOD;

  // operator^ was the cross product.
  DirectX::XMVECTOR up = DirectX::XMLoadFloat3(&m_up);
  DirectX::XMVECTOR const right = DirectX::XMVector3Cross(up, DirectX::XMLoadFloat3(&m_front));

  // RotateAround took its angle from the axis magnitude and did nothing below
  // 1e-8 squared.
  DirectX::XMVECTOR const rotation = DirectX::XMVectorScale(right, m_force * m_force * 0.15f);
  float const rotationLengthSquared = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(rotation));
  if (rotationLengthSquared >= 1e-8f)
  {
    float const spin = sqrtf(rotationLengthSquared);
    up = DirectX::XMVector3Transform(up, DirectX::XMMatrixRotationAxis(DirectX::XMVectorScale(rotation, 1.0f / spin), spin));
  }

  DirectX::XMStoreFloat3(&m_front, DirectX::XMVector3Normalize(DirectX::XMVector3Cross(right, up)));
  DirectX::XMStoreFloat3(&m_up, DirectX::XMVector3Normalize(up));

  DirectX::XMVECTOR const predictedPos =
    DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(_predictionTime), DirectX::XMLoadFloat3(&m_pos));

  DirectX::XMFLOAT4X4 transform;
  DirectX::XMStoreFloat4x4(&transform, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::XMLoadFloat3(&m_up), predictedPos));

  g_renderer->SetObjectLighting();
  glEnable(GL_CULL_FACE);
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_COLOR_MATERIAL);
  glDisable(GL_BLEND);

  m_shape->Render(_predictionTime, transform);

  glEnable(GL_BLEND);
  glDisable(GL_COLOR_MATERIAL);
  g_renderer->UnsetObjectLighting();


  int numFlashes = int(GetHighResTime() - m_birthTime);
  if (numFlashes > m_numFlashes)
  {
    TriggerSoundEvent("Flash");
    m_numFlashes = numFlashes;
  }

  float flashAlpha = 1.0f - ((GetHighResTime() - m_birthTime) - numFlashes);
  if (flashAlpha < 0.2f)
  {
    // Hoisted out of the eight vertex expressions that each called one.
    DirectX::XMFLOAT3 const camPosStore = g_camera->GetPos();
    DirectX::XMFLOAT3 const camUpStore = g_camera->GetUp();
    DirectX::XMFLOAT3 const camRightStore = g_camera->GetRight();
    DirectX::XMVECTOR const camUp = DirectX::XMLoadFloat3(&camUpStore);
    DirectX::XMVECTOR const camRight = DirectX::XMLoadFloat3(&camRightStore);

    float distToThrowable =
      DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&camPosStore), predictedPos)));

    float size = 1000.0f / sqrtf(distToThrowable);
    glColor4ub(m_colour.r, m_colour.g, m_colour.b, 200);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Starburst.bmp"));
    glDisable(GL_CULL_FACE);
    glBegin(GL_QUADS);
    glTexCoord2i(0, 0);
    EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camUp, DirectX::XMVectorReplicate(size), predictedPos));
    glTexCoord2i(1, 0);
    EmitVertex(DirectX::XMVectorMultiplyAdd(camRight, DirectX::XMVectorReplicate(size), predictedPos));
    glTexCoord2i(1, 1);
    EmitVertex(DirectX::XMVectorMultiplyAdd(camUp, DirectX::XMVectorReplicate(size), predictedPos));
    glTexCoord2i(0, 1);
    EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camRight, DirectX::XMVectorReplicate(size), predictedPos));
    glEnd();
    size *= 0.4f;
    glColor4f(1.0f, 1.0f, 1.0f, 0.7f);
    glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Starburst.bmp"));
    glBegin(GL_QUADS);
    glTexCoord2i(0, 1);
    EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camUp, DirectX::XMVectorReplicate(size), predictedPos));
    glTexCoord2i(1, 1);
    EmitVertex(DirectX::XMVectorMultiplyAdd(camRight, DirectX::XMVectorReplicate(size), predictedPos));
    glTexCoord2i(1, 0);
    EmitVertex(DirectX::XMVectorMultiplyAdd(camUp, DirectX::XMVectorReplicate(size), predictedPos));
    glTexCoord2i(0, 0);
    EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camRight, DirectX::XMVectorReplicate(size), predictedPos));
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_CULL_FACE);
  }
}

float ThrowableWeapon::GetMaxForce(int _researchLevel)
{
  switch (_researchLevel)
  {
  case 0:
    return 50.0f;
  case 1:
    return 80.0f;
  case 2:
    return 110.0f;
  case 3:
    return 140.0f;
  case 4:
    return 170.0f;
  default:
    return 0.0f;
  }
}

float ThrowableWeapon::GetApproxMaxRange(float _maxForce) { return _maxForce + (_maxForce * 0.75f) + (_maxForce * 0.75f * 0.75f); }

// ****************************************************************************
//  Class Grenade
// ****************************************************************************

Grenade::Grenade(DirectX::XMFLOAT3 const& _startPos, DirectX::XMFLOAT3 const& _front, float _force)
  : ThrowableWeapon(EffectThrowableGrenade, _startPos, _front, _force),
    m_life(3.0f),
    m_power(25.0f)
{
  m_colour.Set(255, 100, 100);
}


bool Grenade::Advance()
{
  m_life -= SERVER_ADVANCE_PERIOD;

  if (m_life <= 0.0f)
  {
    TriggerSoundEvent("Explode");

    g_location->Bang(m_pos, m_power / 2.0f, m_power * 2.0f);
    return true;
  }

  return ThrowableWeapon::Advance();
}


// ****************************************************************************
//  Class AirStrikeMarker
// ****************************************************************************


AirStrikeMarker::AirStrikeMarker(DirectX::XMFLOAT3 const& _startPos, DirectX::XMFLOAT3 const& _front, float _force)
  : ThrowableWeapon(EffectThrowableAirstrikeMarker, _startPos, _front, _force)
{
  m_colour.Set(255, 100, 100);
}


bool AirStrikeMarker::Advance()
{
  ThrowableWeapon::Advance();


  //
  // Dump loads of smoke for the bombers to see

  DirectX::XMFLOAT3 vel;
  DirectX::XMStoreFloat3(&vel, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), 1.0f / 4.0f));
  vel.x += syncsfrand(5.0f);
  vel.y += syncsfrand(5.0f);
  vel.z += syncsfrand(5.0f);
  g_particleSystem->CreateParticle(m_pos, vel, Particle::TypeFire, 100.0f);


  //
  // If its time, summon the actual airstrike

  if (GetHighResTime() > m_birthTime + 2.0f)
  {
    if (m_airstrikeUnit.GetTeamId() != -1 && m_airstrikeUnit.GetUnitId() != -1)
    {
      // Air strike unit has been created
      Unit* unit = g_location->GetUnit(m_airstrikeUnit);
      if (!unit)
      {
        m_airstrikeUnit.SetInvalid();
        return true;
      }
      AirstrikeUnit* airStrikeUnit = (AirstrikeUnit*)unit;
      if (airStrikeUnit->m_state == AirstrikeUnit::StateLeaving)
      {
        m_airstrikeUnit.SetInvalid();
        return true;
      }
    }
    else
    {
      // Summon an air strike now
      int teamId = g_globalWorld->m_myTeamId;
      int unitId;
      Team* team = g_location->GetMyTeam();

      int airStikeResearch = g_globalWorld->m_research->CurrentLevel(GlobalResearch::TypeAirStrike);
      AirstrikeUnit* unit = (AirstrikeUnit*)team->NewUnit(Entity::TypeSpaceInvader, airStikeResearch, &unitId, m_pos);
      unit->m_effectId = m_id.GetIndex();
      m_airstrikeUnit.Set(teamId, unitId, -1, -1);
    }
  }

  return false;
}


// ****************************************************************************
//  Class ControllerGrenade
// ****************************************************************************

ControllerGrenade::ControllerGrenade(DirectX::XMFLOAT3 const& _startPos, DirectX::XMFLOAT3 const& _front, float _force)
  : ThrowableWeapon(EffectThrowableControllerGrenade, _startPos, _front, _force)
{
  m_colour.Set(100, 100, 255);
}


bool ControllerGrenade::Advance()
{
  ThrowableWeapon::Advance();

  if (GetHighResTime() > m_birthTime + 3.0f)
  {
    g_soundSystem->TriggerOtherEvent(SoundSourceOf(this), "ExplodeController", SoundSourceBlueprint::TypeGrenade);

    int numFlashes = 5 + speciesRandom() % 5;
    for (int i = 0; i < numFlashes; ++i)
    {
      // One constructor call on purpose: the three draws are ordered by the
      // compiler's argument evaluation, not by this line.
      DirectX::XMFLOAT3 const vel(sfrand(15.0f), frand(35.0f), sfrand(15.0f));
      g_particleSystem->CreateParticle(m_pos, vel, Particle::TypeControlFlash, 100.0f);
    }

    Task* currentTask = g_taskManager->GetCurrentTask();
    if (currentTask && currentTask->m_type == GlobalResearch::TypeSquad)
    {
      Unit* owner = g_location->GetUnit(currentTask->m_objId);
      if (owner && owner->m_troopType == Entity::TypeInsertionSquadie)
      {
        InsertionSquad* squad = (InsertionSquad*)owner;

        int numFound;
        WorldObjectId* ids = g_location->m_entityGrid->GetFriends(m_pos.x, m_pos.z, 50.0f, &numFound, m_id.GetTeamId());
        for (int i = 0; i < numFound; ++i)
        {
          WorldObjectId id = ids[i];
          Entity* entity = g_location->GetEntity(id);
          if (entity && entity->m_type == Entity::TypeCitizen)
          {
            Citizen* citizen = (Citizen*)entity;
            citizen->TakeControl(squad->m_controllerId);
          }
        }
      }
    }

    return true;
  }

  return false;
}


// ****************************************************************************
//  Class Rocket
// ****************************************************************************

Rocket::Rocket(DirectX::XMFLOAT3 _startPos, DirectX::XMFLOAT3 _targetPos)
  : m_target(_targetPos),
    m_fromTeamId(255)
{
  m_pos = _startPos;
  m_pos.y += 2.0f;

  // Native Normalise behaviour, per NeuronMath.h. A rocket fired at its own
  // muzzle is the only zero-length input, and FireRocket's target comes from a
  // landscape or entity hit rather than from the firing position.
  DirectX::XMStoreFloat3(
    &m_vel, DirectX::XMVectorScale(
              DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&_targetPos), DirectX::XMLoadFloat3(&m_pos))), 50.0f));

  m_shape = g_resource->GetShape("Throwable.shp");

  m_timer = GetHighResTime();
  m_type = EffectRocket;
}


void Rocket::Initialise() { g_soundSystem->TriggerOtherEvent(SoundSourceOf(this), "Create", SoundSourceBlueprint::TypeRocket); }


bool Rocket::Advance()
{
  //
  // Move a little bit

  if (DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&m_vel))) < 150.0f)
  {
    DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), 1.2f));
  }

  DirectX::XMStoreFloat3(&m_pos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD),
                                                              DirectX::XMLoadFloat3(&m_pos)));


  //
  // Create smoke trail

  if (syncfrand() > 0.2f)
  {
    DirectX::XMFLOAT3 vel;
    DirectX::XMStoreFloat3(&vel, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), 1.0f / 10.0f));
    vel.x += syncsfrand(4.0f);
    vel.y += syncsfrand(4.0f);
    vel.z += syncsfrand(4.0f);

    DirectX::XMFLOAT3 trailPos;
    DirectX::XMStoreFloat3(&trailPos,
                           DirectX::XMVectorNegativeMultiplySubtract(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD),
                                                                     DirectX::XMLoadFloat3(&m_pos)));
    g_particleSystem->CreateParticle(trailPos, vel, Particle::TypeFire);
  }


  //
  // Have we run out of steam?

  int rocketResearch = g_globalWorld->m_research->CurrentLevel(GlobalResearch::TypeRocket);
  float maxLife = 0.0f;
  switch (rocketResearch)
  {
  case 0:
    maxLife = 0.25f;
    break;
  case 1:
    maxLife = 0.5f;
    break;
  case 2:
    maxLife = 0.75f;
    break;
  case 3:
    maxLife = 1.0f;
    break;
  case 4:
    maxLife = 1.5f;
    break;
  }

  if (GetHighResTime() > m_timer + maxLife)
  {
    g_location->Bang(m_pos, 15.0f, 25.0f);
    g_soundSystem->StopAllSounds(m_id, "Rocket Create");
    g_soundSystem->TriggerOtherEvent(SoundSourceOf(this), "Explode", SoundSourceBlueprint::TypeRocket);
    return true;
  }


  //
  // Have we hit the ground?

  if (g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z) >= m_pos.y)
  {
    g_location->Bang(m_pos, 15.0f, 25.0f);
    g_soundSystem->TriggerOtherEvent(SoundSourceOf(this), "Explode", SoundSourceBlueprint::TypeRocket);
    return true;
  }


  //
  // Have we hit any buildings?

  std::vector<int> const* buildings = g_location->m_obstructionGrid->GetBuildings(m_pos.x, m_pos.z);

  for (int buildingId : *buildings)
  {
    Building* building = g_location->GetBuilding(buildingId);
    if (building->DoesSphereHit(m_pos, 3.0f))
    {
      g_location->Bang(m_pos, 15.0f, 25.0f);
      g_soundSystem->TriggerOtherEvent(SoundSourceOf(this), "Explode", SoundSourceBlueprint::TypeRocket);
      return true;
    }
  }

  return false;
}


void Rocket::Render(float predictionTime)
{
  DirectX::XMVECTOR const predictedPos =
    DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(predictionTime), DirectX::XMLoadFloat3(&m_pos));

  DirectX::XMVECTOR const front = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&m_vel));
  // operator^ was the cross product; g_XMIdentityR1 is the (0,1,0,0) that
  // g_upVector holds.
  DirectX::XMVECTOR const right = DirectX::XMVector3Cross(front, DirectX::g_XMIdentityR1);
  DirectX::XMVECTOR const up = DirectX::XMVector3Cross(right, front);

  DirectX::XMFLOAT4X4 transform;
  DirectX::XMStoreFloat4x4(&transform, BasisFromFrontAndUp(front, up, predictedPos));

  g_renderer->SetObjectLighting();
  glEnable(GL_CULL_FACE);
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_COLOR_MATERIAL);
  glDisable(GL_BLEND);

  m_shape->Render(predictionTime, transform);

  glEnable(GL_BLEND);
  glDisable(GL_COLOR_MATERIAL);
  g_renderer->UnsetObjectLighting();

  for (int i = 0; i < 5; ++i)
  {
    DirectX::XMFLOAT3 vel;
    DirectX::XMStoreFloat3(&vel, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), 1.0f / -20.0f));
    vel.x += sfrand(8.0f);
    vel.y += sfrand(8.0f);
    vel.z += sfrand(8.0f);

    DirectX::XMFLOAT3 pos;
    DirectX::XMStoreFloat3(
      &pos, DirectX::XMVectorNegativeMultiplySubtract(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(0.05f + frand(0.05f)), predictedPos));
    pos.x += sfrand(8.0f);
    pos.y += sfrand(8.0f);
    pos.z += sfrand(8.0f);
    float size = 50.0f + frand(50.0f);
    g_particleSystem->CreateParticle(pos, vel, Particle::TypeMissileFire, size);
  }
}


// ****************************************************************************
//  Class Laser
// ****************************************************************************

void Laser::Initialise(float _lifeTime)
{
  m_life = _lifeTime;
  m_harmless = false;
  m_bounced = false;

  g_soundSystem->TriggerOtherEvent(SoundSourceOf(this), "Create", SoundSourceBlueprint::TypeLaser);
}


bool Laser::Advance()
{
  m_life -= SERVER_ADVANCE_PERIOD;
  if (m_life <= 0.0f)
  {
    return true;
  }

  DirectX::XMFLOAT3 const oldPos = m_pos;
  DirectX::XMStoreFloat3(&m_pos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD),
                                                              DirectX::XMLoadFloat3(&m_pos)));

  //
  // Detect collisions with landscape / buildings / people

  float landHeight = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);

  if (m_pos.y <= landHeight)
  {
    if (m_bounced)
    {
      // Second richochet, so die
      return true;
    }
    else
    {
      // Richochet
      // Braced to zero because RayHit's result is IGNORED and hitPoint is read
      // either way: Vector3's default constructor zeroed and XMFLOAT3's does not.
      DirectX::XMFLOAT3 hitPoint(0.0f, 0.0f, 0.0f);
      DirectX::XMFLOAT3 vel;
      DirectX::XMStoreFloat3(&vel, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&m_vel)));

      g_location->m_landscape.RayHit(oldPos, vel, &hitPoint);

      DirectX::XMVECTOR const velVec = DirectX::XMLoadFloat3(&m_vel);
      float distanceTravelled =
        DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&hitPoint), DirectX::XMLoadFloat3(&oldPos))));
      float distanceTotal = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorScale(velVec, SERVER_ADVANCE_PERIOD)));

      DirectX::XMFLOAT3 const normalStore = g_location->m_landscape.m_normalMap->GetValue(hitPoint.x, hitPoint.z);
      DirectX::XMVECTOR const normal = DirectX::XMLoadFloat3(&normalStore);
      DirectX::XMVECTOR const incomingVel = DirectX::XMVectorScale(velVec, -1.0f);

      // operator* between two Vector3s was the DOT product, not a scale.
      float dotProd = DirectX::XMVectorGetX(DirectX::XMVector3Dot(normal, incomingVel));
      DirectX::XMVECTOR const reflected = DirectX::XMVectorSubtract(DirectX::XMVectorScale(normal, 2.0f * dotProd), incomingVel);
      DirectX::XMStoreFloat3(&m_vel, reflected);

      // SetLength on the reflected velocity, which is non-zero whenever the
      // incoming one was.
      DirectX::XMVECTOR const distanceRemaining = DirectX::XMVectorScale(DirectX::XMVector3Normalize(reflected), distanceTotal - distanceTravelled);

      DirectX::XMStoreFloat3(&m_pos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&hitPoint), distanceRemaining));
      g_soundSystem->TriggerOtherEvent(SoundSourceOf(this), "Richochet", SoundSourceBlueprint::TypeLaser);

      m_bounced = true;
    }
  }
  else if (m_pos.x < 0 || m_pos.x > g_location->m_landscape.GetWorldSizeX() || m_pos.z < 0 || m_pos.z > g_location->m_landscape.GetWorldSizeZ())
  {
    // Outside game world
    return true;
  }
  else
  {
    //
    // Detect collisions with buildings

    DirectX::XMFLOAT3 rayDir;
    DirectX::XMStoreFloat3(&rayDir, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&m_vel)));
    // Pure out-parameters -- written by DoesRayHit and never read afterwards --
    // so they become native rather than needing a conversion at the call.
    DirectX::XMFLOAT3 hitPos(0.0f, 0.0f, 0.0f);
    DirectX::XMFLOAT3 hitNorm(0.0f, 0.0f, 0.0f);

    float const rayLen =
      DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), SERVER_ADVANCE_PERIOD)));

    std::vector<int> const* nearbyBuildings = g_location->m_obstructionGrid->GetBuildings(m_pos.x, m_pos.z);
    for (int buildingId : *nearbyBuildings)
    {
      Building* building = g_location->GetBuilding(buildingId);
      if (building->DoesRayHit(m_pos, rayDir, rayLen, &hitPos, &hitNorm))
      {
        DirectX::XMFLOAT3 vel;
        DirectX::XMStoreFloat3(&vel, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), -1.0f / 15.0f));
        vel.x += sfrand(10.0f);
        vel.y += sfrand(10.0f);
        vel.z += sfrand(10.0f);
        g_particleSystem->CreateParticle(m_pos, vel, Particle::TypeRocketTrail);
        g_soundSystem->TriggerOtherEvent(SoundSourceOf(this), "HitBuilding", SoundSourceBlueprint::TypeLaser);
        return true;
      }
    }


    //
    // Detect collisions with entities

    if (!m_harmless)
    {
      DirectX::XMVECTOR const halfDelta = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), SERVER_ADVANCE_PERIOD * 0.5f);
      DirectX::XMFLOAT3 rayStart;
      DirectX::XMFLOAT3 rayEnd;
      DirectX::XMStoreFloat3(&rayStart, DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), halfDelta));
      DirectX::XMStoreFloat3(&rayEnd, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), halfDelta));
      int numFound;
      float maxRadius = DirectX::XMVectorGetX(DirectX::XMVector3Length(halfDelta)) * 2.0f;
      WorldObjectId* ids = g_location->m_entityGrid->GetEnemies(m_pos.x, m_pos.z, maxRadius, &numFound, m_fromTeamId);
      for (int i = 0; i < numFound; ++i)
      {
        WorldObjectId id = ids[i];
        WorldObject* wobj = g_location->GetEntity(id);
        Entity* entity = (Entity*)wobj;

        // Vector2's converting constructor from a Vector3 dropped y; XMFLOAT2
        // has no such constructor, so the (x, z) projection is written out.
        DirectX::XMFLOAT2 const entityPos2D(entity->m_pos.x, entity->m_pos.z);
        DirectX::XMFLOAT2 const rayStart2D(rayStart.x, rayStart.z);
        DirectX::XMFLOAT2 const rayEnd2D(rayEnd.x, rayEnd.z);
        if (PointSegDist2D(entityPos2D, rayStart2D, rayEnd2D) < 10.0f)
        {
          g_soundSystem->TriggerOtherEvent(SoundSourceOf(this), "HitEntity", SoundSourceBlueprint::TypeLaser);
          if (entity->m_type == Entity::TypeSpider || entity->m_type == Entity::TypeSporeGenerator || entity->m_type == Entity::TypeEngineer ||
              entity->m_type == Entity::TypeTriffidEgg || entity->m_type == Entity::TypeSoulDestroyer || entity->m_type == Entity::TypeArmour)
          {
            // entity->ChangeHealth(-1);
          }
          else
          {
            float damage = 20.0f + syncfrand(20.0f);
            entity->ChangeHealth((int)-damage);

            // SetLength on the laser's own velocity, which is non-zero.
            DirectX::XMFLOAT3 push;
            DirectX::XMStoreFloat3(&push, DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&m_vel)), syncfrand(10.0f)));
            push.y = 7.0f + syncfrand(5.0f);
            if (entity->m_type != Entity::TypeInsertionSquadie && entity->m_type != Entity::TypeVirii)
            {
              DirectX::XMStoreFloat3(&entity->m_front, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&push)));
            }
            if (entity->m_type != Entity::TypeVirii)
            {
              DirectX::XMStoreFloat3(&entity->m_vel, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&entity->m_vel), DirectX::XMLoadFloat3(&push)));
            }
            // entity->m_onGround = false;
            m_harmless = true;
          }
          return false;
        }
      }
    }
  }

  return false;
}


void Laser::Render(float predictionTime)
{
  DirectX::XMVECTOR const predictedPos =
    DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(predictionTime), DirectX::XMLoadFloat3(&m_pos));

  //
  // No richochet occurred recently
  // SetLength on the laser's own velocity, which is non-zero.
  DirectX::XMVECTOR const lengthVector = DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&m_vel)), 10.0f);
  DirectX::XMVECTOR const fromPos = predictedPos;
  DirectX::XMVECTOR const toPos = DirectX::XMVectorSubtract(predictedPos, lengthVector);

  DirectX::XMVECTOR const midPoint = DirectX::XMVectorAdd(fromPos, DirectX::XMVectorScale(DirectX::XMVectorSubtract(toPos, fromPos), 0.5f));

  DirectX::XMFLOAT3 const camPosStore = g_camera->GetPos();
  DirectX::XMVECTOR const camToMidPoint = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&camPosStore), midPoint);
  float camDistSqd = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(camToMidPoint));

  // operator^ was the cross product.
  DirectX::XMVECTOR const rightAngle =
    DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMVector3Cross(camToMidPoint, DirectX::XMVectorSubtract(midPoint, toPos))), 0.8f);

  RGBAColour const& colour = g_location->m_teams[m_fromTeamId].m_colour;
  glColor4ub(colour.r, colour.g, colour.b, 255);

  glBegin(GL_QUADS);
  for (int i = 0; i < 5; ++i)
  {
    glTexCoord2i(0, 0);
    EmitVertex(DirectX::XMVectorSubtract(fromPos, rightAngle));
    glTexCoord2i(0, 1);
    EmitVertex(DirectX::XMVectorAdd(fromPos, rightAngle));
    glTexCoord2i(1, 1);
    EmitVertex(DirectX::XMVectorAdd(toPos, rightAngle));
    glTexCoord2i(1, 0);
    EmitVertex(DirectX::XMVectorSubtract(toPos, rightAngle));
  }
  glEnd();

  if (camDistSqd < 200.0f)
  {
    g_camera->CreateCameraShake(0.5f);
  }
}


// ****************************************************************************
// Class Shockwave
// ****************************************************************************

Shockwave::Shockwave(int _teamId, float _size)
  : m_teamId(_teamId),
    m_size(_size),
    m_life(_size),
    m_shape(nullptr)
{
  //    m_shape = g_resource->GetShape( "shockwave.shp" );
  m_type = EffectShockwave;
}


bool Shockwave::Advance()
{
  m_life -= SERVER_ADVANCE_PERIOD;
  if (m_life <= 0.0f)
  {
    return true;
  }

  float radius = 35.0f + 40.0f * (m_size - m_life);
  int numFound;
  WorldObjectId* ids;
  if (m_teamId != 255)
  {
    ids = g_location->m_entityGrid->GetEnemies(m_pos.x, m_pos.z, radius, &numFound, m_teamId);
  }
  else
  {
    ids = g_location->m_entityGrid->GetNeighbours(m_pos.x, m_pos.z, radius, &numFound);
  }

  for (int i = 0; i < numFound; ++i)
  {
    WorldObjectId id = ids[i];
    WorldObject* wobj = g_location->GetEntity(id);
    Entity* ent = (Entity*)wobj;
    float distance =
      DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&ent->m_pos), DirectX::XMLoadFloat3(&m_pos))));
    if (fabs(distance - radius) < 10.0f)
    {
      float fraction = (m_life / m_size);
      if (ent->m_onGround)
      {
        // Native Normalise behaviour: an entity exactly on the blast centre is
        // filtered out above, since fabs(distance - radius) < 10 needs a radius
        // of at most 10 and radius starts at 35.
        DirectX::XMFLOAT3 push;
        DirectX::XMStoreFloat3(
          &push, DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&ent->m_pos), DirectX::XMLoadFloat3(&m_pos))));
        push.y = 3.0f;
        DirectX::XMStoreFloat3(&push, DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&push)), 25.0f * fraction));
        DirectX::XMStoreFloat3(&ent->m_vel, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&ent->m_vel), DirectX::XMLoadFloat3(&push)));
        ent->m_onGround = false;
      }
      if (ent->m_type == Entity::TypeCitizen)
      {
        if (syncfrand() < 0.1f)
        {
          ((Citizen*)ent)->SetFire();
        }
        else
        {
          ent->ChangeHealth(-40 * fraction);
        }
      }
      else
      {
        ent->ChangeHealth(-40 * fraction);
      }
    }
  }

  return false;
}

void Shockwave::Render(float predictionTime)
{
  glShadeModel(GL_SMOOTH);
  glDisable(GL_DEPTH_TEST);

  glColor4f(1.0f, 1.0f, 0.0f, 1.0f);
  int numSteps = 50.0f;
  float predictedLife = m_life - predictionTime;
  float radius = 35.0f + 40.0f * (m_size - predictedLife);
  float alpha = 0.6f * predictedLife / m_size;

  glBegin(GL_TRIANGLE_FAN);
  glColor4f(1.0f, 1.0f, 0.0f, 0.0f);
  EmitVertex(DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMVectorSet(0.0f, 5.0f, 0.0f, 0.0f)));
  glColor4f(1.0f, 0.5f, 0.5f, alpha);

  float angle = 0.0f;
  for (int i = 0; i <= numSteps; ++i)
  {
    float xDiff = radius * sinf(angle);
    float zDiff = radius * cosf(angle);
    EmitVertex(DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMVectorSet(xDiff, 5.0f, zDiff, 0.0f)));
    angle += 2.0f * M_PI / (float)numSteps;
  }

  glEnd();

  glShadeModel(GL_FLAT);
  glEnable(GL_DEPTH_TEST);

  //
  // Render screen flash if we are new

  if (m_size - predictedLife < 0.1f)
  {
    if (g_camera->PosInViewFrustum(m_pos))
    {
      DirectX::XMFLOAT3 const camPosStore = g_camera->GetPos();
      float distance = DirectX::XMVectorGetX(
        DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&camPosStore), DirectX::XMLoadFloat3(&m_pos))));
      float distanceFactor = 1.0f - (distance / 500.0f);
      if (distanceFactor < 0.0f)
        distanceFactor = 0.0f;

      float alpha = 1.0f - (m_size - predictedLife) / 0.1f;
      alpha *= distanceFactor;
      glColor4f(1, 1, 1, alpha);
      g_renderer->SetupMatricesFor2D();
      int screenW = g_renderer->ScreenW();
      int screenH = g_renderer->ScreenH();
      glDisable(GL_CULL_FACE);
      glBegin(GL_QUADS);
      glVertex2i(0, 0);
      glVertex2i(screenW, 0);
      glVertex2i(screenW, screenH);
      glVertex2i(0, screenH);
      glEnd();
      glEnable(GL_CULL_FACE);
      g_renderer->SetupMatricesFor3D();

      g_camera->CreateCameraShake(alpha);
    }
  }


  //
  // Render big blast

  if (m_size - predictedLife < 1.0f)
  {
    // Hoisted out of the eight vertex expressions that each called one.
    DirectX::XMFLOAT3 const camPosStore = g_camera->GetPos();
    DirectX::XMFLOAT3 const camUpStore = g_camera->GetUp();
    DirectX::XMFLOAT3 const camRightStore = g_camera->GetRight();
    DirectX::XMVECTOR const camUp = DirectX::XMLoadFloat3(&camUpStore);
    DirectX::XMVECTOR const camRight = DirectX::XMLoadFloat3(&camRightStore);

    float distToBang =
      DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&camPosStore), DirectX::XMLoadFloat3(&m_pos))));
    DirectX::XMVECTOR const predictedPos = DirectX::XMLoadFloat3(&m_pos);
    float size = (m_size * 2000.0f) / sqrtf(distToBang);
    float alpha = 1.0f - (m_size - predictedLife) / 1.0f;
    glColor4f(1.0f, 0.4f, 0.4f, alpha);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Starburst.bmp"));
    glDisable(GL_CULL_FACE);
    glBegin(GL_QUADS);
    glTexCoord2i(0, 0);
    EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camUp, DirectX::XMVectorReplicate(size), predictedPos));
    glTexCoord2i(1, 0);
    EmitVertex(DirectX::XMVectorMultiplyAdd(camRight, DirectX::XMVectorReplicate(size), predictedPos));
    glTexCoord2i(1, 1);
    EmitVertex(DirectX::XMVectorMultiplyAdd(camUp, DirectX::XMVectorReplicate(size), predictedPos));
    glTexCoord2i(0, 1);
    EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camRight, DirectX::XMVectorReplicate(size), predictedPos));
    glEnd();

    size *= 0.4f;
    glColor4f(1.0f, 1.0f, 0.0f, alpha);
    glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Starburst.bmp"));
    glBegin(GL_QUADS);
    glTexCoord2i(0, 1);
    EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camUp, DirectX::XMVectorReplicate(size), predictedPos));
    glTexCoord2i(1, 1);
    EmitVertex(DirectX::XMVectorMultiplyAdd(camRight, DirectX::XMVectorReplicate(size), predictedPos));
    glTexCoord2i(1, 0);
    EmitVertex(DirectX::XMVectorMultiplyAdd(camUp, DirectX::XMVectorReplicate(size), predictedPos));
    glTexCoord2i(0, 0);
    EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camRight, DirectX::XMVectorReplicate(size), predictedPos));
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_CULL_FACE);
  }
}


// ****************************************************************************
// Class MuzzleFlash
// ****************************************************************************


MuzzleFlash::MuzzleFlash()
  : WorldObject(),
    m_size(1.0f),
    m_life(1.0f)
{
  m_type = EffectMuzzleFlash;
}


MuzzleFlash::MuzzleFlash(DirectX::XMFLOAT3 const& _pos, DirectX::XMFLOAT3 const& _front, float _size, float _life)
  : WorldObject(),
    m_front(_front),
    m_size(_size),
    m_life(_life)
{
  m_pos = _pos;
}


bool MuzzleFlash::Advance()
{
  if (m_life <= 0.0f)
    return true;

  DirectX::XMStoreFloat3(&m_pos,
                         DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_front), DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD * 10.0f),
                                                      DirectX::XMLoadFloat3(&m_pos)));
  m_life -= SERVER_ADVANCE_PERIOD * 10.0f;

  return false;
}


void MuzzleFlash::Render(float _predictionTime)
{
  float predictedLife = m_life - _predictionTime * 10.0f;
  // float predictedLife = m_life;
  DirectX::XMVECTOR const front = DirectX::XMLoadFloat3(&m_front);
  DirectX::XMVECTOR const predictedPos =
    DirectX::XMVectorMultiplyAdd(front, DirectX::XMVectorReplicate(_predictionTime * 10.0f), DirectX::XMLoadFloat3(&m_pos));

  // The legacy `right` and the two camera getters here were dead -- computed and
  // never read -- so they go rather than being converted.

  DirectX::XMVECTOR const fromPos = predictedPos;
  DirectX::XMVECTOR const toPos = DirectX::XMVectorMultiplyAdd(front, DirectX::XMVectorReplicate(m_size), predictedPos);

  DirectX::XMVECTOR const midPoint = DirectX::XMVectorAdd(fromPos, DirectX::XMVectorScale(DirectX::XMVectorSubtract(toPos, fromPos), 0.5f));

  DirectX::XMFLOAT3 const camPosStore = g_camera->GetPos();
  DirectX::XMVECTOR const camToMidPoint = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&camPosStore), midPoint);

  // operator^ was the cross product.
  DirectX::XMVECTOR rightAngle = DirectX::XMVectorScale(
    DirectX::XMVector3Normalize(DirectX::XMVector3Cross(camToMidPoint, DirectX::XMVectorSubtract(midPoint, toPos))), m_size * 0.5f);
  DirectX::XMVECTOR toPosToFromPos = DirectX::XMVectorSubtract(toPos, fromPos);

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/MuzzleFlash.bmp"));
  glDepthMask(false);

  float alpha = predictedLife;
  alpha = std::min(1.0f, alpha);
  alpha = std::max(0.0f, alpha);
  glColor4f(1.0f, 1.0f, 1.0f, alpha);

  glBegin(GL_QUADS);
  for (int i = 0; i < 5; ++i)
  {
    rightAngle = DirectX::XMVectorScale(rightAngle, 0.8f);
    toPosToFromPos = DirectX::XMVectorScale(toPosToFromPos, 0.8f);
    glTexCoord2i(0, 0);
    EmitVertex(DirectX::XMVectorSubtract(fromPos, rightAngle));
    glTexCoord2i(0, 1);
    EmitVertex(DirectX::XMVectorAdd(fromPos, rightAngle));
    glTexCoord2i(1, 1);
    EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(fromPos, toPosToFromPos), rightAngle));
    glTexCoord2i(1, 0);
    EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorAdd(fromPos, toPosToFromPos), rightAngle));
  }
  glEnd();

  glDisable(GL_TEXTURE_2D);
}


// ****************************************************************************
// Class Missile
// ****************************************************************************

Missile::Missile()
  : WorldObject()
{
  m_shape = g_resource->GetShape("Missile.shp");
  m_booster = m_shape->m_rootFragment->LookupMarker("MarkerBooster");

  int rocketResearch = g_globalWorld->m_research->CurrentLevel(GlobalResearch::TypeRocket);
  m_life = 5.0f + rocketResearch * 5.0f;
}


bool Missile::AdvanceToTargetPosition(DirectX::XMFLOAT3 const& _pos)
{
  float amountToTurn = SERVER_ADVANCE_PERIOD * 2.0f;
  DirectX::XMVECTOR const target = DirectX::XMLoadFloat3(&_pos);
  DirectX::XMVECTOR targetDir = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(target, DirectX::XMLoadFloat3(&m_pos)));

  // Look ahead to see if we're about to hit the ground
  DirectX::XMFLOAT3 forwardPos;
  DirectX::XMStoreFloat3(&forwardPos, DirectX::XMVectorMultiplyAdd(targetDir, DirectX::XMVectorReplicate(100.0f), DirectX::XMLoadFloat3(&m_pos)));
  float landHeight = g_location->m_landscape.m_heightMap->GetValue(forwardPos.x, forwardPos.z);
  if (forwardPos.y <= landHeight &&
      DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&forwardPos), target))) > 100.0f)
  {
    // g_upVector is still a Vector3; g_XMIdentityR1 is the (0,1,0,0) it holds.
    targetDir = DirectX::g_XMIdentityR1;
  }

  DirectX::XMVECTOR const actualDir = DirectX::XMVector3Normalize(DirectX::XMVectorAdd(
    DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_front), 1.0f - amountToTurn), DirectX::XMVectorScale(targetDir, amountToTurn)));
  float speed = 200.0f;

  DirectX::XMVECTOR const oldPos = DirectX::XMLoadFloat3(&m_pos);
  DirectX::XMFLOAT3 newPosStore;
  DirectX::XMStoreFloat3(&newPosStore, DirectX::XMVectorMultiplyAdd(actualDir, DirectX::XMVectorReplicate(speed * SERVER_ADVANCE_PERIOD), oldPos));
  landHeight = g_location->m_landscape.m_heightMap->GetValue(newPosStore.x, newPosStore.z);
  if (newPosStore.y <= landHeight)
    return true;

  DirectX::XMVECTOR moved = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&newPosStore), oldPos);
  if (DirectX::XMVectorGetX(DirectX::XMVector3Length(moved)) > speed * SERVER_ADVANCE_PERIOD)
  {
    // SetLength, guarded by the magnitude test above, so never zero-length.
    moved = DirectX::XMVectorScale(DirectX::XMVector3Normalize(moved), speed * SERVER_ADVANCE_PERIOD);
  }

  DirectX::XMVECTOR const newPos = DirectX::XMVectorAdd(oldPos, moved);
  DirectX::XMStoreFloat3(&m_pos, newPos);
  DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(DirectX::XMVectorSubtract(newPos, oldPos), 1.0f / SERVER_ADVANCE_PERIOD));
  DirectX::XMStoreFloat3(&m_front, actualDir);

  // operator^ was the cross product.
  DirectX::XMVECTOR const right = DirectX::XMVector3Cross(actualDir, DirectX::g_XMIdentityR1);
  DirectX::XMStoreFloat3(&m_up, DirectX::XMVector3Cross(right, actualDir));

  return DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(newPos, target))) < 20.0f;
}


bool Missile::Advance()
{
  bool dead = false;
  m_life -= SERVER_ADVANCE_PERIOD;
  if (m_life < 0.0f)
  {
    Explode();
    return true;
  }

  //    Tank *tank = (Tank *) g_location->GetEntitySafe( m_tankId, Entity::TypeTank );
  //    if( tank )
  //    {
  //        m_target = tank->GetMissileTarget();
  //    }
  //
  bool arrived = AdvanceToTargetPosition(m_target);
  if (arrived)
  {
    Explode();
    return true;
  }

  m_history.push_back(m_pos);

  //
  // Create smoke trail

  DirectX::XMFLOAT3 vel;
  DirectX::XMStoreFloat3(&vel, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), 1.0f / -20.0f));
  vel.x += syncsfrand(2.0f);
  vel.y += syncsfrand(2.0f);
  vel.z += syncsfrand(2.0f);
  float size = 50.0f + syncfrand(150.0f);
  float backPos = syncfrand(3.0f);

  DirectX::XMFLOAT4X4 mat;
  DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::XMLoadFloat3(&m_up), DirectX::XMLoadFloat3(&m_pos)));

  DirectX::XMFLOAT3 const boosterPosStore = m_booster->GetWorldPosition(mat);
  DirectX::XMVECTOR const boosterPos = DirectX::XMLoadFloat3(&boosterPosStore);
  DirectX::XMVECTOR const velVec = DirectX::XMLoadFloat3(&m_vel);

  DirectX::XMFLOAT3 trailA;
  DirectX::XMFLOAT3 trailB;
  DirectX::XMStoreFloat3(&trailA,
                         DirectX::XMVectorNegativeMultiplySubtract(velVec, DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD * 2.0f), boosterPos));
  DirectX::XMStoreFloat3(&trailB,
                         DirectX::XMVectorNegativeMultiplySubtract(velVec, DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD * 1.5f), boosterPos));
  g_particleSystem->CreateParticle(trailA, vel, Particle::TypeMissileTrail, size);
  g_particleSystem->CreateParticle(trailB, vel, Particle::TypeMissileTrail, size);

  return false;
}


void Missile::Explode() { g_location->Bang(m_pos, 20.0f, 100.0f); }


void Missile::Render(float _predictionTime)
{
  DirectX::XMVECTOR const predictedPos =
    DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(_predictionTime), DirectX::XMLoadFloat3(&m_pos));

  DirectX::XMFLOAT4X4 mat;
  DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::XMLoadFloat3(&m_up), predictedPos));

  glDisable(GL_BLEND);

  g_renderer->SetObjectLighting();
  glEnable(GL_CULL_FACE);
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_COLOR_MATERIAL);
  glDisable(GL_BLEND);

  m_shape->Render(_predictionTime, mat);
  g_renderer->UnsetObjectLighting();

  glEnable(GL_BLEND);
  glDisable(GL_COLOR_MATERIAL);
  glDisable(GL_CULL_FACE);

  m_fire.m_pos = m_booster->GetWorldPosition(mat);
  m_fire.m_vel = m_vel;
  m_fire.m_size = 30.0f + frand(20.0f);
  DirectX::XMStoreFloat3(&m_fire.m_front, DirectX::XMVectorNegate(DirectX::XMLoadFloat3(&m_front)));
  m_fire.Render(_predictionTime);

  for (int i = 0; i < 5; ++i)
  {
    DirectX::XMFLOAT3 vel;
    DirectX::XMStoreFloat3(&vel, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), 1.0f / -10.0f));
    vel.x += sfrand(8.0f);
    vel.y += sfrand(8.0f);
    vel.z += sfrand(8.0f);

    DirectX::XMFLOAT3 pos;
    DirectX::XMStoreFloat3(
      &pos, DirectX::XMVectorNegativeMultiplySubtract(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(0.1f + frand(0.1f)), predictedPos));
    pos.x += sfrand(8.0f);
    pos.y += sfrand(8.0f);
    pos.z += sfrand(8.0f);
    float size = 200.0f + frand(200.0f);
    g_particleSystem->CreateParticle(pos, vel, Particle::TypeMissileFire, size);
  }
}


// ****************************************************************************
// Class TurretShell
// ****************************************************************************

TurretShell::TurretShell(float _life)
  : WorldObject(),
    m_life(_life)
{
  m_type = EffectGunTurretShell;
}


bool TurretShell::Advance()
{
  //
  // Update our position

  DirectX::XMFLOAT3 const oldPos = m_pos;

  m_life -= SERVER_ADVANCE_PERIOD;
  // m_vel.y -= 10.0f * SERVER_ADVANCE_PERIOD;
  // m_vel.x *= ( 1.0f - SERVER_ADVANCE_PERIOD );
  // m_vel.z *= ( 1.0f - SERVER_ADVANCE_PERIOD );
  // if( m_vel.y > 0 ) m_vel.y *= ( 1.0f - SERVER_ADVANCE_PERIOD );
  DirectX::XMStoreFloat3(&m_pos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD),
                                                              DirectX::XMLoadFloat3(&m_pos)));

  if (m_life <= 0.0f)
  {
    return true;
  }


  if (m_pos.x < 0 || m_pos.x > g_location->m_landscape.GetWorldSizeX() || m_pos.z < 0 || m_pos.z > g_location->m_landscape.GetWorldSizeZ() ||
      m_pos.y < 0)
  {
    // Outside of world
    return true;
  }


  //
  // Did we hit anyone?

  DirectX::XMFLOAT3 centrePos;
  DirectX::XMStoreFloat3(&centrePos,
                         DirectX::XMVectorScale(DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&oldPos)), 0.5f));
  float radius =
    DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&oldPos)))) / 1.0f;
  int numFound;
  WorldObjectId* ids = g_location->m_entityGrid->GetNeighbours(centrePos.x, centrePos.z, radius, &numFound);

  for (int i = 0; i < numFound; ++i)
  {
    WorldObjectId id = ids[i];
    Entity* entity = g_location->GetEntity(id);
    if (entity)
    {
      DirectX::XMFLOAT3 rayDir;
      DirectX::XMStoreFloat3(&rayDir, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&m_vel)));
      if (entity->RayHit(oldPos, rayDir))
      {
        entity->ChangeHealth(-10);
        if (entity->m_onGround)
        {
          DirectX::XMFLOAT3 push;
          DirectX::XMStoreFloat3(
            &push, DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&entity->m_pos), DirectX::XMLoadFloat3(&m_pos))));
          // One constructor call on purpose: the three draws are ordered by the
          // compiler's argument evaluation, not by this line.
          DirectX::XMFLOAT3 const jitter(syncsfrand(3.0f), syncfrand(3.0f), syncsfrand(3.0f));
          push.x += jitter.x;
          push.y += jitter.y;
          push.z += jitter.z;
          DirectX::XMStoreFloat3(&push, DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&push)), 20.0f));
          DirectX::XMStoreFloat3(&entity->m_vel, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&entity->m_vel), DirectX::XMLoadFloat3(&push)));
          entity->m_onGround = false;
        }
      }
    }
  }


  //
  // Did we hit any buildings?

  {
    DirectX::XMFLOAT3 rayDir;
    DirectX::XMStoreFloat3(&rayDir, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&m_vel)));
    // Pure out-parameters -- written by DoesRayHit and never read afterwards --
    // so they become native rather than needing a conversion at the call.
    DirectX::XMFLOAT3 hitPos(0.0f, 0.0f, 0.0f);
    DirectX::XMFLOAT3 hitNorm(0.0f, 0.0f, 0.0f);

    float const rayLen =
      DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), SERVER_ADVANCE_PERIOD)));

    for (int i = 0; i < g_location->m_buildings.Size(); ++i)
    {
      if (g_location->m_buildings.ValidIndex(i))
      {
        Building* building = g_location->m_buildings.GetData(i);
        if (building->DoesRayHit(m_pos, rayDir, rayLen, &hitPos, &hitNorm))
        {
          for (int p = 0; p < 3; ++p)
          {
            DirectX::XMFLOAT3 vel;
            DirectX::XMStoreFloat3(&vel, DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(
                                                                  DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&building->m_centrePos))),
                                                                50.0f));
            vel.x += sfrand(10.0f);
            vel.y += frand(10.0f);
            vel.z += sfrand(10.0f);
            float size = 25.0f + frand(25.0f);
            g_particleSystem->CreateParticle(m_pos, vel, Particle::TypeRocketTrail, size);
          }
          building->Damage(-2);
          return true;
        }
      }
    }
  }


  //
  // Did we hit the landscape?

  float landHeight = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);

  if (m_pos.y <= landHeight)
  {
    for (int i = 0; i < 3; ++i)
    {
      DirectX::XMFLOAT3 vel = g_location->m_landscape.m_normalMap->GetValue(m_pos.x, m_pos.z);
      DirectX::XMStoreFloat3(&vel, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&vel), 50.0f));
      vel.x += sfrand(10.0f);
      vel.y += frand(10.0f);
      vel.z += sfrand(10.0f);
      float size = 25.0f + frand(25.0f);
      g_particleSystem->CreateParticle(oldPos, vel, Particle::TypeRocketTrail, size);
    }

    return true;
  }

  return false;
}


void TurretShell::Render(float predictionTime)
{
  predictionTime += SERVER_ADVANCE_PERIOD;

  // Vector3 predictedVel = m_vel;
  // predictedVel.y -= 30.0f * predictionTime;
  // Vector3 predictedPos = m_pos + predictedVel * predictionTime;
  // RenderSphere( predictedPos, 1.0f );


  DirectX::XMVECTOR const predictedPos =
    DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(predictionTime), DirectX::XMLoadFloat3(&m_pos));
  DirectX::XMVECTOR const predictedFront = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&m_vel));
  // operator^ was the cross product; g_XMIdentityR1 is the (0,1,0,0) that
  // g_upVector holds.
  DirectX::XMVECTOR const right = DirectX::XMVector3Cross(predictedFront, DirectX::g_XMIdentityR1);
  DirectX::XMVECTOR const up = DirectX::XMVector3Cross(right, predictedFront);
  Shape* shape = g_resource->GetShape("TurretShell.shp");


  DirectX::XMFLOAT4X4 shellMat;
  DirectX::XMStoreFloat4x4(&shellMat, BasisFromFrontAndUp(predictedFront, up, predictedPos));

  glDisable(GL_BLEND);
  g_renderer->SetObjectLighting();
  shape->Render(predictionTime, shellMat);
  g_renderer->UnsetObjectLighting();
  glEnable(GL_BLEND);
}
