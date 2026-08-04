#include "pch.h"

#include <math.h>

#include "MathUtils.h"
#include "Vector2.h"
#include "Vector3.h"


float Log2(float x)
{
  static float oneOverLogOf2 = 1.0f / logf(2.0f);
  return logf(x) * oneOverLogOf2;
}


double RampUpAndDown(double _startTime, double _duration, double _timeNow)
{
  if (_timeNow > _startTime + _duration)
    return 1.0001f;

  double fractionalTime = (_timeNow - _startTime) / _duration;

  if (fractionalTime < 0.5)
  {
    double t = fractionalTime * 2.0;
    t *= t;
    t *= 0.5;
    return t;
  }
  else
  {
    double t = (1.0 - fractionalTime) * 2.0;
    t *= t;
    t *= 0.5;
    t = 1.0 - t;
    return t;
  }
}


// ****************************************************************************
// Mersenne Twister Random Number Routines
// ****************************************************************************

/* This code was taken from
   http://www.math.keio.ac.jp/~matumoto/MT2002/emt19937ar.html

   C-program for MT19937, with initialization improved 2002/1/26.
   Coded by Takuji Nishimura and Makoto Matsumoto.

   Before using, initialize the state by using init_genrand(seed)
   or init_by_array(init_key, key_length).

   Copyright (C) 1997 - 2002, Makoto Matsumoto and Takuji Nishimura,
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

     1. Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.

     2. Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.

     3. The names of its contributors may not be used to endorse or promote
        products derived from this software without specific prior written
        permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   Any feedback is very welcome.
   http://www.math.keio.ac.jp/matumoto/emt.html
   email: matumoto@math.keio.ac.jp
*/

/* Period parameters */
#define N 624
#define M 397
#define MATRIX_A 0x9908b0dfUL   /* constant vector a */
#define UPPER_MASK 0x80000000UL /* most significant w-r bits */
#define LOWER_MASK 0x7fffffffUL /* least significant r bits */

static unsigned long mt[N]; /* the array for the state vector  */
static int mti = N + 1;     /* mti==N+1 means mt[N] is not initialized */

/* initializes mt[N] with a seed */
static void init_genrand(unsigned long s)
{
  mt[0] = s & 0xffffffffUL;
  for (mti = 1; mti < N; mti++)
  {
    mt[mti] = (1812433253UL * (mt[mti - 1] ^ (mt[mti - 1] >> 30)) + mti);
    /* See Knuth TAOCP Vol2. 3rd Ed. P.106 for multiplier. */
    /* In the previous versions, MSBs of the seed affect   */
    /* only MSBs of the array mt[].                        */
    /* 2002/01/09 modified by Makoto Matsumoto             */
    mt[mti] &= 0xffffffffUL;
    /* for >32 bit machines */
  }
}


// Generates a random number on [0,0xffffffff]-interval
unsigned long syncrand()
{
  unsigned long y;
  static unsigned long mag01[2] = {0x0UL, MATRIX_A};
  /* mag01[x] = x * MATRIX_A  for x=0,1 */

  if (mti >= N)
  { /* generate N words at one time */
    int kk;

    if (mti == N + 1)       /* if init_genrand() has not been called, */
      init_genrand(5489UL); /* a default initial seed is used */

    for (kk = 0; kk < N - M; kk++)
    {
      y = (mt[kk] & UPPER_MASK) | (mt[kk + 1] & LOWER_MASK);
      mt[kk] = mt[kk + M] ^ (y >> 1) ^ mag01[y & 0x1UL];
    }
    for (; kk < N - 1; kk++)
    {
      y = (mt[kk] & UPPER_MASK) | (mt[kk + 1] & LOWER_MASK);
      mt[kk] = mt[kk + (M - N)] ^ (y >> 1) ^ mag01[y & 0x1UL];
    }
    y = (mt[N - 1] & UPPER_MASK) | (mt[0] & LOWER_MASK);
    mt[N - 1] = mt[M - 1] ^ (y >> 1) ^ mag01[y & 0x1UL];

    mti = 0;
  }

  y = mt[mti++];

  /* Tempering */
  y ^= (y >> 11);
  y ^= (y << 7) & 0x9d2c5680UL;
  y ^= (y << 15) & 0xefc60000UL;
  y ^= (y >> 18);

  return y;
}


// ****************************************************************************
// 2D Intersection Tests
// ****************************************************************************

