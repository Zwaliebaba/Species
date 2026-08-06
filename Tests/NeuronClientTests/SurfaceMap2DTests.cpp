#include "pch.h"

#include "2dSurfaceMap.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Characterisation of SurfaceMap2D, written BEFORE landscape-index-safety T2
// and T4 change how it behaves at its limits. See
// tasks/landscape-index-safety.yaml.
//
// READ THIS BEFORE CHANGING A NUMBER HERE. Two kinds of assertion live in this
// file and they are not interchangeable:
//
//   - The INTERIOR interpolation values are the contract. They are what the
//     landscape height map and normal map answer everywhere a query is not
//     against the outermost cell, which is almost everywhere. T2 and T4 must
//     not move them, and neither may the chunked storage that replaces the
//     whole-world allocation later (tasks/large-location.yaml T3).
//
//   - The EDGE values are a snapshot of a DEFECT. GetValue truncates its
//     interpolation indices to unsigned short and then wraps anything at or
//     past the far edge to ZERO rather than clamping it (2dSurfaceMap.h,
//     `if (x1 >= this->m_numColumns) x1 = 0;` and its three siblings), so the
//     outermost cell blends the far edge of the map with the NEAR edge, and a
//     query past the edge aliases cell zero outright. Each such test is marked
//     DEFECT and states what a clamping implementation would answer instead.
//     landscape-index-safety T4 makes those the expected values; when it does,
//     these tests change with it and the DEFECT markers go.
//
// Every value below is exact in binary: the cell size is 4 (so 1/cellSize is
// 0.25 exactly), the queries land on quarter-cell boundaries, and the stored
// data are small integers. So the arithmetic cannot round differently on a
// different compiler or instruction set, and exact equality is the right
// assertion rather than a tolerance.

namespace NeuronClientTests
{
  namespace
  {
    // A 4x4-cell map covering 16x16 world units at cell size 4.
    constexpr float MAP_EXTENT = 16.0f;
    constexpr float CELL_SIZE = 4.0f;
    constexpr float OUTSIDE = -1.0f;

    // Cell (x, y) holds x + 10 * y, so a failure message names the cell that
    // was actually read.
    constexpr float CellValue(int _x, int _y) { return static_cast<float>(_x) + 10.0f * static_cast<float>(_y); }

    void FillHeights(Neuron::SurfaceMap2D<float>& _map)
    {
      for (unsigned short y = 0; y < _map.GetNumRows(); ++y)
      {
        for (unsigned short x = 0; x < _map.GetNumColumns(); ++x)
        {
          _map.PutData(x, y, CellValue(x, y));
        }
      }
    }

    // The normal map's element type. Each component varies independently so a
    // single assertion distinguishes an x/y mix-up from a wrong cell.
    void FillNormals(Neuron::SurfaceMap2D<DirectX::XMFLOAT3>& _map)
    {
      for (unsigned short y = 0; y < _map.GetNumRows(); ++y)
      {
        for (unsigned short x = 0; x < _map.GetNumColumns(); ++x)
        {
          _map.PutData(x, y, DirectX::XMFLOAT3(static_cast<float>(x), 10.0f * static_cast<float>(y), 100.0f));
        }
      }
    }
  } // namespace

