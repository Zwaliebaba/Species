#include "pch.h"

#include <iterator>
#include <memory>
#include <string_view>
#include <type_traits>

#include "InputTypes.h"
#include "ControlBindings.h"

using std::unique_ptr;

// This has to be in the same order as the enum in
// ControlTypes.h
// These are the names of the control actions as they
// appear in an input configuration file
//
// constexpr rather than merely static since language-hygiene T11, so that the
// positional agreement below can be asserted rather than trusted. Nothing has
// ever written to it.


namespace Neuron
{
  constexpr ControlAction s_actions[] = {
#define DEF_CONTROL_TYPE(name, type) {#name, type},
#include "ControlTypes.inc"
#undef DEF_CONTROL_TYPE
    {"", InputType::INPUT_TYPE_ANY}, {nullptr, InputType::INPUT_TYPE_FAIL}};


// THE TWO TABLES ARE NOW CHECKED AGAINST EACH OTHER BY THE COMPILER, which is
// what ControlTypes.h used to ask a reader to do by eye. Both are generated
// from ControlTypes.inc, so this walks the .inc a third time and asserts that
// the enumerator it declares at position i names the entry s_actions holds at
// position i. Reorder, insert or delete a line in the .inc and every pair from
// there on fails the build, naming the first one that drifted.
// The parameters are x and y, NOT name and type, and that is load-bearing: the
// member being read below is also called `name`, so a parameter of that name is
// substituted into `.name` and the assert reads a member that does not exist.
// It cost a CI round.
#define DEF_CONTROL_TYPE(x, y)                                                              \
  static_assert(std::string_view(s_actions[Neuron::I(ControlType::Control##x)].name) == #x, \
                "ControlTypes.inc and s_actions have drifted apart at " #x);
#include "ControlTypes.inc"
#undef DEF_CONTROL_TYPE

  // ControlNull's own row, and the null terminator getControlID walks to, sit
  // past the generated ones. NumControlTypes counts ControlNull, so the table is
  // one longer than that: the terminator is at index NumControlTypes.
  static_assert(std::size(s_actions) == Neuron::I(ControlType::NumControlTypes) + 1,
                "s_actions must hold one row per control type plus the null terminator");
  static_assert(std::string_view(s_actions[Neuron::I(ControlType::ControlNull)].name).empty(), "ControlNull's row must be the empty-name row");
  static_assert(s_actions[Neuron::I(ControlType::NumControlTypes)].name == nullptr,
                "the row at NumControlTypes must be the terminator getControlID stops on");


  namespace
  {
    using control_underlying_t = std::underlying_type_t<ControlType>;

    // `0 <= id && id < NumControlTypes`, which stopped being writable directly
    // when the enum was scoped. It has to compare on the underlying type rather
    // than through Neuron::I(): I() yields size_t, and a negative control type —
    // which is exactly what these guards exist to reject — converts to a very
    // large one rather than to something less than zero.
    constexpr bool isRealControlType(ControlType id) noexcept
    {
      auto const value = static_cast<control_underlying_t>(id);
      return 0 <= value && value < static_cast<control_underlying_t>(ControlType::NumControlTypes);
    }
  } // namespace


  ControlBindings::ControlBindings() { memset(suppressed, 0, sizeof(suppressed)); }


  std::optional<ControlType> ControlBindings::getControlID(std::string const& name)
  {
    int i = 0;
    const char* cmd = s_actions[i].name;
    while (cmd && stricmp(name.c_str(), cmd) != 0)
      cmd = s_actions[++i].name;
    if (i < static_cast<control_underlying_t>(ControlType::NumControlTypes))
      return static_cast<ControlType>(i);
    else
      return std::nullopt;
  }


  void ControlBindings::getControlString(ControlType type, std::string& name)
  {
    if (isRealControlType(type))
      name = s_actions[Neuron::I(type)].name;
    else
      throw "Bad control type.";
  }


  bool ControlBindings::isAcceptibleInputType(ControlType binding, inputtype_t type)
  {
    if (isRealControlType(binding))
    {
      // Typed rather than an int now, which is the whole point of the scoping:
      // the & and | below are InputType's own operators, so nothing here can be
      // silently mixed with a ControlType or a condition id.
      InputType validTypes = s_actions[Neuron::I(binding)].type;
      if ((validTypes & InputType::INPUT_TYPE_BOOL) == InputType::INPUT_TYPE_BOOL)
        validTypes = validTypes | InputType::INPUT_TYPE_1D; // BOOL implies 1D (can use triggers)
      return (validTypes & type) == type;
    }
    return false;
  }


  const InputSpecList& ControlBindings::operator[](ControlType id) const
  {
    if (isRealControlType(id))
      return bindings[Neuron::I(id)];
    else
      throw "Invalid control type.";
  }


  const std::string& ControlBindings::getIcon(ControlType id) const
  {
    if (isRealControlType(id))
      return icons[Neuron::I(id)];
    else
      throw "Invalid control type.";
  }


  void ControlBindings::setIcon(ControlType id, std::string const& iconfile)
  {
    if (isRealControlType(id))
      icons[Neuron::I(id)] = iconfile;
    else
      throw "Invalid control type.";
  }


  bool ControlBindings::bind(ControlType type, InputSpec const& spec, bool replace)
  {
    if (isAcceptibleInputType(type, spec.type))
    {
      std::unique_ptr<const InputSpec> specCopy(new InputSpec(spec));
      if (replace && bindings[Neuron::I(type)].size() > 0)
      {
        // Was erase(0) then insert(0, ...) — auto_vector's erase deleted the
        // element it dropped, and assigning a unique_ptr does the same.
        bindings[Neuron::I(type)][0] = std::move(specCopy);
      }
      else
        bindings[Neuron::I(type)].push_back(std::move(specCopy));
      return true;
    }
    else
      return false;
  }


  // bool ControlBindings::replacePrimaryBinding( controltype_t type, InputSpec const &spec )
  //{
  //	if ( isAcceptibleInputType( type, spec.type ) ) {
  //		std::unique_ptr<const InputSpec> specCopy( new InputSpec( spec ) );
  //		if ( bindings[ type ].size() > 0 )
  //			bindings[ type ].erase( 0 );
  //		bindings[ type ].insert( 0, specCopy );
  //		return true;
  //	} else
  //		return false;
  // }


  void ControlBindings::Advance() { memset(suppressed, 0, sizeof(suppressed)); }


  void ControlBindings::Clear()
  {
    for (ControlType const type : RangeControlType())
      bindings[Neuron::I(type)].clear();
  }


  void ControlBindings::suppress(ControlType id) { suppressed[Neuron::I(id)] = 1; }


  bool ControlBindings::isActive(ControlType id) const { return (suppressed[Neuron::I(id)] == 0); }
} // namespace Neuron
