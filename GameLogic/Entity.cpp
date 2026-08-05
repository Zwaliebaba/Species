#include "pch.h"
#include "SoundSources.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "Debug.h"
#include "DebugRender.h"
#include "MathUtils.h"
#include "Profiler.h"
#include "Resource.h"
#include "Shape.h"
#include "TextureUv.h"
#include "TextStreamReaders.h"
#include "LanguageTable.h"

#include "GameTime.h"
#include "SoundSystem.h"
#include "Unit.h"
#include "Location.h"
#include "EntityGrid.h"
#include "ObstructionGrid.h"
#include "RoutingSystem.h"
#include "LevelFile.h"

#include "Entity.h"
#include "RadarDish.h"
#include "Bridge.h"
#include "InsertionSquad.h"
#include "Engineer.h"
#include "Virii.h"
#include "InsertionSquad.h"
#include "LaserTrooper.h"
#include "Egg.h"
#include "SporeGenerator.h"
#include "Lander.h"
#include "Tripod.h"
#include "Centipede.h"
#include "Airstrike.h"
#include "Spider.h"
#include "Citizen.h"
#include "Officer.h"
#include "ArmyAnt.h"
#include "Armour.h"
#include "SoulDestroyer.h"
#include "Triffid.h"
#include "Ai.h"
#include "LaserFence.h"
#include "WorldPointers.h"
#include "AppState.h"


// ****************************************************************************
//  Class EntityBlueprint
// ****************************************************************************

char* EntityBlueprint::m_names[Entity::NumEntityTypes];
float EntityBlueprint::m_stats[Entity::NumEntityTypes][Entity::NumStats];


void EntityBlueprint::Initialise()
{
  TextReader* theFile = g_resource->GetTextReader("Stats.txt");
  ASSERT_TEXT(theFile && theFile->IsOpen(), "Couldn't open stats.txt");

  int entityIndex = 0;

  while (theFile->ReadLine())
  {
    if (!theFile->TokenAvailable())
      continue;
    ASSERT_TEXT(entityIndex < Entity::NumEntityTypes, "Too many entity blueprints defined");

    m_names[entityIndex] = strdup(theFile->GetNextToken());
    m_stats[entityIndex][0] = atof(theFile->GetNextToken());
    m_stats[entityIndex][1] = atof(theFile->GetNextToken());
    m_stats[entityIndex][2] = atof(theFile->GetNextToken());

    ++entityIndex;
  }

  delete theFile;
}

char* EntityBlueprint::GetName(unsigned char _type)
{
  DEBUG_ASSERT(_type < Entity::NumEntityTypes);
  return m_names[_type];
}

float EntityBlueprint::GetStat(unsigned char _type, int _stat)
{
  DEBUG_ASSERT(_type < Entity::NumEntityTypes);
  DEBUG_ASSERT(_stat < Entity::NumStats);

  if (_stat == Entity::StatSpeed)
  {
    if (_type != Entity::TypeSpaceInvader)
      return m_stats[_type][_stat] * (1.0f + (float)g_difficultyLevel / 10.0f);
  }

  return m_stats[_type][_stat];
}


// ****************************************************************************
//  Class Entity
// ****************************************************************************

Entity::Entity()
  : WorldObject(),
    m_formationIndex(-1),
    m_dead(false),
    m_reloading(0.0f),
    m_shape(nullptr),
    m_justFired(false),
    m_radius(0.0f),
    m_buildingId(-1),
    m_roamRange(0.0f),
    m_inWater(-1.0f),
    m_renderDamaged(false),
    m_routeId(-1),
    m_routeWayPointId(-1),
    m_routeTriggerDistance(10.0f)
{
  memset(m_stats, 0, NumStats * sizeof(m_stats[0]));
}


Entity::~Entity() {}


void Entity::SetType(unsigned char _type)
{
  m_type = _type;

  for (int i = 0; i < NumStats; ++i)
  {
    m_stats[i] = EntityBlueprint::GetStat(m_type, i);
  }


  m_reloading = syncfrand(m_stats[StatRate]);
}


