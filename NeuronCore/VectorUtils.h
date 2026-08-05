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

  // Copies into a fixed char array, truncating rather than overrunning.
  //
  // Transitional, like EmptyAndDelete above. It exists for fields that cannot
  // become a std::string yet because something else still writes through a raw
  // char* into them — InputField does exactly that for every editable name in
  // the editor, and strings-modernised/T5 is what changes it. Until then the
  // copies into those fields have to be bounded somehow, and one helper is
  // better than the same six lines written out at each site.
  //
  // Not std::min in the length clamp: MathUtils.h defines function-style min
  // and max macros, so `std::min(` becomes `std::(...)(` anywhere it is
  // reachable. See tasks/language-hygiene.yaml T8.
  template <size_t N> inline void CopyInto(char (&_dest)[N], std::string_view _source)
  {
    size_t length = _source.size();
    if (length > N - 1)
    {
      length = N - 1;
    }
    std::memcpy(_dest, _source.data(), length);
    _dest[length] = '\0';
  }
} // namespace Neuron
