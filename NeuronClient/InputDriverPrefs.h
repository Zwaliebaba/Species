#pragma once

#include <memory>
#include <string>
#include <vector>

#include "InputDriver.h"


namespace Neuron
{
  class PrefsInputDriver : public InputDriver
  {
    private:
      // List of preference keys we use, not ordered since we want to preserve
      // positions despite additions
      std::vector<std::unique_ptr<std::string>> m_keys;

      // Searches for a key in m_keys. Adds one if it has to.
      int keyPosition(std::string const& key);

    public:
      PrefsInputDriver();

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

      // Get the name of the driver (debuggung purposes)
      const std::string& getName();
  };
} // namespace Neuron
