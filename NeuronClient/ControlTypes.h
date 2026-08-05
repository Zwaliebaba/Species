#pragma once

#include "NeuronHelper.h"


// Scoped by language-hygiene T11. int is the explicit underlying type because
// that is what the `typedef int controltype_t` this was reached through has
// always been, and because ControlBindings still stores one in an array bound.
//
// THE ENUMERATOR LIST IS GENERATED FROM ControlTypes.inc, and ControlBindings'
// s_actions table is generated from the SAME file and keyed to it
// POSITIONALLY — index i of that table is the enumerator whose value is i.
// The comment that used to sit here asked you to keep the two in step by hand,
// and named a file (control_bindings.cpp) that has not existed under that name
// for some time. ControlBindings.cpp now static_asserts all 133 pairs from the
// .inc itself, so the two cannot drift without failing the build.
enum class ControlType : int
{
#define DEF_CONTROL_TYPE(x, y) Control##x,
#include "ControlTypes.inc"
#undef DEF_CONTROL_TYPE

  ControlNull, // This is never triggered

  NumControlTypes
};

// ControlType ENUMERATES, which is what separates it from InputType — see
// InputTypes.h, where only ENUM_HELPER's bitwise half means anything. Here it
// is the other way round: the range, the size and ++ are what this enum wants,
// and nothing ORs two control types together. The bitwise operators come along
// because the macro is one macro; do not use them.
//
// The range is [ControlMenuLeft, ControlNull], which is 0..ControlNull
// inclusive — every valid index into ControlBindings' three arrays, and
// exactly NumControlTypes of them. SizeOfControlType() is therefore
// NumControlTypes, and either spelling is correct.
ENUM_HELPER(ControlType, ControlMenuLeft, ControlNull)

static_assert(SizeOfControlType() == Neuron::I(ControlType::NumControlTypes),
              "ControlNull must be the last real control type: NumControlTypes is the array bound");

// Kept as a name for the scoped enum rather than deleted, which is what
// language-hygiene T10 did with `inputtype_t` (InputSpec.h) for the same
// reason: the two names appear in different halves of this API and retiring
// the alias is a rename, not a scoping change.
//
// IT USED TO BE `typedef int`, AND THAT IS WHY ControlBindings DECLARED FIVE
// OF ITS MEMBERS TWICE — once on ControlType and once on controltype_t, which
// were different types only because one of them was int. They are one
// declaration each now. See ControlBindings.h.
using controltype_t = ControlType;
