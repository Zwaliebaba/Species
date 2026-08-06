#include "pch.h"

#include "2dArray.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Characterisation of Array2D, written BEFORE landscape-index-safety T2 and T4
// change how the class and its SurfaceMap2D subclass behave at their limits.
// Nothing here asserts that the current behaviour is CORRECT — it asserts what
// it currently IS, so that the two conversions change exactly what they mean to
// and nothing else. See tasks/landscape-index-safety.yaml.
//
// The one thing in this file that is a genuine invariant rather than a snapshot
// is the out-of-range contract: every accessor bounds-checks and answers
// m_outsideValue. Landscape and ObstructionGrid both lean on it, and T2/T4
// preserve it.

namespace NeuronCoreTests
{
  namespace
  {
    // Distinct, exactly representable, and readable in a failure message:
    // cell (x, y) holds x + 10 * y.
    constexpr int CellValue(int _x, int _y) { return _x + 10 * _y; }

    // Fills in place rather than returning by value: Array2D declares a
    // destructor, which suppresses the implicit move constructor, so returning
    // one copies the whole vector for no reason.
    void Fill(Array2D<int>& _array)
    {
      for (unsigned short y = 0; y < _array.GetNumRows(); ++y)
      {
        for (unsigned short x = 0; x < _array.GetNumColumns(); ++x)
        {
          _array.PutData(x, y, CellValue(x, y));
        }
      }
    }
  } // namespace

