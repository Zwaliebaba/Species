#include "pch.h"

#include "SliceDArray.h"
#include "SliceWalker.h"
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

    std::vector<std::pair<int, int>> WalkWithWalker(int _size, int _slices)
    {
      Neuron::SliceWalker walker;
      walker.SetTotalNumSlices(_slices);

      std::vector<std::pair<int, int>> bounds;
      for (int slice = 0; slice < _slices; ++slice)
      {
        int lower = 0;
        int upper = 0;
        walker.GetNextSliceBounds(slice, _size, &lower, &upper);
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
  // simulation behaviour rather than container plumbing — which is exactly why
  // containers-replaced T12 takes it OUT of the container. SliceDArray carried
  // it as a base class; Neuron::SliceWalker is a free-standing object the
  // caller owns beside a plain SlotMap. Nothing pinned SliceDArray before this
  // file existed.
  //
  // The first test is the one that matters: it runs both containers over the
  // same sizes and asserts the sequences are identical, so the conversion is
  // proven equivalent rather than argued to be. The rest pin the individual
  // properties that make the arithmetic surprising, because once SliceDArray
  // is deleted the differential test goes with it and only those remain.
  TEST_CLASS(SliceWalkerTests)
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
          const auto walker = WalkWithWalker(size, SLICES);
          Assert::IsTrue(legacy == walker,
                         (L"size " + std::to_wstring(size) + L": SliceDArray " + Describe(legacy) + L"vs SliceWalker " + Describe(walker)).c_str());
        }
      }

      TEST_METHOD(SlicesArePartitionsWithNoOverlapAndNoGap)
      {
        // Each slice starts one past the previous slice's last index. A caller
        // looping lower..upper inclusive therefore visits each index once.
        const auto bounds = WalkWithWalker(100, SLICES);
        for (size_t i = 1; i < bounds.size(); ++i)
          Assert::AreEqual(bounds[i - 1].second + 1, bounds[i].first);
      }

      TEST_METHOD(EachSliceSpansOneMoreThanTheEvenShare)
      {
        // upper = lower + numPerSlice, and the bounds are inclusive, so a
        // slice covers numPerSlice + 1 indices. That is why the walk outruns
        // the container.
        const auto bounds = WalkWithWalker(100, SLICES);
        Assert::AreEqual(0, bounds[0].first);
        Assert::AreEqual(10, bounds[0].second); // int(100/10.0) = 10, inclusive => 11 indices
      }

      TEST_METHOD(TheFinalSliceCanBeInverted)
      {
        // Size 25 over ten slices: the walk reaches 26 by slice 8, past the
        // last index, and the final slice asks for [27, 24]. Callers guard
        // every index with ValidIndex, which is what makes this harmless — and
        // is why the bounds must not be "fixed" into an even division.
        const auto bounds = WalkWithWalker(25, SLICES);
        Assert::AreEqual(27, bounds[SLICES - 1].first);
        Assert::AreEqual(24, bounds[SLICES - 1].second);
        Assert::IsTrue(bounds[SLICES - 1].first > bounds[SLICES - 1].second);
      }

      TEST_METHOD(BoundsDivideCapacityNotTheLiveCount)
      {
        // The walker is handed a capacity, never a live count, and separating
        // it from the container is what makes that impossible to get wrong: a
        // caller passing NumUsed() instead of Size() would move entities
        // between frames, and now that is a visible argument rather than a
        // hidden base-class choice.
        Neuron::SlotMap<int> slots;
        slots.SetSize(100);
        for (int i = 0; i < 100; i += 2)
          slots.MarkUsed(i);

        Assert::AreEqual(100, slots.Size());
        Assert::AreEqual(50, slots.NumUsed());

        Assert::IsTrue(WalkWithWalker(slots.Size(), SLICES) == WalkLegacy(100, SLICES), L"capacity drives the walk, whatever the occupancy");
      }

      TEST_METHOD(ResetRestartsTheWalk)
      {
        Neuron::SliceWalker walker;
        walker.SetTotalNumSlices(SLICES);

        int lower = 0, upper = 0;
        walker.GetNextSliceBounds(0, 100, &lower, &upper);
        walker.GetNextSliceBounds(1, 100, &lower, &upper);

        walker.Reset();

        // Slice 0 again rather than slice 2, which the ordering assert would
        // otherwise reject.
        walker.GetNextSliceBounds(0, 100, &lower, &upper);
        Assert::AreEqual(0, lower);
        Assert::AreEqual(10, upper);
      }
  };
} // namespace NeuronCoreTests
