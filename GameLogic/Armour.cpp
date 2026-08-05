#include "pch.h"
#include "SoundSources.h"
#include "Resource.h"
#include "Shape.h"
#include "HiResTime.h"
#include "MathUtils.h"
#include "Input.h"
#include "Armour.h"
#include "GunTurret.h"
#include "SoundSystem.h"
#include "Location.h"
#include "GameTime.h"
#include "ParticleSystem.h"
#include "Explosion.h"
#include "GlobalWorld.h"
#include "ObstructionGrid.h"
#include "Team.h"
#include "EntityGrid.h"
#include "WorldPointers.h"


namespace Species
{
  Armour::Armour()
    : Entity(),
      m_speed(0.0f),
      m_numPassengers(0),
      m_previousUnloadTimer(0.0f),
      m_newOrdersTimer(0.0f),
      m_state(StateIdle)
  {
    SetType(TypeArmour);

    m_shape = g_resource->GetShape("Armour.shp");
    m_markerEntrance = m_shape->m_rootFragment->LookupMarker("MarkerEntrance");
    m_markerFlag = m_shape->m_rootFragment->LookupMarker("MarkerFlag");

    DirectX::XMFLOAT4X4 identity; // was g_identityMatrix34
    DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixIdentity());
    m_centrePos = m_shape->CalculateCentre(identity);
    m_radius = m_shape->CalculateRadius(identity, m_centrePos);
  }

  Armour::~Armour()
  {
    if (m_id.GetTeamId() != 255)
    {
      Team* team = &g_location->m_teams[m_id.GetTeamId()];
      team->UnRegisterSpecial(m_id);
    }
  }

  void Armour::Begin()
  {
    Entity::Begin();

    m_wayPoint = m_pos;

    if (m_id.GetTeamId() != 255)
    {
      Team* team = &g_location->m_teams[m_id.GetTeamId()];
      team->RegisterSpecial(m_id);
    }

    m_flag.SetPosition(m_pos);
    m_flag.SetOrientation(m_front, m_up);
    m_flag.SetSize(20.0f);
    m_flag.Initialise();
  }

  void Armour::ChangeHealth(int _amount)
  {
    bool dead = m_dead;

    if (_amount < 0 && _amount > -1000)
      _amount = -1;

    if (!m_dead)
    {
      if (_amount < 0)
        g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "LoseHealth");

      int oldHealth = m_stats[StatHealth];
      int newHealth = oldHealth + _amount;
      newHealth = std::max(newHealth, 0);
      newHealth = std::min(newHealth, 255);
      m_stats[StatHealth] = newHealth;

      int healthBandBefore = static_cast<int>(oldHealth / 20.0f);
      int healthBandAfter = static_cast<int>(newHealth / 20.0f);

      if (healthBandBefore != healthBandAfter || newHealth == 0)
      {
        float fractionDead = 1.0f - static_cast<float>(newHealth) / EntityBlueprint::GetStat(TypeArmour, StatHealth);
        DirectX::XMFLOAT4X4 bodyMat;
        DirectX::XMStoreFloat4x4(&bodyMat,
                                 BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::XMLoadFloat3(&m_up), DirectX::XMLoadFloat3(&m_pos)));
        g_explosionManager.AddExplosion(m_shape, bodyMat, fractionDead);
      }

      if (newHealth == 0)
      {
        m_dead = true;
        g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "Die");
      }
    }
  }

  void Armour::ConvertToGunTurret()
  {
    GunTurret turretTemplate;
    turretTemplate.m_pos = m_pos;
    turretTemplate.m_front = m_front;
    turretTemplate.m_dynamic = true;

    auto turret = static_cast<GunTurret*>(Building::CreateBuilding(Building::TypeGunTurret));
    g_location->m_buildings.PutData(turret);
    turret->Initialise(&turretTemplate);
    int id = g_globalWorld->GenerateBuildingId();
    turret->m_id.SetUnitId(UNIT_BUILDINGS);
    turret->m_id.SetUniqueId(id);
    g_location->m_obstructionGrid->CalculateAll();

    //
    // Explode some polys, to cover the ropey change
    DirectX::XMFLOAT4X4 bodyMat;
    DirectX::XMStoreFloat4x4(&bodyMat,
                             BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::XMLoadFloat3(&m_up), DirectX::XMLoadFloat3(&m_pos)));
    g_explosionManager.AddExplosion(m_shape, bodyMat);
    turret->ExplodeBody();
  }

  void Armour::AdvanceToTargetPos()
  {
    //
    // Turn to face our waypoint

    DirectX::XMVECTOR const wayPoint = DirectX::XMLoadFloat3(&m_wayPoint);
    DirectX::XMVECTOR const ourPos = DirectX::XMLoadFloat3(&m_pos);

    // HorizontalAndNormalise: flatten to the XZ plane, then normalise.
    DirectX::XMVECTOR const toTarget = DirectX::XMVector3Normalize(DirectX::XMVectorSetY(DirectX::XMVectorSubtract(wayPoint, ourPos), 0.0f));
    float angle = acosf(DirectX::XMVectorGetX(DirectX::XMVector3Dot(toTarget, DirectX::XMLoadFloat3(&m_front))));

    if (DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(wayPoint, ourPos))) > 10.0f)
    {
      // SetLength then RotateAround, and the zero-length fallback is LOAD-BEARING
      // exactly as it is in GunTurret's aiming: when the tank faces precisely
      // away from its waypoint the cross product is zero, and the fallback's
      // rotation about world X is what breaks the deadlock. Taking the native
      // QNaN would leave the 1e-8 guard below to skip the turn forever.
      DirectX::XMVECTOR front = DirectX::XMLoadFloat3(&m_front);
      DirectX::XMVECTOR const rotationAxis = DirectX::XMVector3Cross(front, toTarget);
      float const axisLength = DirectX::XMVectorGetX(DirectX::XMVector3Length(rotationAxis));
      float const rotateSpeed = angle * 0.05f;
      DirectX::XMVECTOR const rotation = NearlyEquals(axisLength, 0.0f) ? DirectX::XMVectorSet(rotateSpeed, 0.0f, 0.0f, 0.0f)
                                                                        : DirectX::XMVectorScale(rotationAxis, rotateSpeed / axisLength);

      // RotateAround's angle is the vector's magnitude, with the same 1e-8 guard.
      float const rotationLengthSquared = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(rotation));
      if (rotationLengthSquared >= 1e-8f)
      {
        float const spin = sqrtf(rotationLengthSquared);
        front = DirectX::XMVector3Transform(front, DirectX::XMMatrixRotationAxis(DirectX::XMVectorScale(rotation, 1.0f / spin), spin));
      }
      DirectX::XMStoreFloat3(&m_front, DirectX::XMVector3Normalize(front));
    }

    //
    // Move towards waypoint

    float distance = DirectX::XMVectorGetX(
      DirectX::XMVector3Length(DirectX::XMVectorSetY(DirectX::XMVectorSubtract(wayPoint, DirectX::XMLoadFloat3(&m_pos)), 0.0f)));
    if (distance > 100.0f && angle < 1.0f)
    {
      m_speed += 10.0f * SERVER_ADVANCE_PERIOD;
      m_speed = std::min(m_speed, static_cast<float>(m_stats[StatSpeed]));
    }
    else if (distance > 10.0f)
    {
      float targetSpeed = distance * 0.2f;
      m_speed = m_speed * 0.95f + targetSpeed * 0.05f;
      m_speed = std::min(m_speed, static_cast<float>(m_stats[StatSpeed]));
      m_speed = std::max(m_speed, 0.0f);
    }
    else
    {
      m_speed -= 5.0f * SERVER_ADVANCE_PERIOD;
      m_speed = std::max(m_speed, 0.0f);
    }

    //
    // Hover above the ground

    DirectX::XMFLOAT3 const oldPos = m_pos;
    DirectX::XMStoreFloat3(&m_pos,
                           DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_front), DirectX::XMVectorReplicate(m_speed * SERVER_ADVANCE_PERIOD),
                                                        DirectX::XMLoadFloat3(&m_pos)));
    PushFromObstructions(m_pos);
    float landHeight = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);
    if (m_pos.y <= landHeight)
    {
      float factor = SERVER_ADVANCE_PERIOD * 5.0f;
      m_pos.y = m_pos.y * (1.0f - factor) + landHeight * factor;

      factor = SERVER_ADVANCE_PERIOD * 5.0f;
      // SurfaceMap2D<Vector3> still returns a Vector3 -- Landscape belongs to T18.
      DirectX::XMFLOAT3 const landUp = g_location->m_landscape.m_normalMap->GetValue(m_pos.x, m_pos.z);
      DirectX::XMStoreFloat3(&m_up, DirectX::XMVectorLerp(DirectX::XMLoadFloat3(&m_up), DirectX::XMLoadFloat3(&landUp), factor));

      DirectX::XMVECTOR const old = DirectX::XMLoadFloat3(&oldPos);
      DirectX::XMVECTOR const travelled = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), old);
      float distTravelled = DirectX::XMVectorGetX(DirectX::XMVector3Length(travelled));
      if (distTravelled > m_speed * SERVER_ADVANCE_PERIOD)
      {
        // SetLength, guarded by the test above, so never zero-length here.
        DirectX::XMStoreFloat3(&m_pos, DirectX::XMVectorMultiplyAdd(DirectX::XMVector3Normalize(travelled),
                                                                    DirectX::XMVectorReplicate(m_speed * SERVER_ADVANCE_PERIOD), old));
      }
    }
    else
    {
      float heightAboveGround = m_pos.y - landHeight;
      m_vel.y -= 5.0f;
      m_pos.y += m_vel.y * SERVER_ADVANCE_PERIOD;
      m_pos.y = std::max(m_pos.y, landHeight);

      float factor = SERVER_ADVANCE_PERIOD * 0.5f;

      // SurfaceMap2D<Vector3> still returns a Vector3 -- Landscape belongs to T18.
      DirectX::XMFLOAT3 const landUp = g_location->m_landscape.m_normalMap->GetValue(m_pos.x, m_pos.z);
      DirectX::XMStoreFloat3(&m_up, DirectX::XMVectorLerp(DirectX::XMLoadFloat3(&m_up), DirectX::XMLoadFloat3(&landUp), factor));
    }

    DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&oldPos)),
                                                          1.0f / SERVER_ADVANCE_PERIOD));
  }

  void Armour::DetectCollisions()
  {
    DirectX::XMFLOAT3 pos;
    DirectX::XMStoreFloat3(&pos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&m_vel)));
    int numFound;
    WorldObjectId* neighbours = g_location->m_entityGrid->GetNeighbours(pos.x, pos.z, 30.0f, &numFound);

    // Vector3's default constructor zeroed this and XMFLOAT3's does not; it is
    // accumulated into below, so the zero is load-bearing.
    DirectX::XMVECTOR escapeVector = DirectX::XMVectorZero();
    bool collisionDetected = false;

    for (int i = 0; i < numFound; ++i)
    {
      Entity* ent = g_location->GetEntity(neighbours[i]);
      if (ent->m_type == TypeArmour && ent->m_id != m_id)
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
      DirectX::XMStoreFloat3(&m_wayPoint,
                             DirectX::XMVectorMultiplyAdd(escapeVector, DirectX::XMVectorReplicate(40.0f), DirectX::XMLoadFloat3(&m_pos)));
  }

  bool Armour::Advance(Unit* _unit)
  {
    if (m_dead)
      return true;

    m_newOrdersTimer += SERVER_ADVANCE_PERIOD;

    AdvanceToTargetPos();
    DetectCollisions();

    //
    // Create some smoke

    float velocity = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&m_vel)));
    int numSmoke = 1 + static_cast<int>(velocity / 20.0f);
    for (int i = 0; i < numSmoke; ++i)
    {
      // Every RNG call below stays in this order: they advance the synchronised
      // RNG and the migration may not change the sequence.
      DirectX::XMVECTOR vel = DirectX::XMLoadFloat3(&m_front);

      // RotateAround's angle is the vector's magnitude -- here m_up scaled by a
      // random angle, so the magnitude is |m_up| * angle, not the angle. Kept as
      // written, with RotateAround's own 1e-8 guard.
      DirectX::XMVECTOR const rotation = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_up), syncfrand(2.0f * M_PI));
      float const rotationLengthSquared = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(rotation));
      if (rotationLengthSquared >= 1e-8f)
      {
        float const spin = sqrtf(rotationLengthSquared);
        vel = DirectX::XMVector3Transform(vel, DirectX::XMMatrixRotationAxis(DirectX::XMVectorScale(rotation, 1.0f / spin), spin));
      }

      // SetLength on a rotated unit front, so never zero-length.
      vel = DirectX::XMVectorScale(DirectX::XMVector3Normalize(vel), syncfrand(5.0f));

      DirectX::XMFLOAT3 smokeVel;
      DirectX::XMStoreFloat3(&smokeVel, vel);

      DirectX::XMFLOAT3 pos;
      DirectX::XMStoreFloat3(&pos, DirectX::XMVectorMultiplyAdd(vel, DirectX::XMVectorReplicate(2.0f), DirectX::XMLoadFloat3(&m_pos)));
      pos.y += 3.0f;

      float size = 50.0f + (syncrand() % 50);
      g_particleSystem->CreateParticle(pos, smokeVel, Particle::TypeMissileTrail, size);
    }

    //
    // Are we supposed to be converting here?

    // Was `m_conversionPoint != g_zeroVector`, which is Vector3::operator!= -- a
    // PER-COMPONENT NearlyEquals at 1e-6, not an exact comparison.
    if (!DirectX::XMVector3NearEqual(DirectX::XMLoadFloat3(&m_conversionPoint), DirectX::XMVectorZero(), DirectX::XMVectorReplicate(1e-6f)))
    {
      float distance = DirectX::XMVectorGetX(
        DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_conversionPoint), DirectX::XMLoadFloat3(&m_pos))));
      if (distance < 50.0f && m_numPassengers > 0)
      {
        // We are close and need to unload our passengers
        m_state = StateUnloading;
        m_newOrdersTimer = 2.0f;
      }

      if (distance < 10.0f && m_numPassengers == 0)
      {
        ConvertToGunTurret();
        return true;
      }
    }

    return Entity::Advance(_unit);
  }

  void Armour::SetOrders(DirectX::XMFLOAT3 const& _orders)
  {
    m_newOrdersTimer = 0.0f;

    float distance =
      DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&_orders), DirectX::XMLoadFloat3(&m_pos))));

    if (distance > 50.0f)
      SetConversionPoint(_orders);
    else
    {
      static float lastOrdersSet = 0.0;
      float timeNow = GetHighResTime();

      if (timeNow > lastOrdersSet + 0.3f)
      {
        ToggleLoading();
        lastOrdersSet = timeNow;
      }
    }
  }

  void Armour::SetDirectOrders()
  {
    static float lastOrdersSet = 0.0;
    float timeNow = GetHighResTime();

    if (timeNow > lastOrdersSet + 0.3f)
    {
      ToggleLoading();
      lastOrdersSet = timeNow;
    }
  }

  void Armour::SetWayPoint(DirectX::XMFLOAT3 const& _wayPoint)
  {
    m_conversionPoint = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

    m_wayPoint = _wayPoint;
    m_wayPoint.y = g_location->m_landscape.m_heightMap->GetValue(m_wayPoint.x, m_wayPoint.z);
  }

  void Armour::SetConversionPoint(DirectX::XMFLOAT3 const& _conversionPoint)
  {
    m_conversionPoint = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

    // SurfaceMap2D<Vector3> still returns a Vector3 -- Landscape belongs to T18.
    DirectX::XMFLOAT3 const landNormal = g_location->m_landscape.m_normalMap->GetValue(_conversionPoint.x, _conversionPoint.z);
    if (landNormal.y > 0.95f)
    {
      SetWayPoint(_conversionPoint);

      m_conversionPoint = _conversionPoint;
      m_conversionPoint.y = g_location->m_landscape.m_heightMap->GetValue(m_conversionPoint.x, m_conversionPoint.z);
    }
    else
    {
    }
  }

  bool Armour::IsLoading() { return (m_state == StateLoading && m_numPassengers < Capacity() && m_newOrdersTimer > 1.0f); }

  bool Armour::IsUnloading()
  {
    return (m_state == StateUnloading && m_numPassengers > 0 && m_newOrdersTimer > 1.0f &&
            GetHighResTime() >= m_previousUnloadTimer + ARMOUR_UNLOADPERIOD);
  }

  int Armour::Capacity()
  {
    int research = g_globalWorld->m_research->CurrentLevel(GlobalResearch::TypeArmour);
    switch (research)
    {
    case 0:
    case 1:
      return 10;
    case 2:
      return 20;
    case 3:
      return 30;
    case 4:
      return 40;
    }

    return 10;
  }

  void Armour::ToggleLoading()
  {
    switch (m_state)
    {
    case StateIdle:
      if (m_numPassengers > 0)
        m_state = StateUnloading;
      else
        m_state = StateLoading;
      break;

    case StateUnloading:
      m_state = StateLoading;
      break;

    case StateLoading:
      m_state = StateIdle;
      break;
    }
  }

  void Armour::AddPassenger()
  {
    ++m_numPassengers;

    g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "LoadCitizen");
  }

  void Armour::RemovePassenger()
  {
    --m_numPassengers;
    m_previousUnloadTimer = GetHighResTime();

    g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "UnloadCitizen");
  }

  void Armour::GetEntrance(DirectX::XMFLOAT3& _exitPos, DirectX::XMFLOAT3& _exitDir)
  {
    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::XMLoadFloat3(&m_up), DirectX::XMLoadFloat3(&m_pos)));

    DirectX::XMFLOAT4X4 const entranceMat = m_markerEntrance->GetWorldMatrix(mat);
    _exitPos = DirectX::XMFLOAT3(entranceMat._41, entranceMat._42, entranceMat._43);
    _exitDir = DirectX::XMFLOAT3(entranceMat._31, entranceMat._32, entranceMat._33);
  }

  void Armour::ListSoundEvents(std::vector<const char*>* _list)
  {
    Entity::ListSoundEvents(_list);

    _list->push_back("LoadCitizen");
    _list->push_back("UnloadCitizen");
  }

  /*
  void Armour::SetMissileTarget( Vector3 const &_startRay, Vector3 const &_rayDir )
  {
      //
      // Look for enemy entities

      WorldObjectId id = g_location->GetEntityId( _startRay, _rayDir, 1 );
      Entity *entity = g_location->GetEntity( id );
      if( entity )
      {
          m_missileTarget = AsLegacy(entity->m_pos)+AsLegacy(entity->m_centrePos);
          return;
      }


      //
      // Hit against the landscape

      Vector3 landscapeHit;
      bool hit = g_location->m_landscape.RayHit( _startRay, _rayDir, &landscapeHit );
      if( hit )
      {
          m_missileTarget = landscapeHit;
          return;
      }
  }


  Vector3 Armour::GetMissileTarget()
  {
      return m_missileTarget;
  }


  void Armour::LaunchMissile()
  {
      Missile *missile = new Missile();
      missile->m_pos = AsLegacy(m_pos) + Vector3(0,30,0);

      Vector3 front = (AsLegacy(m_front) + m_up).Normalise();
      Vector3 right = front ^ g_upVector;
      Vector3 up = (right ^ front).Normalise();
      front = (up ^ right).Normalise();
      missile->m_front = front;
      missile->m_up = up;
      missile->m_vel = front * 200.0f;
      missile->m_tankId = m_id;
      missile->m_target = m_missileTarget;
      g_location->m_effects.PutData( missile );
  }*/

  void Armour::Render(float _predictionTime)
  {
    if (m_dead)
      return;

    // #ifdef DEBUG_RENDER_ENABLED
    //     glDisable( GL_DEPTH_TEST );
    //     RenderArrow( m_pos+Vector3(0,10,0), m_wayPoint, 1.0f, RGBAColour(255,255,255,155) );
    //     if( m_attackTarget != g_zeroVector )
    //     {
    //         RenderArrow( m_pos+Vector3(0,10,0), m_attackTarget, 1.0f, RGBAColour(255,50,50,255) );
    //     }
    //     glEnable( GL_DEPTH_TEST );
    // #endif

    //
    // Work out our predicted position

    DirectX::XMFLOAT3 predictedPos;
    DirectX::XMStoreFloat3(&predictedPos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(_predictionTime),
                                                                       DirectX::XMLoadFloat3(&m_pos)));
    // predictedPos.y = g_location->m_landscape.m_heightMap->GetValue( predictedPos.x, predictedPos.z );
    // predictedPos.y = max( predictedPos.y, 0.0f );
    predictedPos.y += sinf(g_gameTime + m_id.GetUniqueId()) * 2;

    DirectX::XMFLOAT3 upWobble = m_up; // g_location->m_landscape.m_normalMap->GetValue( predictedPos.x, predictedPos.z );
    upWobble.x += sinf((g_gameTime + m_id.GetUniqueId()) * 2) * 0.05f;
    upWobble.z += cosf(g_gameTime + m_id.GetUniqueId()) * 0.05f;

    // right is built from the UNNORMALISED wobbled up, then up is normalised and
    // front rebuilt from the two. Order preserved.
    DirectX::XMVECTOR const right = DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&upWobble), DirectX::XMLoadFloat3(&m_front));
    DirectX::XMVECTOR const predictedUp = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&upWobble));
    DirectX::XMVECTOR const predictedFront = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(right, predictedUp));

    //
    // Render the tank body

    DirectX::XMMATRIX body = BasisFromFrontAndUp(predictedFront, predictedUp, DirectX::XMLoadFloat3(&predictedPos));

    if (m_renderDamaged)
    {
      glBlendFunc(GL_ONE, GL_ONE);
      glEnable(GL_BLEND);

      // r is row 0 and f is row 2, in the row-vector convention NeuronMath fixes.
      if (frand() > 0.5f)
        body.r[0] = DirectX::XMVectorScale(body.r[0], 1.0f + sinf(g_gameTime) * 0.5f);
      else
        body.r[2] = DirectX::XMVectorScale(body.r[2], 1.0f + sinf(g_gameTime) * 0.5f);
    }

    DirectX::XMFLOAT4X4 bodyMat;
    DirectX::XMStoreFloat4x4(&bodyMat, body);

    g_renderer->SetObjectLighting();
    m_shape->Render(_predictionTime, bodyMat);
    g_renderer->UnsetObjectLighting();

    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //
    // Render the flag

    float timeIndex = g_gameTime + m_id.GetUniqueId() * 10;
    DirectX::XMFLOAT3 const flagPos = m_markerFlag->GetWorldPosition(bodyMat);
    m_flag.SetPosition(flagPos);

    DirectX::XMFLOAT3 flagFront, flagUp;
    DirectX::XMStoreFloat3(&flagFront, DirectX::XMVectorNegate(predictedFront));
    DirectX::XMStoreFloat3(&flagUp, predictedUp);
    m_flag.SetOrientation(flagFront, flagUp);
    m_flag.SetSize(20.0f);

    switch (m_state)
    {
    case StateIdle:
      m_flag.SetTexture(g_resource->GetTexture("Icons/BannerNone.bmp"));
      break;
    case StateUnloading:
      m_flag.SetTexture(g_resource->GetTexture("Icons/BannerUnload.bmp"));
      break;
    case StateLoading:
      m_flag.SetTexture(g_resource->GetTexture("Icons/BannerFollow.bmp"));
      break;
    }

    m_flag.Render();

    if (m_numPassengers > 0)
    {
      m_flag.RenderText(2, 2, std::format("{}", m_numPassengers));
    }

    //
    // If we are about to deploy, render a flag at the target

    // Was `m_conversionPoint != g_zeroVector`, which is Vector3::operator!= -- a
    // PER-COMPONENT NearlyEquals at 1e-6, not an exact comparison.
    if (!DirectX::XMVector3NearEqual(DirectX::XMLoadFloat3(&m_conversionPoint), DirectX::XMVectorZero(), DirectX::XMVectorReplicate(1e-6f)))
    {
      DirectX::XMVECTOR const front =
        DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f), DirectX::XMMatrixRotationY(g_gameTime * 0.5f));

      // RotateAround's angle is the vector's magnitude, with its own 1e-8 guard.
      DirectX::XMVECTOR up = DirectX::g_XMIdentityR1;
      DirectX::XMVECTOR const rotation = DirectX::XMVectorScale(front, sinf(timeIndex * 3) * 0.3f);
      float const rotationLengthSquared = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(rotation));
      if (rotationLengthSquared >= 1e-8f)
      {
        float const spin = sqrtf(rotationLengthSquared);
        up = DirectX::XMVector3Transform(up, DirectX::XMMatrixRotationAxis(DirectX::XMVectorScale(rotation, 1.0f / spin), spin));
      }

      DirectX::XMFLOAT3 deployFront, deployUp;
      DirectX::XMStoreFloat3(&deployFront, front);
      DirectX::XMStoreFloat3(&deployUp, up);

      m_deployFlag.SetPosition(m_conversionPoint);
      m_deployFlag.SetOrientation(deployFront, deployUp);
      m_deployFlag.SetSize(30.0f);
      m_deployFlag.SetTexture(g_resource->GetTexture("Icons/BannerDeploy.bmp"));
      m_deployFlag.Render();
    }
  }
} // namespace Species
