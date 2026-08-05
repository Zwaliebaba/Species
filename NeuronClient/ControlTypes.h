#pragma once


typedef int controltype_t;


// If you change this, you must also change the actions in control_bindings.cpp
enum ControlType
{

#define DEF_CONTROL_TYPE(x, y) Control##x,
#include "ControlTypes.inc"
#undef DEF_CONTROL_TYPE

  ControlNull, // This is never triggered

  NumControlTypes
};