void Entity::ChangeHealth(int amount)
{
  if (!m_dead)
  {
    if (amount < 0)
    {
      g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "LoseHealth");
    }

    if (m_stats[StatHealth] + amount <= 0)
    {
      m_stats[StatHealth] = 100;
      m_dead = true;
      g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "Die");
      DirectX::XMFLOAT3 spiritVel;
      DirectX::XMStoreFloat3(&spiritVel, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), 0.5f));
      g_location->SpawnSpirit(m_pos, spiritVel, m_id.GetTeamId(), m_id);
    }
    else if (m_stats[StatHealth] + amount > 255)
    {
      m_stats[StatHealth] = 255;
    }
    else
    {
      m_stats[StatHealth] += amount;
    }
  }
}


void Entity::Attack(DirectX::XMFLOAT3 const& pos)
{
  if (m_reloading == 0.0f)
  {
    // The two syncsfrand draws stay in this order and are still made
    // unconditionally, which is what the plan protects: the RNG call sequence
    // is not sanctioned to move even though the float results are.
    float const spreadX = syncsfrand(15.0f);
    float const spreadZ = syncsfrand(15.0f);
    DirectX::XMVECTOR const toPos = DirectX::XMVectorSet(pos.x + spreadX, pos.y, pos.z + spreadZ, 0.0f);

    DirectX::XMFLOAT3 fromPos = m_pos;
    fromPos.y += 2.0f;

    // SetLength(200) on a zero-length delta left (200,0,0) behind; scaling a
    // normalised vector yields zero instead, which is the native normalise
    // behaviour NeuronMath.h fixes. Reachable only if the entity is standing
    // exactly on its own aim point, two units below it.
    DirectX::XMVECTOR const delta = DirectX::XMVectorSubtract(toPos, DirectX::XMLoadFloat3(&fromPos));
    DirectX::XMFLOAT3 velocity;
    DirectX::XMStoreFloat3(&velocity, DirectX::XMVectorScale(DirectX::XMVector3Normalize(delta), 200.0f));
    g_location->FireLaser(fromPos, velocity, m_id.GetTeamId());

    m_reloading = m_stats[StatRate];
    m_justFired = true;

    g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "Attack");
  }
}


bool Entity::AdvanceDead(Unit* _unit)
{
  int newHealth = m_stats[StatHealth];
  if (m_onGround)
    newHealth -= 40 * SERVER_ADVANCE_PERIOD;
  else
    newHealth -= 20 * SERVER_ADVANCE_PERIOD;

  if (newHealth <= 0)
  {
    m_stats[StatHealth] = 0;
    return true;
  }
  else
  {
    m_stats[StatHealth] = newHealth;
  }

  return false;
}


int Entity::EnterTeleports(int _requiredId)
{
  std::vector<int> const* buildings = g_location->m_obstructionGrid->GetBuildings(m_pos.x, m_pos.z);

  for (int buildingId : *buildings)
  {
    if (_requiredId != -1 && _requiredId != buildingId)
    {
      // We are only permitted to enter building with id _requiredId
      continue;
    }

    Building* building = g_location->GetBuilding(buildingId);
    DEBUG_ASSERT(building);

    if (building->m_type == Building::TypeRadarDish)
    {
      RadarDish* radarDish = (RadarDish*)building;
      // AsLegacy reaches them without naming the type; the storage is native.
      DirectX::XMFLOAT3 entrancePos{0.0f, 0.0f, 0.0f};
      DirectX::XMFLOAT3 entranceFront{0.0f, 0.0f, 0.0f};
      radarDish->GetEntrance(entrancePos, entranceFront);

      DirectX::XMVECTOR const toEntrance = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&entrancePos));
      float const range = DirectX::XMVectorGetX(DirectX::XMVector3Length(toEntrance));
      float const facing = DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMLoadFloat3(&m_front), DirectX::XMLoadFloat3(&entranceFront)));
      if (radarDish->ReadyToSend() && range < 15.0f && facing < 0.0f)
      {
        WorldObjectId id(m_id);
        radarDish->EnterTeleport(id);
        g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "EnterTeleport");
        return buildingId;
      }
    }
    else if (building->m_type == Building::TypeBridge)
    {
      Bridge* bridge = (Bridge*)building;
      DirectX::XMFLOAT3 entrancePos{0.0f, 0.0f, 0.0f};
      DirectX::XMFLOAT3 entranceFront{0.0f, 0.0f, 0.0f};
      if (bridge->GetEntrance(entrancePos, entranceFront))
      {
        DirectX::XMVECTOR const toBridge = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&bridge->m_pos));
        float const range = DirectX::XMVectorGetX(DirectX::XMVector3Length(toBridge));
        float const facing = DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMLoadFloat3(&m_front), DirectX::XMLoadFloat3(&entranceFront)));
        if (bridge->ReadyToSend() && range < 25.0f && facing < 0.0f)
        {
          WorldObjectId id(m_id);
          bridge->EnterTeleport(id);
          g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "EnterTeleport");
          return buildingId;
        }
      }
    }
  }

  return -1;
}


