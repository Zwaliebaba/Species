#pragma once


#include <windows.h>

#include <string>
#include <vector>

#include "InputDriverSimple.h"
#include "InputEvents.h"
#include "KeyDefs.h"

// <windows.h> IS INCLUDED HERE NOW, AND IT HAS TO BE. WndProc below takes
// HWND, UINT, WPARAM and LPARAM and returns LRESULT, and every one of those
// used to arrive through Win32EventProc.h, which T10 deleted along with the
// W32EventProcessor base. Deleting a header withdraws everything it
// included, not just what it declared.


namespace Neuron
{
  // A frame's worth of messages is a handful. This bound exists so a window
  // flooded while the frame loop is stalled — a breakpoint, a long load —
  // cannot grow the queue without limit. Dropping the OLDEST would reorder
  // edges, so the NEWEST are dropped: the state stays consistent with what was
  // delivered, it just stops partway.
  constexpr size_t MaxQueuedEvents = 4096;

  class W32InputDriver : public SimpleInputDriver
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

      // True when mouse movement should be read from the raw deltas rather than
      // from client-position differences. See the definition — it is a claim
      // about packets actually seen, not about the registration succeeding.
      bool UsingRawMouseMovement() const;

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

      // Derives this frame's state from the events the pump has queued. The
      // message pump itself is NOT called from here any more: InputManager
      // runs it once, at the top of its own Advance, and this driver no longer
      // has a PollForEvents at all.
      void Advance();

      // Offers this frame's events to the router, UI first, and hides from the
      // bindings whatever a sink says it consumed. Called after every driver
      // has advanced and after the cursor has moved — see InputDriver.h.
      void DispatchEvents(InputRouter const& router) override;

      // Fill out a description of the input defined by spec
      bool getInputDescription(InputSpec const& spec, InputDescription& desc);

      // Get the name of the driver (debuggung purposes)
      const std::string& getName();

      // Called by W32EventHandler for every message it did not handle itself.
      // Returns 0 if the event is handled here, -1 otherwise.
      //
      // Not an override of anything since T10: W32EventProcessor existed so
      // that the handler could hold a LIST of these, and the list never had
      // more than this one driver in it.
      LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

      // Warp the mouse to a particular position and pretend it has always been there
      void SetMousePosNoVelocity(int _x, int _y);

      // Release every key and button this driver believes is held, reporting an
      // up edge for each. Called by W32EventHandler when the window loses
      // focus, because Windows sends no release for anything held at that
      // moment. Enqueues a FocusLost event like every other message; the
      // release edges are produced by the derivation, in frame order with
      // everything else.
      void OnFocusLost();

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

      // WHAT THE UI TOOK. A control a sink consumed is marked here and stops
      // answering the bindings until it is released — so clicking a button does
      // not also give a unit an order, and typing a name does not fire whatever
      // game control that letter is bound to, including on the way back up.
      //
      // It is deliberately NOT "suppress everything while the UI is up". Only
      // what a sink actually ACTED ON is marked: enter, escape and the arrows
      // produce no character a text field accepts, so they are never marked and
      // still reach the bindings, which is what keeps menu navigation and
      // escape-to-close working while the cursor sits in a text box.
      //
      // The mask is over READS, not over the state: m_keys, m_keyDeltas and
      // m_mb stay truthful underneath it, so nothing is left stuck when a mark
      // clears, and getFirstActiveInput — the key-rebinding window's door — is
      // unaffected. See getMouseInput for why the button mask covers edges only.
      bool m_consumedKeys[KEY_MAX]{};
      bool m_consumedButtons[NUM_MB]{};

      // How many of m_events this frame's derivation took. The events are held
      // rather than erased at the end of Advance, because DispatchEvents walks
      // the same ones later in the frame.
      size_t m_frameEventCount{0};

      // Records a consumed event against the control it belongs to.
      // _lastKeyDown is the key a Char event came from; a character carries no
      // key code of its own.
      void MarkConsumed(InputEvent const& _event, int _lastKeyDown);

      // Has a RELATIVE raw mouse packet ever arrived? Registration succeeding
      // is not enough to answer it — an absolute-reporting device registers
      // fine and then never sends one — and the answer decides which of the two
      // movement sources the bindings are served from.
      bool m_rawMouseMovementSeen{false};

      // Applies the side effects of the events a frame consumed on the WINDOW:
      // mouse capture, and nothing else since the Eclipse calls moved into the
      // router's sink. Separate from DeriveFrameState so that stays pure and
      // testable. The ModifierState this used to carry went with them — the
      // sink tracks shift, control and alt from the events it is handed.
      void ApplyConsumedEventSideEffects(size_t _consumed);

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


  // THE ONE DRIVER, DECLARED WHERE IT IS DEFINED at last. No header used to
  // declare it, so each of its users wrote its own extern — and WindowManager.cpp
  // carries a long comment about why that extern had to sit at namespace scope
  // rather than inside a function, learned from a link error during
  // namespace-migration T2: a BLOCK-SCOPE extern binds to the GLOBAL namespace,
  // so it named ::g_win32InputDriver while the definition was
  // Neuron::g_win32InputDriver. T10 gave the pointer a second user — the window
  // handler reaches the driver through it now that the processor list is gone —
  // and two hand-written externs is one too many to leave uncentralised.
  extern W32InputDriver* g_win32InputDriver;
} // namespace Neuron
