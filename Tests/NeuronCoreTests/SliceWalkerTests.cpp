#include "pch.h"

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

    // The sequences SliceDArray produced, recorded while it still existed.
    //
    // These were not written by hand. The differential test that used to live
    // here constructed a real SliceDArray and asserted the walker matched it
    // over these same eight sizes; when containers-replaced T16 deleted the
    // template, the sequences it had been comparing against were captured
    // verbatim so the proof survives the thing it was proving against. Change
    // one of these numbers and you are changing which entity advances on which
    // frame, which is a simulation difference on a lockstep protocol.
    //
    // The sizes cover every shape the arithmetic has: an exact multiple of the
    // slice count, one that leaves a remainder, one smaller than the slice
    // count, and empty. Note size 25 asking for [27, 24] and size 0 asking for
    // [9, -1] — the walk outruns the container and the final slice inverts.
    // That is correct and callers guard it with ValidIndex.
    struct GoldenWalk
    {
        int m_size;
        std::pair<int, int> m_bounds[SLICES];
    };

    constexpr GoldenWalk GOLDEN[] = {
      {100, {{0, 10}, {11, 21}, {22, 32}, {33, 43}, {44, 54}, {55, 65}, {66, 76}, {77, 87}, {88, 98}, {99, 99}}},
      {25, {{0, 2}, {3, 5}, {6, 8}, {9, 11}, {12, 14}, {15, 17}, {18, 20}, {21, 23}, {24, 26}, {27, 24}}},
      {3, {{0, 0}, {1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}, {6, 6}, {7, 7}, {8, 8}, {9, 2}}},
      {0, {{0, 0}, {1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}, {6, 6}, {7, 7}, {8, 8}, {9, -1}}},
      {1, {{0, 0}, {1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}, {6, 6}, {7, 7}, {8, 8}, {9, 0}}},
      {10, {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {8, 9}, {10, 11}, {12, 13}, {14, 15}, {16, 17}, {18, 9}}},
      {11, {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {8, 9}, {10, 11}, {12, 13}, {14, 15}, {16, 17}, {18, 10}}},
      {999, {{0, 99}, {100, 199}, {200, 299}, {300, 399}, {400, 499}, {500, 599}, {600, 699}, {700, 799}, {800, 899}, {900, 998}}},
    };

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
  // containers-replaced T12 took it OUT of the container. The legacy sliced
  // array carried it as a base class; Neuron::SliceWalker is a free-standing
  // object the caller owns beside a plain SlotMap. Nothing pinned the legacy
  // arithmetic before this file existed.
  //
  // The first test is the one that matters: it walks the eight sizes and
  // asserts the bounds still match what the legacy container produced. It used
  // to construct a SliceDArray and compare live; T16 deleted the template, so
  // the sequences it compared against are recorded above instead. The rest pin
  // the individual properties that make the arithmetic surprising.
  TEST_CLASS(SliceWalkerTests)
  {
    public:
      TEST_METHOD(SliceBoundsStillMatchTheLegacyContainer)
      {
        for (auto const& golden : GOLDEN)
        {
          const auto walker = WalkWithWalker(golden.m_size, SLICES);
          const std::vector<std::pair<int, int>> expected(std::begin(golden.m_bounds), std::end(golden.m_bounds));
          Assert::IsTrue(
            expected == walker,
            (L"size " + std::to_wstring(golden.m_size) + L": expected " + Describe(expected) + L"but walked " + Describe(walker)).c_str());
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

        // Half the slots are holes, and the walk is identical to the one a
        // full container of the same capacity gets — GOLDEN[0] is size 100.
        const std::vector<std::pair<int, int>> forCapacity100(std::begin(GOLDEN[0].m_bounds), std::end(GOLDEN[0].m_bounds));
        Assert::IsTrue(WalkWithWalker(slots.Size(), SLICES) == forCapacity100, L"capacity drives the walk, whatever the occupancy");
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