void Entity::AdvanceInAir(Unit* _unit)
{
  // The literal was `Vector3(0, -15.0, 0)` — a DOUBLE, narrowed to float by the
  // constructor. XMVectorSet takes floats, so the gravity constant is spelled
  // as one; -15.0 is exactly representable, so this is a spelling change.
  DirectX::XMVECTOR const gravity = DirectX::XMVectorSet(0.0f, -15.0f, 0.0f, 0.0f);
  DirectX::XMVECTOR velocity =
    DirectX::XMVectorMultiplyAdd(gravity, DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD), DirectX::XMLoadFloat3(&m_vel));
  DirectX::XMStoreFloat3(&m_vel, velocity);
  DirectX::XMStoreFloat3(&m_pos,
                         DirectX::XMVectorMultiplyAdd(velocity, DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD), DirectX::XMLoadFloat3(&m_pos)));

  if (!m_dead)
  {
    float groundLevel = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z) + 0.1f;
    if (m_pos.y <= groundLevel)
    {
      m_onGround = true;
      m_vel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
      m_pos.y = groundLevel + 0.1f;
    }
    if (m_pos.y <= 0.0f /*seaLevel*/)
    {
      m_inWater = 0;
      m_vel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
    }
  }
}


void Entity::AdvanceInWater(Unit* _unit)
{
  m_inWater += SERVER_ADVANCE_PERIOD;
  if (m_inWater > 10.0f /* && m_type != TypeInsertionSquadie */)
  {
    ChangeHealth(-500);
  }

  DirectX::XMVECTOR targetPos = DirectX::XMVectorZero();
  if (_unit)
  {
    DirectX::XMFLOAT3 const wayPoint = _unit->GetWayPoint();
    DirectX::XMFLOAT3 const offset = _unit->GetFormationOffset(Unit::FormationRectangle, m_id.GetIndex());
    targetPos = DirectX::XMVectorSetY(DirectX::XMLoadFloat3(&wayPoint), 0.0f);
    targetPos = DirectX::XMVectorAdd(targetPos, DirectX::XMLoadFloat3(&offset));
  }

  // Was `targetPos != g_zeroVector`, which is Vector3::operator!= — a
  // PER-COMPONENT NearlyEquals at 1e-6, not an exact comparison, so
  // XMVector3NotEqual would be a different test. XMVector3NearEqual with an
  // epsilon of 1e-6 in every lane is the same one.
  DirectX::XMVECTOR const epsilon = DirectX::XMVectorReplicate(1e-6f);
  if (!DirectX::XMVector3NearEqual(targetPos, DirectX::XMVectorZero(), epsilon))
  {
    DirectX::XMVECTOR const distance = DirectX::XMVectorSetY(DirectX::XMVectorSubtract(targetPos, DirectX::XMLoadFloat3(&m_pos)), 0.0f);

    // Normalise then scale, which is what `m_vel = distance; m_vel.Normalise()`
    // did — except that the legacy Normalise answered (0,0,1) for a zero-length
    // input and the native one does not. Reachable here: an entity standing
    // exactly on its own waypoint. It now gets a zero velocity and a zero
    // front rather than being pointed at +z, and SetLength below turns the zero
    // velocity into a zero rather than into (5,0,0).
    DirectX::XMVECTOR const direction = DirectX::XMVector3Normalize(distance);
    DirectX::XMStoreFloat3(&m_front, direction);

    DirectX::XMVECTOR velocity = DirectX::XMVectorScale(direction, m_stats[StatSpeed] * 0.3f);

    float const distanceLength = DirectX::XMVectorGetX(DirectX::XMVector3Length(distance));
    float const speed = DirectX::XMVectorGetX(DirectX::XMVector3Length(velocity));
    if (distanceLength < speed * SERVER_ADVANCE_PERIOD)
    {
      velocity = DirectX::XMVectorScale(distance, 1.0f / SERVER_ADVANCE_PERIOD);
    }
    DirectX::XMStoreFloat3(&m_vel, velocity);
  }

  DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&m_vel)), 5.0f));
  DirectX::XMStoreFloat3(&m_pos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD),
                                                              DirectX::XMLoadFloat3(&m_pos)));
  m_pos.y = 0.0f - sinf(m_inWater * 3.0f) - 4.0f;

  float groundLevel = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);
  if (groundLevel > 0.0f)
  {
    m_inWater = -1;
  }
}


