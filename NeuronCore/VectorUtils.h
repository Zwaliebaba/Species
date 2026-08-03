#pragma once

#include <vector>

namespace Neuron
{
  // The bounds test the legacy list had and std::vector does not.
  //
  // This exists because of a crash. LList answered an out-of-range GetData
  // with nullptr and an out-of-range ValidIndex with false, so a great deal of
  // inherited code uses a subscript where it means "is there an element here",
  // and some of it uses one as a loop condition: EclWindow's destructor ran
  // `while (m_buttons[0])`, which was a legal way to ask "is the list empty"
  // right up until m_buttons became a vector, at which point it read off the
  // front of an empty one.
  //
  // Converting those by hand, one judgement at a time, is how the next one
  // gets missed. Spelling the test out here keeps every converted site
  // provably identical to what the list did, and — unlike a member function on
  // a container — it reads as the range check it is rather than as something
  // the container promises about its contents.
  //
  // Note this is a RANGE test only. The legacy lists were dense, so range and
  // occupancy were the same question for them. SlotMap is not dense and answers
  // its own ValidIndex, which additionally tests the occupancy bit; do not
  // replace one with the other.
  template <typename T> [[nodiscard]] inline bool ValidIndex(std::vector<T> const& _vector, int _index)
  {
    return _index >= 0 && _index < static_cast<int>(_vector.size());
  }

  // Transitional, exactly as SlotMap::EmptyAndDelete is: for callers that have
  // moved off the legacy lists but still hold raw owning pointers. Ownership
  // conversion is migration stage 5 (tasks/ownership.yaml), and every use of
  // this should disappear there when the element type becomes unique_ptr.
  //
  // Note the form. The legacy EmptyAndDelete was plain `delete` and the legacy
  // EmptyAndDeleteArray was `delete[]`, and picking the wrong one is undefined
  // behaviour that nothing diagnoses. This is the `delete` flavour, so it is
  // correct only for elements allocated with `new`. Elements from `new[]`,
  // strdup or malloc need their own loop with the matching form — there is at
  // least one of each in this tree, and one of them was calling the wrong one
  // long before the conversion started.
  template <typename T> inline void EmptyAndDelete(std::vector<T*>& _vector)
  {
    for (T* element : _vector)
      delete element;
    _vector.clear();
  }
} // namespace Neuron
