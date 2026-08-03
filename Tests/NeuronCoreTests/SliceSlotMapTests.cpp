#include "pch.h"

#include "SliceDArray.h"
#include "SlotMap.h"

#include <string>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronCoreTests
{
  namespace
  {
    constexpr int SLICES = 10; // NUM_SLICES_PER_FRAME, what the game uses

    std::vector<std::pair<int, int>> WalkLegacy(int _size, int _slices)
    {
      SliceDArray<int> legacy;
      legacy.SetTotalNumSlices(_slices);
      legacy.SetSize(_size);

      std::vector<std::pair<int, int>> bounds;
      for (int slice = 0; slice < _slices; ++slice)
      {
        int lower = 0;
        int upper = 0;
        legacy.GetNextSliceBounds(slice, &lower, &upper);
        bounds.emplace_back(lower, upper);
      }
      return bounds;
    }

    std::vector<std::pair<int, int>> WalkSlotMap(int _size, int _slices)
    {
      Neuron::SliceSlotMap<int> slots;
      slots.SetTotalNumSlices(_slices);
      slots.SetSize(_size);

      std::vector<std::pair<int, int>> bounds;
      for (int slice = 0; slice < _slices; ++slice)
      {
        int lower = 0;
        int upper = 0;
        slots.GetNextSliceBounds(slice, &lower, &upper);
        bounds.emplace_back(lower, upper);
      }
      return bounds;
    }

    std::wstring Describe(std::vector<std::pair<int, int>> const& _bounds)
    {
      std::wstring text;
      for (auto const& [lower, upper] : _bounds)
        text += L"[" + std::to_wstring(lower) + L"," + std::to_wstring(upper) + L"] ";
      return text;
    }
  } // namespace

  // The slice walk decides which entities advance on which frame, so it is
  // simulation behaviour rather than container plumbing. containers-replaced
  // T12 replaces SliceDArray with Neuron::SliceSlotMap across the world core,
  // and nothing pinned SliceDArray before this file existed.
  //
  // The first test is the one that matters: it runs both containers over the
  // same sizes and asserts the sequences are identical, so the conversion is
  // proven equivalent rather than argued to be. The rest pin the individual
  // properties that make the arithmetic surprising, because once SliceDArray
  // is deleted the differential test goes with it and only those remain.
  TEST_CLASS(SliceSlotMapTests)
  {
    public:
      TEST_METHOD(SliceBoundsMatchSliceDArrayExactly)
      {
        // Sizes chosen to hit every shape the arithmetic has: an exact
        // multiple of the slice count, a size that leaves a remainder, one
        // smaller than the slice count, and empty.
        for (int size : {100, 25, 3, 0, 1, 10, 11, 999})
        {
          const auto legacy = WalkLegacy(size, SLICES);
          const auto slots = WalkSlotMap(size, SLICES);
          Assert::IsTrue(legacy == slots,
                         (L"size " + std::to_wstring(size) + L": legacy " + Describe(legacy) + L"vs slotmap " + Describe(slots)).c_str());
        }
      }

      TEST_METHOD(SlicesArePartitionsWithNoOverlapAndNoGap)
      {
        // Each slice starts one past the previous slice's last index. A caller
        // looping lower..upper inclusive therefore visits each index once.
        const auto bounds = WalkSlotMap(100, SLICES);
        for (size_t i = 1; i < bounds.size(); ++i)
          Assert::AreEqual(bounds[i - 1].second + 1, bounds[i].first);
      }

      TEST_METHOD(EachSliceSpansOneMoreThanTheEvenShare)
      {
        // upper = lower + numPerSlice, and the bounds are inclusive, so a
        // slice covers numPerSlice + 1 indices. That is why the walk outruns
        // the container.
        const auto bounds = WalkSlotMap(100, SLICES);
        Assert::AreEqual(0, bounds[0].first);
        Assert::AreEqual(10, bounds[0].second); // int(100/10.0) = 10, inclusive => 11 indices
      }

      TEST_METHOD(TheFinalSliceCanBeInverted)
      {
        // Size 25 over ten slices: the walk reaches 26 by slice 8, past the
        // last index, and the final slice asks for [27, 24]. Callers guard
        // every index with ValidIndex, which is what makes this harmless — and
        // is why the bounds must not be "fixed" into an even division.
        const auto bounds = WalkSlotMap(25, SLICES);
        Assert::AreEqual(27, bounds[SLICES - 1].first);
        Assert::AreEqual(24, bounds[SLICES - 1].second);
        Assert::IsTrue(bounds[SLICES - 1].first > bounds[SLICES - 1].second);
      }

      TEST_METHOD(BoundsDivideCapacityNotTheLiveCount)
      {
        // Freed slots still count. Two maps with the same capacity and
        // different occupancy walk identically, so compacting a container
        // would move entities between frames.
        Neuron::SliceSlotMap<int> allFree;
        allFree.SetTotalNumSlices(SLICES);
        allFree.SetSize(100);

        Neuron::SliceSlotMap<int> halfUsed;
        halfUsed.SetTotalNumSlices(SLICES);
        halfUsed.SetSize(100);
        for (int i = 0; i < 100; i += 2)
          halfUsed.MarkUsed(i);

        Assert::AreEqual(allFree.Size(), halfUsed.Size());
        Assert::AreNotEqual(allFree.NumUsed(), halfUsed.NumUsed());

        for (int slice = 0; slice < SLICES; ++slice)
        {
          int freeLower = 0, freeUpper = 0, usedLower = 0, usedUpper = 0;
          allFree.GetNextSliceBounds(slice, &freeLower, &freeUpper);
          halfUsed.GetNextSliceBounds(slice, &usedLower, &usedUpper);
          Assert::AreEqual(freeLower, usedLower);
          Assert::AreEqual(freeUpper, usedUpper);
        }
      }

      TEST_METHOD(EmptyRestartsTheWalk)
      {
        Neuron::SliceSlotMap<int> slots;
        slots.SetTotalNumSlices(SLICES);
        slots.SetSize(100);

        int lower = 0, upper = 0;
        slots.GetNextSliceBounds(0, &lower, &upper);
        slots.GetNextSliceBounds(1, &lower, &upper);

        slots.Empty();
        Assert::AreEqual(0, slots.Size());

        // Slice 0 again rather than slice 2, which the ordering assert would
        // otherwise reject.
        slots.SetSize(100);
        slots.GetNextSliceBounds(0, &lower, &upper);
        Assert::AreEqual(0, lower);
        Assert::AreEqual(10, upper);
      }

      TEST_METHOD(SlotAssignmentKeepsTheFastDArrayFlavour)
      {
        // SliceDArray derived from FastDArray, so the slice variant defaults
        // to MostRecentFirst reuse. A WorldObjectId's m_index is one of these
        // numbers, so the flavour is network identity.
        Neuron::SliceSlotMap<int> slots;
        Assert::AreEqual(0, slots.PutData(10));
        Assert::AreEqual(1, slots.PutData(11));
        Assert::AreEqual(2, slots.PutData(12));

        slots.MarkNotUsed(1);
        Assert::AreEqual(1, slots.PutData(13), L"the most recently freed slot must come back first");
      }
  };
} // namespace NeuronCoreTests
