#include "pch.h"

#include "WorldObjectId.h"


int WorldObjectId::s_nextUniqueId = 0;

#define ID_MAXTEAMS 256
#define ID_MAXUNITS 65536
#define ID_MAXTROOPS 65536

WorldObjectId::WorldObjectId()
  : m_teamId(255),
    m_unitId(-1),
    m_index(-1),
    m_uniqueId(-1)
{
}


WorldObjectId::WorldObjectId(unsigned char _teamId, int _unitId, int _index, int _uniqueId) { Set(_teamId, _unitId, _index, _uniqueId); }


void WorldObjectId::Set(unsigned char _teamId, int _unitId, int _index, int _uniqueId)
{
  DEBUG_ASSERT(_teamId < ID_MAXTEAMS);
  DEBUG_ASSERT(_unitId < ID_MAXUNITS);
  DEBUG_ASSERT(_index < ID_MAXTROOPS);

  m_teamId = _teamId;
  m_unitId = _unitId;
  m_index = _index;
  m_uniqueId = _uniqueId;
}


void WorldObjectId::SetInvalid()
{
  m_teamId = 255;
  m_unitId = -1;
  m_index = -1;
  m_uniqueId = -1;
}


bool WorldObjectId::operator!=(WorldObjectId const& w) const
{
  return (m_teamId != w.m_teamId || m_unitId != w.m_unitId || m_index != w.m_index || m_uniqueId != w.m_uniqueId);
}


bool WorldObjectId::operator==(WorldObjectId const& w) const
{
  return (m_teamId == w.m_teamId && m_unitId == w.m_unitId && m_index == w.m_index && m_uniqueId == w.m_uniqueId);
}


void WorldObjectId::GenerateUniqueId()
{
  ++s_nextUniqueId;
  m_uniqueId = s_nextUniqueId;
}
