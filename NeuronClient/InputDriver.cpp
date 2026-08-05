#include "pch.h"

#include <iostream>

#include "InputDriver.h"


struct ConditionInfo
{
    inputtype_t type; // InputType
    char const* name;
    InputCondition cond;
};

// The braces are new; the entries are not. This was a brace-elided flat list
// terminated by `NULL, nullptr, NULL`, and the terminator's first and third
// fields were the only two NULLs left in NeuronClient. Both were zero, and
// both still are: INPUT_TYPE_FAIL is 0 and COND_DOWN is 0. Nothing about the
// sentinel's VALUE changed — see the note in getDefaultConditionID about what
// that costs, which is a defect this task recorded rather than fixed.
static ConditionInfo s_conditions[] = {
  // Type            Name        Condition
  {INPUT_TYPE_BOOL, "down", InputCondition::COND_DOWN},       // Button was just pushed down
  {INPUT_TYPE_BOOL, "up", InputCondition::COND_UP},           // Button was just released
  {INPUT_TYPE_BOOL, "pressed", InputCondition::COND_PRESSED}, // Button is still pressed
  //	INPUT_TYPE_BOOL,   "action",   COND_ACTION,   // Something happened (as in "key any action")
  {INPUT_TYPE_ANALOG, "release", InputCondition::COND_RELEASED}, // Analog just entered dead zone
  {INPUT_TYPE_ANALOG, "position", InputCondition::COND_NONZERO}, // Analog is outside dead zone (return position)
  {INPUT_TYPE_ANALOG, "move", InputCondition::COND_MOVED},       // Analog is outside dead zone (return delta)
  {INPUT_TYPE_ANALOG, "zero", InputCondition::COND_ZERO},        // Analog is still in dead zone
  {INPUT_TYPE_ANALOG, "read", InputCondition::COND_READ},        // Analog always triggers (return default device info)
  {INPUT_TYPE_FAIL, nullptr, InputCondition::COND_DOWN}};

condition_t InputDriver::getDefaultConditionID(std::string const& name, inputtype_t& type)
{
  int i = 0;
  ConditionInfo info = s_conditions[i];
  while (info.name && ((type & info.type) != info.type || name != info.name))
    info = s_conditions[++i];
  // PRESERVED, NOT FIXED, and it is wrong: the table holds eight entries plus
  // a terminator, so an unmatched name leaves i at 8, and 8 is less than
  // NumInputConditions (10). The failure path therefore returns the
  // terminator's cond — COND_DOWN — rather than -1. See T9's notes.
  if (i < static_cast<int>(InputCondition::NumInputConditions))
  {
    type = info.type;
    return static_cast<condition_t>(info.cond);
  }
  else
    return -1;
}


bool InputDriver::getDefaultPrefsString(condition_t condition_id, inputtype_t type, std::string& prefsString)
{
  int i = 0;
  ConditionInfo info = s_conditions[i];
  while (info.name && ((type & info.type) != info.type || condition_id != static_cast<condition_t>(info.cond)))
    info = s_conditions[++i];
  // Same off-by-two as above, and worse here: on the failure path info.name is
  // the terminator's nullptr, and assigning nullptr to a std::string is
  // undefined. Preserved rather than fixed — see T9's notes.
  if (i < static_cast<int>(InputCondition::NumInputConditions))
  {
    prefsString = info.name;
    return true;
  }
  else
  {
    prefsString = "unknown";
    return false;
  }
}


bool InputDriver::getDefaultVerb(condition_t condition_id, inputtype_t type, std::string& verb)
{
  std::string base;
  bool ans = getDefaultPrefsString(condition_id, type, base);
  verb = "control_verb_";
  verb.append(base);
  return ans;
}


void InputDriver::PollForEvents() {}


bool InputDriver::isIdle() { return true; }


InputMode InputDriver::getInputMode() { return InputMode::INPUT_MODE_NONE; }


bool InputDriver::getInputDescription(InputSpec const& spec, InputDescription& desc) { return false; }


bool InputDriver::getFirstActiveInput(InputSpec& spec, bool instant) { return false; }


void InputDriver::setName(std::string const& name) { m_name = name; }


const std::string& InputDriver::getName() const { return m_name; }


std::ostream& operator<<(std::ostream& stream, InputDriver const& driver) { return stream << "InputDriver (" << driver.getName() << ")"; }