  TEST_CLASS(SurfaceMap2DTests)
  {
    public:
      TEST_METHOD(ConstructorDerivesCellCountsFromExtentAndCellSize)
      {
        Neuron::SurfaceMap2D<float> const map(MAP_EXTENT, MAP_EXTENT, 0.0f, 0.0f, CELL_SIZE, CELL_SIZE, OUTSIDE);
        Assert::AreEqual(4, static_cast<int>(map.GetNumColumns()));
        Assert::AreEqual(4, static_cast<int>(map.GetNumRows()));
      }

      // THE CONTRACT. Bilinear interpolation over the four surrounding samples,
      // weights (1-fx)(1-fy), (1-fx)fy, fx(1-fy), fx*fy, summed in that order.
      TEST_METHOD(InteriorInterpolationIsBilinear)
      {
        Neuron::SurfaceMap2D<float> map(MAP_EXTENT, MAP_EXTENT, 0.0f, 0.0f, CELL_SIZE, CELL_SIZE, OUTSIDE);
        FillHeights(map);

        // (2, 6) is the centre of the cell square (0,1)-(1,2): the mean of
        // 10, 20, 11, 21.
        Assert::AreEqual(15.5f, map.GetValue(2.0f, 6.0f), L"cell-square centre");

        // (1, 3) sits at fx = 0.25, fy = 0.75 in the square (0,0)-(1,1), so the
        // weights are 0.1875, 0.5625, 0.0625, 0.1875 over 0, 10, 1, 11.
        Assert::AreEqual(7.75f, map.GetValue(1.0f, 3.0f), L"asymmetric weights");

        // Exactly on a sample: both fractions are zero, so the result is that
        // sample and the other three contribute nothing.
        Assert::AreEqual(CellValue(1, 2), map.GetValue(4.0f, 8.0f), L"exactly on sample (1,2)");
        Assert::AreEqual(CellValue(0, 0), map.GetValue(0.0f, 0.0f), L"exactly on the origin sample");
      }

      TEST_METHOD(InteriorInterpolationIsBilinearForTheXmfloat3Specialisation)
      {
        // SurfaceMap2D<XMFLOAT3> has its OWN GetValue — XMFLOAT3 has neither
        // operator* nor operator+, so the arithmetic goes through XMVECTOR.
        // It is a separate body with the same index arithmetic copied into it,
        // which is exactly why it needs its own coverage: a fix applied to one
        // and not the other would leave the normal map behaving differently
        // from the height map.
        Neuron::SurfaceMap2D<DirectX::XMFLOAT3> map(MAP_EXTENT, MAP_EXTENT, 0.0f, 0.0f, CELL_SIZE, CELL_SIZE, DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));
        FillNormals(map);

        DirectX::XMFLOAT3 const centre = map.GetValue(2.0f, 6.0f);
        Assert::AreEqual(0.5f, centre.x, L"x interpolates the column index");
        Assert::AreEqual(15.0f, centre.y, L"y interpolates the row index");
        Assert::AreEqual(100.0f, centre.z, L"a constant component stays constant");

        DirectX::XMFLOAT3 const asymmetric = map.GetValue(1.0f, 3.0f);
        Assert::AreEqual(0.25f, asymmetric.x);
        Assert::AreEqual(7.5f, asymmetric.y);
        Assert::AreEqual(100.0f, asymmetric.z);

        DirectX::XMFLOAT3 const onSample = map.GetValue(4.0f, 8.0f);
        Assert::AreEqual(1.0f, onSample.x, L"exactly on sample (1,2)");
        Assert::AreEqual(20.0f, onSample.y);
      }

      // DEFECT, characterised. A query inside the LAST cell has x2 = 4, which is
      // past the last column, so the wrap sets it to 0 and the outermost cell
      // interpolates the far edge against the NEAR edge.
      //
      // A clamping implementation would answer 8.0 here — the mean of column 3
      // with itself. It answers 6.5, because half the weight lands on column 0.
      // landscape-index-safety T4 changes this to 8.0.
      TEST_METHOD(DefectOutermostCellWrapsToColumnZeroInsteadOfClamping)
      {
        Neuron::SurfaceMap2D<float> map(MAP_EXTENT, MAP_EXTENT, 0.0f, 0.0f, CELL_SIZE, CELL_SIZE, OUTSIDE);
        FillHeights(map);

        float const clampedWouldBe = 0.25f * (CellValue(3, 0) + CellValue(3, 1) + CellValue(3, 0) + CellValue(3, 1));
        Assert::AreEqual(8.0f, clampedWouldBe, L"what a clamping implementation would answer");

        Assert::AreEqual(6.5f, map.GetValue(14.0f, 2.0f), L"DEFECT: column 3 blended with column 0");
        Assert::AreNotEqual(clampedWouldBe, map.GetValue(14.0f, 2.0f), L"and it differs from clamping by a whole 1.5 units");

        // Both axes at once, for completeness: the far corner blends with the
        // near corner on x and on y.
        Assert::AreEqual(16.5f, map.GetValue(14.0f, 14.0f), L"DEFECT: far corner blends with near corner");
      }

      // DEFECT, characterised. Past the far edge both x1 and x2 wrap to zero, so
      // the query aliases cell zero outright rather than answering the edge
      // sample or m_outsideValue. Entities clamp themselves to GetWorldSizeX/Z
      // (GameLogic/Entity.cpp), which is exactly this coordinate.
      TEST_METHOD(DefectPastTheFarEdgeAliasesCellZero)
      {
        Neuron::SurfaceMap2D<float> map(MAP_EXTENT, MAP_EXTENT, 0.0f, 0.0f, CELL_SIZE, CELL_SIZE, OUTSIDE);
        FillHeights(map);

        // 22 is cell coordinate 5.5, well past the last column at index 3.
        Assert::AreEqual(5.0f, map.GetValue(22.0f, 2.0f), L"DEFECT: answers column-zero data");
        Assert::AreEqual(map.GetValue(0.0f, 2.0f), map.GetValue(22.0f, 2.0f), L"DEFECT: indistinguishable from the near edge");

        // And it is not m_outsideValue, which is what Array2D would have
        // answered had the index reached it unwrapped.
        Assert::AreNotEqual(OUTSIDE, map.GetValue(22.0f, 2.0f), L"the wrap happens before Array2D's bounds check");
      }

