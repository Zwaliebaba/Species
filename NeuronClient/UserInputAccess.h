#pragma once

#include "Vector3.h"

// What the layers below Species ask the user-input system for.
//
// UserInput lives in Species, so the two GameLogic entities that aim at the
// mouse had to include UserInput.h upward. Measured surface:
// `grep -rhoE "g_userInput->[A-Za-z_]+" GameLogic NeuronClient` finds one
// member. Everything else UserInput does — driving menus, the frame advance,
// the stretchy icons — is Species' own business.
//
// Same shape as CameraAccess.h and RendererAccess.h: g_userInput in
// WorldPointers.h is a UserInputAccess*, and Species reaches the rest through
// TheUserInput() in UserInput.h.
class UserInputAccess
{
  public:
    virtual ~UserInputAccess() = default;

    // The cached mouse position, ray-cast against the landscape once a frame.
    virtual Vector3 GetMousePos3d() = 0;
};
