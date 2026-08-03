#pragma once

#include <type_traits>
#include <vector>

#include "Debug.h"

namespace Neuron
{
  // The replacement for DArray and FastDArray: storage with stable indices,
  // an occupancy mask, and — critically — the exact index-assignment
  // behaviour of the legacy templates, because those indices are network
  // identity (WorldObjectId serialises them verbatim) and the sync value is
  // accumulated in index order. See CODING_STANDARDS.md, Determinism, and
  // the pinned sequences in Tests/NeuronCoreTests/DArrayTests.cpp and
  // SlotMapTests.cpp.
  //
  // The two legacy templates assign DIFFERENT indices after removals, so the
  // flavour is part of the type: LowestFirst scans from zero the way DArray
  // did; MostRecentFirst pops a freelist the way FastDArray did. A caller
  // migrating off one legacy container must take the matching flavour, or
  // the same spawn sequence hands out different identities.
  //
  // Deliberately preserved oddities, because callers and the wire depend on
  // them: Size() is the capacity the sync traversal walks (holes included),
  // not the live count — NumUsed() is the live count; growth adds one slot
  // at a time unless SetStepSize/SetStepDouble say otherwise; growing the
  // freelist flavour chains the fresh slots in ascending order.
  enum class SlotReuse
  {
    LowestFirst,
    MostRecentFirst
  };

