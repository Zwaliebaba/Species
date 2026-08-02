
#pragma once

#include "Building.h"


class Wall : public Building
{
protected:
    float m_damage;
    float m_fallSpeed;

public:
    Wall();

    bool Advance    ();
    void Damage     ( float _damage );
    void Render     ( float _predictionTime );

};


