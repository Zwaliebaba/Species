#include "pch.h"

#include <cmath>

#include "MathUtils.h"
#include "Matrix33.h"
#include "Matrix34.h"
#include "NeuronMath.h"
#include "Vector2.h"
#include "Vector3.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronCoreTests
{
  // The conversion seam directxmath-migration T1 installs, and the conventions
  // it fixes. These tests exist because the migration's most expensive failure
  // mode is silent: a transposed matrix compiles cleanly, renders plausibly,
  // and is only obviously wrong once somebody looks at the geometry.
  //
  // Most of this suite dies with T25, which deletes the legacy classes. What
  // survives in spirit is the convention: v * M, rows are right/up/front/pos.
  TEST_CLASS(NeuronMathTests)
  {
      // RELATIVE, not absolute, and the first CI run is why. An absolute 1e-6f
      // failed on Matrix34TransformsIdenticallyThroughTheNativeType with
      // "Expected:<22.7791> Actual:<22.7791>" — a float carries about seven
      // significant digits, so one ULP at that magnitude is already ~2.7e-6 and
      // the tolerance was tighter than the type.
      //
      // That difference IS the migration's whole premise, showing up in the
      // smallest case there is: the legacy operator sums r.x*v.x + u.x*v.y +
      // f.x*v.z + pos.x in that order, and XMVector3Transform does it four lanes
      // at a time, possibly contracting to FMA. They agree to the precision
      // float has and not beyond it, by design.
      //
      // This is loose enough to absorb a few ULP and nowhere near loose enough
      // to hide a transposed matrix, which moves a coordinate by whole units.
      // Every later task in the migration wants this helper, not a literal.
      static void AssertNearlyEqual(float _expected, float _actual)
      {
        float const tolerance = std::max(1e-5f, std::fabs(_expected) * 1e-5f);
        Assert::AreEqual(_expected, _actual, tolerance);
      }

      static void AssertVectorEquals(DirectX::FXMVECTOR _expected, DirectX::FXMVECTOR _actual)
      {
        AssertNearlyEqual(DirectX::XMVectorGetX(_expected), DirectX::XMVectorGetX(_actual));
        AssertNearlyEqual(DirectX::XMVectorGetY(_expected), DirectX::XMVectorGetY(_actual));
        AssertNearlyEqual(DirectX::XMVectorGetZ(_expected), DirectX::XMVectorGetZ(_actual));
      }

    public:
      TEST_METHOD(Vector3ConvertsToAndFromTheNativeTypeWithoutCopying)
      {
        Vector3 legacy(1.0f, 2.0f, 3.0f);

        DirectX::XMFLOAT3 const& asNative = legacy;
        Assert::AreEqual(1.0f, asNative.x);
        Assert::AreEqual(2.0f, asNative.y);
        Assert::AreEqual(3.0f, asNative.z);

        // The reference conversion aliases rather than copies, which is the
        // property that lets a Vector3* be handed to an XMFLOAT3* parameter.
        Assert::IsTrue(static_cast<void const*>(&asNative) == static_cast<void const*>(&legacy));

        Vector3 roundTripped = DirectX::XMFLOAT3(4.0f, 5.0f, 6.0f);
        Assert::AreEqual(4.0f, roundTripped.x);
        Assert::AreEqual(5.0f, roundTripped.y);
        Assert::AreEqual(6.0f, roundTripped.z);
      }

      // AsLegacy is the other half of the seam, added by T14 so that the 45
      // files reading WorldObject's converted storage keep compiling until
      // their own tasks convert them. The property that matters is that it
      // ALIASES: 97 of those sites mutate through the member, and a helper that
      // copied would drop their writes with nothing to catch it.
      TEST_METHOD(AsLegacyAliasesTheNativeTypeRatherThanCopyingIt)
      {
        DirectX::XMFLOAT3 native(1.0f, 2.0f, 3.0f);

        Assert::IsTrue(static_cast<void const*>(&AsLegacy(native)) == static_cast<void const*>(&native));
        Assert::AreEqual(1.0f, AsLegacy(native).x);
        AssertNearlyEqual(sqrtf(14.0f), AsLegacy(native).Mag());

        // A write through the legacy view has to land in the native object.
        AsLegacy(native) *= 2.0f;
        Assert::AreEqual(2.0f, native.x);
        Assert::AreEqual(4.0f, native.y);
        Assert::AreEqual(6.0f, native.z);

        AsLegacy(native).Normalise();
        AssertNearlyEqual(1.0f, AsLegacy(native).Mag());

        DirectX::XMFLOAT3 const readOnly(7.0f, 8.0f, 9.0f);
        Assert::AreEqual(8.0f, AsLegacy(readOnly).y);
      }

      TEST_METHOD(Vector2ConvertsToAndFromTheNativeType)
      {
        Vector2 legacy(1.5f, 2.5f);

        DirectX::XMFLOAT2 const& asNative = legacy;
        Assert::AreEqual(1.5f, asNative.x);
        Assert::AreEqual(2.5f, asNative.y);
        Assert::IsTrue(static_cast<void const*>(&asNative) == static_cast<void const*>(&legacy));

        Vector2 roundTripped = DirectX::XMFLOAT2(3.5f, 4.5f);
        Assert::AreEqual(3.5f, roundTripped.x);
        Assert::AreEqual(4.5f, roundTripped.y);
      }

      // Vector3::FastRotateAround is Rodrigues' formula, and the conversion
      // every remaining task reaches for is XMVector3Transform with
      // XMMatrixRotationAxis. That they agree is a CLAIM ABOUT HANDEDNESS, and
      // DirectXMath's own documentation describes its rotations as clockwise
      // looking along the axis, which reads like the opposite convention. It is
      // not -- the matrix is the same one -- but "reads like the opposite" is
      // exactly how a migration ends up mirrored, so it is pinned rather than
      // argued. T14 converts three of these; T15 converts far more.
      TEST_METHOD(NativeAxisRotationMatchesTheLegacyRodriguesRotation)
      {
        Vector3 const axis = Vector3(0.3f, 0.8f, -0.5f).Normalise();
        float const angle = 0.9f;

        Vector3 legacy(4.0f, -2.0f, 7.0f);
        legacy.FastRotateAround(axis, angle);

        DirectX::XMFLOAT3 const start(4.0f, -2.0f, 7.0f);
        DirectX::XMFLOAT3 native;
        DirectX::XMStoreFloat3(&native, DirectX::XMVector3Transform(
                                          DirectX::XMLoadFloat3(&start),
                                          DirectX::XMMatrixRotationAxis(DirectX::XMLoadFloat3(&static_cast<DirectX::XMFLOAT3 const&>(axis)), angle)));

        AssertNearlyEqual(legacy.x, native.x);
        AssertNearlyEqual(legacy.y, native.y);
        AssertNearlyEqual(legacy.z, native.z);
      }

      // The same claim for the axis-aligned helper, which is the one T14 uses.
      TEST_METHOD(NativeRotationYMatchesLegacyRotateAroundY)
      {
        Vector3 legacy(4.0f, -2.0f, 7.0f);
        legacy.RotateAroundY(0.9f);

        DirectX::XMFLOAT3 const start(4.0f, -2.0f, 7.0f);
        DirectX::XMFLOAT3 native;
        DirectX::XMStoreFloat3(&native, DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&start), DirectX::XMMatrixRotationY(0.9f)));

        AssertNearlyEqual(legacy.x, native.x);
        AssertNearlyEqual(legacy.y, native.y);
        AssertNearlyEqual(legacy.z, native.z);
      }

      // Matrix33(yaw, dive, roll) is the constructor Explosion rebuilds every
      // tick for tumbling debris, and it has five other users -- LaserFence,
      // Location, Shape, Camera and Matrix34 itself.
      //
      // IT IS NOT XMMatrixRotationRollPitchYaw, and that is the whole point of
      // this test. The constructor calls RotateAroundZ, then RotateAroundX,
      // then RotateAroundY, which reads exactly like RPY's documented "roll,
      // then pitch, then yaw" order -- so the obvious conversion is
      // XMMatrixRotationRollPitchYaw(dive, yaw, roll) with the first two
      // arguments swapped. That is WRONG. The legacy calls rotate the matrix's
      // three basis ROWS in place rather than composing in row-vector order,
      // which reverses the composition: the equivalent is
      // RotationY * RotationX * RotationZ, where RPY gives Z * X * Y.
      //
      // Both compile. Both are rotations. The wrong one tumbles debris the
      // wrong way and nothing in the build, the suite or CI would say so, which
      // is why directxmath-migration T19 stopped and wrote this before
      // converting Explosion.
      TEST_METHOD(NativeRotationMatchesTheLegacyYawDiveRollConstructor)
      {
        float const yaw = 0.7f, dive = -0.35f, roll = 1.1f;
        Vector3 const v(4.0f, -2.0f, 7.0f);

        Vector3 const legacy = Matrix33(yaw, dive, roll) * v;

        DirectX::XMMATRIX const native = DirectX::XMMatrixRotationY(yaw) * DirectX::XMMatrixRotationX(dive) * DirectX::XMMatrixRotationZ(roll);
        DirectX::XMFLOAT3 const start(4.0f, -2.0f, 7.0f);
        DirectX::XMFLOAT3 result;
        DirectX::XMStoreFloat3(&result, DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&start), native));

        AssertNearlyEqual(legacy.x, result.x);
        AssertNearlyEqual(legacy.y, result.y);
        AssertNearlyEqual(legacy.z, result.z);
      }

      // The SECOND trap in the same conversion, and the nastier of the two.
      //
      // Matrix33::operator* is an ordinary row-major product, but Matrix33
      // transforms a vector as M * v (its rows are dotted with v) while
      // XMVector3Transform computes v * M. The native matrix is therefore the
      // transpose of the legacy one, and transposing reverses a product:
      // (M * R) transposed is R-transposed * M-transposed. So
      //
      //     m_rotMat *= rotIncrement          (Tumbler::Advance)
      //
      // becomes rotMat = increment * rotMat, NOT rotMat * increment.
      //
      // Writing it in the original order is wrong by about two percent after
      // one tick -- close enough to look right, and it compounds every frame,
      // which is the worst way for this to be wrong.
      TEST_METHOD(NativeMatrixProductReversesRelativeToTheLegacyOne)
      {
        Matrix33 const legacyM(0.20f, -0.11f, 0.37f);
        Matrix33 const legacyR(0.05f, 0.09f, -0.03f);
        Vector3 const legacy = (legacyM * legacyR) * Vector3(4.0f, -2.0f, 7.0f);

        auto const native = [](float yaw, float dive, float roll)
        { return DirectX::XMMatrixRotationY(yaw) * DirectX::XMMatrixRotationX(dive) * DirectX::XMMatrixRotationZ(roll); };
        DirectX::XMFLOAT3 const start(4.0f, -2.0f, 7.0f);

        DirectX::XMFLOAT3 reversed;
        DirectX::XMStoreFloat3(
          &reversed, DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&start), native(0.05f, 0.09f, -0.03f) * native(0.20f, -0.11f, 0.37f)));
        AssertNearlyEqual(legacy.x, reversed.x);
        AssertNearlyEqual(legacy.y, reversed.y);
        AssertNearlyEqual(legacy.z, reversed.z);

        // And the same-order version really does differ, so this is a fact
        // about the conversion rather than a coincidence of these angles.
        DirectX::XMFLOAT3 sameOrder;
        DirectX::XMStoreFloat3(
          &sameOrder, DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&start), native(0.20f, -0.11f, 0.37f) * native(0.05f, 0.09f, -0.03f)));
        Assert::IsTrue(std::fabs(legacy.x - sameOrder.x) > 0.05f || std::fabs(legacy.y - sameOrder.y) > 0.02f ||
                         std::fabs(legacy.z - sameOrder.z) > 0.05f,
                       L"the product order stopped mattering; re-derive the conversion in Tumbler::Advance.");
      }

      // The negative control for the test above: the reading that looks right
      // is measurably wrong, so if someone "simplifies" the conversion to RPY
      // later, this fails and says why.
      TEST_METHOD(TheRollPitchYawReadingOfThatConstructorIsWrong)
      {
        float const yaw = 0.7f, dive = -0.35f, roll = 1.1f;
        Vector3 const legacy = Matrix33(yaw, dive, roll) * Vector3(4.0f, -2.0f, 7.0f);

        DirectX::XMFLOAT3 const start(4.0f, -2.0f, 7.0f);
        DirectX::XMFLOAT3 rpy;
        DirectX::XMStoreFloat3(&rpy,
                               DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&start), DirectX::XMMatrixRotationRollPitchYaw(dive, yaw, roll)));

        Assert::IsTrue(std::fabs(legacy.x - rpy.x) > 0.5f || std::fabs(legacy.y - rpy.y) > 0.5f || std::fabs(legacy.z - rpy.z) > 0.5f,
                       L"RollPitchYaw now agrees with the legacy constructor; re-derive the mapping in the test above.");
      }

      // BasisFromFrontAndUp replaces the Matrix34(front, up, pos) constructor
      // that 196 sites call, so it is asserted AGAINST that constructor rather
      // than against my reading of it — the discipline T1 settled on. A
      // reversed cross product mirrors the model and compiles perfectly.
      TEST_METHOD(BasisFromFrontAndUpReproducesTheLegacyConstructor)
      {
        Vector3 const front(0.3f, 0.6f, -0.74f);
        Vector3 const up(0.0f, 1.0f, 0.0f);
        Vector3 const position(11.0f, -3.0f, 7.5f);

        Matrix34 const legacy(front, up, position);

        DirectX::XMFLOAT4X4 native;
        DirectX::XMStoreFloat4x4(&native, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&static_cast<DirectX::XMFLOAT3 const&>(front)),
                                                              DirectX::XMLoadFloat3(&static_cast<DirectX::XMFLOAT3 const&>(up)),
                                                              DirectX::XMLoadFloat3(&static_cast<DirectX::XMFLOAT3 const&>(position))));

        DirectX::XMFLOAT4X4 const expected = legacy.ToNative();
        for (int row = 0; row < 4; ++row)
        {
          for (int column = 0; column < 4; ++column)
          {
            AssertNearlyEqual(expected.m[row][column], native.m[row][column]);
          }
        }
      }

      // The legacy constructor stored the front vector EXACTLY as given and
      // normalised only the two rows it derived. Several callers pass a scaled
      // front and are scaling the model by doing it, so normalising row 2
      // "for tidiness" would silently resize geometry.
      TEST_METHOD(BasisFromFrontAndUpLeavesTheFrontRowUnnormalised)
      {
        Vector3 const front(0.0f, 0.0f, 4.0f);
        Vector3 const up(0.0f, 1.0f, 0.0f);
        Vector3 const position(0.0f, 0.0f, 0.0f);

        DirectX::XMFLOAT4X4 native;
        DirectX::XMStoreFloat4x4(&native, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&static_cast<DirectX::XMFLOAT3 const&>(front)),
                                                              DirectX::XMLoadFloat3(&static_cast<DirectX::XMFLOAT3 const&>(up)),
                                                              DirectX::XMLoadFloat3(&static_cast<DirectX::XMFLOAT3 const&>(position))));

        AssertNearlyEqual(4.0f, native._33);
        AssertNearlyEqual(1.0f, native._11); // right stays unit
        AssertNearlyEqual(1.0f, native._22); // and so does up
        Assert::AreEqual(1.0f, native._44);
      }

      // THE TEST THIS WHOLE TASK EXISTS FOR. If the row-vector decision is
      // wrong, or if ToNative transposes when it should not, this fails —
      // rather than the game rendering inside out three hundred commits later.
      TEST_METHOD(Matrix34TransformsIdenticallyThroughTheNativeType)
      {
        Matrix34 legacy(0);
        legacy.RotateAroundY(0.7f);
        legacy.RotateAroundX(0.3f);
        legacy.pos = Vector3(10.0f, 20.0f, 30.0f);

        Vector3 const point(1.0f, 2.0f, 3.0f);
        Vector3 const legacyResult = legacy * point;

        DirectX::XMFLOAT4X4 const native = legacy.ToNative();
        DirectX::XMVECTOR const nativeResult =
          DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&static_cast<DirectX::XMFLOAT3 const&>(point)), DirectX::XMLoadFloat4x4(&native));

        AssertVectorEquals(DirectX::XMLoadFloat3(&static_cast<DirectX::XMFLOAT3 const&>(legacyResult)), nativeResult);
      }

      TEST_METHOD(Matrix34RowsAreRightUpFrontAndPosition)
      {
        Matrix34 legacy(0);
        legacy.r = Vector3(1.0f, 2.0f, 3.0f);
        legacy.u = Vector3(4.0f, 5.0f, 6.0f);
        legacy.f = Vector3(7.0f, 8.0f, 9.0f);
        legacy.pos = Vector3(10.0f, 11.0f, 12.0f);

        DirectX::XMFLOAT4X4 const native = legacy.ToNative();

        Assert::AreEqual(1.0f, native._11);
        Assert::AreEqual(2.0f, native._12);
        Assert::AreEqual(3.0f, native._13);
        Assert::AreEqual(4.0f, native._21);
        Assert::AreEqual(7.0f, native._31);
        Assert::AreEqual(10.0f, native._41);
        Assert::AreEqual(1.0f, native._44);
      }

      // Matrix33 reads the same member names as ROWS where Matrix34 reads them
      // as columns, so this conversion transposes and Matrix34's does not.
      // Pinning both together is the only way the asymmetry stays deliberate.
      TEST_METHOD(Matrix33TransformsIdenticallyThroughTheNativeTypeAndTransposes)
      {
        Matrix33 legacy(0);
        legacy.RotateAroundY(0.7f);
        legacy.RotateAroundZ(0.2f);

        Vector3 const point(1.0f, 2.0f, 3.0f);
        Vector3 const legacyResult = legacy * point;

        DirectX::XMFLOAT3X3 const native = legacy.ToNative();
        DirectX::XMVECTOR const nativeResult =
          DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&static_cast<DirectX::XMFLOAT3 const&>(point)), DirectX::XMLoadFloat3x3(&native));

        AssertVectorEquals(DirectX::XMLoadFloat3(&static_cast<DirectX::XMFLOAT3 const&>(legacyResult)), nativeResult);

        // Stated explicitly: the native rows are the legacy columns.
        Assert::AreEqual(legacy.r.x, native._11);
        Assert::AreEqual(legacy.u.x, native._12);
        Assert::AreEqual(legacy.f.x, native._13);
        Assert::AreEqual(legacy.r.y, native._21);
      }

      TEST_METHOD(MatrixRoundTripsThroughTheNativeTypeUnchanged)
      {
        Matrix34 legacy(0);
        legacy.RotateAroundZ(1.1f);
        legacy.pos = Vector3(-1.0f, 2.0f, -3.0f);

        Matrix34 const roundTripped = Matrix34::FromNative(legacy.ToNative());

        Assert::IsTrue(legacy == roundTripped);
      }

      // The legacy classes make `v * m` and `m * v` the same expression — both
      // operators have byte-identical bodies. That is not a typo in the test:
      // it is the contract the tree has today, and the migration must carry
      // what the code does rather than what the operand order suggests.
      // Native XMVector3Transform is v * M and means it.
      TEST_METHOD(LegacyOperandOrderIsDecorative)
      {
        Matrix34 legacy(0);
        legacy.RotateAroundY(0.9f);
        legacy.pos = Vector3(5.0f, 6.0f, 7.0f);

        Vector3 const point(1.0f, 2.0f, 3.0f);

        Assert::IsTrue((legacy * point) == (point * legacy));
      }

      // The behaviour change the owner signed off on. Vector3::Normalise
      // answers a zero-length input with (0,0,1); XMVector3Normalize does not,
      // and the tree is taking the native answer. Pinned so that the change is
      // a decision on the record rather than a surprise in a later diff.
      TEST_METHOD(NativeNormaliseDoesNotReproduceTheLegacyZeroLengthFallback)
      {
        Vector3 legacyZero(0.0f, 0.0f, 0.0f);
        legacyZero.Normalise();
        Assert::AreEqual(0.0f, legacyZero.x);
        Assert::AreEqual(0.0f, legacyZero.y);
        Assert::AreEqual(1.0f, legacyZero.z);

        DirectX::XMFLOAT3 const zero(0.0f, 0.0f, 0.0f);
        DirectX::XMVECTOR const nativeZero = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&zero));
        Assert::AreNotEqual(1.0f, DirectX::XMVectorGetZ(nativeZero));

        // On a non-degenerate input the two agree, which is why the fallback is
        // the only thing being given up.
        Vector3 legacy(3.0f, 0.0f, 4.0f);
        legacy.Normalise();
        DirectX::XMFLOAT3 const same(3.0f, 0.0f, 4.0f);
        AssertVectorEquals(DirectX::XMLoadFloat3(&static_cast<DirectX::XMFLOAT3 const&>(legacy)),
                           DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&same)));
      }
  };

  // SphereTriangleIntersection, which directxmath-migration T5 rebuilt on
  // DirectX::BoundingSphere::Intersects.
  //
  // These state the NEW contract rather than reproducing the old one, and that
  // is deliberate. What was here before projected the sphere centre into plane
  // space, ran a triangle-area point test with a 1.0001f fudge factor, and fell
  // back to three 2D point-segment distances for the outside-the-triangle case
  // — about a third of MathUtils.cpp, and six helper functions that existed for
  // no other caller. BoundingSphere::Intersects is a different algorithm, so
  // edge and near-degenerate cases can answer differently. Its one caller is
  // Shape.cpp's collision test, where the visible effect is which shape
  // fragment a click or a projectile registers against.
  TEST_CLASS(SphereTriangleIntersectionTests)
  {
      // A right triangle on the ground plane, so the geometry is easy to reason
      // about: corner at the origin, ten units along x, ten along z.
      static Vector3 T1() { return Vector3(0.0f, 0.0f, 0.0f); }
      static Vector3 T2() { return Vector3(10.0f, 0.0f, 0.0f); }
      static Vector3 T3() { return Vector3(0.0f, 0.0f, 10.0f); }

    public:
      TEST_METHOD(ASphereAboveTheFaceIntersectsWhenItReachesThePlane)
      {
        Assert::IsTrue(SphereTriangleIntersection(Vector3(2.0f, 1.0f, 2.0f), 2.0f, T1(), T2(), T3()));
      }

      TEST_METHOD(ASphereTooHighAboveTheFaceMisses) { Assert::IsFalse(SphereTriangleIntersection(Vector3(2.0f, 50.0f, 2.0f), 2.0f, T1(), T2(), T3())); }

      TEST_METHOD(ASphereTouchingAnEdgeIntersects)
      {
        // Centre sits two units outside the x-axis edge, radius three.
        Assert::IsTrue(SphereTriangleIntersection(Vector3(5.0f, 0.0f, -2.0f), 3.0f, T1(), T2(), T3()));
      }

      TEST_METHOD(ASphereJustShortOfAnEdgeMisses) { Assert::IsFalse(SphereTriangleIntersection(Vector3(5.0f, 0.0f, -8.0f), 3.0f, T1(), T2(), T3())); }

      TEST_METHOD(ASphereContainingTheWholeTriangleIntersects)
      {
        Assert::IsTrue(SphereTriangleIntersection(Vector3(3.0f, 0.0f, 3.0f), 100.0f, T1(), T2(), T3()));
      }

      TEST_METHOD(ASphereAtAVertexIntersects) { Assert::IsTrue(SphereTriangleIntersection(Vector3(0.0f, 0.0f, 0.0f), 0.5f, T1(), T2(), T3())); }

      // The outside-the-triangle-but-near-the-plane case, which is what the six
      // deleted helpers existed to handle. Beyond the hypotenuse, far enough out
      // that only the edge distance can decide it.
      TEST_METHOD(ASphereBeyondTheHypotenuseMisses)
      {
        Assert::IsFalse(SphereTriangleIntersection(Vector3(20.0f, 0.0f, 20.0f), 3.0f, T1(), T2(), T3()));
      }
  };

  // RayTriIntersection, which directxmath-migration T7 replaced with
  // DirectX::TriangleTests::Intersects. This is the one intersection test with
  // an exact DirectXCollision equivalent including the distance out-parameter,
  // so it is a true replacement rather than a rewrite.
  //
  // TWO THINGS DID CHANGE, and both are visible here rather than buried.
  //
  // The direction is normalised first, because TriangleTests::Intersects
  // requires a unit direction and asserts on one that is not.
  //
  // And the _rayLen cutoff is now a plain distance comparison. What it replaces
  // was `sqrt(t*t + u*u + v*v) > _rayLen` over Moller's barycentric outputs —
  // mixing a distance with two barycentric coordinates, which is not a length
  // of anything. Directly above it sat the commented-out `if (result.x >
  // _rayLen)`, which is the test this now does. For a unit ray and the 1e10
  // cutoff every caller but LaserFence passes, the two agree; where they differ
  // the old one rejected slightly sooner.
  TEST_CLASS(RayTriIntersectionTests)
  {
      static void AssertNearlyEqual(float _expected, float _actual)
      {
        float const tolerance = std::max(1e-5f, std::fabs(_expected) * 1e-5f);
        Assert::AreEqual(_expected, _actual, tolerance);
      }

      // A triangle standing in the x/y plane ten units down z, straddling the
      // axis so a ray along z passes through its middle.
      static Vector3 V0() { return Vector3(-5.0f, -5.0f, 10.0f); }
      static Vector3 V1() { return Vector3(5.0f, -5.0f, 10.0f); }
      static Vector3 V2() { return Vector3(0.0f, 5.0f, 10.0f); }

    public:
      TEST_METHOD(ARayThroughTheMiddleHitsAtTheTrianglesDepth)
      {
        Vector3 result;
        Assert::IsTrue(RayTriIntersection(Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f), V0(), V1(), V2(), 1e10f, &result));

        AssertNearlyEqual(0.0f, result.x);
        AssertNearlyEqual(0.0f, result.y);
        AssertNearlyEqual(10.0f, result.z);
      }

      TEST_METHOD(ARayBesideTheTriangleMisses)
      {
        Assert::IsFalse(RayTriIntersection(Vector3(50.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f), V0(), V1(), V2(), 1e10f));
      }

      TEST_METHOD(ARayPointingAwayMisses)
      {
        Assert::IsFalse(RayTriIntersection(Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, -1.0f), V0(), V1(), V2(), 1e10f));
      }

      TEST_METHOD(TheRayLengthCutoffRejectsAHitBeyondIt)
      {
        Assert::IsTrue(RayTriIntersection(Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f), V0(), V1(), V2(), 11.0f));
        Assert::IsFalse(RayTriIntersection(Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f), V0(), V1(), V2(), 9.0f));
      }

      // Backfaces still count. Moller's formulation here rejected only a
      // near-zero determinant, never a negative one, so it was two-sided;
      // TriangleTests::Intersects is two-sided as well. Landscape.cpp raycasts
      // terrain triangles whose winding depends on which way the mouse ray came
      // in, so this is load-bearing rather than incidental.
      TEST_METHOD(ARayHittingTheBackFaceStillHits)
      {
        Vector3 result;
        Assert::IsTrue(RayTriIntersection(Vector3(0.0f, 0.0f, 20.0f), Vector3(0.0f, 0.0f, -1.0f), V0(), V1(), V2(), 1e10f, &result));

        AssertNearlyEqual(10.0f, result.z);
      }

      // A non-unit direction is normalised rather than asserted on. The hit
      // point must land in the same place either way.
      TEST_METHOD(ANonUnitDirectionIsHandledRatherThanAsserted)
      {
        Vector3 result;
        Assert::IsTrue(RayTriIntersection(Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 7.0f), V0(), V1(), V2(), 1e10f, &result));

        AssertNearlyEqual(10.0f, result.z);
      }

      TEST_METHOD(AZeroLengthDirectionMissesRatherThanDividingByZero)
      {
        Assert::IsFalse(RayTriIntersection(Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), V0(), V1(), V2(), 1e10f));
      }
  };

  // The sphere routines, which directxmath-migration T6 rebuilt on XMVECTOR.
  //
  // RaySphereIntersection kept its geometric solution rather than becoming a
  // BoundingSphere::Intersects call: that reports a distance, and fourteen
  // caller files here want the intersection POINT and the surface NORMAL.
  // SphereSphereIntersection did become one, because a bool is all it returns.
  TEST_CLASS(SphereIntersectionTests)
  {
      static void AssertNearlyEqual(float _expected, float _actual)
      {
        float const tolerance = std::max(1e-5f, std::fabs(_expected) * 1e-5f);
        Assert::AreEqual(_expected, _actual, tolerance);
      }

    public:
      // A unit sphere of radius 2 at x=10, hit head-on from the origin. The near
      // face is at x=8 and the normal there points back down the ray.
      TEST_METHOD(ARayHitsTheNearFaceAndTheNormalPointsOutwards)
      {
        Vector3 pos, normal;
        Assert::IsTrue(
          RaySphereIntersection(Vector3(0.0f, 0.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f), Vector3(10.0f, 0.0f, 0.0f), 2.0f, 1e10f, &pos, &normal));

        AssertNearlyEqual(8.0f, pos.x);
        AssertNearlyEqual(0.0f, pos.y);
        AssertNearlyEqual(-1.0f, normal.x);
      }

      TEST_METHOD(ARayPointingAwayFromTheSphereMisses)
      {
        Assert::IsFalse(RaySphereIntersection(Vector3(0.0f, 0.0f, 0.0f), Vector3(-1.0f, 0.0f, 0.0f), Vector3(10.0f, 0.0f, 0.0f), 2.0f));
      }

      TEST_METHOD(ARayPassingBesideTheSphereMisses)
      {
        Assert::IsFalse(RaySphereIntersection(Vector3(0.0f, 50.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f), Vector3(10.0f, 0.0f, 0.0f), 2.0f));
      }

      // The cutoff is a world distance, and the near face is eight units out.
      TEST_METHOD(TheRayLengthCutoffRejectsAHitBeyondIt)
      {
        Assert::IsTrue(RaySphereIntersection(Vector3(0.0f, 0.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f), Vector3(10.0f, 0.0f, 0.0f), 2.0f, 9.0f));
        Assert::IsFalse(RaySphereIntersection(Vector3(0.0f, 0.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f), Vector3(10.0f, 0.0f, 0.0f), 2.0f, 7.0f));
      }

      // Asking for the normal WITHOUT the position. The body this replaces
      // computed the normal as *pos - spherePos and so dereferenced pos whether
      // or not the caller supplied one — a null dereference that no call site in
      // the tree happens to reach, since the one caller wanting a normal
      // (GameLogic/Tree.cpp) asks for both. The rewrite holds the hit point in a
      // local, so the shape is safe rather than accidentally unreached.
      TEST_METHOD(TheNormalCanBeAskedForWithoutThePosition)
      {
        Vector3 normal;
        Assert::IsTrue(
          RaySphereIntersection(Vector3(0.0f, 0.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f), Vector3(10.0f, 0.0f, 0.0f), 2.0f, 1e10f, nullptr, &normal));

        AssertNearlyEqual(-1.0f, normal.x);
      }

      TEST_METHOD(SpheresThatOverlapIntersect)
      {
        Assert::IsTrue(SphereSphereIntersection(Vector3(0.0f, 0.0f, 0.0f), 5.0f, Vector3(8.0f, 0.0f, 0.0f), 5.0f));
      }

      TEST_METHOD(SpheresThatAreApartDoNot) { Assert::IsFalse(SphereSphereIntersection(Vector3(0.0f, 0.0f, 0.0f), 5.0f, Vector3(20.0f, 0.0f, 0.0f), 5.0f)); }

      // Exactly touching counts as intersecting, which is what the <= in the
      // routine this replaces did. Worth pinning: it is the kind of boundary a
      // library swap silently flips.
      TEST_METHOD(SpheresExactlyTouchingIntersect)
      {
        Assert::IsTrue(SphereSphereIntersection(Vector3(0.0f, 0.0f, 0.0f), 5.0f, Vector3(10.0f, 0.0f, 0.0f), 5.0f));
      }
  };

  // RayRayDist, which directxmath-migration T8 rebuilt on XMPlaneFromPoints and
  // XMPlaneIntersectLine. Deleting the Plane class and RayPlaneIntersection with
  // it left this the only routine in the tree that needs a plane at all, and its
  // three call sites are all in GameLogic/Tripod.cpp — the leg geometry.
  TEST_CLASS(RayRayDistTests)
  {
      static void AssertNearlyEqual(float _expected, float _actual)
      {
        float const tolerance = std::max(1e-5f, std::fabs(_expected) * 1e-5f);
        Assert::AreEqual(_expected, _actual, tolerance);
      }

    public:
      // Two perpendicular skew lines: one along x at the origin, one along z
      // five units up. The closest approach is the vertical gap between them.
      TEST_METHOD(PerpendicularSkewLinesAreSeparatedByTheGapBetweenThem)
      {
        Vector3 posOnA, posOnB;
        float const dist =
          RayRayDist(Vector3(0.0f, 0.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 5.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f), &posOnA, &posOnB);

        AssertNearlyEqual(5.0f, dist);
        AssertNearlyEqual(0.0f, posOnA.y);
        AssertNearlyEqual(5.0f, posOnB.y);
      }

      TEST_METHOD(IntersectingLinesAreZeroApart)
      {
        Vector3 posOnA, posOnB;
        float const dist =
          RayRayDist(Vector3(0.0f, 0.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f), Vector3(3.0f, 0.0f, -4.0f), Vector3(0.0f, 0.0f, 1.0f), &posOnA, &posOnB);

        AssertNearlyEqual(0.0f, dist);
        AssertNearlyEqual(3.0f, posOnA.x);
      }

      TEST_METHOD(TheOutParametersAreOptional)
      {
        float const dist = RayRayDist(Vector3(0.0f, 0.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 5.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f));

        AssertNearlyEqual(5.0f, dist);
      }

      // THE DEGENERATE CASE, and the one place this rewrite had to make a
      // decision rather than a translation. Parallel rays have no unique closest
      // approach: the cross product is zero, so the constructed planes are
      // degenerate. The routine this replaces asked RayPlaneIntersection, got a
      // status code back meaning "parallel, no intersection", ignored it, and
      // left the out-parameters holding whatever the caller passed in.
      //
      // XMPlaneIntersectLine says QNaN instead. Propagating that would put a NaN
      // into Tripod's leg positions and from there into GenerateSyncValue, where
      // it would surface as a desync assert minutes later in a Debug build. So
      // the NaN is tested for and the out-parameters are left alone, which is
      // what callers have always observed. What this asserts is therefore "no
      // NaN escapes", not a distance.
      TEST_METHOD(ParallelLinesLeaveTheOutParametersAloneRatherThanReturningNaN)
      {
        Vector3 posOnA(1.0f, 2.0f, 3.0f);
        Vector3 posOnB(4.0f, 5.0f, 6.0f);

        float const dist =
          RayRayDist(Vector3(0.0f, 0.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 5.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f), &posOnA, &posOnB);

        Assert::IsFalse(std::isnan(posOnA.x) || std::isnan(posOnA.y) || std::isnan(posOnA.z));
        Assert::IsFalse(std::isnan(posOnB.x) || std::isnan(posOnB.y) || std::isnan(posOnB.z));
        Assert::IsFalse(std::isnan(dist));

        AssertNearlyEqual(1.0f, posOnA.x);
        AssertNearlyEqual(4.0f, posOnB.x);
      }
  };


  // THE ZERO-LENGTH NORMALISE DIVERGENCE, pinned after it shipped.
  //
  // NeuronMath.h records the decision: Vector3::Normalise answered a
  // zero-length input with (0,0,1), XMVector3Normalize answers zero, and the
  // migration takes the native behaviour rather than reproducing the fallback.
  // Every call site was supposed to be audited for whether it can actually see
  // a zero-length input. Tree::RenderBranch was missed, and it was the worst
  // possible one to miss: the trunk is RenderBranch((0,0,0),(0,1,0)), so the
  // two vectors it crosses are IDENTICAL and the cross product is exactly zero
  // on every tree. The trees rendered as invisible zero-width lines, and the
  // owner found it in the Garden smoke test rather than CI, because nothing
  // here or in the compiler can see it.
  //
  // These pin the divergence itself so the next audit has something to point
  // at. The second is the negative control: it fails if somebody ever
  // "helpfully" gives XMVector3Normalize the legacy fallback, which would
  // silently change every other converted site back.
  TEST_CLASS(ZeroLengthNormaliseTests)
  {
    public:
      TEST_METHOD(TheCrossProductOfAVectorWithItselfIsExactlyZero)
      {
        DirectX::XMVECTOR const v = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        DirectX::XMVECTOR const cross = DirectX::XMVector3Cross(v, v);

        Assert::AreEqual(0.0f, DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(cross)));
      }

      TEST_METHOD(LegacyNormaliseAnsweredUnitZWhereTheNativeRoutineAnswersZero)
      {
        Vector3 legacy(0.0f, 0.0f, 0.0f);
        legacy.Normalise();

        AssertNearlyEqual(0.0f, legacy.x);
        AssertNearlyEqual(0.0f, legacy.y);
        AssertNearlyEqual(1.0f, legacy.z);

        DirectX::XMVECTOR const native = DirectX::XMVector3Normalize(DirectX::XMVectorZero());

        // The divergence, stated as the assertion rather than as a comment: the
        // native routine does NOT reproduce the legacy fallback, so a site that
        // depended on it has to say so locally.
        Assert::AreEqual(0.0f, DirectX::XMVectorGetZ(native));
        Assert::AreEqual(0.0f, DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(native)));
      }
  };
} // namespace NeuronCoreTests
