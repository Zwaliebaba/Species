#include "pch.h"

#include "DArray.h"
#include "FastDArray.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronCoreTests
{
  namespace
  {
    // Counts destructions, so the tests can show EmptyAndDelete owns the
    // pointees.
    struct Probe
    {
        static inline int sm_destroyed = 0;
        ~Probe() { ++sm_destroyed; }
    };
  } // namespace

  // DArray slot indices are network identity: WorldObjectId serialises them
  // verbatim, and GenerateSyncValue() accumulates in index order. These tests
  // pin the index-assignment behaviour bit-for-bit, because it is the
  // contract Neuron::SlotMap (tasks/containers-replaced.yaml T3) must
  // reproduce before any caller migrates.
  //
  // The one fact that matters most: after removals, DArray::PutData reuses
  // the LOWEST free slot (linear scan from zero) while FastDArray::PutData
  // reuses the MOST RECENTLY freed slot (freelist push/pop). The two tests
  // below run the same script and pin the two different answers.
  TEST_CLASS(DArrayTests)
  {
    public:
      TEST_METHOD(PutDataFillsTheLowestFreeSlot)
      {
        DArray<int> slots;
        Assert::AreEqual(0, slots.PutData(100));
        Assert::AreEqual(1, slots.PutData(101));
        Assert::AreEqual(2, slots.PutData(102));

        slots.MarkNotUsed(0);
        slots.MarkNotUsed(1);

        // The linear scan finds slot 0 first, then slot 1, and only grows
        // once every hole is filled.
        Assert::AreEqual(0, slots.PutData(103));
        Assert::AreEqual(1, slots.PutData(104));
        Assert::AreEqual(3, slots.PutData(105));
      }

      TEST_METHOD(FastDArrayReusesTheMostRecentlyFreedSlotFirst)
      {
        FastDArray<int> slots;
        Assert::AreEqual(0, slots.PutData(100));
        Assert::AreEqual(1, slots.PutData(101));
        Assert::AreEqual(2, slots.PutData(102));

        slots.MarkNotUsed(0);
        slots.MarkNotUsed(1);

        // Same script as PutDataFillsTheLowestFreeSlot, different answer:
        // MarkNotUsed pushes onto the freelist head, so reuse pops in
        // reverse-free order. A SlotMap that merged the two behaviours would
        // hand different network identities to the same spawn sequence.
        Assert::AreEqual(1, slots.PutData(103));
        Assert::AreEqual(0, slots.PutData(104));
        Assert::AreEqual(3, slots.PutData(105));
      }

      TEST_METHOD(SizeIsCapacityAndNumUsedIsTheLiveCount)
      {
        DArray<int> slots;
        slots.PutData(100);
        slots.PutData(101);
        slots.PutData(102);
        slots.MarkNotUsed(1);

        // Size() never shrinks on removal — it is the capacity the sync
        // traversal walks, holes included. ValidIndex is what distinguishes
        // a live slot from a hole.
        Assert::AreEqual(3, slots.Size());
        Assert::AreEqual(2, slots.NumUsed());
        Assert::IsTrue(slots.ValidIndex(0));
        Assert::IsFalse(slots.ValidIndex(1));
        Assert::IsTrue(slots.ValidIndex(2));
        Assert::IsFalse(slots.ValidIndex(3));
      }

      TEST_METHOD(DefaultGrowthAddsOneSlotAtATime)
      {
        DArray<int> slots;
        slots.PutData(100);
        Assert::AreEqual(1, slots.Size());
        slots.PutData(101);
        Assert::AreEqual(2, slots.Size());
        slots.PutData(102);
        Assert::AreEqual(3, slots.Size());
      }

      TEST_METHOD(StepDoubleGrowsByDoubling)
      {
        DArray<int> slots;
        slots.SetStepDouble();

        constexpr int expectedSizes[] = {1, 2, 4, 4, 8};
        for (int put = 0; put < 5; ++put)
        {
          slots.PutData(put);
          Assert::AreEqual(expectedSizes[put], slots.Size());
        }
      }

      TEST_METHOD(FindDataIgnoresFreedSlots)
      {
        DArray<int> slots;
        slots.PutData(7);
        slots.PutData(8);
        slots.PutData(7);

        Assert::AreEqual(0, slots.FindData(7));

        // Freeing the slot leaves the stale value in memory but takes it out
        // of every search and traversal.
        slots.MarkNotUsed(0);
        Assert::AreEqual(2, slots.FindData(7));
      }

      TEST_METHOD(FastDArrayGrowthChainsFreshSlotsInAscendingOrder)
      {
        // Growing by more than one slot chains the new free slots low to
        // high, so a burst of puts after a grow still assigns ascending
        // indices.
        FastDArray<int> slots;
        slots.SetStepSize(4);

        Assert::AreEqual(0, slots.PutData(100));
        Assert::AreEqual(1, slots.PutData(101));
        Assert::AreEqual(2, slots.PutData(102));
        Assert::AreEqual(3, slots.PutData(103));
        Assert::AreEqual(4, slots.PutData(104));
        Assert::AreEqual(8, slots.Size());
      }

      TEST_METHOD(FastDArrayGetNextFreeReservesTheSlot)
      {
        FastDArray<int> slots;
        slots.PutData(100);

        const int reserved = slots.GetNextFree();
        Assert::AreEqual(1, reserved);
        Assert::IsTrue(slots.ValidIndex(reserved));
        Assert::AreEqual(2, slots.NumUsed());
      }

      TEST_METHOD(EmptyAndDeleteDeletesThePointees)
      {
        Probe::sm_destroyed = 0;

        DArray<Probe*> slots;
        slots.PutData(new Probe());
        slots.PutData(new Probe());
        slots.EmptyAndDelete();

        Assert::AreEqual(2, Probe::sm_destroyed);
        Assert::AreEqual(0, slots.Size());
      }
  };
} // namespace NeuronCoreTests
