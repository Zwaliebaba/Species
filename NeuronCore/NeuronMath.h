#pragma once

#include <DirectXCollision.h>
#include <DirectXMath.h>
#include <cstddef>
#include <type_traits>

// The tree's math conventions, fixed once so that a 200-file migration cannot
// end up half transposed. Read this before converting a call site.
// See tasks/directxmath-migration.yaml T1.
//
// DirectXMath is header-only and ships in the Windows SDK, so including it adds
// no library to link and no dependency to vendor. Nothing else in this header
// is a type or a function: the storage types ARE DirectX::XMFLOAT2/3,
// XMFLOAT3X3 and XMFLOAT4X4, and the arithmetic IS XMLoad / XMVector* /
// XMStore at the call site. There is no Neuron vector type and there will not
// be one.
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
//   is why storage is the XMFLOAT types. Functions taking vectors by value use
//   XM_CALLCONV with FXMVECTOR / GXMVECTOR / CXMVECTOR in the documented order.
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
