#pragma once

#include <memory>
#include <vector>

#include "InputSpec.h"


// Owning, as auto_vector was. The difference is where the ownership lives: it
// used to be a promise the container's destructor kept about raw pointers, and
// it is now in the element type, so nothing can hold one of these elements
// without holding the ownership too.


namespace Neuron
{
  typedef std::vector<std::unique_ptr<const InputSpec>> InputSpecList;

  typedef InputSpecList::const_iterator InputSpecIt;

  typedef std::unique_ptr<const InputSpec> InputSpecPtr;

  typedef std::unique_ptr<const InputSpecList> InputSpecListPtr;
} // namespace Neuron
