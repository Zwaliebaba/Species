#pragma once

// The player-input payload of the wire protocol. It lives in NeuronCore rather
// than beside Team because NetworkUpdate serialises it: the flags word goes out
// through GetFlags/SetFlags on every Alive update. Nothing here touches the game
// model — it is a mouse position and a bitfield.
//
// EVERYTHING IN HERE GOES ON THE WIRE, which is a property input-native-events
// T3 restored rather than one the struct always had. It used to carry four
// gamepad delta ints that GetByteStream never wrote and ReadByteStream never
// read, so the simulation read zeros for them on every machine including the
// one that set them — every letter is serialised, single player included. Do
// not add a field here without adding it to both halves of the codec: a member
// this struct carries and the wire does not is a value that exists locally and
// nowhere else, which is the shape of a desync.

#include "NeuronMath.h"

class TeamControls
{
  public:
    TeamControls();

    unsigned short GetFlags() const;
    void SetFlags(unsigned short _flags);
    void ClearFlags();
    // Defined in Species, not here: populating these fields means reading the
    // camera, mouse and input manager. Only the data and its flags encoding
    // are core, because that is what goes on the wire.
    void Advance();
    void Clear();

  public:
    // Zeroed by Clear()'s memset as well; stated here so the guarantee is local
    // rather than dependent on another file, and so check_math_types.py can see it.
    DirectX::XMFLOAT3 m_mousePos{0.0f, 0.0f, 0.0f};

    // Be sure to update GetFlags, SetFlags, ZeroFlags if you change these flags
    // Also, NetworkUpdate::GetByteStream and NetworkUpdate::ReadByteStream
    // if you use more than 8 bits

    unsigned int m_unitMove : 1;
    unsigned int m_primaryFireTarget : 1;
    unsigned int m_secondaryFireTarget : 1;
    unsigned int m_primaryFireDirected : 1;
    unsigned int m_secondaryFireDirected : 1;
    unsigned int m_cameraEntityTracking : 1;
    unsigned int m_directUnitMove : 1;
    unsigned int m_unitSecondaryMode : 1;
    unsigned int m_endSetTarget : 1;
};
