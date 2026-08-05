#pragma once

#include "NeuronMath.h"

// For GL/gl.h. Included directly rather than leaning on the includer's pch, so
// this header stands on its own.
#include "NeuronCore.h"


// The bridge between an XMVECTOR and a legacy OpenGL immediate-mode vertex, and
// the reason directxmath-migration needs one at all.
//
// The old code wrote `glVertex3fv(v.GetData())`, where GetData handed &x to the
// driver as three contiguous floats -- a Vector3 IS three packed floats, so
// that was sound. An XMVECTOR is not: it is four lanes in a register, with no
// address to take and a w component the driver must not read. Every one of the
// 513 glVertex3fv call sites in the tree therefore has to round-trip through an
// XMFLOAT3 on its way out.
//
// Written once here rather than as a file-static in each of the 52 files that
// need it. T10 and T14 had already grown five byte-identical copies -- in
// TextRenderer, Engineer, Officer, Citizen and LaserTrooper, each with a
// comment pointing at the others -- and T16 folded them onto this one on its
// way past. Reach for it instead of writing a sixth.
//
// It is NOT transitional. Unlike AsLegacy, this outlives T25, because the
// mismatch it bridges is between DirectXMath and OpenGL rather than between the
// old math types and the new ones.
inline void EmitVertex(DirectX::FXMVECTOR _position)
{
  DirectX::XMFLOAT3 vertex;
  DirectX::XMStoreFloat3(&vertex, _position);
  glVertex3fv(&vertex.x);
}