  template <typename T, SlotReuse Reuse = SlotReuse::LowestFirst> class SlotMap
  {
    public:
      SlotMap() = default;
      explicit SlotMap(int _stepSize)
        : m_stepSize(_stepSize)
      {
      }

      void SetStepSize(int _stepSize) { m_stepSize = _stepSize; }
      void SetStepDouble() { m_stepSize = -1; }

      // Returns the index the value was stored at — the slot's identity for
      // as long as it stays occupied.
      int PutData(const T& _value)
      {
        const int freeSlot = TakeFreeSlot();
        m_slots[freeSlot] = _value;
        return freeSlot;
      }

      void PutData(const T& _value, int _index)
      {
        DEBUG_ASSERT(_index >= 0 && _index < Size());
        m_slots[_index] = _value;
        Occupy(_index);
      }

      // The move overloads exist so a SlotMap can hold std::unique_ptr, which
      // is what migration stage 5 needs of it: without them a caller
      // converting an owning SlotMap<T*> has nowhere to go, because the
      // copying overloads above cannot bind a move-only type. Which slot is
      // chosen is unchanged — TakeFreeSlot decides that, and it does not know
      // or care how the value arrives.
      int PutData(T&& _value)
      {
        const int freeSlot = TakeFreeSlot();
        m_slots[freeSlot] = std::move(_value);
        return freeSlot;
      }

      void PutData(T&& _value, int _index)
      {
        DEBUG_ASSERT(_index >= 0 && _index < Size());
        m_slots[_index] = std::move(_value);
        Occupy(_index);
      }

      // Reserves and returns the next slot the flavour would assign, leaving
      // its value untouched. FastDArray called this GetNextFree.
      int GetNextFree() { return TakeFreeSlot(); }

      void MarkUsed(int _index)
      {
        DEBUG_ASSERT(_index >= 0 && _index < Size());
        DEBUG_ASSERT(!m_occupied[_index]);
        Occupy(_index);
      }

      void MarkNotUsed(int _index)
      {
        DEBUG_ASSERT(ValidIndex(_index));
        m_occupied[_index] = 0;
        --m_numUsed;
        if constexpr (Reuse == SlotReuse::MostRecentFirst)
        {
          m_freeList[_index] = m_firstFree;
          m_firstFree = _index;
        }
      }

      [[nodiscard]] T GetData(int _index) const
      {
        DEBUG_ASSERT(ValidIndex(_index));
        return m_slots[_index];
      }

      [[nodiscard]] T* GetPointer(int _index)
      {
        DEBUG_ASSERT(ValidIndex(_index));
        return &m_slots[_index];
      }

      [[nodiscard]] const T* GetPointer(int _index) const
      {
        DEBUG_ASSERT(ValidIndex(_index));
        return &m_slots[_index];
      }

      T& operator[](int _index)
      {
        DEBUG_ASSERT(ValidIndex(_index));
        return m_slots[_index];
      }

      const T& operator[](int _index) const
      {
        DEBUG_ASSERT(ValidIndex(_index));
        return m_slots[_index];
      }

      [[nodiscard]] bool ValidIndex(int _index) const { return _index >= 0 && _index < Size() && m_occupied[_index] != 0; }

      // Capacity, holes included — what the sync traversal walks. The live
      // count is NumUsed().
      [[nodiscard]] int Size() const { return static_cast<int>(m_slots.size()); }

      [[nodiscard]] int NumUsed() const { return m_numUsed; }

      [[nodiscard]] int FindData(const T& _value) const
      {
        for (int index = 0; index < Size(); ++index)
          if (m_occupied[index] && m_slots[index] == _value)
            return index;
        return -1;
      }

      void SetSize(int _newSize)
      {
        const int oldSize = Size();
        if (_newSize == oldSize)
          return;

        m_slots.resize(_newSize);
        m_occupied.resize(_newSize, 0);
        if constexpr (Reuse == SlotReuse::MostRecentFirst)
        {
          m_freeList.resize(_newSize);
          if (_newSize > oldSize)
          {
            // Fresh slots chain in ascending order, ending on whatever was
            // free before the grow — FastDArray::SetSize behaviour, pinned
            // by FastDArrayGrowthChainsFreshSlotsInAscendingOrder.
            for (int index = oldSize; index < _newSize - 1; ++index)
              m_freeList[index] = index + 1;
            m_freeList[_newSize - 1] = m_firstFree;
            m_firstFree = oldSize;
          }
          else
            RebuildFreeList();
        }
        if (_newSize < oldSize)
          RecountUsed();
      }

      void Empty()
      {
        m_slots.clear();
        m_occupied.clear();
        m_numUsed = 0;
        if constexpr (Reuse == SlotReuse::MostRecentFirst)
        {
          m_freeList.clear();
          m_firstFree = -1;
        }
      }

      // Transitional, for callers migrating off the legacy containers while
      // they still hold raw owning pointers; ownership conversion is
      // migration stage 5. A SlotMap of unique_ptr does not need this and
      // will not compile it — the static_assert below is the diagnostic.
      void EmptyAndDelete()
      {
        static_assert(std::is_pointer_v<T>, "EmptyAndDelete only makes sense for pointer elements");
        for (int index = 0; index < Size(); ++index)
          if (m_occupied[index])
            delete m_slots[index];
        Empty();
      }

      // Iteration visits occupied slots only, in index order — the only
      // traversal order simulation code may observe.
      class ConstIterator
      {
        public:
          ConstIterator(const SlotMap* _map, int _index)
            : m_map(_map),
              m_index(_index)
          {
            SkipHoles();
          }

          const T& operator*() const { return m_map->m_slots[m_index]; }
          [[nodiscard]] int Index() const { return m_index; }

          ConstIterator& operator++()
          {
            ++m_index;
            SkipHoles();
            return *this;
          }

          bool operator==(const ConstIterator& _other) const { return m_index == _other.m_index; }
          bool operator!=(const ConstIterator& _other) const { return m_index != _other.m_index; }

        private:
          void SkipHoles()
          {
            while (m_index < m_map->Size() && !m_map->m_occupied[m_index])
              ++m_index;
          }

          const SlotMap* m_map;
          int m_index;
      };

      [[nodiscard]] ConstIterator begin() const { return ConstIterator(this, 0); }
      [[nodiscard]] ConstIterator end() const { return ConstIterator(this, Size()); }

    private:
      void Grow()
      {
        if (m_stepSize == -1)
          SetSize(Size() == 0 ? 1 : Size() * 2);
        else
          SetSize(Size() + m_stepSize);
      }

      int TakeFreeSlot()
      {
        int freeSlot = -1;
        if constexpr (Reuse == SlotReuse::LowestFirst)
        {
          for (int index = 0; index < Size(); ++index)
            if (!m_occupied[index])
            {
              freeSlot = index;
              break;
            }
          if (freeSlot == -1)
          {
            freeSlot = Size();
            Grow();
          }
        }
        else
        {
          if (m_firstFree == -1)
            Grow();
          freeSlot = m_firstFree;
          m_firstFree = m_freeList[freeSlot];
          m_freeList[freeSlot] = -2;
        }
        Occupy(freeSlot);
        return freeSlot;
      }

      void Occupy(int _index)
      {
        if (!m_occupied[_index])
          ++m_numUsed;
        m_occupied[_index] = 1;
        if constexpr (Reuse == SlotReuse::MostRecentFirst)
        {
          if (m_freeList[_index] != -2)
            RebuildFreeList();
        }
      }

      // Free slots re-chain in ascending order — FastDArray::RebuildFreeList
      // behaviour, reached through MarkUsed, indexed PutData and shrinking.
      void RebuildFreeList()
      {
        m_firstFree = -1;
        int lastFree = -1;
        for (int index = 0; index < Size(); ++index)
        {
          if (!m_occupied[index])
          {
            if (m_firstFree == -1)
              m_firstFree = index;
            else
              m_freeList[lastFree] = index;
            m_freeList[index] = -1;
            lastFree = index;
          }
          else
            m_freeList[index] = -2;
        }
      }

      void RecountUsed()
      {
        m_numUsed = 0;
        for (int index = 0; index < Size(); ++index)
          if (m_occupied[index])
            ++m_numUsed;
      }

      std::vector<T> m_slots;
      std::vector<char> m_occupied;
      std::vector<int> m_freeList;
      int m_firstFree = -1;
      int m_numUsed = 0;
      int m_stepSize = 1;
  };

