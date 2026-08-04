#pragma once

#include "NeuronMath.h"

class Vector3;

class Vector2
{
  private:
    bool Compare(Vector2 const& b) const;

  public:
    float x, y;

    Vector2();
    Vector2(Vector3 const&);
    Vector2(float _x, float _y);

    // TRANSITIONAL — see Vector3.h. T25 deletes this class and this seam with it.
    Vector2(DirectX::XMFLOAT2 const& _b)
      : x(_b.x),
        y(_b.y)
    {
    }
    operator DirectX::XMFLOAT2 const&() const { return *reinterpret_cast<DirectX::XMFLOAT2 const*>(this); }
    operator DirectX::XMFLOAT2&() { return *reinterpret_cast<DirectX::XMFLOAT2*>(this); }

    void Zero();
    void Set(float _x, float _y);

    float operator^(Vector2 const& b) const; // Cross product
    float operator*(Vector2 const& b) const; // Dot product

    Vector2 operator-() const; // Negate
    Vector2 operator+(Vector2 const& b) const;
    Vector2 operator-(Vector2 const& b) const;
    Vector2 operator*(float const b) const; // Scale
    Vector2 operator/(float const b) const;

    void operator=(Vector2 const& b);
    void operator=(Vector3 const& b);
    void operator*=(float const b);
    void operator/=(float const b);
    void operator+=(Vector2 const& b);
    void operator-=(Vector2 const& b);

    bool operator==(Vector2 const& b) const; // Uses FLT_EPSILON
    bool operator!=(Vector2 const& b) const; // Uses FLT_EPSILON

    Vector2 const& Normalise();
    void SetLength(float _len);

    float Mag() const;
    float MagSquared() const;

    float* GetData();
};


// TRANSITIONAL, with the seam above.
static_assert(sizeof(Vector2) == sizeof(DirectX::XMFLOAT2), "Vector2 and XMFLOAT2 must be the same size");
static_assert(std::is_standard_layout_v<Vector2>, "Vector2 must be standard layout for the reference conversions to be legal");
static_assert(offsetof(Vector2, x) == offsetof(DirectX::XMFLOAT2, x), "Vector2::x must sit where XMFLOAT2::x does");
static_assert(offsetof(Vector2, y) == offsetof(DirectX::XMFLOAT2, y), "Vector2::y must sit where XMFLOAT2::y does");


// Operator * between float and Vector2
inline Vector2 operator*(float _scale, Vector2 const& _v) { return _v * _scale; }
