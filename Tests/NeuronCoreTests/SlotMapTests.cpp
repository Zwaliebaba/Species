#include "pch.h"

#include <memory>

#include "SlotMap.h"

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

  // SlotMap replaces DArray and FastDArray, whose slot indices are network
  // identity. Every scripted sequence here is copied verbatim from
  // DArrayTests.cpp with the same expected answers — that equivalence is the
  // whole point: a caller migrating to the matching flavour gets identical
  // identities for an identical spawn sequence. When the legacy templates
  // are deleted (containers-replaced T16), DArrayTests retires and this
  // file carries the contract alone.
  TEST_CLASS(SlotMapTests)
  {
    public:
      TEST_METHOD(LowestFirstFillsTheLowestFreeSlot)
      {
        Neuron::SlotMap<int> slots;
        Assert::AreEqual(0, slots.PutData(100));
        Assert::AreEqual(1, slots.PutData(101));
        Assert::AreEqual(2, slots.PutData(102));

        slots.MarkNotUsed(0);
        slots.MarkNotUsed(1);

        Assert::AreEqual(0, slots.PutData(103));
        Assert::AreEqual(1, slots.PutData(104));
        Assert::AreEqual(3, slots.PutData(105));
      }

      TEST_METHOD(MostRecentFirstReusesTheMostRecentlyFreedSlot)
      {
        Neuron::FastSlotMap<int> slots;
        Assert::AreEqual(0, slots.PutData(100));
        Assert::AreEqual(1, slots.PutData(101));
        Assert::AreEqual(2, slots.PutData(102));

        slots.MarkNotUsed(0);
        slots.MarkNotUsed(1);

        Assert::AreEqual(1, slots.PutData(103));
        Assert::AreEqual(0, slots.PutData(104));
        Assert::AreEqual(3, slots.PutData(105));
      }

      TEST_METHOD(SizeIsCapacityAndNumUsedIsTheLiveCount)
      {
        Neuron::SlotMap<int> slots;
        slots.PutData(100);
        slots.PutData(101);
        slots.PutData(102);
        slots.MarkNotUsed(1);

        Assert::AreEqual(3, slots.Size());
        Assert::AreEqual(2, slots.NumUsed());
        Assert::IsTrue(slots.ValidIndex(0));
        Assert::IsFalse(slots.ValidIndex(1));
        Assert::IsTrue(slots.ValidIndex(2));
        Assert::IsFalse(slots.ValidIndex(3));
      }

      TEST_METHOD(DefaultGrowthAddsOneSlotAtATime)
      {
        Neuron::SlotMap<int> slots;
        slots.PutData(100);
        Assert::AreEqual(1, slots.Size());
        slots.PutData(101);
        Assert::AreEqual(2, slots.Size());
        slots.PutData(102);
        Assert::AreEqual(3, slots.Size());
      }

      TEST_METHOD(StepDoubleGrowsByDoubling)
      {
        Neuron::SlotMap<int> slots;
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
        Neuron::SlotMap<int> slots;
        slots.PutData(7);
        slots.PutData(8);
        slots.PutData(7);

        Assert::AreEqual(0, slots.FindData(7));

        slots.MarkNotUsed(0);
        Assert::AreEqual(2, slots.FindData(7));
      }

      TEST_METHOD(MostRecentFirstGrowthChainsFreshSlotsInAscendingOrder)
      {
        Neuron::FastSlotMap<int> slots;
        slots.SetStepSize(4);

        Assert::AreEqual(0, slots.PutData(100));
        Assert::AreEqual(1, slots.PutData(101));
        Assert::AreEqual(2, slots.PutData(102));
        Assert::AreEqual(3, slots.PutData(103));
        Assert::AreEqual(4, slots.PutData(104));
        Assert::AreEqual(8, slots.Size());
      }

      TEST_METHOD(GetNextFreeReservesTheSlot)
      {
        Neuron::FastSlotMap<int> slots;
        slots.PutData(100);

        const int reserved = slots.GetNextFree();
        Assert::AreEqual(1, reserved);
        Assert::IsTrue(slots.ValidIndex(reserved));
        Assert::AreEqual(2, slots.NumUsed());
      }

      TEST_METHOD(MarkUsedRebuildsTheFreeChainAscending)
      {
        // FastDArray rebuilt its freelist low-to-high on MarkUsed; reuse
        // after a rebuild is therefore lowest-first until the next
        // MarkNotUsed pushes. Pinned because indexed placement happens on
        // level load, and the slots handed out afterwards are identity.
        Neuron::FastSlotMap<int> slots;
        slots.SetSize(4);
        slots.MarkUsed(2);

        Assert::AreEqual(0, slots.PutData(100));
        Assert::AreEqual(1, slots.PutData(101));
        Assert::AreEqual(3, slots.PutData(102));
      }

      TEST_METHOD(IterationVisitsOccupiedSlotsInIndexOrder)
      {
        Neuron::SlotMap<int> slots;
        slots.PutData(100);
        slots.PutData(101);
        slots.PutData(102);
        slots.PutData(103);
        slots.MarkNotUsed(1);
        slots.MarkNotUsed(3);

        int expectedIndices[] = {0, 2};
        int expectedValues[] = {100, 102};
        int seen = 0;
        for (auto it = slots.begin(); it != slots.end(); ++it)
        {
          Assert::AreEqual(expectedIndices[seen], it.Index());
          Assert::AreEqual(expectedValues[seen], *it);
          ++seen;
        }
        Assert::AreEqual(2, seen);
      }

      // Stage 5 needs a SlotMap that can own. The copying PutData overloads
      // cannot bind a move-only type, so without these a caller converting an
      // owning SlotMap<T*> to unique_ptr has nowhere to go.
      TEST_METHOD(AMoveOnlyValueCanBeStoredAndRetrieved)
      {
        Neuron::SlotMap<std::unique_ptr<int>> map;

        const int first = map.PutData(std::make_unique<int>(11));
        const int second = map.PutData(std::make_unique<int>(22));

        Assert::AreEqual(0, first);
        Assert::AreEqual(1, second);
        Assert::AreEqual(11, *map[0]);
        Assert::AreEqual(22, *map[1]);
        Assert::AreEqual(2, map.NumUsed());
      }

      // The slot chosen must not depend on how the value arrives. TakeFreeSlot
      // decides that, and this pins that the move overload did not quietly get
      // its own path — the sequence here is the same one
      // LowestFirstFillsTheLowestFreeSlot asserts for copied values.
      TEST_METHOD(MovingAValueInChoosesTheSameSlotAsCopyingWould)
      {
        Neuron::SlotMap<std::unique_ptr<int>> map;

        map.PutData(std::make_unique<int>(1));
        const int reused = map.PutData(std::make_unique<int>(2));
        map.PutData(std::make_unique<int>(3));
        map.MarkNotUsed(reused);

        Assert::AreEqual(reused, map.PutData(std::make_unique<int>(4)));
        Assert::AreEqual(4, *map[reused]);
      }

      // Owning slots free their contents when the map does, with no
      // EmptyAndDelete in sight — that method does not compile for a
      // unique_ptr element and is not needed.
      TEST_METHOD(AMoveOnlyValueIsFreedWhenItsSlotIsReused)
      {
        Neuron::SlotMap<std::unique_ptr<Probe>> map;
        Probe::sm_destroyed = 0;

        const int slot = map.PutData(std::make_unique<Probe>());
        Assert::AreEqual(0, Probe::sm_destroyed);

        map.PutData(std::make_unique<Probe>(), slot);

        Assert::AreEqual(1, Probe::sm_destroyed, L"overwriting a slot must free what it held");
      }

      TEST_METHOD(EmptyAndDeleteDeletesThePointees)
      {
        Probe::sm_destroyed = 0;

        Neuron::SlotMap<Probe*> slots;
        slots.PutData(new Probe());
        slots.PutData(new Probe());
        slots.EmptyAndDelete();

        Assert::AreEqual(2, Probe::sm_destroyed);
        Assert::AreEqual(0, slots.Size());
      }
  };
} // namespace NeuronCoreTests