// Finds the point on the line segment that is nearest to the specified point.
// Often this will be one of the end points of the line segment
float PointSegDist2D(Vector2 const& p,                     // Point
                     Vector2 const& l0, Vector2 const& l1, // Line seg
                     Vector2* result)
{
  // Get direction of line
  Vector2 v = l1 - l0;

  // Get vector from start of line to point
  Vector2 w = p - l0;

  // Compute w dot v;
  float c1 = w.x * v.x + w.y * v.y;

  // If c1 <= 0.0f then the end point l0 is the nearest to p
  if (c1 <= 0.0f)
  {
    if (result)
      *result = l0;
    Vector2 delta = l0 - p;
    return delta.Mag();
  }

  // Compute length squared of v, equivalent to v dot v (a dot b = |a| |b| cos theta)
  float c2 = v.MagSquared();

  // If c2 <= c1 then the end point l1 is the nearest to p
  if (c2 <= c1)
  {
    if (result)
      *result = l1;
    Vector2 delta = l1 - p;
    return delta.Mag();
  }

  // Otherwise the nearest point is somewhere along the segment
  float b = c1 / c2;
  if (result)
    *result = l0 + b * v;

  Vector2 delta = (l0 + b * v) - p;
  return delta.Mag();
}


// Adapted from comp.graphics.algorithms FAQ item 1.03
bool SegRayIntersection2D(Vector2 const& _lineStart, Vector2 const& _lineEnd, Vector2 const& _rayStart, Vector2 const& _rayDir, Vector2* _result)
{
  float r = (_lineStart.y - _rayStart.y) * _rayDir.x - (_lineStart.x - _rayStart.x) * _rayDir.y;
  r /= (_lineEnd.x - _lineStart.x) * _rayDir.y - (_lineEnd.y - _lineStart.y) * _rayDir.x;

  float s = (_lineStart.y - _rayStart.y) * (_lineEnd.x - _lineStart.x) - (_lineStart.x - _rayStart.x) * (_lineEnd.y - _lineStart.y);
  s /= (_lineEnd.x - _lineStart.x) * _rayDir.y - (_lineEnd.y - _lineStart.y) * _rayDir.x;

  if (r >= 0.0f && r <= 1.0f && s >= 0.0f)
  {
    if (_result)
    {
      *_result = _rayStart + _rayDir * s;
    }

    return true;
  }

  return false;
}


// ****************************************************************************
// 3D Intersection Tests
// ****************************************************************************

// Returns the distance between to infinite 3D lines, assuming that they are skew.
// Stores the points of closest approach in posOnA and posOnB
float RayRayDist(Vector3 const& a, Vector3 const& aDir, Vector3 const& b, Vector3 const& bDir, Vector3* posOnA, Vector3* posOnB)
{
  Vector3 temp1, temp2;
  if (posOnA == nullptr)
    posOnA = &temp1;
  if (posOnB == nullptr)
    posOnB = &temp2;

  DirectX::XMVECTOR const aStart = DirectX::XMLoadFloat3(&static_cast<DirectX::XMFLOAT3 const&>(a));
  DirectX::XMVECTOR const bStart = DirectX::XMLoadFloat3(&static_cast<DirectX::XMFLOAT3 const&>(b));
  DirectX::XMVECTOR const aDirection = DirectX::XMLoadFloat3(&static_cast<DirectX::XMFLOAT3 const&>(aDir));
  DirectX::XMVECTOR const bDirection = DirectX::XMLoadFloat3(&static_cast<DirectX::XMFLOAT3 const&>(bDir));

  // The connecting line is perpendicular to both, so a plane containing one ray
  // and parallel to it also contains the other ray's closest approach.
  DirectX::XMVECTOR const cDir = DirectX::XMVector3Cross(aDirection, bDirection);

  DirectX::XMVECTOR const planeA = DirectX::XMPlaneFromPoints(aStart, DirectX::XMVectorAdd(aStart, aDirection), DirectX::XMVectorAdd(aStart, cDir));
  DirectX::XMVECTOR const planeB = DirectX::XMPlaneFromPoints(bStart, DirectX::XMVectorAdd(bStart, bDirection), DirectX::XMVectorAdd(bStart, cDir));

  DirectX::XMVECTOR const onA = DirectX::XMPlaneIntersectLine(planeB, aStart, DirectX::XMVectorAdd(aStart, aDirection));
  DirectX::XMVECTOR const onB = DirectX::XMPlaneIntersectLine(planeA, bStart, DirectX::XMVectorAdd(bStart, bDirection));

  // XMPlaneIntersectLine answers a line parallel to the plane with QNaN, where
  // the routine this replaces returned a status code and left the out-parameter
  // alone. Leaving it alone is the behaviour callers have always seen, so a
  // degenerate pair still yields whatever the caller passed in rather than
  // propagating a NaN into the simulation.
  if (!DirectX::XMVector3IsNaN(onA))
    DirectX::XMStoreFloat3(&static_cast<DirectX::XMFLOAT3&>(*posOnA), onA);
  if (!DirectX::XMVector3IsNaN(onB))
    DirectX::XMStoreFloat3(&static_cast<DirectX::XMFLOAT3&>(*posOnB), onB);

  DirectX::XMVECTOR const separation = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&static_cast<DirectX::XMFLOAT3 const&>(*posOnA)),
                                                                 DirectX::XMLoadFloat3(&static_cast<DirectX::XMFLOAT3 const&>(*posOnB)));

  return DirectX::XMVectorGetX(DirectX::XMVector3Length(separation));
}