void Entity::Begin()
{
  g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "Create");

  if (m_shape)
  {
    DirectX::XMFLOAT4X4 const identity(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    m_centrePos = m_shape->CalculateCentre(identity);
    m_radius = m_shape->CalculateRadius(identity, m_centrePos);
  }
}


bool Entity::Advance(Unit* _unit)
{
  if (!m_enabled)
    return false;

  if (m_dead)
  {
    bool amIDead = AdvanceDead(_unit);
    if (amIDead)
    {
      return true;
    }
  }

  // m_pos = PushFromObstructions( m_pos );
  // m_pos = PushFromEachOther( m_pos );

  if (m_reloading > 0.0f)
  {
    m_reloading -= SERVER_ADVANCE_PERIOD;
    if (m_reloading < 0.0f)
      m_reloading = 0.0f;
  }

  float healthFraction = (float)m_stats[StatHealth] / (float)EntityBlueprint::GetStat(m_type, StatHealth);
  float timeIndex = g_gameTime + m_id.GetUniqueId() * 10;
  m_renderDamaged = (frand(0.75f) * (1.0f - fabs(sinf(timeIndex)) * 0.8f) > healthFraction);

  if (m_inWater != -1)
    AdvanceInWater(_unit);

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

  if (_unit && !m_dead)
  {
    _unit->UpdateEntityPosition(m_pos, m_radius);
  }

  if (m_routeId != -1)
  {
    FollowRoute();
  }

  return false;
}


void Entity::Render(float predictionTime) {}


bool Entity::IsInView() { return true; }


DirectX::XMFLOAT3 Entity::PushFromEachOther(DirectX::XMFLOAT3 const& _pos)
{
  DirectX::XMVECTOR const source = DirectX::XMLoadFloat3(&_pos);
  DirectX::XMVECTOR result = source;

  int numFound;
  WorldObjectId* neighbours = g_location->m_entityGrid->GetNeighbours(_pos.x, _pos.z, 2.0f, &numFound);


  for (int i = 0; i < numFound; ++i)
  {
    WorldObjectId id = neighbours[i];
    if (!(id == m_id))
    {
      WorldObject* obj = g_location->GetEntity(id);
      //            float distance = (obj->m_pos - thisPos).Mag();
      //            while( distance < 1.0f )
      //            {
      //                Vector3 pushForce = (obj->m_pos - thisPos).Normalise() * 0.1f;
      //                if( obj->m_pos == thisPos ) pushForce = Vector3(0.0f,0.0f,1.0f);
      //                thisPos -= pushForce;
      //                distance = (obj->m_pos - thisPos).Mag();
      //            }

      DirectX::XMVECTOR const diff = DirectX::XMVectorSubtract(source, DirectX::XMLoadFloat3(&obj->m_pos));
      float const force = 0.1f / DirectX::XMVectorGetX(DirectX::XMVector3Length(diff));

      result = DirectX::XMVectorMultiplyAdd(DirectX::XMVector3Normalize(diff), DirectX::XMVectorReplicate(force), result);
    }
  }

  DirectX::XMFLOAT3 pushed;
  DirectX::XMStoreFloat3(&pushed, result);
  pushed.y = g_location->m_landscape.m_heightMap->GetValue(pushed.x, pushed.z);
  return pushed;
}


