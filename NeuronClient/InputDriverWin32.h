#pragma once


#include <string>
#include <vector>

#include "InputDriverSimple.h"
#include "InputEvents.h"
#include "KeyDefs.h"
#include "Win32EventProc.h"


namespace Neuron
{
  // A frame's worth of messages is a handful. This bound exists so a window
  // flooded while the frame loop is stalled — a breakpoint, a long load —
  // cannot grow the queue without limit. Dropping the OLDEST would reorder
  // edges, so the NEWEST are dropped: the state stays consistent with what was
  // delivered, it just stops partway.
  constexpr size_t MaxQueuedEvents = 4096;

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
      // sends no release for anything held at that moment. Enqueues a FocusLost
      // event like every other message; the release edges are produced by the
      // derivation, in frame order with everything else.
      void OnFocusLost() override;

    private:
      // THE MESSAGES, NOT THE STATE. WndProc appends here and does nothing
      // else, so two messages about the same key in one frame are two records
      // rather than one overwriting the other. Advance drains what a frame can
      // represent and leaves the rest; see DeriveFrameState.
      std::vector<InputEvent> m_events;

      // Everything the bindings read, rebuilt from m_events once per frame.
      // These used to be the file-scope g_keys and g_keyDeltas plus six driver
      // arrays; nothing outside this class reads them now.
      InputFrameState m_state;

      // THE CAPTURE RULE, in its first and narrowest form. A key whose
      // character a focused text field took is marked here, and getKeyInput
      // answers false for it until it is released — so typing a name does not
      // also fire whatever game control that letter is bound to, and `key g up`
      // does not fire when you let go either.
      //
      // It is deliberately NOT "suppress every key while a field has focus".
      // Enter, escape and the arrows produce no character the field accepts, so
      // they are not marked and still reach the bindings exactly as they did —
      // which is what keeps menu navigation and escape-to-close working while
      // the cursor is in a text box.
      //
      // The mask is a mask over READS, not over the state: m_keys and
      // m_keyDeltas stay truthful underneath it, so nothing is left stuck when
      // the mark clears, and getFirstActiveInput — the key-rebinding window's
      // door — is unaffected. T8 replaces all of this with the router's
      // consuming sinks, of which this is the one case that could not wait.
      bool m_textConsumedKeys[KEY_MAX]{};

      // Shift, control and alt as they were at one particular moment. Passed
      // through the side-effect walk rather than read off the frame state,
      // because the frame state is what the modifiers ended up as and Eclipse
      // needs what they were when each key was struck.
      struct ModifierState
      {
          bool m_shift{false};
          bool m_control{false};
          bool m_alt{false};
      };

      // Applies the side effects of the events a frame consumed -- mouse
      // capture, the Eclipse keyboard notification and the character delivery.
      // Separate from DeriveFrameState so that stays pure and testable.
      void ApplyConsumedEventSideEffects(size_t _consumed, ModifierState _modifiers);

      // Appends one event, or drops it if the queue has hit its bound. The
      // callable fills in the fields the type uses; everything else stays
      // zero. Templated so each call site's lambda inlines away.
      template <typename Fill> void Enqueue(InputEventType _type, Fill _fill)
      {
        if (m_events.size() >= MaxQueuedEvents)
          return;

        InputEvent event;
        event.m_type = _type;
        _fill(event);
        m_events.push_back(event);
      }
  };
} // namespace Neuron
