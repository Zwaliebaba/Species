#pragma once


#include "MathUtils.h"
#include "NeuronMath.h"
#include "Vector2.h"

class Vector3;

extern Vector3 const g_upVector;
extern Vector3 const g_zeroVector;


class Vector3
{
  private:
    bool Compare(Vector3 const& b) const { return (NearlyEquals(x, b.x) && NearlyEquals(y, b.y) && NearlyEquals(z, b.z)); }

  public:
    float x, y, z;

    // *** Constructors ***
    Vector3()
      : x(0.0f),
        y(0.0f),
        z(0.0f)
    {
    }

    Vector3(float _x, float _y, float _z)
      : x(_x),
        y(_y),
        z(_z)
    {
    }

    Vector3(Vector2 const& _b)
      : x(_b.x),
        y(0.0f),
        z(_b.y)
    {
    }

    // TRANSITIONAL. The seam that lets a converted file compile against an
    // unconverted API while directxmath-migration runs. It is a wrapper, it is
    // temporary, and T25 deletes it along with this whole class. Do not build on
    // it: new code stores XMFLOAT3 and computes with XMVECTOR.
    //
    // The reference conversions reinterpret rather than copy, so a Vector3* can
    // still be handed to something expecting an XMFLOAT3*. The static_asserts
    // below the class are what make that legal rather than hopeful.
    Vector3(DirectX::XMFLOAT3 const& _b)
      : x(_b.x),
        y(_b.y),
        z(_b.z)
    {
    }
    operator DirectX::XMFLOAT3 const&() const { return *reinterpret_cast<DirectX::XMFLOAT3 const*>(this); }
    operator DirectX::XMFLOAT3&() { return *reinterpret_cast<DirectX::XMFLOAT3*>(this); }

    void Zero() { x = y = z = 0.0f; }

    void Set(float _x, float _y, float _z)
    {
      x = _x;
      y = _y;
      z = _z;
    }

    Vector3 operator^(Vector3 const& b) const { return Vector3(y * b.z - z * b.y, z * b.x - x * b.z, x * b.y - y * b.x); }

    float operator*(Vector3 const& b) const { return x * b.x + y * b.y + z * b.z; }

    Vector3 operator-() const { return Vector3(-x, -y, -z); }

    Vector3 operator+(Vector3 const& b) const { return Vector3(x + b.x, y + b.y, z + b.z); }

    Vector3 operator-(Vector3 const& b) const { return Vector3(x - b.x, y - b.y, z - b.z); }

    Vector3 operator*(float const b) const { return Vector3(x * b, y * b, z * b); }

    Vector3 operator/(float const b) const
    {
      float multiplier = 1.0f / b;
      return Vector3(x * multiplier, y * multiplier, z * multiplier);
    }

    Vector3 const& operator=(Vector3 const& b)
    {
      x = b.x;
      y = b.y;
      z = b.z;
      return *this;
    }

    Vector3 const& operator*=(float const b)
    {
      x *= b;
      y *= b;
      z *= b;
      return *this;
    }

    Vector3 const& operator/=(float const b)
    {
      float multiplier = 1.0f / b;
      x *= multiplier;
      y *= multiplier;
      z *= multiplier;
      return *this;
    }

    Vector3 const& operator+=(Vector3 const& b)
    {
      x += b.x;
      y += b.y;
      z += b.z;
      return *this;
    }

    Vector3 const& operator-=(Vector3 const& b)
    {
      x -= b.x;
      y -= b.y;
      z -= b.z;
      return *this;
    }

    Vector3 const& Normalise()
    {
      float lenSqrd = x * x + y * y + z * z;
      if (lenSqrd > 0.0f)
      {
        float invLen = 1.0f / sqrtf(lenSqrd);
        x *= invLen;
        y *= invLen;
        z *= invLen;
      }
      else
      {
        x = y = 0.0f;
        z = 1.0f;
      }

      return *this;
    }

    Vector3 const& SetLength(float _len)
    {
      float mag = Mag();
      if (NearlyEquals(mag, 0.0f))
      {
        x = _len;
        return *this;
      }

      float scaler = _len / Mag();
      *this *= scaler;
      return *this;
    }


