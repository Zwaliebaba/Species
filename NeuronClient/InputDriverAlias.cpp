#include "pch.h"

#include "InputDriverAlias.h"
#include "Input.h"

using std::string;


namespace Neuron
{
  AliasInputDriver::AliasInputDriver() { setName("Alias"); }


  InputParserState AliasInputDriver::parseInputSpecification(InputSpecTokens const& tokens, InputSpec& spec)
  {
    InputParserState state = InputParserState::STATE_WANT_DRIVER;
    int idx = 0;
    if ((idx >= tokens.length()) || (tokens[idx++] != "alias"))
      return state;

    state = InputParserState::STATE_WANT_CONTROL;
    if (idx >= tokens.length())
      return state;

    // THIS DRIVER IS THE ONLY ONE THAT STORES A ControlType IN A control_id_t.
    // Every other driver's control_id is a keycode or a button number, so the
    // field cannot become the enum; the conversion happens here and is undone by
    // the two bounds checks below. That is the boundary language-hygiene T11
    // lists rather than removes.
    std::optional<ControlType> const control = g_inputManager->getControlID(tokens[idx++]);
    if (!control.has_value())
    {
      return state;
    }
    spec.control_id = static_cast<control_id_t>(*control);

    spec.type = InputType::INPUT_TYPE_BOOL;

    return (idx < tokens.length()) ? InputParserState::STATE_OVERSTEP : InputParserState::STATE_DONE;
  }


  bool AliasInputDriver::getInput(InputSpec const& spec, InputDetails& details)
  {
    // spec.control_id is a DRIVER's raw id (control_id_t, an int) that this
    // driver alone happens to fill with a ControlType, so the bound has to be
    // brought down to int rather than the id being lifted to the enum — a
    // boundary language-hygiene T11 lists rather than casts away.
    if (0 <= spec.control_id && spec.control_id < static_cast<control_id_t>(ControlType::NumControlTypes))
      return g_inputManager->controlEvent(static_cast<ControlType>(spec.control_id), details);
    else
      return false; // We should never get here!
  }


  void AliasInputDriver::Advance() {}


  // In the same order as enum InputParserState (see inputdriver.h)
  static string errors[] = {"An unknown error occurred.",
                            "The driver type was not recognised.",
                            "The control alias was not recognised.",
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
  const string& AliasInputDriver::getLastParseError(InputParserState state) { return errors[static_cast<size_t>(state)]; }


  bool AliasInputDriver::getInputDescription(InputSpec const& spec, InputDescription& desc)
  {
    // TODO: This isn't quite right.
    // May break translations.
    // spec.control_id is a DRIVER's raw id (control_id_t, an int) that this
    // driver alone happens to fill with a ControlType, so the bound has to be
    // brought down to int rather than the id being lifted to the enum — a
    // boundary language-hygiene T11 lists rather than casts away.
    if (0 <= spec.control_id && spec.control_id < static_cast<control_id_t>(ControlType::NumControlTypes))
      return g_inputManager->getBoundInputDescription(static_cast<ControlType>(spec.control_id), desc);
    else
      return false; // We should never get here!
  }
} // namespace Neuron
