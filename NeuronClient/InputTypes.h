#pragma once

#include <string>
#include <memory>
#include <vector>


// Scoped by language-hygiene T10. int is the explicit underlying type because
// that is what the `typedef int inputtype_t` this was reached through has
// always been.
//
// THIS IS A BIT FIELD, which is what separates it from every other enum this
// plan has scoped, and the VALUES ARE THE MEANING rather than mere labels:
//
//   ANALOG is bit 1. 1D is bit 2 PLUS ANALOG's; 2D is bit 3 PLUS ANALOG's.
//   So `(t & INPUT_TYPE_ANALOG) == INPUT_TYPE_ANALOG` is true for 1D and 2D
//   as well as for ANALOG itself, and that implication is relied on.
//   ANY is all four bits, so `(t & ANY) == t` for every value.
//
// ControlBindings::isAcceptibleInputType is the site that depends on it most
// visibly: it ORs INPUT_TYPE_1D into a BOOL binding so that a trigger can
// drive a button action. Renumber any of these and that stops meaning what it
// says.
enum class InputType : int
{
  INPUT_TYPE_FAIL = 0,   // The InputDetails are invalid
  INPUT_TYPE_BOOL = 1,   // Buttons, etc.
  INPUT_TYPE_ANALOG = 2, // Contains axis information
  INPUT_TYPE_1D = 6,     // XInput trigger buttons (implies ANALOG)
  INPUT_TYPE_2D = 10,    // Mouse or joystick axes (implies ANALOG)
  INPUT_TYPE_ANY = 15    // Not concerned (just want to know if it triggered)
};

// ONLY THE BITWISE HALF OF THIS IS MEANINGFUL HERE. ENUM_HELPER is written for
// enums that enumerate, and it also emits operator++/--, ItInputType,
// RangeInputType and SizeOfInputType(). For a bit field with gaps those are
// nonsense -- RangeInputType would walk 0 to 15, and eleven of those sixteen
// values name nothing. Do not use them. What this is here for is |, &, ~, ^
// and their compound forms, which the code below genuinely needs.
//
// This is the tree's first use of ENUM_HELPER; the macro has been carried,
// unused, since it was written.
ENUM_HELPER(InputType, INPUT_TYPE_FAIL, INPUT_TYPE_ANY)

// The implications described above, checked rather than trusted. Scoping the
// enum stops a value being confused with another enum's; it does nothing to
// stop someone renumbering these, and renumbering is the way this breaks.
static_assert((InputType::INPUT_TYPE_1D & InputType::INPUT_TYPE_ANALOG) == InputType::INPUT_TYPE_ANALOG,
              "INPUT_TYPE_1D must carry INPUT_TYPE_ANALOG's bit: a 1D input is an analog input");
static_assert((InputType::INPUT_TYPE_2D & InputType::INPUT_TYPE_ANALOG) == InputType::INPUT_TYPE_ANALOG,
              "INPUT_TYPE_2D must carry INPUT_TYPE_ANALOG's bit: a 2D input is an analog input");
static_assert((InputType::INPUT_TYPE_ANY & InputType::INPUT_TYPE_BOOL) == InputType::INPUT_TYPE_BOOL &&
                (InputType::INPUT_TYPE_ANY & InputType::INPUT_TYPE_1D) == InputType::INPUT_TYPE_1D &&
                (InputType::INPUT_TYPE_ANY & InputType::INPUT_TYPE_2D) == InputType::INPUT_TYPE_2D,
              "INPUT_TYPE_ANY must be a superset of every other InputType");
static_assert(InputType::INPUT_TYPE_FAIL == InputType{}, "INPUT_TYPE_FAIL must be the zero value: it is the tables' terminator");


struct InputDetails
{
    InputType type; // Determines what we can expect in x and y
    int x;          // Only meaningful if type is INPUT_TYPE_1D or INPUT_TYPE_2D
    int y;          // Only meaningful if type is INPUT_TYPE_2D
};

// Owning — see InputSpecList.h. Note for anyone constructing one: auto_vector's
// one-argument constructor RESERVED that many slots and left the vector empty.
// std::vector's creates that many null elements instead. Call reserve().
typedef std::vector<std::unique_ptr<const InputDetails>> InputDetailsList;
typedef std::unique_ptr<InputDetails> InputDetailsPtr;

class InputDescription
{
  public:
    InputDescription();

    InputDescription(InputDescription const& _desc);

    InputDescription(std::string const& _noun, std::string const& _verb, std::string const& _pref, bool _translated = false);

    void translate();

    std::string noun;

    std::string verb;

    std::string pref;

    bool translated;
};


// Scoped. Only ever used as itself: no int typedef stands in for it and it is
// not a bit field, which is what separates it from InputType — see
// language-hygiene T9.
enum class InputMode
{
  INPUT_MODE_NONE,     // Describes a driver not associated with a specific input mode
  INPUT_MODE_KEYBOARD, // Describes a driver that accepts input from Keyboard or Mouse
  INPUT_MODE_GAMEPAD   // Describes a driver that accepts input from a Gamepad
};
