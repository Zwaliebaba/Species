#pragma once

#include "Vector3.h"
#include "WorldObjectId.h"

// What the layers below GameLogic ask the loaded world.
//
// Four questions, from two files: Resource tells the world to drop and rebuild
// its OpenGL state when the context is torn down, and SoundInstance asks the
// ground height under a positional sound and whether a sound's attached object
// still exists. Those four were the only reason NeuronClient included
// GameLogic's Location.h.
//
// THE SEAM IS A SEPARATE POINTER, not the type of g_location, and that is a
// departure from how CameraAccess and RendererAccess work. The reason is
// measured rather than stylistic: g_location is dereferenced at 1289 sites
// across 106 files, and the top of that distribution is DATA MEMBERS —
// m_landscape 329, m_buildings 123, m_levelFile 97, m_teams 89, m_spirits 81,
// m_entityGrid 65. A pure-virtual class cannot expose a data member, so
// retyping the global would turn every one of those into an accessor call.
// That is a rewrite of the world's API undertaken for the benefit of four call
// sites. See tasks/layering-inversion.yaml T16.
//
// Both questions are narrowed as they cross. SoundInstance used to reach
// g_location->m_landscape.m_heightMap->GetValue(x, z) and
// g_location->GetWorldObject(id); GroundHeight and WorldObjectExists say what
// the caller meant and leak no GameLogic type through their signatures.
class LocationAccess
{
  public:
    virtual ~LocationAccess() = default;

    // The GL context has gone away, or come back. Called from Resource, which
    // owns the texture and display-list caches the world's objects hold
    // handles into.
    virtual void FlushOpenGlState() = 0;
    virtual void RegenerateOpenGlState() = 0;

    // Landscape height under a world position, for sounds that sit on the
    // ground. Returns 0 when no landscape is loaded rather than dereferencing
    // a null heightmap — a Location that has been constructed but not Init'd
    // is a legal object, and GameLogicTests builds one.
    virtual float GroundHeight(float _worldX, float _worldZ) = 0;

    // Whether `_id` still names something in the world. A sound attached to an
    // object that has died has to find a new one.
    virtual bool WorldObjectExists(WorldObjectId const& _id) = 0;

    // Where a sound attached to `_id` is heard from, and how fast that point is
    // moving. False, with the outputs untouched, when the object has gone.
    //
    // Not simply the object's m_pos: a building is heard from its centre rather
    // than its origin, which used to be a `((Building*)obj)->m_centrePos` cast
    // in SoundInstance. That rule is the world's knowledge about its own
    // objects, so it lives on this side of the seam.
    virtual bool GetSoundSource(WorldObjectId const& _id, Vector3* _pos, Vector3* _vel) = 0;
};

// The world currently loaded, or null between levels and in a test DLL.
//
// Location installs itself here in its constructor and removes itself in its
// destructor, so this cannot go stale the way a pointer maintained by
// assignment sites would. The destructor clears it only if the departing
// object is the one installed, which is what makes both level-reload orderings
// safe: Main.cpp deletes the old world before building the new one, but a
// caller that built the replacement first would otherwise have the old one's
// destructor null a pointer to the live world.
extern LocationAccess* g_locationAccess;
