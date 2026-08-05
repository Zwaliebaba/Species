#pragma once

#include <algorithm>
#include <stdlib.h>

#include "NeuronMath.h"
#include "Random.h"

inline float frand(float range = 1.0f) { return range * ((float)speciesRandom() / (float)SPECIES_RAND_MAX); }
inline float sfrand(float range = 1.0f) { return (0.5f - (float)speciesRandom() / (float)(SPECIES_RAND_MAX)) * range; }

// Network Syncronised versions (only call inside Net-Safe code)
unsigned long syncrand();
inline float syncfrand(float range = 1.0f) { return syncrand() * (range / 4294967296.0f); }
inline float syncsfrand(float range = 1.0f) { return (syncfrand() - 0.5f) * range; }

#ifndef M_PI
#define M_PI 3.1415926535897932384626f
#endif

// There were function-style min and max macros here. NeuronCore.h sets NOMINMAX
// to keep the Windows pair out and this header put an equivalent pair straight
// back, so std::min and std::max did not compile anywhere this header is
// reachable — which, from NeuronCore.h, is every translation unit in the tree.
// Use std::min and std::max. See language-hygiene T8.

#define sign(a) ((a) < 0 ? -1 : 1)
#define signf(a) ((a) < 0.0f ? -1.0f : 1.0f)


// Performs fast float to int conversion. It may round up or down, depending on
// the current CPU rounding mode - so DON'T USE IT if you want repeatable results
// #ifdef _DEBUG
#define Round(a) (int)(a)
// #else
//	#define Round(a) RoundFunc(a)
//	inline int RoundFunc(float a)
//	{
//		int retval;
//
//		__asm
//		{
//			fld a
//	//		fist retval
//			fistp retval
//		}
//
//		return retval;
//	}
// #endif


// NB: this is a statement macro that clamps 'a' in place. It used to be called
// 'clamp', which collides with std::clamp once <algorithm> is included.
#define ClampInPlace(a, low, high) \
  if (a > high)                    \
    a = high;                      \
  else if (a < low)                \
    a = low;

#define NearlyEquals(a, b) (fabsf((a) - (b)) < 1e-6 ? 1 : 0)

float Log2(float x);

#define ASSERT_FLOAT_IS_SANE(x) DEBUG_ASSERT((x + 1.0f) != x);
#define ASSERT_VECTOR3_IS_SANE(v) \
  ASSERT_FLOAT_IS_SANE((v).x)     \
  ASSERT_FLOAT_IS_SANE((v).y)     \
  ASSERT_FLOAT_IS_SANE((v).z)

// Imagine that you have a stationary object that you want to move from
// A to B in _duration. This function helps you do that with linear acceleration
// to the midpoint, then linear deceleration to the end point. It returns the
// fractional distance along the route. Used in the camera MoveToTarget routine.
double RampUpAndDown(double _startTime, double _duration, double _timeNow);


// *********************
// 2D Intersection Tests
// *********************

float PointSegDist2D(DirectX::XMFLOAT2 const& p,                               // Point
                     DirectX::XMFLOAT2 const& l0, DirectX::XMFLOAT2 const& l1, // Line seg
                     DirectX::XMFLOAT2* result = nullptr);
bool SegRayIntersection2D(DirectX::XMFLOAT2 const& _lineStart, DirectX::XMFLOAT2 const& _lineEnd, DirectX::XMFLOAT2 const& _rayStart,
                          DirectX::XMFLOAT2 const& _rayDir, DirectX::XMFLOAT2* _result = nullptr);

// *********************
// 3D Intersection Tests
// *********************

float RayRayDist(DirectX::XMFLOAT3 const& a, DirectX::XMFLOAT3 const& aDir, DirectX::XMFLOAT3 const& b, DirectX::XMFLOAT3 const& bDir,
                 DirectX::XMFLOAT3* posOnA = nullptr, DirectX::XMFLOAT3* posOnB = nullptr);

bool RayTriIntersection(DirectX::XMFLOAT3 const& orig, DirectX::XMFLOAT3 const& dir, DirectX::XMFLOAT3 const& vert0, DirectX::XMFLOAT3 const& vert1,
                        DirectX::XMFLOAT3 const& vert2, float _rayLen = 1e10, DirectX::XMFLOAT3* result = nullptr);

bool RaySphereIntersection(DirectX::XMFLOAT3 const& rayStart, DirectX::XMFLOAT3 const& rayDir, DirectX::XMFLOAT3 const& spherePos, float sphereRadius,
                           float _rayLen = 1e10, DirectX::XMFLOAT3* pos = nullptr, DirectX::XMFLOAT3* normal = nullptr);

bool SphereSphereIntersection(DirectX::XMFLOAT3 const& _sphere1Pos, float _sphere1Radius, DirectX::XMFLOAT3 const& _sphere2Pos, float _sphere2Radius);

bool SphereTriangleIntersection(DirectX::XMFLOAT3 const& sphereCentre, float sphereRadius, DirectX::XMFLOAT3 const& t1, DirectX::XMFLOAT3 const& t2,
                                DirectX::XMFLOAT3 const& t3);
