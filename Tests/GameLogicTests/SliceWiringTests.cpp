#include "pch.h"

#include "Globals.h"
#include "Location.h"
#include "SliceWalker.h"
#include "Team.h"
#include "WorldPointers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// GameLogic is in namespace Species since namespace-migration T4, and unlike
// namespace Neuron it has no tree-wide using-directive to reach it by. A test
// source is the right place for one: it is a .cpp, so nothing includes it.
using namespace Species;

namespace GameLogicTests
{
  namespace
  {
    // Walks one whole tick and reports which indices the ten slices between
    // them asked for. Bounds are inclusive and the final slice can come back
    // inverted, so the loop below has to tolerate upper < lower — that is the
    // shape the callers guard with ValidIndex, and reproducing it here is the
    // point of the test rather than an accident of it.
    std::vector<int> WalkOneTick(SliceWalker& _walker, int _size)
    {
      std::vector<int> visited;
      for (int slice = 0; slice < NUM_SLICES_PER_FRAME; ++slice)
      {
        int lower = 0;
        int upper = 0;
        _walker.GetNextSliceBounds(slice, _size, &lower, &upper);
        for (int i = lower; i <= upper; ++i)
          visited.push_back(i);
      }
      return visited;
    }
  } // namespace

  // containers-replaced T12 split the slice bookkeeping out of the container:
  // SliceDArray carried it as a base class, and it is a sibling SliceWalker
  // now. SliceWalkerTests pins the arithmetic in isolation; what those cannot
  // see is the WIRING, and the wiring is where this conversion can go wrong in
  // a way no compiler catches.
  //
  // Two mistakes are possible and both are silent. Passing NumUsed() instead of
  // Size() would divide the live count rather than the capacity, so entities
  // would migrate between slices as the world filled and emptied — a
  // simulation difference on a lockstep protocol. And pairing a walker with the
  // wrong container would go on compiling: they are all SliceWalker, and every
  // container answers Size().
  TEST_CLASS(SliceWiringTests)
  {
    public:
      TEST_METHOD(TheWorldWalksItsSpiritCapacityNotItsLiveCount)
      {
        Location location;

        // The constructor reserves 100 spirit slots and fills none of them.
        // A walk driven by the live count would visit nothing at all.
        Assert::AreEqual(100, location.m_spirits.Size());
        Assert::AreEqual(0, location.m_spirits.NumUsed());

        const std::vector<int> visited = WalkOneTick(location.m_spiritsWalker, location.m_spirits.Size());
        Assert::AreEqual(0, visited.front());
        Assert::AreEqual(99, visited.back());
      }

      TEST_METHOD(TenSlicesCoverEveryIndexExactlyOnce)
      {
        Location location;

        const int size = location.m_effects.Size();
        Assert::AreEqual(100, size); // the constructor reserves these too

        const std::vector<int> visited = WalkOneTick(location.m_effectsWalker, size);

        // Every slot advances exactly once per tick, in index order. At other
        // capacities the walk outruns the container and asks for indices past
        // the end — SliceWalkerTests pins that case; the bound check below is
        // what lets this one stay agnostic about which capacity it got.
        std::vector<bool> seen(size, false);
        int previous = -1;
        for (int index : visited)
        {
          Assert::IsTrue(index > previous, L"slice bounds must advance");
          previous = index;
          if (index < size)
          {
            Assert::IsFalse(seen[index], L"an index advanced twice in one tick");
            seen[index] = true;
          }
        }
        for (int i = 0; i < size; ++i)
          Assert::IsTrue(seen[i], L"an index was never advanced");
      }

      TEST_METHOD(EmptyingTheWorldRestartsEveryWalk)
      {
        Location location;

        // Part-way through a tick, so every walker holds a non-zero position.
        int lower = 0;
        int upper = 0;
        location.m_spiritsWalker.GetNextSliceBounds(0, location.m_spirits.Size(), &lower, &upper);
        location.m_buildingsWalker.GetNextSliceBounds(0, location.m_buildings.Size(), &lower, &upper);
        location.m_lasersWalker.GetNextSliceBounds(0, location.m_lasers.Size(), &lower, &upper);
        location.m_effectsWalker.GetNextSliceBounds(0, location.m_effects.Size(), &lower, &upper);
        Assert::IsTrue(location.m_spiritsWalker.GetLastUpdated() > 0);

        // The legacy container reset its own bookkeeping inside Empty. Now that
        // the two are separate objects, Location::Empty has to reset the
        // walkers itself — miss one and the next world resumes mid-tick against
        // a container that no longer has those slots.
        location.Empty();

        Assert::AreEqual(0, location.m_spiritsWalker.GetLastUpdated());
        Assert::AreEqual(0, location.m_buildingsWalker.GetLastUpdated());
        Assert::AreEqual(0, location.m_lasersWalker.GetLastUpdated());
        Assert::AreEqual(0, location.m_effectsWalker.GetLastUpdated());

        // Reset also puts the walk back at slice 0. Asking for slice 0 again is
        // only legal because of that — GetNextSliceBounds asserts the sequence.
        location.m_spiritsWalker.GetNextSliceBounds(0, location.m_spirits.Size(), &lower, &upper);
        Assert::AreEqual(0, lower);
      }

      TEST_METHOD(ATeamWalksItsOwnEntityCapacity)
      {
        // Not Initialise() — that loads textures through g_resource. The
        // constructor is what installs the walker, and that is what is under
        // test here.
        Team team;

        // Nothing has spawned, so both are zero and the walk is degenerate
        // rather than wrong: the final slice clamps to size - 1 and inverts.
        Assert::AreEqual(0, team.m_others.Size());

        int lower = 0;
        int upper = 0;
        team.m_othersWalker.GetNextSliceBounds(0, team.m_others.Size(), &lower, &upper);
        Assert::AreEqual(0, lower);
        Assert::AreEqual(0, upper);
      }
  };
} // namespace GameLogicTests
