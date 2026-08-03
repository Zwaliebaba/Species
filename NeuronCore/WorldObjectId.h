#pragma once

// Network identity for everything in the world.
//
// It lived in GameLogic/WorldObject.h, next to WorldObject, which meant the
// sound system had to include GameLogic upward to name the type its API is
// written in. Nothing about it is GameLogic's: it is four scalars with no
// dependencies, it goes onto the wire whole through WRITE_WORLDOBJECTID, and
// its comparison and validity rules are protocol rather than convenience.
// See tasks/layering-inversion.yaml T10.
//
// m_index is a raw container slot. Replacing a container with one that assigns
// slots differently repoints every reference in the world — see
// CODING_STANDARDS.md, Determinism.

#define UNIT_BUILDINGS -100
#define UNIT_SPIRITS -101
#define UNIT_LASERS -102
#define UNIT_EFFECTS -103


class WorldObjectId
{
  protected:
    unsigned char m_teamId;
    int m_unitId;
    int m_index;
    int m_uniqueId;

  protected:
    static int s_nextUniqueId;

  public:
    WorldObjectId();
    WorldObjectId(unsigned char _teamId, int _unitId, int _index, int _uniqueId);
    void Set(unsigned char _teamId, int _unitId, int _index, int _uniqueId);

    void SetInvalid();

    void SetTeamId(unsigned char _teamId) { m_teamId = _teamId; }
    void SetUnitId(int _unitId) { m_unitId = _unitId; }
    void SetIndex(int _index) { m_index = _index; }
    void SetUniqueId(int _uniqueId) { m_uniqueId = _uniqueId; }
    void GenerateUniqueId();

    unsigned char GetTeamId() const { return m_teamId; }
    int GetUnitId() const { return m_unitId; }
    int GetIndex() const { return m_index; }
    int GetUniqueId() const { return m_uniqueId; }

    bool IsValid() const { return (m_teamId != 255 || m_unitId != -1 || m_index != -1); }

    bool operator!=(WorldObjectId const& w) const;
    bool operator==(WorldObjectId const& w) const;
    WorldObjectId const& operator=(WorldObjectId const& w);
};
