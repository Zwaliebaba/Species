#include "pch.h"

#include <cmath>

#include "MathUtils.h"
#include "NeuronMath.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronCoreTests
{
  // WHAT SURVIVED T25, and why the rest did not.
  //
  // Most of what stood here compared a native result against the legacy class
  // it replaced: Matrix34 against XMFLOAT4X4, Matrix33's transposing
  // conversion, the two Matrix33 constructor pins, Vector3::Normalise's
  // zero-length fallback, and the seam's own aliasing guarantees. Every one of
  // those tests named a type this task deleted, and a test that cannot name
  // its subject is not one that can be updated. They did their job — each
  // pinned a mapping while it was being applied, and two of them, the
  // yaw/dive/roll composition order and the reversed matrix product, caught
  // readings that compiled and were wrong.
  //
  // What is left is the conventions themselves, asserted natively: the rows
  // are right / up / front / position, in that order; a transform is
  // row-vector; and the front row is stored as given. Those outlive the
  // migration, because the planned Direct3D backend and every future call site
  // depend on them.
  TEST_CLASS(NeuronMathTests)
  {
      // RELATIVE, not absolute, and the first CI run of this suite is why. An
      // absolute 1e-6f failed with "Expected:<22.7791> Actual:<22.7791>" — a
      // float carries about seven significant digits, so one ULP at that
      // magnitude is already ~2.7e-6 and the tolerance was tighter than the
      // type. Loose enough to absorb a few ULP, nowhere near loose enough to
      // hide a transposed matrix, which moves a coordinate by whole units.
      static void AssertNearlyEqual(float _expected, float _actual)
      {
        float const tolerance = std::max(1e-5f, std::fabs(_expected) * 1e-5f);
        Assert::AreEqual(_expected, _actual, tolerance);
      }

    public:
      // THE CONVENTION THE WHOLE PLAN TURNS ON, stated without reference to
      // anything it replaced. Row 0 is right, row 1 is up, row 2 is front, row
      // 3 is the position with w = 1.
      TEST_METHOD(BasisFromFrontAndUpProducesRightUpFrontAndPosition)
      {
        DirectX::XMFLOAT3 const front(0.0f, 0.0f, 1.0f);
        DirectX::XMFLOAT3 const up(0.0f, 1.0f, 0.0f);
        DirectX::XMFLOAT3 const position(11.0f, -3.0f, 7.5f);

        DirectX::XMFLOAT4X4 basis;
        DirectX::XMStoreFloat4x4(&basis,
                                 BasisFromFrontAndUp(DirectX::XMLoadFloat3(&front), DirectX::XMLoadFloat3(&up), DirectX::XMLoadFloat3(&position)));

        // right is up x front, which for (0,1,0) x (0,0,1) is (1,0,0). The
        // operand order is the thing being pinned: front x up is its negative.
        AssertNearlyEqual(1.0f, basis._11);
        AssertNearlyEqual(0.0f, basis._12);
        AssertNearlyEqual(0.0f, basis._13);

        // up is front x right, back to (0,1,0).
        AssertNearlyEqual(0.0f, basis._21);
        AssertNearlyEqual(1.0f, basis._22);
        AssertNearlyEqual(0.0f, basis._23);

        // front is row 2, exactly as handed in.
        AssertNearlyEqual(0.0f, basis._31);
        AssertNearlyEqual(0.0f, basis._32);
        AssertNearlyEqual(1.0f, basis._33);

        // position is row 3, and w is 1 — the column Matrix34 never had, and
        // the one a writer that fills only twelve floats leaves as garbage.
        AssertNearlyEqual(11.0f, basis._41);
        AssertNearlyEqual(-3.0f, basis._42);
        AssertNearlyEqual(7.5f, basis._43);
        Assert::AreEqual(1.0f, basis._44);
      }

      // A point transformed by that basis lands where the row-vector reading
      // says: v.x*row0 + v.y*row1 + v.z*row2 + row3. Asserted rather than
      // assumed, because `M * v` is the other answer and both compile.
      TEST_METHOD(TransformIsRowVector)
      {
        DirectX::XMFLOAT3 const front(0.0f, 0.0f, 1.0f);
        DirectX::XMFLOAT3 const up(0.0f, 1.0f, 0.0f);
        DirectX::XMFLOAT3 const position(10.0f, 20.0f, 30.0f);

        DirectX::XMMATRIX const basis =
          BasisFromFrontAndUp(DirectX::XMLoadFloat3(&front), DirectX::XMLoadFloat3(&up), DirectX::XMLoadFloat3(&position));

        DirectX::XMFLOAT3 const point(1.0f, 2.0f, 3.0f);
        DirectX::XMFLOAT3 result;
        DirectX::XMStoreFloat3(&result, DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&point), basis));

        AssertNearlyEqual(11.0f, result.x);
        AssertNearlyEqual(22.0f, result.y);
        AssertNearlyEqual(33.0f, result.z);
      }

      // The front row is stored EXACTLY as given and only the two derived rows
      // are normalised. Several callers pass a scaled front and are scaling the
      // model by doing it, so normalising row 2 "for tidiness" would silently
      // resize geometry.
      TEST_METHOD(BasisFromFrontAndUpLeavesTheFrontRowUnnormalised)
      {
        DirectX::XMFLOAT3 const front(0.0f, 0.0f, 4.0f);
        DirectX::XMFLOAT3 const up(0.0f, 1.0f, 0.0f);
        DirectX::XMFLOAT3 const position(0.0f, 0.0f, 0.0f);

        DirectX::XMFLOAT4X4 native;
        DirectX::XMStoreFloat4x4(&native,
                                 BasisFromFrontAndUp(DirectX::XMLoadFloat3(&front), DirectX::XMLoadFloat3(&up), DirectX::XMLoadFloat3(&position)));

        AssertNearlyEqual(4.0f, native._33);
        AssertNearlyEqual(1.0f, native._11); // right stays unit
        AssertNearlyEqual(1.0f, native._22); // and so does up
        Assert::AreEqual(1.0f, native._44);
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
      static DirectX::XMFLOAT3 T1() { return DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f); }
      static DirectX::XMFLOAT3 T2() { return DirectX::XMFLOAT3(10.0f, 0.0f, 0.0f); }
      static DirectX::XMFLOAT3 T3() { return DirectX::XMFLOAT3(0.0f, 0.0f, 10.0f); }

    public:
      TEST_METHOD(ASphereAboveTheFaceIntersectsWhenItReachesThePlane)
      {
        Assert::IsTrue(SphereTriangleIntersection(DirectX::XMFLOAT3(2.0f, 1.0f, 2.0f), 2.0f, T1(), T2(), T3()));
      }

      TEST_METHOD(ASphereTooHighAboveTheFaceMisses)
      {
        Assert::IsFalse(SphereTriangleIntersection(DirectX::XMFLOAT3(2.0f, 50.0f, 2.0f), 2.0f, T1(), T2(), T3()));
      }

      TEST_METHOD(ASphereTouchingAnEdgeIntersects)
      {
        // Centre sits two units outside the x-axis edge, radius three.
        Assert::IsTrue(SphereTriangleIntersection(DirectX::XMFLOAT3(5.0f, 0.0f, -2.0f), 3.0f, T1(), T2(), T3()));
      }

      TEST_METHOD(ASphereJustShortOfAnEdgeMisses)
      {
        Assert::IsFalse(SphereTriangleIntersection(DirectX::XMFLOAT3(5.0f, 0.0f, -8.0f), 3.0f, T1(), T2(), T3()));
      }

      TEST_METHOD(ASphereContainingTheWholeTriangleIntersects)
      {
        Assert::IsTrue(SphereTriangleIntersection(DirectX::XMFLOAT3(3.0f, 0.0f, 3.0f), 100.0f, T1(), T2(), T3()));
      }

      TEST_METHOD(ASphereAtAVertexIntersects) { Assert::IsTrue(SphereTriangleIntersection(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), 0.5f, T1(), T2(), T3())); }

      // The outside-the-triangle-but-near-the-plane case, which is what the six
      // deleted helpers existed to handle. Beyond the hypotenuse, far enough out
      // that only the edge distance can decide it.
      TEST_METHOD(ASphereBeyondTheHypotenuseMisses)
      {
        Assert::IsFalse(SphereTriangleIntersection(DirectX::XMFLOAT3(20.0f, 0.0f, 20.0f), 3.0f, T1(), T2(), T3()));
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
      static DirectX::XMFLOAT3 V0() { return DirectX::XMFLOAT3(-5.0f, -5.0f, 10.0f); }
      static DirectX::XMFLOAT3 V1() { return DirectX::XMFLOAT3(5.0f, -5.0f, 10.0f); }
      static DirectX::XMFLOAT3 V2() { return DirectX::XMFLOAT3(0.0f, 5.0f, 10.0f); }

    public:
      TEST_METHOD(ARayThroughTheMiddleHitsAtTheTrianglesDepth)
      {
        DirectX::XMFLOAT3 result;
        Assert::IsTrue(
          RayTriIntersection(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f), V0(), V1(), V2(), 1e10f, &result));

        AssertNearlyEqual(0.0f, result.x);
        AssertNearlyEqual(0.0f, result.y);
        AssertNearlyEqual(10.0f, result.z);
      }

      TEST_METHOD(ARayBesideTheTriangleMisses)
      {
        Assert::IsFalse(RayTriIntersection(DirectX::XMFLOAT3(50.0f, 0.0f, 0.0f), DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f), V0(), V1(), V2(), 1e10f));
      }

      TEST_METHOD(ARayPointingAwayMisses)
      {
        Assert::IsFalse(RayTriIntersection(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f), V0(), V1(), V2(), 1e10f));
      }

      TEST_METHOD(TheRayLengthCutoffRejectsAHitBeyondIt)
      {
        Assert::IsTrue(RayTriIntersection(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f), V0(), V1(), V2(), 11.0f));
        Assert::IsFalse(RayTriIntersection(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f), V0(), V1(), V2(), 9.0f));
      }

      // Backfaces still count. Moller's formulation here rejected only a
      // near-zero determinant, never a negative one, so it was two-sided;
      // TriangleTests::Intersects is two-sided as well. Landscape.cpp raycasts
      // terrain triangles whose winding depends on which way the mouse ray came
      // in, so this is load-bearing rather than incidental.
      TEST_METHOD(ARayHittingTheBackFaceStillHits)
      {
        DirectX::XMFLOAT3 result;
        Assert::IsTrue(
          RayTriIntersection(DirectX::XMFLOAT3(0.0f, 0.0f, 20.0f), DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f), V0(), V1(), V2(), 1e10f, &result));

        AssertNearlyEqual(10.0f, result.z);
      }

      // A non-unit direction is normalised rather than asserted on. The hit
      // point must land in the same place either way.
      TEST_METHOD(ANonUnitDirectionIsHandledRatherThanAsserted)
      {
        DirectX::XMFLOAT3 result;
        Assert::IsTrue(
          RayTriIntersection(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), DirectX::XMFLOAT3(0.0f, 0.0f, 7.0f), V0(), V1(), V2(), 1e10f, &result));

        AssertNearlyEqual(10.0f, result.z);
      }

      TEST_METHOD(AZeroLengthDirectionMissesRatherThanDividingByZero)
      {
        Assert::IsFalse(RayTriIntersection(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), V0(), V1(), V2(), 1e10f));
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
        DirectX::XMFLOAT3 pos, normal;
        Assert::IsTrue(RaySphereIntersection(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f),
                                             DirectX::XMFLOAT3(10.0f, 0.0f, 0.0f), 2.0f, 1e10f, &pos, &normal));

        AssertNearlyEqual(8.0f, pos.x);
        AssertNearlyEqual(0.0f, pos.y);
        AssertNearlyEqual(-1.0f, normal.x);
      }

      TEST_METHOD(ARayPointingAwayFromTheSphereMisses)
      {
        Assert::IsFalse(RaySphereIntersection(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), DirectX::XMFLOAT3(-1.0f, 0.0f, 0.0f),
                                              DirectX::XMFLOAT3(10.0f, 0.0f, 0.0f), 2.0f));
      }

      TEST_METHOD(ARayPassingBesideTheSphereMisses)
      {
        Assert::IsFalse(RaySphereIntersection(DirectX::XMFLOAT3(0.0f, 50.0f, 0.0f), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f),
                                              DirectX::XMFLOAT3(10.0f, 0.0f, 0.0f), 2.0f));
      }

      // The cutoff is a world distance, and the near face is eight units out.
      TEST_METHOD(TheRayLengthCutoffRejectsAHitBeyondIt)
      {
        Assert::IsTrue(RaySphereIntersection(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f),
                                             DirectX::XMFLOAT3(10.0f, 0.0f, 0.0f), 2.0f, 9.0f));
        Assert::IsFalse(RaySphereIntersection(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f),
                                              DirectX::XMFLOAT3(10.0f, 0.0f, 0.0f), 2.0f, 7.0f));
      }

      // Asking for the normal WITHOUT the position. The body this replaces
      // computed the normal as *pos - spherePos and so dereferenced pos whether
      // or not the caller supplied one — a null dereference that no call site in
      // the tree happens to reach, since the one caller wanting a normal
      // (GameLogic/Tree.cpp) asks for both. The rewrite holds the hit point in a
      // local, so the shape is safe rather than accidentally unreached.
      TEST_METHOD(TheNormalCanBeAskedForWithoutThePosition)
      {
        DirectX::XMFLOAT3 normal;
        Assert::IsTrue(RaySphereIntersection(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f),
                                             DirectX::XMFLOAT3(10.0f, 0.0f, 0.0f), 2.0f, 1e10f, nullptr, &normal));

        AssertNearlyEqual(-1.0f, normal.x);
      }

      TEST_METHOD(SpheresThatOverlapIntersect)
      {
        Assert::IsTrue(SphereSphereIntersection(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), 5.0f, DirectX::XMFLOAT3(8.0f, 0.0f, 0.0f), 5.0f));
      }

      TEST_METHOD(SpheresThatAreApartDoNot)
      {
        Assert::IsFalse(SphereSphereIntersection(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), 5.0f, DirectX::XMFLOAT3(20.0f, 0.0f, 0.0f), 5.0f));
      }

      // Exactly touching counts as intersecting, which is what the <= in the
      // routine this replaces did. Worth pinning: it is the kind of boundary a
      // library swap silently flips.
      TEST_METHOD(SpheresExactlyTouchingIntersect)
      {
        Assert::IsTrue(SphereSphereIntersection(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), 5.0f, DirectX::XMFLOAT3(10.0f, 0.0f, 0.0f), 5.0f));
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
        DirectX::XMFLOAT3 posOnA, posOnB;
        float const dist = RayRayDist(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f), DirectX::XMFLOAT3(0.0f, 5.0f, 0.0f),
                                      DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f), &posOnA, &posOnB);

        AssertNearlyEqual(5.0f, dist);
        AssertNearlyEqual(0.0f, posOnA.y);
        AssertNearlyEqual(5.0f, posOnB.y);
      }

      TEST_METHOD(IntersectingLinesAreZeroApart)
      {
        DirectX::XMFLOAT3 posOnA, posOnB;
        float const dist = RayRayDist(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f), DirectX::XMFLOAT3(3.0f, 0.0f, -4.0f),
                                      DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f), &posOnA, &posOnB);

        AssertNearlyEqual(0.0f, dist);
        AssertNearlyEqual(3.0f, posOnA.x);
      }

      TEST_METHOD(TheOutParametersAreOptional)
      {
        float const dist = RayRayDist(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f), DirectX::XMFLOAT3(0.0f, 5.0f, 0.0f),
                                      DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f));

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
        DirectX::XMFLOAT3 posOnA(1.0f, 2.0f, 3.0f);
        DirectX::XMFLOAT3 posOnB(4.0f, 5.0f, 6.0f);

        float const dist = RayRayDist(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f), DirectX::XMFLOAT3(0.0f, 5.0f, 0.0f),
                                      DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f), &posOnA, &posOnB);

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

      // The legacy half of this test constructed a Vector3 and normalised it,
      // asserting the (0,0,1) it answered with. That class is gone, so what
      // is left is the half that still has a subject: XMVector3Normalize of a
      // zero vector is ZERO, not unit z. It fails if somebody ever "helpfully"
      // gives the native routine the fallback the tree decided against, which
      // would silently change every converted site back.
      TEST_METHOD(TheNativeRoutineAnswersZeroRatherThanUnitZ)
      {
        DirectX::XMVECTOR const native = DirectX::XMVector3Normalize(DirectX::XMVectorZero());

        // Exact, not nearly: nothing here is computed from a length.
        Assert::AreEqual(0.0f, DirectX::XMVectorGetZ(native));
        Assert::AreEqual(0.0f, DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(native)));
      }
  };
} // namespace NeuronCoreTests
