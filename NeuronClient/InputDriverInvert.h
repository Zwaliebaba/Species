#pragma once

#include "InputDriver.h"
#include "InputSpecList.h"


// Enables boolean input specifications to be negated with "not" or "!"


namespace Neuron
{
  class InvertInputDriver : public InputDriver
  {
    private:
      // List of InputSpec
      InputSpecList m_specs;
      std::string& lastError;

    public:
      InvertInputDriver();

      // Return STATE_DONE if we managed to parse these tokens, and put the parsed information
      // into spec. Anything else means we failed.
      InputParserState parseInputSpecification(InputSpecTokens const& tokens, InputSpec& spec);

      // Get input state. True if the input was triggered (input condition met). If true,
      // details are placed in details.
      bool getInput(InputSpec const& spec, InputDetails& details);

      // This triggers a read from the input hardware and does message polling
      void Advance();

      // Return a helpful error string when there's a problem
      const std::string& getLastParseError(InputParserState state);

      // Fill out a description of the input defined by spec
      bool getInputDescription(InputSpec const& spec, InputDescription& desc);
  };
} // namespace Neuron
