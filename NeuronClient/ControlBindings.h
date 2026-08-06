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

      // THE ICON PATHS ARE GONE. getIcon, setIcon and the array behind them
      // were dead at BOTH ends: nothing in the tree rendered a control icon,
      // and the `~` lines that fed them were at zero in GameData/Input,
      // because the eighteen input-native-events T2 removed were all of them.
      // A parser branch reading a syntax no file uses into a map nothing asks
      // about is not a feature waiting for a consumer.

      // Associate an InputSpec with a control type, returning true on success
      bool bind(ControlType type, InputSpec const& spec, bool replace = false);

      // bool replacePrimaryBinding( controltype_t type, InputSpec const &spec );

      // THE SUPPRESSION MECHANISM IS GONE, and with it Advance(), which existed
      // only to clear it once a frame. suppress() marked one control name dead
      // for the rest of the frame, and its single caller was
      // UserInput::AdvanceMenus taking back a click it had already read once it
      // noticed the click was over a window. A sink that consumes the EVENT
      // says so before anything reads it, and says it about the input rather
      // than about one of the control names bound to that input. See
      // InputRouter.h.

      // Remove all bindings
      void Clear();
  };


  // FIVE MEMBERS ABOVE WERE DECLARED TWICE UNTIL language-hygiene T11 — once
  // taking ControlType and once taking controltype_t — and the pair compiled
  // only because controltype_t was `typedef int`. Two of the five have since
  // been deleted outright with the icon paths; operator[] and its int-taking
  // sibling are the surviving example, and they differed in BEHAVIOUR as well
  // as in type: the ControlType
  // overloads indexed unchecked, the int ones bounds-checked and threw. Making
  // controltype_t name ControlType makes each pair one function, so one body had
  // to win, and THE CHECKED ONE DID. That is a behaviour change on exactly the
  // inputs that were already out of bounds — every existing caller passes a real
  // enumerator or a value getControlID has already vouched for, so nothing on a
  // reachable path starts throwing. See ControlBindings.cpp.
} // namespace Neuron