DirectX::XMFLOAT3 Entity::PushFromCliffs(DirectX::XMFLOAT3 const& pos, DirectX::XMFLOAT3 const& oldPos)
{
  DirectX::XMVECTOR const old = DirectX::XMLoadFloat3(&oldPos);

  DirectX::XMVECTOR horiz = DirectX::XMVectorSetY(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&pos), old), 0.0f);
  float horizDistance = DirectX::XMVectorGetX(DirectX::XMVector3Length(horiz));
  float gradient = (pos.y - oldPos.y) / horizDistance;

  DirectX::XMFLOAT3 result = pos;

  if (gradient > 1.0f)
  {
    float distance = 0.0f;
    while (distance < 50.0f)
    {
      float angle = distance * 2.0f * M_PI;
      DirectX::XMVECTOR const offset = DirectX::XMVectorSet(cosf(angle) * distance, 0.0f, sinf(angle) * distance, 0.0f);

      DirectX::XMFLOAT3 newPos;
      DirectX::XMStoreFloat3(&newPos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&result), offset));
      newPos.y = g_location->m_landscape.m_heightMap->GetValue(newPos.x, newPos.z);

      horiz = DirectX::XMVectorSetY(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&newPos), old), 0.0f);
      horizDistance = DirectX::XMVectorGetX(DirectX::XMVector3Length(horiz));
      float newGradient = (newPos.y - oldPos.y) / horizDistance;
      if (newGradient < 1.0f)
      {
        result = newPos;
        break;
      }
      distance += 0.2f;
    }
  }

  return result;
}


DirectX::XMFLOAT3 Entity::PushFromObstructions(DirectX::XMFLOAT3 const& pos, bool killem)
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
    float pushAngle = syncsfrand(1.0f);
    float distance = 0.0f;
    while (distance < 50.0f)
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
        break;
      }
      distance += 1.0f;
    }
  }


  //
  // Push from buildings

  std::vector<int> const* buildings = g_location->m_obstructionGrid->GetBuildings(result.x, result.z);

  for (int buildingId : *buildings)
  {
    Building* building = g_location->GetBuilding(buildingId);
    if (building)
    {
      bool hit = false;
      if (m_shape && building->DoesShapeHit(m_shape, transform))
        hit = true;
      if (!m_shape && building->DoesSphereHit(result, 1.0f))
        hit = true;

      if (hit)
      {
        if (building->m_type == Building::TypeLaserFence && killem && ((LaserFence*)building)->IsEnabled())
        {
          ChangeHealth(-9999);
          ((LaserFence*)building)->Electrocute(m_pos);
        }
        else
        {
          // SetLength(2) on a zero-length delta left (2,0,0); scaling a
          // normalised vector gives zero instead, so an entity standing exactly
          // on a building's origin no longer gets pushed along +x — it stops
          // being pushed at all, and this loop would then not terminate. Guarded
          // rather than left to the native normalise, because the old fallback
          // was load-bearing for the loop below and nothing said so.
          DirectX::XMVECTOR const toResult = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&result), DirectX::XMLoadFloat3(&building->m_pos));
          DirectX::XMVECTOR pushForce = DirectX::XMVectorScale(DirectX::XMVector3Normalize(toResult), -2.0f);
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
    }
  }

  return result;
}


static float s_nearPlaneStart;

void Entity::BeginRenderShadow()
{
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Glow.bmp"));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glDisable(GL_CULL_FACE);
  glDepthMask(false);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_COLOR);
  glColor4f(0.6f, 0.6f, 0.6f, 0.0f);

  s_nearPlaneStart = g_renderer->GetNearPlane();
  g_camera->SetupProjectionMatrix(s_nearPlaneStart * 1.05f, g_renderer->GetFarPlane());
}


