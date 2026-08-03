#pragma once

#include "LList.h"
#include "Vector3.h"

#include "UserInputAccess.h"
#include "WorldObject.h"
#include "WorldPointers.h"

class StretchyIcons;
class Building;
class Engineer;


class UserInput : public UserInputAccess
{
  public:
    bool m_removeTopLevelMenu;

  private:
    Vector3 m_mousePos3d;

    void AdvanceMouse();
    void AdvanceMenus();

    LList<Vector3*> m_mousePosHistory;

  public:
    UserInput();
    void Advance();
    void Render();

    void RecalcMousePos3d(); // Updates the cached value of m_mousePos3d by doing a ray cast against landscape
    Vector3 GetMousePos3d(); // Returns the cached value "m_mousePos3d"
};


// g_userInput is a UserInputAccess* so the layers below Species need only the
// interface. Species reaches the whole class through here, at every call site.
// The cast is safe because App is the only thing that assigns g_userInput.
inline UserInput* TheUserInput() { return static_cast<UserInput*>(g_userInput); }
