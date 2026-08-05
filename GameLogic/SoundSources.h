#pragma once

#include "SoundSource.h"
#include "WorldObject.h"

// Fills a SoundSource from a live world object.
//
// The sound system's Trigger*Event methods used to take `Entity*`, `Building*`
// and `WorldObject*` directly, which put GameLogic types in a NeuronClient
// header and was one of the last two upward includes in the tree. They take a
// SoundSource now, and this is where a caller turns the object it has into one.
// See tasks/layering-inversion.yaml T17.
//
// One function rather than three overloads, because the three would have been
// identical: m_id, m_type, m_pos and m_vel are all WorldObject members, and
// Entity and Building add none of their own. An Entity* or Building* converts
// on the way in.
//
// Not every field is read by every path — TriggerBuildingEvent never touched a
// velocity and TriggerOtherEvent is told its blueprint type separately — but
// filling all four here is cheaper than remembering which is which, and
// SoundSource.h records what each one is for.


namespace Species
{
  inline SoundSource SoundSourceOf(WorldObject const* _object) { return {_object->m_id, _object->m_type, _object->m_pos, _object->m_vel}; }
} // namespace Species