      // GetValueNearest does NOT subtract m_x0/m_y0, unlike GetValue. Harmless
      // today because every SurfaceMap2D in the tree is built with x0 = y0 = 0
      // (Landscape.cpp and ObstructionGrid.cpp both pass 0.0f), but it means the
      // two accessors disagree the moment one is not. Recorded rather than
      // fixed: no task owns it, and ObstructionGrid::GetBuildings is its only
      // caller.
      TEST_METHOD(GetValueNearestTruncatesAndIgnoresTheOriginOffset)
      {
        Neuron::SurfaceMap2D<float> map(MAP_EXTENT, MAP_EXTENT, 0.0f, 0.0f, CELL_SIZE, CELL_SIZE, OUTSIDE);
        FillHeights(map);

        Assert::AreEqual(CellValue(0, 1), map.GetValueNearest(2.0f, 6.0f), L"floor, not round");
        Assert::AreEqual(CellValue(3, 3), map.GetValueNearest(14.0f, 14.0f), L"the outermost cell reads cleanly");

        // No wrap here — GetValueNearest goes straight to Array2D, whose bounds
        // check answers m_outsideValue. That is the behaviour GetValue is being
        // changed to resemble.
        Assert::AreEqual(OUTSIDE, map.GetValueNearest(22.0f, 2.0f), L"past the edge answers m_outsideValue, no aliasing");
      }

      // DEFECT, characterised. The inner loop bounds x on m_numRows instead of
      // m_numColumns, so on any map with more columns than rows it scans a
      // sub-rectangle and can miss the real maximum. Every map in the tree is
      // square, which is why this has never been seen.
      //
      // landscape-index-safety T4 fixes the bound; this test then becomes a
      // plain assertion that the highest value is found.
      TEST_METHOD(DefectGetHighestValueScansRowsByRowsNotColumnsByRows)
      {
        // Square: the bug is invisible, and the answer is right.
        Neuron::SurfaceMap2D<float> square(MAP_EXTENT, MAP_EXTENT, 0.0f, 0.0f, CELL_SIZE, CELL_SIZE, OUTSIDE);
        FillHeights(square);
        Assert::AreEqual(CellValue(3, 3), square.GetHighestValue(), L"a square map hides it");

        // 8 columns, 2 rows. The true maximum is cell (7, 1) = 17, but the scan
        // only reaches columns 0 and 1, so it answers cell (1, 1) = 11.
        Neuron::SurfaceMap2D<float> wide(32.0f, 8.0f, 0.0f, 0.0f, CELL_SIZE, CELL_SIZE, OUTSIDE);
        FillHeights(wide);
        Assert::AreEqual(8, static_cast<int>(wide.GetNumColumns()));
        Assert::AreEqual(2, static_cast<int>(wide.GetNumRows()));

        Assert::AreEqual(CellValue(7, 1), 17.0f, L"the true maximum of the wide map");
        Assert::AreEqual(11.0f, wide.GetHighestValue(), L"DEFECT: only columns 0..numRows-1 are scanned");
      }

      TEST_METHOD(MapIndexAndRealCoordinateRoundTrip)
      {
        Neuron::SurfaceMap2D<float> const map(MAP_EXTENT, MAP_EXTENT, 0.0f, 0.0f, CELL_SIZE, CELL_SIZE, OUTSIDE);

        Assert::AreEqual(0, map.GetMapIndexX(0.0f));
        Assert::AreEqual(0, map.GetMapIndexX(3.99f), L"truncates within the cell");
        Assert::AreEqual(1, map.GetMapIndexX(4.0f));
        Assert::AreEqual(3, map.GetMapIndexY(15.0f));

        // Deliberately NOT clamped: the index runs past the last cell, which is
        // why every caller bounds-checks it (Landscape::RayHit, SphereHit).
        Assert::AreEqual(5, map.GetMapIndexX(22.0f), L"past the edge is not clamped");

        Assert::AreEqual(0.0f, map.GetRealX(0));
        Assert::AreEqual(12.0f, map.GetRealX(3));
        Assert::AreEqual(12.0f, map.GetRealY(3));
      }
  };
} // namespace NeuronClientTests
