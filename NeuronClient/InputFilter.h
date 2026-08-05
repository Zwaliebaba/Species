#pragma once

#include <string>
#include <iostream>

#include "InputFilterSpec.h"
#include "InputTypes.h"


namespace Neuron
{
  class InputFilter
  {
    private:
      std::string m_name;

    protected:
      void setName(std::string const& name);

    public:
      // Apply the filter to a set of InputDetails to get another InputDetails
      virtual bool filter(InputDetailsList const& inDetails, InputFilterSpec const& filterSpec, InputDetails& outDetails) = 0;

      // Return true if the tokens were successfully parsed into an InputFilterSpec
      virtual bool parseFilterSpecification(InputSpecTokens const& tokens, InputFilterSpec& spec) = 0;

      // Called once per input system update
      virtual void Advance() = 0;

      virtual const std::string& getName() const;
  };


  std::ostream& operator<<(std::ostream& stream, InputFilter const& filter);
} // namespace Neuron
