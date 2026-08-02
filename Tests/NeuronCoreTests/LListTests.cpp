#include "pch.h"

#include "LList.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronCoreTests
{
  namespace
  {
    // Counts destructions, so the tests can show which Empty variant owns the
    // pointees.
    struct Probe
    {
        static inline int sm_destroyed = 0;
        ~Probe() { ++sm_destroyed; }
    };
  } // namespace

  // LList is stage 3's largest conversion target — 127 files when
  // tasks/containers-replaced.yaml was written. Every conversion to a std
  // container claims behaviour preservation; these tests are that claim made
  // checkable. They pin the traversal and index behaviour call sites depend
  // on, including the parts a std::vector does differently by default:
  // out-of-range reads return T(0) rather than asserting, and removal shifts
  // every later index down. See CODING_STANDARDS.md, Determinism.
  TEST_CLASS(LListTests)
  {
    public:
      TEST_METHOD(PutDataAppendsInInsertionOrder)
      {
        LList<int> list;
        list.PutData(10);
        list.PutData(20);
        list.PutData(30);

        Assert::AreEqual(3, list.Size());
        Assert::AreEqual(10, list.GetData(0));
        Assert::AreEqual(20, list.GetData(1));
        Assert::AreEqual(30, list.GetData(2));
      }

      TEST_METHOD(PutDataAtStartPrepends)
      {
        LList<int> list;
        list.PutData(10);
        list.PutData(20);
        list.PutDataAtStart(5);

        Assert::AreEqual(5, list.GetData(0));
        Assert::AreEqual(10, list.GetData(1));
        Assert::AreEqual(20, list.GetData(2));
      }

      TEST_METHOD(PutDataAtIndexInsertsAtThatPosition)
      {
        LList<int> list;
        list.PutData(10);
        list.PutData(20);
        list.PutData(30);

        list.PutDataAtIndex(15, 1);
        Assert::AreEqual(10, list.GetData(0));
        Assert::AreEqual(15, list.GetData(1));
        Assert::AreEqual(20, list.GetData(2));
        Assert::AreEqual(30, list.GetData(3));

        // Index == Size() appends rather than being rejected.
        list.PutDataAtIndex(40, 4);
        Assert::AreEqual(40, list.GetData(4));
      }

      TEST_METHOD(RemoveDataShiftsLaterIndicesDown)
      {
        // Unlike DArray, LList indices are not stable: removal closes the
        // gap. Conversions may use an order-preserving std::vector precisely
        // because call sites already cannot rely on an index surviving a
        // removal.
        LList<int> list;
        list.PutData(10);
        list.PutData(20);
        list.PutData(30);

        list.RemoveData(0);
        Assert::AreEqual(2, list.Size());
        Assert::AreEqual(20, list.GetData(0));
        Assert::AreEqual(30, list.GetData(1));
      }

      TEST_METHOD(FindDataReturnsTheFirstMatchingIndex)
      {
        LList<int> list;
        list.PutData(10);
        list.PutData(20);
        list.PutData(10);

        Assert::AreEqual(0, list.FindData(10));
        Assert::AreEqual(1, list.FindData(20));
        Assert::AreEqual(-1, list.FindData(99));
      }

      TEST_METHOD(OutOfRangeReadsReturnZero)
      {
        // The legacy contract call sites lean on: an invalid index is not an
        // error, it is T(0). A conversion that switches to at()-style
        // checking changes behaviour.
        LList<int> list;
        Assert::AreEqual(0, list.GetData(0));
        Assert::IsFalse(list.ValidIndex(0));
        Assert::IsFalse(list.ValidIndex(-1));

        list.PutData(10);
        Assert::AreEqual(0, list.GetData(1));
        Assert::AreEqual(0, list.GetData(-1));
        Assert::IsTrue(list.ValidIndex(0));
      }

      TEST_METHOD(ReadsStayCorrectAfterMutationDespiteTheCursor)
      {
        // LList keeps a cached cursor to make sequential GetData calls fast.
        // Every mutation resets it; this pins that reads after a removal see
        // the post-removal list, not the cache.
        LList<int> list;
        for (int value = 0; value < 6; ++value)
          list.PutData(value * 10);

        Assert::AreEqual(20, list.GetData(2));
        list.RemoveData(1);
        Assert::AreEqual(20, list.GetData(1));
        Assert::AreEqual(30, list.GetData(2));
        Assert::AreEqual(40, list.GetData(3));
      }

      TEST_METHOD(EmptyDoesNotDeletePointees)
      {
        Probe::sm_destroyed = 0;
        Probe* first = new Probe();
        Probe* second = new Probe();

        LList<Probe*> list;
        list.PutData(first);
        list.PutData(second);
        list.Empty();

        Assert::AreEqual(0, Probe::sm_destroyed);
        Assert::AreEqual(0, list.Size());

        delete first;
        delete second;
      }

      TEST_METHOD(EmptyAndDeleteDeletesThePointees)
      {
        Probe::sm_destroyed = 0;

        LList<Probe*> list;
        list.PutData(new Probe());
        list.PutData(new Probe());
        list.EmptyAndDelete();

        Assert::AreEqual(2, Probe::sm_destroyed);
        Assert::AreEqual(0, list.Size());
      }
  };
} // namespace NeuronCoreTests
