#pragma once

#include "NeuronMath.h"
#include "WorldObjectId.h"

// What the sound system needs to know about the thing making a sound.
//
// The Trigger*Event methods used to take an `Entity*`, a `Building*` and a
// `WorldObject*` — GameLogic types, in a NeuronClient header, which is the
// wrong direction and was one of the last two upward includes in the tree. Each
// body read the same four fields off whatever it was handed and nothing else,
// so this carries them across instead.
//
// GameLogic fills one from a live object through the `SoundSourceOf` overloads
// in GameLogic/SoundSources.h. See tasks/layering-inversion.yaml T17.
struct SoundSource
{
    WorldObjectId m_id;

    // Entity type for TriggerEntityEvent, building type for
    // TriggerBuildingEvent. Unused by TriggerOtherEvent, which is told its
    // blueprint type separately.
    int m_type;

    DirectX::XMFLOAT3 m_pos;

    // Read only by TriggerEntityEvent. TriggerBuildingEvent set the instance's
    // position and left its velocity as the blueprint had it, and this keeps
    // doing that.
    DirectX::XMFLOAT3 m_vel;
};
