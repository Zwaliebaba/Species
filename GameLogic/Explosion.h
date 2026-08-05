#pragma once

#include <memory>
#include <vector>


#include "NeuronMath.h"
#include "RgbColour.h"
#include "Shape.h"


//*****************************************************************************
// Class Tumbler
//*****************************************************************************

// This class maintains a rotation matrix that is used to make an ExplodingTri
// tumble as it flys through space. The m_rotMat matrix has a little bit of
// rotation multiplied on to it every frame.
class Tumbler
{
  public:
    // Rotation only, so 3x3 storage. How it is BUILT and how it is MULTIPLIED
    // both reverse relative to the legacy Matrix33 — see the two pins in
    // NeuronMathTests and directxmath-migration T19's notes.
    // IDENTITY BY DEFAULT, for the reason ShapeMarker::m_transform carries on
    // main: the legacy Matrix33's default constructor did nothing either, but
    // Tumbler() called SetToIdentity() and that was the only thing standing
    // between this and stack garbage. Declaring it here means a second
    // constructor cannot lose it.
    DirectX::XMFLOAT3X3 m_rotMat{1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    DirectX::XMFLOAT3 m_angVel{0.0f, 0.0f, 0.0f};

    Tumbler();
    void Advance();
};


//*****************************************************************************
// Class ExplodingTri
//*****************************************************************************

class ExplodingTri
{
  public:
    // ExplodingTri is memcpy'd out of a vector in Explosion's constructor, so
    // these stay trivially copyable. Every one is assigned there before use.
    DirectX::XMFLOAT3 m_vel{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 m_pos{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 m_v1{0.0f, 0.0f, 0.0f}, m_v2{0.0f, 0.0f, 0.0f}, m_v3{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 m_norm{0.0f, 0.0f, 0.0f};
    Tumbler* m_tumbler;
    float m_timeToDie;
    RGBAColour m_colour;
};


//*****************************************************************************
// Class Explosion
//*****************************************************************************

class Explosion
{
  protected:
    Tumbler* m_tumblers;
    unsigned int m_numTumblers;
    ExplodingTri* m_tris;
    unsigned int m_numTris;
    float m_timeToDie;

  public:
    Explosion(ShapeFragment* _frag, DirectX::XMFLOAT4X4 const& _transform, float _fraction);
    ~Explosion();

    bool Advance(); // Returns true if the explosion has finished
    void Render();

    DirectX::XMFLOAT3 GetCenter() const;
};


//*****************************************************************************
// Class ExplosionManager
//*****************************************************************************

class ExplosionManager
{
  protected:
    // Owning. Object lifetimes here are sync-relevant: an explosion is
    // destroyed at exactly the tick Advance says so, and the erase below is
    // that point.
    std::vector<std::unique_ptr<Explosion>> m_explosions;

  public:
    ExplosionManager();
    ~ExplosionManager();

    void AddExplosion(ShapeFragment* _frag, DirectX::XMFLOAT4X4 const& _transform, bool _recurse, float _fraction);
    void AddExplosion(Shape* _shape, DirectX::XMFLOAT4X4 const& _transform, float _fraction = 1.0f);
    void Reset();

    void Advance();
    void Render();

    const std::vector<std::unique_ptr<Explosion>>& GetExplosionList() { return m_explosions; } // read access for DeformEffect
};


extern ExplosionManager g_explosionManager;
