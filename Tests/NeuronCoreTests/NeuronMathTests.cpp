#include "pch.h"

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
      static constexpr float Tolerance = 1e-6f;

      static void AssertVectorEquals(DirectX::FXMVECTOR _expected, DirectX::FXMVECTOR _actual)
      {
        Assert::AreEqual(DirectX::XMVectorGetX(_expected), DirectX::XMVectorGetX(_actual), Tolerance);
        Assert::AreEqual(DirectX::XMVectorGetY(_expected), DirectX::XMVectorGetY(_actual), Tolerance);
        Assert::AreEqual(DirectX::XMVectorGetZ(_expected), DirectX::XMVectorGetZ(_actual), Tolerance);
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
} // namespace NeuronCoreTests
