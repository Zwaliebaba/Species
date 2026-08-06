#pragma once


#include <string>

#include "InputDriverSimple.h"
#include "KeyDefs.h"
#include "Win32EventProc.h"

#define NUM_MB 3
#define NUM_AXES 3


namespace Neuron
{
  class W32InputDriver : public SimpleInputDriver, W32EventProcessor
  {
    private:
      int lastAcceptedDriver; // We're handling multiple driver types

      bool acceptDriver(std::string const& name);

      control_id_t getControlID(std::string const& name);

      inputtype_t getControlType(control_id_t control_id);

      InputParserState writeExtraSpecInfo(InputSpec& spec);

      control_id_t getKeyId(std::string const& keyName);

      inputtype_t getMouseControlType(control_id_t control_id);

      bool getKeyInput(InputSpec const& spec, InputDetails& details);

      bool getMouseInput(InputSpec const& spec, InputDetails& details);

    public:
      W32InputDriver();

      ~W32InputDriver();

      // Get input state. True if the input was triggered (input condition met). If true,
      // details are placed in details.
      bool getInput(InputSpec const& spec, InputDetails& details);

      // Returns true if the InputDriver is receiving no user input. Used to access screensaver
      // type modes.
      bool isIdle();

      // Returns the input mode associated with the InputDriver (keyboard or gamepad or none)
      InputMode getInputMode();

      // Returns true if there was an "active" input event this frame. Fills spec with
      // the details of the input. Active inputs are primarily things like button presses
      // but not buttons held down or released or 2D analog events.
      bool getFirstActiveInput(InputSpec& spec, bool instant);

      // This triggers a read from the input hardware and does message polling
      void Advance();

      // Poll for system events that may require immediate, hard-coded action
      void PollForEvents();

      // Fill out a description of the input defined by spec
      bool getInputDescription(InputSpec const& spec, InputDescription& desc);

      // Get the name of the driver (debuggung purposes)
      const std::string& getName();

      // This is a callback for Windows events
      // Returns 0 if the event is handled here, -1 otherwise
      LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

      // Warp the mouse to a particular position and pretend it has always been there
      void SetMousePosNoVelocity(int _x, int _y);

      // Release every key and button this driver believes is held, reporting an
      // up edge for each. Called when the window loses focus, because Windows
      // sends no release for anything held at that moment.
      void OnFocusLost() override;

    private:
      bool m_mb[NUM_MB];      // Mouse button states from this frame
      bool m_mbOld[NUM_MB];   // Mouse button states from last frame
      int m_mbDeltas[NUM_MB]; // Mouse button diffs

      int m_mousePos[NUM_AXES];    // X Y Z
      int m_mousePosOld[NUM_AXES]; // X Y Z
      int m_mouseVel[NUM_AXES];    // X Y Z

      // Wheel movement too small to be a whole detent, carried to the next
      // message. A high-resolution wheel sends deltas well under WHEEL_DELTA,
      // and dividing each one on its own threw all of them away.
      int m_wheelRemainder;

      signed char m_keyNewDeltas[KEY_MAX];
  };
} // namespace Neuron
