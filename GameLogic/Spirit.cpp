#include "pch.h"
#include "GlVertex.h"
#include "SoundSources.h"

#include <math.h>

#include "MathUtils.h"
#include "Debug.h"
#include "DebugRender.h"

#include "EntityGrid.h"
#include "ObstructionGrid.h"
#include "Location.h"
#include "Team.h"
#include "GlobalWorld.h"

#include "SoundSystem.h"

#include "Spirit.h"
#include "Virii.h"
#include "Egg.h"
#include "WorldPointers.h"
#include "AppState.h"


Spirit::Spirit()
  : WorldObject(),
    m_teamId(255),
    m_positionOffset(0.0f),
    m_state(StateUnknown),
    m_eggSearchTimer(0.0f),
    m_numNearbyEggs(0),
    m_pushFromBuildings(true)
{
}

Spirit::~Spirit() {}

void Spirit::Begin()
{
  m_timeSync = 6.0f;
  m_state = StateBirth;

  m_positionOffset = syncfrand(10.0f);
  m_xaxisRate = syncfrand(2.0f);
  m_yaxisRate = syncfrand(2.0f);
  m_zaxisRate = syncfrand(2.0f);

  m_numNearbyEggs = 0;
  m_eggSearchTimer = 0.0f;

  g_soundSystem->TriggerOtherEvent(SoundSourceOf(this), "Create", SoundSourceBlueprint::TypeSpirit);
}

bool Spirit::Advance()
{
  DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), 0.9f));

  if (m_state != StateAttached && m_state != StateInEgg)
  {
    //
    // Make me float around slowly

    m_timeSync -= SERVER_ADVANCE_PERIOD;
    m_positionOffset += SERVER_ADVANCE_PERIOD;
    m_xaxisRate += syncsfrand(1.0f);
    m_yaxisRate += syncsfrand(1.0f);
    m_zaxisRate += syncsfrand(1.0f);
    if (m_xaxisRate > 2.0f)
      m_xaxisRate = 2.0f;
    if (m_xaxisRate < 0.0f)
      m_xaxisRate = 0.0f;
    if (m_yaxisRate > 2.0f)
      m_yaxisRate = 2.0f;
    if (m_yaxisRate < 0.0f)
      m_yaxisRate = 0.0f;
    if (m_zaxisRate > 2.0f)
      m_zaxisRate = 2.0f;
    if (m_zaxisRate < 0.0f)
      m_zaxisRate = 0.0f;
    m_hover.x = sinf(m_positionOffset) * m_xaxisRate;
    m_hover.y = sinf(m_positionOffset) * m_yaxisRate;
    m_hover.z = sinf(m_positionOffset) * m_zaxisRate;

    switch (m_state)
    {
    case StateBirth:
    {
      m_hover.y = std::max(m_hover.y, 0.0f + syncfrand(0.5f));
      float heightAboveGround = m_pos.y - g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);
      if (heightAboveGround > 10.0f)
      {
        float fractionAboveGround = heightAboveGround / 100.0f;
        fractionAboveGround = std::min(fractionAboveGround, 1.0f);
        m_hover.y = (-10.0f - syncfrand(10.0f)) * fractionAboveGround;
      }
      else if (m_timeSync <= 0.0f)
      {
        m_state = StateFloating;
        m_timeSync = 120.0f + syncsfrand(60.0f);
      }
      break;
    }

    case StateFloating:
      if (m_timeSync <= 0.0f)
      {
        m_state = StateDeath;
        g_soundSystem->TriggerOtherEvent(SoundSourceOf(this), "BeginAscent", SoundSourceBlueprint::TypeSpirit);
        m_timeSync = 180.0f;
        AddToGlobalWorld();
      }
      break;

    case StateInStore:
      break;

    case StateDeath:
      m_hover.y = std::max(m_hover.y, 2.0f + syncfrand(2.0f));
      if (m_timeSync <= 0.0f)
      {
        // We are now dead
        return true;
      }
      break;
    };
  }

  DirectX::XMFLOAT3 oldPos = m_pos;

  if (m_pushFromBuildings && m_state != StateInStore && m_state != StateDeath)
  {
    PushFromBuildings();
  }

  DirectX::XMVECTOR pos = DirectX::XMLoadFloat3(&m_pos);
  pos = DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD), pos);
  pos = DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_hover), DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD), pos);
  DirectX::XMStoreFloat3(&m_pos, pos);
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

  if (m_state != StateInEgg && m_state != StateDeath)
  {
    float landHeight = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);
    if (m_pos.y < landHeight + 2.0f)
      m_pos.y = landHeight + 2.0f;
  }

  // m_vel = (m_pos - oldPos) / SERVER_ADVANCE_PERIOD;

  //
  // Update nearby eggs

  if (m_state == StateFloating)
  {
    m_eggSearchTimer -= SERVER_ADVANCE_PERIOD;

    if (m_eggSearchTimer <= 0.0f)
    {
      m_eggSearchTimer = 3.0f;
      m_numNearbyEggs = 0;

      int numNeighbours;
      WorldObjectId* ids = g_location->m_entityGrid->GetNeighbours(m_pos.x, m_pos.z, VIRII_MAXSEARCHRANGE, &numNeighbours);

      for (int i = 0; i < numNeighbours; ++i)
      {
        WorldObjectId id = ids[i];
        Egg* egg = (Egg*)g_location->GetEntitySafe(id, Entity::TypeEgg);
        if (egg && egg->m_state == Egg::StateDormant && egg->m_onGround)
        {
          m_nearbyEggs[m_numNearbyEggs] = id;
          ++m_numNearbyEggs;
          if (m_numNearbyEggs == SPIRIT_MAXNEARBYEGGS)
          {
            break;
          }
        }
      }
    }
  }

  return false;
}