  TEST_CLASS(Array2DTests)
  {
    public:
      TEST_METHOD(ConstructorSetsDimensions)
      {
        Array2D<int> const array(4, 3, -1);
        Assert::AreEqual(4, static_cast<int>(array.GetNumColumns()));
        Assert::AreEqual(3, static_cast<int>(array.GetNumRows()));
      }

      // The header promises cells start value-initialised rather than holding
      // whatever was on the heap, and says why: a cell read before it was
      // written used to yield garbage that could differ between two clients
      // running the same simulation. That is a determinism property, so it is
      // pinned rather than assumed.
      TEST_METHOD(CellsStartValueInitialised)
      {
        Array2D<int> const array(4, 3, -1);
        for (unsigned short y = 0; y < 3; ++y)
        {
          for (unsigned short x = 0; x < 4; ++x)
          {
            Assert::AreEqual(0, array.GetData(x, y));
          }
        }
      }

      TEST_METHOD(PutDataAndGetDataRoundTripInRange)
      {
        Array2D<int> array(4, 3, -1);
        Fill(array);
        for (unsigned short y = 0; y < 3; ++y)
        {
          for (unsigned short x = 0; x < 4; ++x)
          {
            Assert::AreEqual(CellValue(x, y), array.GetData(x, y));
          }
        }
      }

      // THE CONTRACT T2 AND T4 MUST PRESERVE. Every accessor bounds-checks;
      // none of them reads or writes outside the vector. Note the check is `>=`
      // against the dimensions with unsigned indices, so there is no negative
      // case to test — a negative index cannot be expressed.
      TEST_METHOD(GetDataAnswersOutsideValueOutOfRange)
      {
        Array2D<int> array(4, 3, -1);
        Fill(array);

        Assert::AreEqual(-1, array.GetData(4, 0), L"one past the last column");
        Assert::AreEqual(-1, array.GetData(0, 3), L"one past the last row");
        Assert::AreEqual(-1, array.GetData(4, 3), L"past both");
        Assert::AreEqual(-1, array.GetData(65535, 65535), L"far out of range");

        Assert::AreEqual(CellValue(3, 2), array.GetData(3, 2), L"the last cell is in range");
      }

      TEST_METHOD(GetConstPointerAnswersOutsideValueOutOfRange)
      {
        Array2D<int> array(4, 3, -1);
        Fill(array);

        int const* const inRange = array.GetConstPointer(3, 2);
        Assert::IsNotNull(inRange);
        Assert::AreEqual(CellValue(3, 2), *inRange);

        int const* const pastColumns = array.GetConstPointer(4, 0);
        Assert::IsNotNull(pastColumns);
        Assert::AreEqual(-1, *pastColumns, L"out of range answers m_outsideValue, not garbage");

        int const* const pastRows = array.GetConstPointer(0, 3);
        Assert::IsNotNull(pastRows);
        Assert::AreEqual(-1, *pastRows);

        // Every out-of-range query answers the SAME object, which is why the
        // non-const GetPointer below is a hazard worth knowing about.
        Assert::IsTrue(pastColumns == pastRows, L"all out-of-range queries share one address");
      }

      // GetPointer hands out a MUTABLE pointer to m_outsideValue when asked for
      // a cell that does not exist. Writing through it corrupts the value every
      // future out-of-range read answers. No caller in the tree does this today;
      // it is pinned so that a future one is a test failure rather than a
      // mystery.
      TEST_METHOD(GetPointerOutOfRangeAliasesTheOutsideValue)
      {
        Array2D<int> array(4, 3, -1);
        Fill(array);

        int* const outside = array.GetPointer(4, 0);
        Assert::IsNotNull(outside);
        Assert::AreEqual(-1, *outside);

        *outside = 42;
        Assert::AreEqual(42, array.GetData(9, 9), L"the shared outside value is writable through GetPointer");
        Assert::AreEqual(CellValue(3, 2), array.GetData(3, 2), L"in-range cells are untouched");
      }

      TEST_METHOD(PutDataOutOfRangeIsSilentlyIgnored)
      {
        Array2D<int> array(4, 3, -1);
        Fill(array);

        array.PutData(4, 0, 999);
        array.PutData(0, 3, 999);

        Assert::AreEqual(-1, array.GetData(4, 0), L"the write did not land in m_outsideValue");
        for (unsigned short y = 0; y < 3; ++y)
        {
          for (unsigned short x = 0; x < 4; ++x)
          {
            Assert::AreEqual(CellValue(x, y), array.GetData(x, y), L"and did not wrap into a real cell");
          }
        }
      }

      TEST_METHOD(AddToDataAccumulatesInRangeAndIgnoresOutOfRange)
      {
        Array2D<int> array(4, 3, -1);
        Fill(array);

        array.AddToData(1, 1, 5);
        Assert::AreEqual(CellValue(1, 1) + 5, array.GetData(1, 1));

        array.AddToData(4, 1, 5);
        Assert::AreEqual(-1, array.GetData(4, 1));
      }

      // The layout is one contiguous row-major block indexed y * columns + x.
      // The landscape height map and the obstruction grid depend on it, and the
      // chunked storage that replaces the whole-world allocation
      // (tasks/large-location.yaml T3) has to reproduce it per chunk.
      TEST_METHOD(StorageIsContiguousRowMajor)
      {
        Array2D<int> array(4, 3, -1);
        Fill(array);

        int const* const base = array.GetConstPointer(0, 0);
        for (unsigned short y = 0; y < 3; ++y)
        {
          for (unsigned short x = 0; x < 4; ++x)
          {
            Assert::IsTrue(array.GetConstPointer(x, y) == base + (y * 4 + x), L"row-major, no padding");
          }
        }
      }

      TEST_METHOD(SetAllOverwritesEveryCellAndNothingElse)
      {
        Array2D<int> array(4, 3, -1);
        Fill(array);

        array.SetAll(7);
        for (unsigned short y = 0; y < 3; ++y)
        {
          for (unsigned short x = 0; x < 4; ++x)
          {
            Assert::AreEqual(7, array.GetData(x, y));
          }
        }
        Assert::AreEqual(-1, array.GetData(4, 3), L"SetAll does not disturb m_outsideValue");
      }

      TEST_METHOD(InitialiseMatchesTheConstructor)
      {
        Array2D<int> array;
        Assert::AreEqual(0, static_cast<int>(array.GetNumColumns()));
        Assert::AreEqual(0, static_cast<int>(array.GetNumRows()));

        array.Initialise(4, 3, -1);
        Assert::AreEqual(4, static_cast<int>(array.GetNumColumns()));
        Assert::AreEqual(3, static_cast<int>(array.GetNumRows()));
        Assert::AreEqual(0, array.GetData(0, 0), L"value-initialised, as after the constructor");
        Assert::AreEqual(-1, array.GetData(4, 3));
      }

      // A zero-sized array answers m_outsideValue for everything. Landscape can
      // reach this state today: SurfaceMap2D's constructor narrows
      // ceilf(worldSize / cellSize) into unsigned short, and a cell count of
      // exactly 65,536 converts to zero. landscape-index-safety T2 makes that
      // construction fail loudly instead; until it does, this is what the tree
      // would do with such a map.
      TEST_METHOD(EmptyArrayAnswersOutsideValueEverywhere)
      {
        Array2D<int> const array(0, 0, -1);
        Assert::AreEqual(-1, array.GetData(0, 0));
        Assert::AreEqual(-1, array.GetData(100, 100));
      }
  };
} // namespace NeuronCoreTests
