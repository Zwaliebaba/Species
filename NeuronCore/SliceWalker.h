#pragma once

#include "Debug.h"

namespace Neuron
{
  // Hands out the index range to advance this slice.
  //
  // WHAT SLICING IS FOR, because the name suggests batching and it is not that.
  // Ten slices are exactly one server tick: Main.cpp's frame loop leaves
  // g_sliceNum at -1 while it waits for the next letter from the server, then
  // sends the sync value, sets g_sliceNum to 0 and advances ten slices. The
  // budget for how many to run this frame is TIME, not work — it exists so a
  // 60fps renderer can interleave with a 10Hz lockstep simulation instead of
  // stalling for a whole tick every 100ms. Single-threaded or not, that is
  // still what it buys.
  //
  // Slices are never skipped. GetNumSlicesToAdvance clamps to [0, 10] and the
  // counter only wraps after the last one, so a slow frame defers slices, it
  // does not drop them. Every entry therefore advances exactly once per tick,
  // in index order.
  //
  // THIS USED TO LIVE INSIDE THE CONTAINER. SliceDArray derived from
  // FastDArray and carried these three ints, which made every container that
  // wanted stable indices also carry a scheduling policy it had nothing to do
  // with. They are separate now: the container is a plain SlotMap, and whoever
  // advances it owns one of these. See tasks/containers-replaced.yaml T12.
  //
  // The arithmetic is reproduced exactly rather than tidied, because it decides
  // which entry advances on which frame. Three properties are surprising and
  // all three are pinned in SliceWalkerTests:
  //
  //   Bounds are INCLUSIVE and each non-final slice spans numPerSlice + 1
  //   entries — upper = lower + numPerSlice. Ten such slices cover more than
  //   the container holds, so the walk outruns it and the final slice, which
  //   clamps to size - 1, is often empty or inverted: at size 25 it asks for
  //   [27, 24]. Callers guard every index with ValidIndex, which is what makes
  //   that harmless and why it must not be "corrected" to an even division.
  //
  //   numPerSlice divides the CAPACITY passed in, holes included, not the live
  //   count. Compacting a container would move entries between frames.
  //
  //   Slices partition with no overlap and no gap: lower is the previous
  //   upper + 1.
  class SliceWalker
  {
    public:
      void SetTotalNumSlices(int _slices) { m_totalNumSlices = _slices; }

      [[nodiscard]] int GetLastUpdated() const { return m_lastUpdated; }

      // Restarts the walk. The container being emptied under a walker is what
      // SliceDArray::Empty did, and the walk has to begin at slice 0 again.
      void Reset()
      {
        m_lastSlice = -1;
        m_lastUpdated = 0;
      }

      // `_size` is the container's capacity — SlotMap::Size(), holes included.
      void GetNextSliceBounds(int _slice, int _size, int* _lower, int* _upper)
      {
        // Slices must be asked for in order, wrapping at the last. The
        // arithmetic carries state between calls, so an out-of-order request
        // quietly returns the range for a different frame.
        DEBUG_ASSERT(m_lastSlice == -1 || _slice == m_lastSlice + 1 || (_slice == 0 && m_lastSlice == m_totalNumSlices - 1));

        *_lower = _slice == 0 ? 0 : m_lastUpdated + 1;

        if (_slice == m_totalNumSlices - 1)
        {
          *_upper = _size - 1;
        }
        else
        {
          const int numPerSlice = int(_size / (float)m_totalNumSlices);
          *_upper = *_lower + numPerSlice;
        }

        m_lastUpdated = *_upper;
        m_lastSlice = _slice;
      }

    private:
      int m_totalNumSlices = 0;
      int m_lastSlice = -1;
      int m_lastUpdated = 0;
  };
} // namespace Neuron