  template <typename T> using FastSlotMap = SlotMap<T, SlotReuse::MostRecentFirst>;

  // A SlotMap that also carries SliceDArray's slice bookkeeping.
  //
  // The simulation advances heavy work a tenth at a time: Location::Advance
  // asks each of these for the [lower, upper] range belonging to the current
  // slice and advances only those entries. Which entity moves on which frame
  // therefore comes out of the arithmetic below, so it is reproduced EXACTLY
  // rather than tidied — the off-by-ones are the specification.
  //
  // Three of them are load-bearing and none is obvious:
  //
  //   The bounds are INCLUSIVE and each non-final slice spans numPerSlice + 1
  //   indices, not numPerSlice: upper = lower + numPerSlice, and the next
  //   lower is upper + 1. So the walk outruns the container — ten slices of
  //   int(Size()/10) + 1 covers more than Size() — and the final slice, which
  //   clamps to Size() - 1, is frequently empty or inverted. With Size 25 it
  //   is [27, 24]. Callers loop `for (i = lower; i <= upper; ++i)` and guard
  //   each index with ValidIndex, which is what makes that harmless; it is
  //   also why the bounds cannot be "tidied" into an even division.
  //
  //   numPerSlice divides Size(), the CAPACITY — holes included — not
  //   NumUsed(). A container with many freed slots therefore spreads its live
  //   entries unevenly across the ten slices, and compacting it would change
  //   which frame each one advances on.
  //
  //   The last slice always ends at Size() - 1 regardless, which is what stops
  //   integer truncation in numPerSlice from leaving a tail unadvanced.
  //
  // Defaults to MostRecentFirst because SliceDArray derived from FastDArray.
  // See tasks/containers-replaced.yaml T12 and SliceSlotMapTests.
  template <typename T, SlotReuse Reuse = SlotReuse::MostRecentFirst> class SliceSlotMap : public SlotMap<T, Reuse>
  {
    public:
      void SetTotalNumSlices(int _slices) { m_totalNumSlices = _slices; }

      [[nodiscard]] int GetLastUpdated() const { return m_lastUpdated; }

      void Empty()
      {
        m_lastSlice = -1;
        m_lastUpdated = 0;
        SlotMap<T, Reuse>::Empty();
      }

      void GetNextSliceBounds(int _slice, int* _lower, int* _upper)
      {
        // Slices must be asked for in order, wrapping at the last one. The
        // arithmetic carries state between calls, so an out-of-order request
        // silently returns a range for the wrong frame.
        DEBUG_ASSERT(m_lastSlice == -1 || _slice == m_lastSlice + 1 || (_slice == 0 && m_lastSlice == m_totalNumSlices - 1));

        *_lower = _slice == 0 ? 0 : m_lastUpdated + 1;

        if (_slice == m_totalNumSlices - 1)
        {
          *_upper = this->Size() - 1;
        }
        else
        {
          const int numPerSlice = int(this->Size() / (float)m_totalNumSlices);
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
