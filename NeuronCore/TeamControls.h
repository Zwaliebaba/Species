#pragma once

// The player-input payload of the wire protocol. It lives in NeuronCore rather
// than beside Team because NetworkUpdate serialises it: the flags word goes out
// through GetFlags/SetFlags on every Alive update. Nothing here touches the game
// model — it is a mouse position, a bitfield and a few deltas.

#include "Vector3.h"

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
    Vector3 m_mousePos;

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

    int m_directUnitMoveDx;
    int m_directUnitMoveDy;
    int m_directUnitFireDx;
    int m_directUnitFireDy;
};
