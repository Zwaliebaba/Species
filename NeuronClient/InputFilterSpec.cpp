#include "pch.h"

#include "InputFilterSpec.h"


namespace Neuron
{
  unsigned long newFilterSpecID()
  {
    static unsigned long nextID = 0;
    return nextID++;
  }
} // namespace Neuron
