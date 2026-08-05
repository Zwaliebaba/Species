#pragma once

#include <DirectXCollision.h>
#include <DirectXMath.h>
#include <cstddef>
#include <type_traits>

// The tree's math conventions, fixed once so that a 200-file migration cannot
// end up half transposed. Read this before converting a call site.
// See tasks/Archive/directxmath-migration.yaml T1.
//
// DirectXMath is header-only and ships in the Windows SDK, so including it adds
// no library to link and no dependency to vendor. This header declares no TYPE:
// the storage types ARE DirectX::XMFLOAT2/3, XMFLOAT3X3 and XMFLOAT4X4, and the
// arithmetic IS XMLoad / XMVector* / XMStore at the call site. There is no
// Neuron vector type and there will not be one.
//
// It declares exactly one FUNCTION, at the bottom, and T14 added it under
// protest of the sentence that used to stand here. BasisFromFrontAndUp replaces
// the Matrix34(front, up, pos) CONSTRUCTOR, which the tree calls 196 times.
// That constructor is not arithmetic — it is the row-vector convention itself,
// written out, and this header exists to fix that convention once. Writing it
// out 196 times instead is 196 chances to get a cross product backwards, in a
// migration whose stated worst failure mode is ending up half transposed. It is
// not the thin end of an operator layer: there is no operator+, no operator*,
// and adding one is still the thing this plan forbids.
//
//
// TYPES
//
//   Vector2   ->  DirectX::XMFLOAT2     storage
//   Vector3   ->  DirectX::XMFLOAT3     storage
//   Matrix33  ->  DirectX::XMFLOAT3X3   storage
//   Matrix34  ->  DirectX::XMFLOAT4X4   storage
//   Plane     ->  a plane XMVECTOR, built by XMPlaneFromPoints
//
//   Computation loads to XMVECTOR / XMMATRIX and stores back. XMVECTOR and
//   XMMATRIX are 16-byte aligned and MUST NOT become data members of
//   heap-allocated game objects without a deliberate alignment decision — that
//   is why storage is the XMFLOAT types.
//
//
// WHICH ONE A PARAMETER TAKES
//
//   XMFLOAT3 const&   the value is STORAGE the caller holds in memory — an
//                     entity's m_pos, a shape marker's world position, a
//                     landscape query's ray. This is most of the tree's APIs.
//
//   FXMVECTOR         the value is a LINK IN A COMPUTATION the caller is
//                     already doing in registers. Then, and only then, the
//                     load is work the caller has done anyway and passing the
//                     register saves a round trip through memory.
//                     A function taking one MUST be XM_CALLCONV, with
//                     FXMVECTOR / GXMVECTOR / HXMVECTOR / CXMVECTOR in the
//                     documented order for the 1st-3rd, 4th, 5th-6th and
//                     remaining vector parameters.
//
//   FXMVECTOR WITHOUT XM_CALLCONV is the one combination to avoid: it asks for
//   a register type and then passes it by the default convention, which costs
//   the alignment constraint and buys none of the speed.
//
//   Do NOT convert a storage-facing API to FXMVECTOR as a tidy-up. Moving the
//   XMLoadFloat3 out of ten callee bodies and into two hundred call sites is a
//   loss, several of those callees only read .x and .z and would spill the
//   register straight back to the stack, and the *Access interfaces are
//   virtual — an override has to match its base exactly, so the choice is not
//   local to one function.
//
//
// MULTIPLICATION IS ROW-VECTOR: v * M, NOT M * v
//
// This is DirectXMath's own convention and the one XMVector3Transform
// implements. A matrix's four rows are the basis vectors and the translation:
//
//   row 0 = right     row 1 = up     row 2 = front     row 3 = position
//
//   v * M  ==  v.x*row0 + v.y*row1 + v.z*row2 + row3
//
//
// THE LEGACY MATRICES DISAGREE WITH EACH OTHER, WHICH IS WHY THIS BLOCK EXISTS
//
// Matrix33 and Matrix34 use OPPOSITE conventions for identically named members,
// and each is internally consistent, so neither looks wrong on its own:
//
//   Matrix34::operator*(Vector3)  ->  v.x*r + v.y*u + v.z*f + pos
//                                     r/u/f are COLUMNS (basis vectors).
//                                     This already IS the row-vector reading,
//                                     so Matrix34's members map straight onto
//                                     XMFLOAT4X4's rows with no transpose.
//
//   Matrix33::operator*(Vector3)  ->  dot(r,v), dot(u,v), dot(f,v)
//                                     r/u/f are ROWS. This is the TRANSPOSE of
//                                     the above, so converting a Matrix33 to
//                                     XMFLOAT3X3 transposes.
//
// Matrix34::GetOr() used to hand r, u and f straight into a Matrix33
// constructor, which transposed the rotation as it crossed. It is gone: its
// only appearances in the tree were four COMMENTED-OUT lines in
// NeuronClient/Shape.cpp, so the bridge between the two conventions was never
// crossed by running code. GetOr and both InverseMultiplyVector overloads were
// deleted with those lines in T10.
//
// The disagreement above therefore matters for the CONVERSIONS ONLY — it is
// why Matrix33::ToNative transposes and Matrix34::ToNative does not — and not
// for any behaviour the game has ever had.
//
// SECOND TRAP, same shape: in BOTH legacy classes `v * M` and `M * v` have
// byte-identical bodies. The operand order is decorative today. Native
// XMVector3Transform is v*M and means it, so a call site that wrote M*v
// expecting the transpose has always been getting the other one — converting it
// "correctly" would be a behaviour change. Read each transform site; do not
// pattern-match the operand order.
//
//
// NORMALISE TAKES THE NATIVE BEHAVIOUR
//
// Vector3::Normalise returns (0,0,1) for a zero-length input.
// XMVector3Normalize returns zero or QNaN. The owner chose native, so the
// fallback is not reproduced. A NaN that reaches an entity's m_pos surfaces
// only in GenerateSyncValue, in Debug, minutes later — see T1's notes for the
// audit of which call sites can actually see a zero-length input.
//
//
// THE COORDINATE SYSTEM IS RIGHT-HANDED, AND STAYS THAT WAY
//
// The inherited code is right-handed, which is the OpenGL convention. It stays
// right-handed: use DirectXMath's *RH variants — XMMatrixLookAtRH,
// XMMatrixPerspectiveFovRH, XMMatrixOrthographicRH — never the LH ones.
//
// This is written down because a renderer migration to Direct3D is planned, and
// LH is D3D's *conventional* choice rather than a requirement. Picking it later
// by reflex would flip winding order and culling against geometry and level data
// that have been right-handed since the code was inherited. D3D renders
// right-handed perfectly well; the rasteriser state is set once to match.
// Owner decision, 2026-08-03.
//
//
// NO MATH TYPE KNOWS WHICH GRAPHICS API IT IS FEEDING
//
// Also because of that planned migration. The math library produces matrices;
// the RENDERER owns whatever a particular API wants doing to them before
// upload. Two things live at that seam and must not migrate inward:
//
//   The transpose. OpenGL takes column-major, D3D's HLSL packs column-major by
//   default, and DirectXMath stores row-major — so both APIs transpose, in
//   renderer code, once.
//
//   The depth range. GL clip space is z in [-1,1] and D3D's is z in [0,1], so
//   XMMatrixPerspectiveFovRH does NOT drop into an OpenGL pipeline unchanged.
//   Camera and projection construction stays in the renderer for exactly this
//   reason. Do not reach for DirectXMath's projection builders from simulation
//   or game code.
//
// A member function named ConvertToOpenGLFormat is the shape being removed
// here, not preserved with a different return type.
//
//
// THE *EST FAMILY IS BANNED IN SIMULATION CODE
//
// XMVector3NormalizeEst, XMVectorReciprocalEst and the rest are explicitly
// permitted to differ between implementations. That makes them a desync
// between two clients on the same build, which is the one thing this migration
// is not allowed to cause. Rendering-only code may use them.
//
//
// WHAT THIS MIGRATION DOES AND DOES NOT CHANGE
//
// It DOES change what the simulation computes: lane arithmetic does not
// reproduce the current scalar arithmetic bit for bit, so a build carrying part
// of this migration desyncs against one that does not. That is sanctioned.
//
// It does NOT change the RNG call sequence, iteration order, container
// identity, or the wire format. Those are separate properties and none of them
// is sanctioned to move.