bool RayTriIntersection(Vector3 const& orig, Vector3 const& dir, Vector3 const& vert0, Vector3 const& vert1, Vector3 const& vert2, float _rayLen,
                        Vector3* _result)
{
  Vector3 edge1, edge2, tvec, pvec, qvec;
  float det, inv_det;

  // Find vectors for two edges sharing vert0
  edge1 = vert1 - vert0;
  edge2 = vert2 - vert0;

  // Begin calculating determinant - also used to calculate U parameter
  pvec = dir ^ edge2;

  // If determinant is near zero, ray lies in plane of triangle
  det = edge1 * pvec;

  if (det > -0.0000001f && det < 0.0000001f)
  {
    return false;
  }
  inv_det = 1.0f / det;

  /* calculate distance from vert0 to ray origin */
  tvec = orig - vert0;

  Vector3 result;

  /* calculate Y parameter and test bounds */
  result.y = (tvec * pvec) * inv_det;
  if (result.y < 0.0f || result.y > 1.0f)
  {
    return false;
  }

  /* prepare to test Z parameter */
  qvec = tvec ^ edge1;

  /* calculate Z parameter and test bounds */
  result.z = (dir * qvec) * inv_det;
  if (result.z < 0.0f || result.y + result.z > 1.0f)
  {
    return false;
  }

  /* calculate X, ray intersects triangle */
  result.x = (edge2 * qvec) * inv_det;

  //    if (result.x > _rayLen ) return false;
  if (result.MagSquared() > _rayLen * _rayLen)
    return false;

  if (_result)
  {
    *_result = orig + dir * result.x;
  }

  return true;
}


bool RaySphereIntersection(Vector3 const& rayStart, Vector3 const& rayDir, Vector3 const& spherePos, float sphereRadius, float _rayLen, Vector3* pos,
                           Vector3* normal)
{
  Vector3 l = spherePos - rayStart;

  // Find tca the distance along ray of point nearest to sphere centre.
  // We'll call this point P
  float tca = l * rayDir;
  if (tca < 0.0f)
    return false;

  // Use Pythagoras now to find dist from P to sphere centre. Actually
  // cheaper to calc dist sqrd and compare to radius sqrd
  float radiusSqrd = sphereRadius * sphereRadius;
  float lMagSqrd = l.MagSquared();
  float d2 = lMagSqrd - (tca * tca);
  if (d2 > radiusSqrd)
    return false;

  float thc = sqrtf(radiusSqrd - d2);
  float t = tca - thc;

  if (t < 0 || t > _rayLen)
    return false;

  if (pos)
  {
    *pos = rayStart + rayDir * t;
  }

  if (normal)
  {
    *normal = *pos - spherePos;
    normal->Normalise();
  }

  return true;
}


bool SphereSphereIntersection(Vector3 const& _sphere1Pos, float _sphere1Radius, Vector3 const& _sphere2Pos, float _sphere2Radius)
{
  float distanceSqrd = (_sphere1Pos - _sphere2Pos).MagSquared();
  float radiiSummed = _sphere1Radius + _sphere2Radius;
  return (distanceSqrd <= radiiSummed * radiiSummed);
}


bool SphereTriangleIntersection(Vector3 const& sphereCentre, float sphereRadius, Vector3 const& t1, Vector3 const& t2, Vector3 const& t3)
{
  DirectX::BoundingSphere const sphere(sphereCentre, sphereRadius);

  return sphere.Intersects(DirectX::XMLoadFloat3(&static_cast<DirectX::XMFLOAT3 const&>(t1)),
                           DirectX::XMLoadFloat3(&static_cast<DirectX::XMFLOAT3 const&>(t2)),
                           DirectX::XMLoadFloat3(&static_cast<DirectX::XMFLOAT3 const&>(t3)));
}