    bool operator==(Vector3 const& b) const; // Uses FLT_EPSILON
    bool operator!=(Vector3 const& b) const; // Uses FLT_EPSILON

    void RotateAroundX(float _angle);
    void RotateAroundY(float _angle);
    void RotateAroundZ(float _angle);
    void FastRotateAround(Vector3 const& _norm, float _angle);
    void RotateAround(Vector3 const& _norm);

    float Mag() const { return sqrtf(x * x + y * y + z * z); }

    float MagSquared() const { return x * x + y * y + z * z; }

    void HorizontalAndNormalise()
    {
      y = 0.0f;
      float invLength = 1.0f / sqrtf(x * x + z * z);
      x *= invLength;
      z *= invLength;
    }

    float* GetData() { return &x; }

    float const* GetDataConst() const { return &x; }
};


// TRANSITIONAL, with the seam above. These pin the claim the reference
// conversions rest on, and the one LandscapeRenderer's vertex-buffer offsets and
// TrunkPort's height-map memset rest on: a Vector3 IS an XMFLOAT3 in memory. If
// either type gains a member, an alignment attribute or a vtable, this fails to
// compile rather than corrupting a vertex buffer at runtime.
static_assert(sizeof(Vector3) == sizeof(DirectX::XMFLOAT3), "Vector3 and XMFLOAT3 must be the same size");
static_assert(std::is_standard_layout_v<Vector3>, "Vector3 must be standard layout for the reference conversions to be legal");
static_assert(offsetof(Vector3, x) == offsetof(DirectX::XMFLOAT3, x), "Vector3::x must sit where XMFLOAT3::x does");
static_assert(offsetof(Vector3, y) == offsetof(DirectX::XMFLOAT3, y), "Vector3::y must sit where XMFLOAT3::y does");
static_assert(offsetof(Vector3, z) == offsetof(DirectX::XMFLOAT3, z), "Vector3::z must sit where XMFLOAT3::z does");
// GetData and GetDataConst hand &x to OpenGL as a three-float array — 46 files
// feed one to glVertex3fv or glNormal3fv — so tight packing is a contract with
// the driver, not an implementation detail. Stated on its own rather than left
// implied by the XMFLOAT3 comparisons above.
static_assert(sizeof(Vector3) == 3 * sizeof(float), "GetData's callers read three contiguous floats from &x");


// Operator * between float and Vector3
inline Vector3 operator*(float _scale, Vector3 const& _v) { return _v * _scale; }


// TRANSITIONAL, AND THE OTHER HALF OF THE SEAM. T25 deletes it with everything
// else in this header. Added by T14; read this before using it anywhere new.
//
// The conversions on the class above go Vector3 -> XMFLOAT3, which is what lets
// a CONVERTED file call an UNCONVERTED api. This goes the other way, and it
// exists because C++ will not do it implicitly however good the layout match
// is: an implicit conversion is never consulted on the left operand of a member
// operator or a member function call, and XMFLOAT3 has neither to offer. So the
// moment WorldObject::m_pos became an XMFLOAT3, `m_pos.Mag()` and `m_vel *= f`
// stopped compiling in 45 files that T14 does not own and cannot convert.
//
// AsLegacy reinterprets rather than copies, so it works for the sites that
// MUTATE through the member as well as the ones that only read — 97 of the 456
// do, and a temporary would have silently dropped their writes. The
// static_asserts above are what make the reinterpretation legal rather than
// hopeful, and it is the same reinterpretation the class's own conversion
// operators already perform in the opposite direction.
//
// EVERY CALL IS A REPAIR AWAITING A CONVERSION, not an idiom. A file still
// holding one has not been converted yet, and `grep -rl AsLegacy` is therefore
// the live list of files T15-T19, T22 and T24 have left to do. There are too
// many to comment individually -- T14 wrote about 480 across 50 files -- so the
// count and the reasoning live in that task's notes rather than at each site.
// Do not reach for this in new code: new code stores XMFLOAT3 and computes with
// XMVECTOR.
inline Vector3& AsLegacy(DirectX::XMFLOAT3& _v) { return reinterpret_cast<Vector3&>(_v); }
inline Vector3 const& AsLegacy(DirectX::XMFLOAT3 const& _v) { return reinterpret_cast<Vector3 const&>(_v); }