void Entity::EndRenderShadow()
{
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_BLEND);

  glDepthMask(true);
  glEnable(GL_CULL_FACE);
  glDisable(GL_TEXTURE_2D);

  g_camera->SetupProjectionMatrix(s_nearPlaneStart, g_renderer->GetFarPlane());
}


void Entity::RenderShadow(DirectX::XMFLOAT3 const& _pos, float _size)
{
  DirectX::XMVECTOR const shadowPos = DirectX::XMLoadFloat3(&_pos);
  DirectX::XMVECTOR const shadowR = DirectX::XMVectorSet(_size, 0.0f, 0.0f, 0.0f);
  DirectX::XMVECTOR const shadowU = DirectX::XMVectorSet(0.0f, 0.0f, _size, 0.0f);

  DirectX::XMFLOAT3 posA;
  DirectX::XMFLOAT3 posB;
  DirectX::XMFLOAT3 posC;
  DirectX::XMFLOAT3 posD;
  DirectX::XMStoreFloat3(&posA, DirectX::XMVectorSubtract(DirectX::XMVectorSubtract(shadowPos, shadowR), shadowU));
  DirectX::XMStoreFloat3(&posB, DirectX::XMVectorSubtract(DirectX::XMVectorAdd(shadowPos, shadowR), shadowU));
  DirectX::XMStoreFloat3(&posC, DirectX::XMVectorAdd(DirectX::XMVectorAdd(shadowPos, shadowR), shadowU));
  DirectX::XMStoreFloat3(&posD, DirectX::XMVectorAdd(DirectX::XMVectorSubtract(shadowPos, shadowR), shadowU));

  posA.y = g_location->m_landscape.m_heightMap->GetValue(posA.x, posA.z) + 0.9f;
  posB.y = g_location->m_landscape.m_heightMap->GetValue(posB.x, posB.z) + 0.9f;
  posC.y = g_location->m_landscape.m_heightMap->GetValue(posC.x, posC.z) + 0.9f;
  posD.y = g_location->m_landscape.m_heightMap->GetValue(posD.x, posD.z) + 0.9f;

  posA.y = std::max(posA.y, 1.0f);
  posB.y = std::max(posB.y, 1.0f);
  posC.y = std::max(posC.y, 1.0f);
  posD.y = std::max(posD.y, 1.0f);

  if (posA.y > _pos.y && posB.y > _pos.y && posC.y > _pos.y && posD.y > _pos.y)
  {
    // The object casting the shadow is beneath the ground
    return;
  }

  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 0.0f);
  glVertex3fv(&posA.x);
  glTexCoord2f(1.0f, 0.0f);
  glVertex3fv(&posB.x);
  glTexCoord2f(1.0f, 1.0f);
  glVertex3fv(&posC.x);
  glTexCoord2f(0.0f, 1.0f);
  glVertex3fv(&posD.x);
  glEnd();
}


Entity* Entity::NewEntity(int _troopType)
{
  Entity* entity = nullptr;

  switch (_troopType)
  {
  case Entity::TypeLaserTroop:
    entity = new LaserTrooper();
    break;
  case Entity::TypeInsertionSquadie:
    entity = new Squadie();
    break;
  case Entity::TypeEngineer:
    entity = new Engineer();
    break;
  case Entity::TypeVirii:
    entity = new Virii();
    break;
  case Entity::TypeEgg:
    entity = new Egg();
    break;
  case Entity::TypeSporeGenerator:
    entity = new SporeGenerator();
    break;
  case Entity::TypeLander:
    entity = new Lander();
    break;
  case Entity::TypeTripod:
    entity = new Tripod();
    break;
  case Entity::TypeCentipede:
    entity = new Centipede();
    break;
  case Entity::TypeSpaceInvader:
    entity = new SpaceInvader();
    break;
  case Entity::TypeSpider:
    entity = new Spider();
    break;
  case Entity::TypeCitizen:
    entity = new Citizen();
    break;
  case Entity::TypeOfficer:
    entity = new Officer();
    break;
  case Entity::TypeArmyAnt:
    entity = new ArmyAnt();
    break;
  case Entity::TypeArmour:
    entity = new Armour();
    break;
  case Entity::TypeSoulDestroyer:
    entity = new SoulDestroyer();
    break;
  case Entity::TypeTriffidEgg:
    entity = new TriffidEgg();
    break;
  case Entity::TypeAI:
    entity = new AI();
    break;

  default:
    DEBUG_ASSERT(false);
  }

  entity->m_id.GenerateUniqueId();

  return entity;
}