// THE TWO LEGACY CONSTANTS, kept by NAME and converted rather than deleted.
//
// g_upVector and g_zeroVector were `extern Vector3 const` in Vector3.cpp and
// T25 had to answer for them. Twenty-eight live sites pass one straight into a
// parameter — SpawnEntities' velocity, CreateParticle's velocity, an entity's
// initial m_up — and every one of them reads better for the name than for a
// braced literal repeated in place. DirectX::g_XMIdentityR1 and
// XMVectorZero() are the right answer where an XMVECTOR is wanted; these are
// for the sites that want STORAGE, which is what the parameter takes.
//
// They are the only two names in this header that are not a DirectXMath
// spelling, and they exist because deleting them would have been churn rather
// than progress. Nothing here computes: a constant is not an operator layer.
inline constexpr DirectX::XMFLOAT3 g_upVector{0.0f, 1.0f, 0.0f};
inline constexpr DirectX::XMFLOAT3 g_zeroVector{0.0f, 0.0f, 0.0f};


// Build a world matrix from a front vector, an up hint and a position — the
// replacement for Matrix34(_f, _u, _pos), which 196 sites in the tree call.
//
// The rows are right / up / front / position, per the convention block above.
// Three details are reproduced from the legacy constructor deliberately, and
// each of them is observable:
//
//   The FRONT ROW IS NOT NORMALISED. The legacy constructor stored _f exactly as
//   given and normalised only the two rows it derived. Callers that pass a
//   scaled front are therefore scaling the model, and several do.
//
//   The RIGHT ROW IS up x front, and the UP ROW IS front x right — in that
//   order, each normalised. Reversing either flips a basis vector, which
//   mirrors the model rather than failing to compile.
//
//   A DEGENERATE BASIS gives what XMVector3Normalize gives, per the normalise
//   decision above: front parallel to up yields a zero cross product and the
//   rows come back zero or QNaN rather than falling back to anything.
inline DirectX::XMMATRIX XM_CALLCONV BasisFromFrontAndUp(DirectX::FXMVECTOR _front, DirectX::FXMVECTOR _up, DirectX::FXMVECTOR _position)
{
  DirectX::XMVECTOR const right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(_up, _front));
  DirectX::XMVECTOR const up = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(_front, right));

  DirectX::XMMATRIX result;
  result.r[0] = DirectX::XMVectorSetW(right, 0.0f);
  result.r[1] = DirectX::XMVectorSetW(up, 0.0f);
  result.r[2] = DirectX::XMVectorSetW(_front, 0.0f);
  result.r[3] = DirectX::XMVectorSetW(_position, 1.0f);
  return result;
}
