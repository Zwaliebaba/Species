#pragma once

#include "Building.h"

class ShapeMarker;


class Cave : public Building
{
protected:
    int             m_troopType;
    int             m_unitId;
    float           m_spawnTimer;
    bool            m_dead;

    ShapeMarker     *m_spawnPoint;

public:
    Cave();

    bool Advance();
    void Damage( float _damage );
};


