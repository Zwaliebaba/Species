#pragma once

#include "RgbColour.h"
#include "SliceWalker.h"
#include "SlotMap.h"
#include "NeuronMath.h"


// ****************************************************************************
// ParticleType
// ****************************************************************************

class ParticleType
{
  public:
    float m_spin; // Rate of spin in radians per second (positive is clockwise)
    float m_fadeInTime;
    float m_fadeOutTime;
    float m_life;
    float m_size;
    float m_friction;     // Amount of friction to apply (0=none 1=a huge amount)
    float m_gravity;      // Amount of gravity to apply (0=none 1=quite a lot)
    RGBAColour m_colour1; // Start of colour range
    RGBAColour m_colour2; // End of colour range

    ParticleType();
};


// ****************************************************************************
// Particle
// ****************************************************************************

class Particle
{
  public:
    enum
    {
      TypeInvalid = -1,
      TypeRocketTrail = 0,
      TypeExplosionCore,
      TypeExplosionDebris,
      TypeMuzzleFlash,
      TypeFire,
      TypeControlFlash,
      TypeSpark,
      TypeBlueSpark,
      TypeBrass,
      TypeMissileTrail,
      TypeMissileFire,
      TypeCitizenFire,
      TypeLeaf,
      TypeNumTypes
    };

  private:
    static ParticleType m_types[TypeNumTypes];

  public:
    // Explicit zero-initialisers: Vector3's default constructor zeroed and
    // XMFLOAT3's does not, and Particle's constructor assigns neither.
    DirectX::XMFLOAT3 m_pos{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 m_vel{0.0f, 0.0f, 0.0f};
    float m_birthTime;
    int m_typeId;
    float m_size;
    RGBAColour m_colour;

    Particle();

    void Initialise(DirectX::XMFLOAT3 const& _pos, DirectX::XMFLOAT3 const& _vel, int _type, float _size = -1.0f);
    bool Advance();
    void Render(float _predictionTime);

    static void SetupParticles();
};


// ****************************************************************************
// ParticleSystem
// ****************************************************************************

class ParticleSystem
{
  private:
    FastSlotMap<Particle> m_particles;

    // The slice bookkeeping the legacy sliced array carried as a base class.
    // Reset alongside the container in Empty(), which is what the legacy
    // Empty did for itself.
    SliceWalker m_particlesWalker;

  public:
    ParticleSystem();

    void CreateParticle(DirectX::XMFLOAT3 const& _pos, DirectX::XMFLOAT3 const& _vel, int _particleTypeId, float _size = -1.0f, RGBAColour col = 0);

    void Advance(int _slice);
    void Render();
    void Empty();
};
