#include "pch.h"

#include <string>

#include "InputSpec.h"
#include "InputDriverSimple.h"

using namespace std;


// In the same order as enum InputParserState (see inputdriver.h)
static string errors[] = {"An unknown error occurred.",
                          "The driver type was not recognised.",
                          "The control type was not recognised.",
                          "The control condition was not recognised.",
                          "The condition modifier was not recognised.",
                          "There was an error writing extra information.",
                          "There are too many tokens in the input description.",
                          "There was an error in the conjunction.",
                          "There was no parsing error."};


InputParserState SimpleInputDriver::parseInputSpecification(InputSpecTokens const& tokens, InputSpec& spec)
{
  InputParserState state = InputParserState::STATE_WANT_DRIVER;
  for (int i = 0; i < tokens.length(); ++i)
  {
    if (!acceptToken(state, tokens[i], spec))
      break;
  }

  if (InputParserState::STATE_WANT_OPTIONAL == state)
    state = InputParserState::STATE_DONE;

  return state;

} // End of parseInputSpecification


bool SimpleInputDriver::acceptToken(InputParserState& state, string const& token, InputSpec& spec)
{
  switch (state)
  {
  case InputParserState::STATE_WANT_DRIVER:
    if (acceptDriver(token))
    {
      state = InputParserState::STATE_WANT_CONTROL;
      return true;
    }
    else
      return false; // Don't continue parsing

  case InputParserState::STATE_WANT_CONTROL:
    spec.control_id = getControlID(token);
    if (spec.control_id >= 0)
    {
      spec.type = getControlType(spec.control_id);
      state = InputParserState::STATE_WANT_COND;
      return true;
    }
    else
      return false; // Don't continue parsing

  case InputParserState::STATE_WANT_COND:
    spec.condition = getConditionID(token, spec.type);
    if (spec.condition >= 0)
    {
      state = writeExtraSpecInfo(spec);
      return true;
    }
    else
      return false; // Don't continue parsing

  case InputParserState::STATE_WANT_MODIFIER:
  case InputParserState::STATE_WANT_OPTIONAL:
    state = parseExtraToken(token, spec);
    return true;

  case InputParserState::STATE_BAD_EXTRA:
    return false; // Don't continue parsing

  case InputParserState::STATE_DONE:
    state = InputParserState::STATE_OVERSTEP;
    return false; // Don't continue parsing

  default:
    state = InputParserState::STATE_ERROR; // We shouldn't be here
    return false;                          // Don't continue parsing
  }
}


condition_t SimpleInputDriver::getConditionID(string const& name, inputtype_t& type) { return getDefaultConditionID(name, type); }


InputParserState SimpleInputDriver::writeExtraSpecInfo(InputSpec& spec)
{
  return InputParserState::STATE_DONE; // No extra info to write. We're done.
}

InputParserState SimpleInputDriver::parseExtraToken(std::string const& token, InputSpec& spec)
{
  return InputParserState::STATE_BAD_EXTRA; // Got into InputParserState::STATE_WANT_MODIFIER with no handler.
}

// PRE-EXISTING AND PRESERVED: this table has NINE strings and
// InputParserState has TEN enumerators, so errors[STATE_DONE] reads one past
// the end, and every message from STATE_WANT_OPTIONAL on is off by one —
// STATE_WANT_OPTIONAL gets BAD_EXTRA's text, and so on. The cast below makes
// the indexing explicit; it does not change which string comes back. Surfaced
// by language-hygiene T4, which had to look at the indexing to compile it.
// Fixing it is a behaviour change and is recorded on that task, not done here.
const string& SimpleInputDriver::getLastParseError(InputParserState state) { return errors[static_cast<size_t>(state)]; }