void Spirit::PushFromBuildings()
{
  std::vector<int> const* obstructions = g_location->m_obstructionGrid->GetBuildings(m_pos.x, m_pos.z);

  bool hitFound = false;

  for (int buildingId : *obstructions)
  {
    Building* building = g_location->GetBuilding(buildingId);
    if (building && building->DoesSphereHit(m_pos, 5.0f))
    {
      hitFound = true;
      DirectX::XMVECTOR const hitVector = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&building->m_pos));
      DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorMultiplyAdd(hitVector, DirectX::XMVectorReplicate(0.1f), DirectX::XMLoadFloat3(&m_vel)));
      m_vel.y = 0.0f;
      // `speed = min(|vel|, 10)` followed by `SetLength(speed)` is a magnitude
      // clamp written in two steps, and XMVector3ClampLength is that operation
      // directly. It also answers a zero vector cleanly, where the legacy pair
      // divided by |vel| and leaned on Vector3::SetLength's (len, 0, 0)
      // fallback to survive it.
      DirectX::XMStoreFloat3(&m_vel, DirectX::XMVector3ClampLength(DirectX::XMLoadFloat3(&m_vel), 0.0f, 10.0f));
    }
  }

  if (!hitFound && DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&m_vel))) < 1.0f)
  {
    // Once we have cleared any buildings we are assumed to be safe
    m_pushFromBuildings = false;
  }
}


void Spirit::SkipStage() { m_timeSync = 0.0f; }


void Spirit::AddToGlobalWorld()
{
  int locationId = g_locationId;
  GlobalLocation* location = g_globalWorld->GetLocation(locationId);
  DEBUG_ASSERT(location);
  location->AddSpirits(1);
}


void Spirit::CollectorArrives()
{
  m_state = StateAttached;
  g_soundSystem->TriggerOtherEvent(SoundSourceOf(this), "PickedUp", SoundSourceBlueprint::TypeSpirit);
}

void Spirit::CollectorDrops()
{
  m_vel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
  m_hover = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
  m_state = StateFloating;
  m_pushFromBuildings = true;
  //    Begin();
  g_soundSystem->TriggerOtherEvent(SoundSourceOf(this), "Dropped", SoundSourceBlueprint::TypeSpirit);
}

