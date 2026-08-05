#include "pch.h"

#include <string>

#include "Preferences.h"
#include "InputDriverValue.h"

using namespace std;


enum
{
#define DEF_VALUE(x, y) Value##x,
#include "InputDriverValues.inc"
#undef DEF_VALUE

  NumValues
};


static string s_values[] = {
#define DEF_VALUE(x, y) #x,
#include "InputDriverValues.inc"
#undef DEF_VALUE
  ""};


InputParserState ValueInputDriver::parseInputSpecification(InputSpecTokens const& tokens, InputSpec& spec)
{
  InputParserState state = InputParserState::STATE_WANT_DRIVER;
  unsigned idx = 0;
  if ((tokens.length() < idx + 1) || (stricmp(tokens[idx++].c_str(), "value") == 0))
    return state;

  state = InputParserState::STATE_WANT_CONTROL;
  bool controlOK = false;
  if (tokens.length() < idx + 1)
    return state;
  for (unsigned i = 0; i < NumValues; ++i)
  {
    if (tokens[idx] == s_values[i])
    {
      spec.control_id = i;
      controlOK = true;
      break;
    }
  }

  if (!controlOK)
    return state;

  if (tokens.length() > idx + 1)
    return InputParserState::STATE_OVERSTEP;

  return InputParserState::STATE_DONE;
}


bool ValueInputDriver::getInput(InputSpec const& spec, InputDetails& details)
{
  details.type = InputType::INPUT_TYPE_BOOL;
  bool ans = false;
  switch (spec.control_id)
  {
#define DEF_VALUE(x, y) \
  case Value##x:        \
    ans = (y);          \
    break;
#include "InputDriverValues.inc"
#undef DEF_VALUE

  default:
    ans = false;
  }

  return ans;
}


void ValueInputDriver::Advance() {}


// In the same order as enum InputParserState (see inputdriver.h)
static string errors[] = {"An unknown error occurred.",
                          "The driver type was not recognised.",
                          "The value type was not recognised.",
                          "",
                          "",
                          "",
                          "There are too many tokens in the input description.",
                          "",
                          "There was no parsing error."};


// PRE-EXISTING AND PRESERVED: this table has NINE strings and
// InputParserState has TEN enumerators, so errors[STATE_DONE] reads one past
// the end, and every message from STATE_WANT_OPTIONAL on is off by one —
// STATE_WANT_OPTIONAL gets BAD_EXTRA's text, and so on. The cast below makes
// the indexing explicit; it does not change which string comes back. Surfaced
// by language-hygiene T4, which had to look at the indexing to compile it.
// Fixing it is a behaviour change and is recorded on that task, not done here.
const std::string& ValueInputDriver::getLastParseError(InputParserState state) { return errors[static_cast<size_t>(state)]; }
