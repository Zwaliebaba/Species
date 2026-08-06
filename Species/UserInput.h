#pragma once

#include "NeuronMath.h"

#include "Input.h"
#include "UserInputAccess.h"
#include "WorldObject.h"
#include "WorldPointers.h"

class StretchyIcons;


namespace Species
{
  class Building;
  class Engineer;

  class UserInput : public UserInputAccess
  {
    public:
      bool m_removeTopLevelMenu;

    private:
      // Braced to zero: Vector3's default constructor did it, XMFLOAT3's does
      // not, and RecalcMousePos3d leaves this untouched on the frames where
      // neither the landscape nor the enclosing sphere is hit.
      DirectX::XMFLOAT3 m_mousePos3d{0.0f, 0.0f, 0.0f};

      void AdvanceMouse();
      void AdvanceMenus();

      std::vector<DirectX::XMFLOAT3*> m_mousePosHistory;

      // PILOT CONVERSION, input-native-events T9. Pause used to be a poll in
      // Advance; it is a subscription now, and this token is what keeps it
      // alive. Destroying UserInput cancels it without UserInput having to
      // remember to.
      Neuron::ControlSubscription m_pauseSubscription;

    public:
      UserInput();
      void Advance();
      void Render();

      // Called once, after InitialiseInputManager, because App builds UserInput
      // BEFORE the input manager exists — g_inputManager is still null in this
      // class's constructor, so the subscription cannot be made there.
      void SubscribeToControls();

      void RecalcMousePos3d();           // Updates the cached value of m_mousePos3d by doing a ray cast against landscape
      DirectX::XMFLOAT3 GetMousePos3d(); // Returns the cached value "m_mousePos3d"
  };


  // g_userInput is a UserInputAccess* so the layers below Species need only the
  // interface. Species reaches the whole class through here, at every call site.
  // The cast is safe because App is the only thing that assigns g_userInput.
  inline UserInput* TheUserInput() { return static_cast<UserInput*>(g_userInput); }
} // namespace Species