void Spirit::InEgg()
{
  m_state = StateInEgg;
  m_vel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
  m_hover = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
  g_soundSystem->TriggerOtherEvent(SoundSourceOf(this), "PlacedInEgg", SoundSourceBlueprint::TypeSpirit);
}

void Spirit::EggDestroyed()
{
  m_vel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
  m_hover = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
  m_state = StateFloating;
  //    Begin();
  g_soundSystem->TriggerOtherEvent(SoundSourceOf(this), "EggDestroyed", SoundSourceBlueprint::TypeSpirit);
}

void Spirit::Render(float predictionTime)
{
  int innerAlpha = 255;
  int outerAlpha = 100;
  float spiritInnerSize = 1.0;
  float spiritOuterSize = 3.0;

  if (m_state == StateBirth)
  {
    float fractionBorn = 1.2f - (m_timeSync / 6.0f);
    if (fractionBorn > 1.0f)
      fractionBorn = 1.0f;
    // innerAlpha *= fractionBorn;
    // outerAlpha *= fractionBorn;
    spiritInnerSize *= fractionBorn;
    spiritOuterSize *= fractionBorn;
  }

  if (m_state == StateDeath)
  {
    float fractionAlive = m_timeSync / 180.0f;
    innerAlpha *= fractionAlive;
    outerAlpha *= fractionAlive;
    spiritInnerSize *= fractionAlive;
    spiritOuterSize *= fractionAlive;
  }

  RGBAColour colour;
  if (m_teamId != 255)
  {
    colour = g_location->m_teams[m_teamId].m_colour;
  }
  else
  {
    colour.Set(255, 255, 255);
  }

  predictionTime -= SERVER_ADVANCE_PERIOD;

  DirectX::XMVECTOR const predictionTimeVec = DirectX::XMVectorReplicate(predictionTime);
  DirectX::XMVECTOR predictedPos = DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), predictionTimeVec, DirectX::XMLoadFloat3(&m_pos));
  predictedPos = DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_hover), predictionTimeVec, predictedPos);

  // GetUp and GetRight still return Vector3 until T12/T22; the seam converts.
  DirectX::XMFLOAT3 const camUpStore = g_camera->GetUp();
  DirectX::XMFLOAT3 const camRightStore = g_camera->GetRight();
  DirectX::XMVECTOR const camUp = DirectX::XMLoadFloat3(&camUpStore);
  DirectX::XMVECTOR const camRight = DirectX::XMLoadFloat3(&camRightStore);

  float size = spiritInnerSize;
  glColor4ub(colour.r, colour.g, colour.b, innerAlpha);

  glBegin(GL_QUADS);
  DirectX::XMVECTOR const sizeVec = DirectX::XMVectorReplicate(size);
  EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camUp, sizeVec, predictedPos));
  EmitVertex(DirectX::XMVectorMultiplyAdd(camRight, sizeVec, predictedPos));
  EmitVertex(DirectX::XMVectorMultiplyAdd(camUp, sizeVec, predictedPos));
  EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camRight, sizeVec, predictedPos));
  glEnd();

  size = spiritOuterSize;
  glColor4ub(colour.r, colour.g, colour.b, outerAlpha);
  glBegin(GL_QUADS);
  DirectX::XMVECTOR const sizeVec = DirectX::XMVectorReplicate(size);
  EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camUp, sizeVec, predictedPos));
  EmitVertex(DirectX::XMVectorMultiplyAdd(camRight, sizeVec, predictedPos));
  EmitVertex(DirectX::XMVectorMultiplyAdd(camUp, sizeVec, predictedPos));
  EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camRight, sizeVec, predictedPos));
  glEnd();
}

int Spirit::NumNearbyEggs() { return m_numNearbyEggs; }

WorldObjectId* Spirit::GetNearbyEggs() { return m_nearbyEggs; }
