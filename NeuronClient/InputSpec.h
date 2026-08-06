#pragma once

#include <string>
#include <vector>
#include <memory>
#include <iostream>

#include "InputTypes.h"


namespace Neuron
{
  typedef int control_id_t;
  // Names InputType rather than int since language-hygiene T10 scoped it. Kept
  // as an alias rather than swept away because it appears in twenty-five
  // signatures and members whose spelling is not this task's business; every one
  // of them now carries the real type.
  using inputtype_t = InputType;
  // Driver-defined, and it stays an int deliberately. The drivers put unrelated
  // things in InputSpec::condition: InputCondition's enumerators (the default
  // driver) and a millisecond count IdleInputDriver parses out of the binding
  // string. Naming either type here would be wrong for the other. There was a
  // third — a deleted driver's own two-value condition enum — until
  // input-native-events T1. See language-hygiene T9's notes.
  typedef int condition_t;
  typedef int handler_id_t;

  struct InputSpec
  {
      unsigned driver;         // ID of InputDriver which handles this input
      inputtype_t type;        // Type of input details to expect
      control_id_t control_id; // Keycode, button number, etc.
      handler_id_t handler_id; // Maybe the driver contains several input handling functions
      condition_t condition;   // Condition upon which this triggers (down, up, held, clicked, etc.)
  };


  // Class to tokenise a prefs string
  class InputSpecTokens
  {
    private:
      std::vector<std::string> m_tokens;
      InputSpecTokens(std::vector<std::string> _tokens);

    public:
      InputSpecTokens(std::string _string);
      ~InputSpecTokens();
      unsigned length() const;
      const std::string& operator[](unsigned _index) const;
      std::unique_ptr<InputSpecTokens> operator()(int _start, int _end) const;
  };


  std::ostream& operator<<(std::ostream& stream, InputSpecTokens const& tokens);
} // namespace Neuron
