#pragma once

#include <optional>
#include <string>

#include "InputSpec.h"
#include "InputSpecList.h"
#include "ControlTypes.h"


namespace Neuron
{
  struct ControlAction
  {
      const char* name; // How this action appears in a control preferences file.
      inputtype_t type; // The types of input that can be handled (bit flags of InputType)
  };


  // This is basically a collection of mappings of ControlType -> vector<InputSpec>
  class ControlBindings
  {
    private:
      // Holds the actual bindings
      InputSpecList bindings[Neuron::I(ControlType::NumControlTypes)];

      // Hold icon file paths
      std::string icons[Neuron::I(ControlType::NumControlTypes)];

      // Allows us to a control event for the rest of the frame
      unsigned char suppressed[Neuron::I(ControlType::NumControlTypes)];

    public:
      ControlBindings();

      // Returns the Control Type with the given name, or nothing with a bad name.
      //
      // IT USED TO RETURN -1, and language-hygiene T11 did not replace that with
      // an enumerator, because there is no enumerator that means "no such
      // control": ControlNull is a real index into the three arrays above and a
      // bindable one. std::optional says what -1 was standing in for, and both
      // callers already tested `>= 0` rather than using the value.
      static std::optional<ControlType> getControlID(std::string const& name);

      static void getControlString(ControlType type, std::string& name);

      // Returns true if a particular input type can be used to feed a
      // particular control type
      static bool isAcceptibleInputType(ControlType binding, inputtype_t type);

      // Grab the list of InputSpec associated with a particular control type
      const InputSpecList& operator[](ControlType id) const;

      // Grab the icon path associated with a particular control type
      const std::string& getIcon(ControlType id) const;

      void setIcon(ControlType id, std::string const& iconfile);

      // Associate an InputSpec with a control type, returning true on success
      bool bind(ControlType type, InputSpec const& spec, bool replace = false);

      // bool replacePrimaryBinding( controltype_t type, InputSpec const &spec );

      // Signals a frame change
      void Advance();

      // Suppress an event until the end of the frame
      void suppress(ControlType id);

      // Check if an event is suppressed
      bool isActive(ControlType id) const;

      // Remove all bindings
      void Clear();
  };


  // FIVE MEMBERS ABOVE WERE DECLARED TWICE UNTIL language-hygiene T11 — once
  // taking ControlType and once taking controltype_t — and the pair compiled
  // only because controltype_t was `typedef int`. operator[], getIcon and their
  // int-taking siblings differed in BEHAVIOUR as well as in type: the ControlType
  // overloads indexed unchecked, the int ones bounds-checked and threw. Making
  // controltype_t name ControlType makes each pair one function, so one body had
  // to win, and THE CHECKED ONE DID. That is a behaviour change on exactly the
  // inputs that were already out of bounds — every existing caller passes a real
  // enumerator or a value getControlID has already vouched for, so nothing on a
  // reachable path starts throwing. See ControlBindings.cpp.
} // namespace Neuron
