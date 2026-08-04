#include "pch.h"

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
} // namespace NeuronCoreTests
