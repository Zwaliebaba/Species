#pragma once

#include "InputSpec.h"
#include "InputTypes.h"


namespace Neuron
{
  typedef int filter_mode_t;
  typedef unsigned filterspec_id_t;

  struct InputFilterSpec
  {
      unsigned filter;     // ID of InputDriver which handles this input
      filterspec_id_t id;  // A unique identifier to identify this in the filter
      InputType type;      // Type of input details to expect out
      unsigned min_inputs; // Number of inputs expected
      unsigned max_inputs;
      filter_mode_t mode;    // Mode of the filter (eg left)
      condition_t condition; // Exact condition to poll
  };
} // namespace Neuron
