#include "pch.h"

#include "InputTypes.h"
#include "ControlBindings.h"

static ControlAction s_actions[] = {

	#define DEF_CONTROL_TYPE(x,y) "x", y,
	#include "ControlTypes.inc"
	#undef DEF_CONTROL_TYPE

	"",                                  INPUT_TYPE_ANY,

	nullptr,                                nullptr

};