int Entity::GetTypeId(char const* _typeName)
{
  for (int i = 0; i < NumEntityTypes; ++i)
  {
    if (stricmp(_typeName, GetTypeName(i)) == 0)
    {
      return i;
    }
  }
  return -1;
}


char const* Entity::GetTypeName(int _troopType)
{
  static char const* typeNames[NumEntityTypes] = {
    "InvalidType",  "LaserTroop", "Engineer", "Virii",   "Squadie", "Egg",    "SporeGenerator", "Lander",     "Tripod", "Centipede",
    "SpaceInvader", "Spider",     "Citizen",  "Officer", "ArmyAnt", "Armour", "SoulDestroyer",  "TriffidEgg", "AI"};

  DEBUG_ASSERT(_troopType >= 0 && _troopType < NumEntityTypes);
  return typeNames[_troopType];
}


char const* Entity::GetTypeNameTranslated(int _troopType)
{
  char const* typeName = GetTypeName(_troopType);

  char stringId[256];
  sprintf(stringId, "entityname_%s", typeName);

  if (ISLANGUAGEPHRASE(stringId))
  {
    return LANGUAGEPHRASE(stringId);
  }
  else
  {
    return typeName;
  }
}


void Entity::ListSoundEvents(std::vector<const char*>* _list)
{
  _list->push_back("Create");
  _list->push_back("Attack");
  _list->push_back("LoseHealth");
  _list->push_back("EnterTeleport");
  _list->push_back("ExitTeleport");
  _list->push_back("Die");
}


bool Entity::RayHit(DirectX::XMFLOAT3 const& _rayStart, DirectX::XMFLOAT3 const& _rayDir)
{
  if (m_shape)
  {
    RayPackage package(_rayStart, _rayDir);

    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));
    return m_shape->RayHit(&package, mat);
  }
  else
  {
    return RaySphereIntersection(_rayStart, _rayDir, m_pos, 5.0f);
  }
}


void Entity::DirectControl(TeamControls const& _teamControls) {}

DirectX::XMFLOAT3 Entity::GetCameraFocusPoint()
{
  DirectX::XMFLOAT3 focus;
  DirectX::XMStoreFloat3(&focus, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&m_vel)));
  return focus;
}

void Entity::SetWaypoint(DirectX::XMFLOAT3 const _waypoint) {}

void Entity::FollowRoute()
{
  DEBUG_ASSERT(m_routeId != -1);
  Route* route = g_location->m_levelFile->GetRoute(m_routeId);
  DEBUG_ASSERT(route);

  if (m_routeWayPointId == -1)
  {
    m_routeWayPointId = 0;
  }

  WayPoint* waypoint = route->m_wayPoints[m_routeWayPointId];

  // Copy-initialising the native type takes the seam without naming it.
  DirectX::XMFLOAT3 const wayPointPos = waypoint->GetPos();
  SetWaypoint(wayPointPos);

  DirectX::XMVECTOR const targetVect = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&wayPointPos), DirectX::XMLoadFloat3(&m_pos));

  m_spawnPoint = wayPointPos;

  if (waypoint->m_type != WayPoint::TypeBuilding && DirectX::XMVectorGetX(DirectX::XMVector3Length(targetVect)) < m_routeTriggerDistance)
  {
    m_routeWayPointId++;
    if (m_routeWayPointId >= static_cast<int>(route->m_wayPoints.size()))
    {
      m_routeWayPointId = -1;
      m_routeId = -1;
    }
  }

  //
  // If its a building instead of a 3D pos, this unit will never
  // get to the next waypoint.  A new unit is created when the unit
  // enters the teleport, and taht new unit will automatically
  // continue towards the next waypoint instead.
  //
}
