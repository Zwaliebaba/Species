#include "pch.h"
#include "GlVertex.h"

#include "Debug.h"
#include "FileWriter.h"
#include "TextStreamReaders.h"
#include "MathUtils.h"
#include "Profiler.h"
#include "Resource.h"
#include "Shape.h"

#include "ProtocolLimits.h"
#include "Location.h"
#include "SoundSystem.h"
#include "Team.h"
#include "Unit.h"

#include "Factory.h"
#include "InsertionSquad.h"
#include "WorldPointers.h"
#include "AppState.h"


Factory::Factory()
  : Building(),
    m_state(StateUnused),
    m_troopType(Entity::TypeInvalid),
    m_initialCapacity(10),
    m_unitId(-1),
    m_numToCreate(-1),
    m_numCreated(-1),
    m_timeToCreate(0.0f),
    m_timeSoFar(0.0f)
{
  m_type = TypeFactory;
  SetShape(g_resource->GetShape("Factory.shp"));
}


void Factory::Initialise(Building* _template)
{
  DEBUG_ASSERT(_template->m_type == Building::TypeFactory);
  Factory* factory = (Factory*)_template;
  m_initialCapacity = factory->m_initialCapacity;

  Building::Initialise(_template);

  ShapeMarker* markerSpiritStore = m_shape->m_rootFragment->LookupMarker("MarkerSpiritStore");
  DirectX::XMFLOAT4X4 rootTransform;
  DirectX::XMStoreFloat4x4(&rootTransform,
                           BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

  DirectX::XMFLOAT3 const markerPos = markerSpiritStore->GetWorldPosition(rootTransform);
  DirectX::XMFLOAT3 const spiritStorePos(markerPos.x, markerPos.y + 11.0f, markerPos.z);
  m_spiritStore.Initialise(m_initialCapacity, 200, spiritStorePos, 5.0f, 10.0f, 5.0f);
}


void Factory::Render(float predictionTime)
{
  //    Shape *oldShape = m_shape;
  //    m_shape = nullptr;
  Building::Render(predictionTime);
  //    m_shape = oldShape;

  //
  // If we are creating render an effect at the source point

  if (m_state == StateCreating)
  {
    DirectX::XMVECTOR const pos = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMVectorSet(2.0f, 20.0f, 20.0f, 0.0f));

    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    EmitVertex(DirectX::XMVectorAdd(pos, DirectX::XMVectorSet(-5.0f, -5.0f, 0.0f, 0.0f)));
    EmitVertex(DirectX::XMVectorAdd(pos, DirectX::XMVectorSet(-5.0f, +5.0f, 0.0f, 0.0f)));
    EmitVertex(DirectX::XMVectorAdd(pos, DirectX::XMVectorSet(+5.0f, +5.0f, 0.0f, 0.0f)));
    EmitVertex(DirectX::XMVectorAdd(pos, DirectX::XMVectorSet(+5.0f, -5.0f, 0.0f, 0.0f)));
    glEnd();
  }
}


void Factory::RenderAlphas(float predictionTime)
{
  if (!g_editing)
  {
    m_spiritStore.Render(predictionTime);
  }
}


void Factory::RequestUnit(unsigned char _troopType, int _numToCreate)
{
  m_troopType = _troopType;

  for (int i = 0; i < Entity::NumStats; ++i)
  {
    m_stats[i] = EntityBlueprint::GetStat(m_troopType, i);
  }

  m_numToCreate = _numToCreate;
  m_numCreated = 0;
  m_timeToCreate = m_numToCreate * 0.1f;
  m_timeSoFar = 0.0f;

  if (_troopType < Entity::TypeEngineer || _troopType == Entity::TypeInsertionSquadie)
  {
    Team* team = &g_location->m_teams[m_id.GetTeamId()];
    Unit* unit = team->NewUnit(_troopType, _numToCreate, &m_unitId, m_pos);
    DirectX::XMFLOAT3 wayPoint;
    DirectX::XMStoreFloat3(
      &wayPoint, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_front), DirectX::XMVectorReplicate(30.0f), DirectX::XMLoadFloat3(&m_pos)));
    unit->SetWayPoint(wayPoint);
  }
  else
  {
    m_unitId = -1;
  }

  m_state = StateCreating;
}


bool Factory::Advance()
{
  switch (m_state)
  {
  case StateUnused:
    AdvanceStateUnused();
    break;
  case StateCreating:
    AdvanceStateCreating();
    break;
  case StateRecharging:
    AdvanceStateRecharging();
    break;
  };


  m_spiritStore.Advance();

  return Building::Advance();
}

void Factory::AdvanceStateUnused() {}

void Factory::AdvanceStateCreating()
{
  float fractionComplete = m_timeSoFar / m_timeToCreate;
  if (fractionComplete > 1.0f)
    fractionComplete = 1.0f;

  int numThatShouldBeCreated = int(m_numToCreate * fractionComplete);
  int numToCreate = (numThatShouldBeCreated - m_numCreated);
  int numActuallyCreated = 0;

  for (int i = 0; i < numToCreate; ++i)
  {
    if (m_spiritStore.NumSpirits() > 0)
    {
      // The five syncsfrand calls stay in this order: they advance the synchronised
      // RNG, and the migration may not change the call sequence.
      DirectX::XMFLOAT3 const offset(syncsfrand(5.0f), 20.0f + syncsfrand(5.0f), 20.0f);
      DirectX::XMFLOAT3 pos;
      DirectX::XMStoreFloat3(&pos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&offset)));
      DirectX::XMFLOAT3 const vel(syncsfrand(1.0f), syncsfrand(1.0f), 5.0f + syncsfrand(1.0f));

      m_spiritStore.RemoveSpirits(1);
      g_location->SpawnEntities(pos, m_id.GetTeamId(), m_unitId, m_troopType, 1, vel, 0.0f);

      ++numActuallyCreated;
    }
  }

  if (numActuallyCreated > 0 || m_spiritStore.NumSpirits() > 0)
  {
    m_numCreated += numActuallyCreated;

    m_timeSoFar += SERVER_ADVANCE_PERIOD;

    if (fractionComplete >= 1.0f)
    {
      m_timeSoFar = 0.0f;
      m_timeToCreate = 3.0f;
      m_state = StateRecharging;
    }
  }
}

void Factory::AdvanceStateRecharging()
{
  m_timeSoFar += SERVER_ADVANCE_PERIOD;

  if (m_timeSoFar >= m_timeToCreate)
  {
    m_state = StateUnused;
  }
}

void Factory::SetTeamId(int _teamId)
{
  Building::SetTeamId(_teamId);

  m_state = StateUnused;
}

void Factory::Read(TextReader* _in, bool _dynamic)
{
  Building::Read(_in, _dynamic);

  char* word = _in->GetNextToken();
  m_initialCapacity = atoi(word);
}

void Factory::Write(FileWriter* _out)
{
  Building::Write(_out);

  _out->printf("%-8d", m_initialCapacity);
}
