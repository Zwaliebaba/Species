#pragma once

#include "RgbColour.h"
#include "Vector3.h"
#include "WorldObjectId.h"


// ****************************************************************************
//  Class WorldObject
// ****************************************************************************

class WorldObject
{
public:
    enum                                            // These enums apply if our unitID is UNIT_EFFECTS
    {
        TypeInvalid,
        EffectThrowableGrenade,
        EffectThrowableAirstrikeMarker,
        EffectThrowableAirstrikeBomb,
        EffectThrowableControllerGrenade,
        EffectGunTurretTarget,
        EffectGunTurretShell,
        EffectSpamInfection,
        EffectBoxKite,
        EffectSnow,
        EffectRocket,
        EffectShockwave,
        EffectMuzzleFlash,
        EffectOfficerOrders,
        EffectZombie
    };

public:
    WorldObjectId   m_id;
    int             m_type;
    Vector3         m_pos;
    Vector3         m_vel;
    bool            m_onGround;
    bool            m_enabled;

    WorldObject						();
    virtual ~WorldObject            ();
	void BounceOffLandscape			();

    virtual bool Advance			();
    virtual void Render				( float _time );
	virtual bool RenderPixelEffect	( float predictionTime );               // Return true if you did anything
};


// ****************************************************************************
//  Class Light
// ****************************************************************************

class Light
{
public:
    float m_colour[4];	// Forth element seems irrelevant but OpenGL insists we specify it
    float m_front[4];	// Forth element must be 0.0f to signify an infinitely distance light

    Light();
	void SetColour(float colour[4]);
	void SetFront(float front[4]);
	void SetFront(Vector3 front);
	void Normalise();
};


