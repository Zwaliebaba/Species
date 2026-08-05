#pragma once


#include "Entity.h"
#include "WorldObject.h"

namespace Neuron
{
  class Shape;
} // namespace Neuron


// ****************************************************************************
//  Class ThrowableWeapon
// ****************************************************************************

class ThrowableWeapon : public WorldObject
{
  protected:
    Shape* m_shape;
    float m_birthTime;
    float m_force;

    DirectX::XMFLOAT3 m_front{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 m_up{0.0f, 0.0f, 0.0f};

    int m_numFlashes;

    RGBAColour m_colour;

    void TriggerSoundEvent(char const* _event);

  public:
    ThrowableWeapon(int _type, DirectX::XMFLOAT3 const& _startPos, DirectX::XMFLOAT3 const& _front, float _force);

    void Initialise();
    bool Advance();
    void Render(float _predictionTime);

    static float GetMaxForce(int _researchLevel);
    static float GetApproxMaxRange(float _maxForce);
};


// ****************************************************************************
//  Class Grenade
// ****************************************************************************

class Grenade : public ThrowableWeapon
{
  public:
    float m_life;
    float m_power;

  public:
    Grenade(DirectX::XMFLOAT3 const& _startPos, DirectX::XMFLOAT3 const& _front, float _force);
    bool Advance();
};


// ****************************************************************************
//  Class AirStrikeMarker
// ****************************************************************************

class AirStrikeMarker : public ThrowableWeapon
{
  public:
    WorldObjectId m_airstrikeUnit;

  public:
    AirStrikeMarker(DirectX::XMFLOAT3 const& _startPos, DirectX::XMFLOAT3 const& _front, float _force);
    bool Advance();
};


// ****************************************************************************
//  Class ControllerGrenade
// ****************************************************************************

class ControllerGrenade : public ThrowableWeapon
{
  public:
    ControllerGrenade(DirectX::XMFLOAT3 const& _startPos, DirectX::XMFLOAT3 const& _front, float _force);
    bool Advance();
};


// ****************************************************************************
//  Class Rocket
// ****************************************************************************

class Rocket : public WorldObject
{
  public:
    unsigned char m_fromTeamId;

    Shape* m_shape;
    float m_timer;

  public:
    // Braced to zero: Vector3's default constructor did it and XMFLOAT3's does
    // not, and the defaulted Rocket() constructor leaves it alone.
    DirectX::XMFLOAT3 m_target{0.0f, 0.0f, 0.0f};

    Rocket() {}
    Rocket(DirectX::XMFLOAT3 _startPos, DirectX::XMFLOAT3 _targetPos);

    void Initialise();
    bool Advance();
    void Render(float predictionTime);
};


// ****************************************************************************
//  Class Laser
// ****************************************************************************

class Laser : public WorldObject
{
  public:
    unsigned char m_fromTeamId;

  protected:
    float m_life;
    bool m_harmless; // becomes true after hitting someone
    bool m_bounced;

  public:
    Laser() {}

    void Initialise(float _lifeTime);
    bool Advance();
    void Render(float predictionTime);
};


// ****************************************************************************
//  Class TurretShell
// ****************************************************************************

class TurretShell : public WorldObject
{
  protected:
    float m_life;

  public:
    TurretShell(float _life);

    bool Advance();
    void Render(float predictionTime);
};


// ****************************************************************************
//  Class Shockwave
// ****************************************************************************

class Shockwave : public WorldObject
{
  public:
    Shape* m_shape;
    int m_teamId;
    float m_size;
    float m_life;

  public:
    Shockwave(int _teamId, float _size);

    bool Advance();
    void Render(float predictionTime);
};


// ****************************************************************************
//  Class MuzzleFlash
// ****************************************************************************

class MuzzleFlash : public WorldObject
{
  public:
    DirectX::XMFLOAT3 m_front{0.0f, 0.0f, 0.0f};
    float m_size;
    float m_life;

  public:
    MuzzleFlash();
    MuzzleFlash(DirectX::XMFLOAT3 const& _pos, DirectX::XMFLOAT3 const& _front, float _size, float _life);

    bool Advance();
    void Render(float _predictionTime);
};


// ****************************************************************************
//  Class Missile
// ****************************************************************************

class Missile : public WorldObject
{
  protected:
    float m_life;
    std::vector<DirectX::XMFLOAT3> m_history;
    Shape* m_shape;
    ShapeMarker* m_booster;
    MuzzleFlash m_fire;

  public:
    WorldObjectId m_tankId; // Who fired me
    DirectX::XMFLOAT3 m_front{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 m_up{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 m_target{0.0f, 0.0f, 0.0f};

  public:
    Missile();

    bool Advance();
    bool AdvanceToTargetPosition(DirectX::XMFLOAT3 const& _pos);
    void Explode();
    void Render(float _predictionTime);
};
