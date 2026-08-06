#include "pch.h"

#include <algorithm>
#include <windows.h>
#include <string>

#include "Eclipse.h"

#include "InputTypes.h"
#include "ControlBindings.h"
#include "Win32EventHandler.h"
#include "InputDriverWin32.h"
#include "InputRouter.h"
#include "KeyNames.h"

#include "WindowManager.h"
#include "WindowManagerWin32.h"
#include "LanguageTable.h"
#include "Debug.h"
#include "AppState.h"

using namespace std;

#define X 0
#define Y 1
#define Z 2

#define L 0
#define R 1
#define M 2


namespace Neuron
{
  enum W32Drivers
  {
    NO_DRIVER,
    KEY_DRIVER,
    MOUSE_DRIVER
  };


  enum MouseControl
  {
    MOUSE_LEFTBUTTON,
    MOUSE_RIGHTBUTTON,
    MOUSE_MIDBUTTON,
    MOUSE_WHEEL,
    MOUSE_MOVEMENT,
    MOUSE_ANY
  };


  W32InputDriver* g_win32InputDriver = nullptr;

  W32InputDriver::W32InputDriver()
  {
    setName("W32");

    // InputFrameState value-initialises every array, so there is nothing to
    // memset. m_events starts empty.
    m_events.reserve(64);

    getW32EventHandler()->AddEventProcessor(this);

    g_win32InputDriver = this;
  }


  W32InputDriver::~W32InputDriver() { getW32EventHandler()->RemoveEventProcessor(this); }


  bool W32InputDriver::getInput(InputSpec const& spec, InputDetails& details)
  {
    switch (spec.handler_id)
    {
    case KEY_DRIVER:
      return getKeyInput(spec, details);
    case MOUSE_DRIVER:
      return getMouseInput(spec, details);
    default:
      return false; // Should never get here!
    }
  }


  bool W32InputDriver::getKeyInput(InputSpec const& spec, InputDetails& details)
  {
    int button = spec.control_id;
    DEBUG_ASSERT(0 <= button && button < KEY_MAX);
    details.type = InputType::INPUT_TYPE_BOOL;

    // The UI took this one. See m_consumedKeys — the state underneath is
    // untouched and still says what the keyboard is doing; this is the read
    // being answered as though the key were not there.
    if (m_consumedKeys[button])
      return false;

    // spec.condition is a driver-defined int; this driver reads it as an
    // InputCondition, which is what the cast says. See InputSpec.h.
    switch (static_cast<InputCondition>(spec.condition))
    {
    case InputCondition::COND_DOWN:
      return (m_state.m_keyDeltas[button] == 1);
    case InputCondition::COND_UP:
      return (m_state.m_keyDeltas[button] == -1);
    case InputCondition::COND_PRESSED:
      return (m_state.m_keys[button] == 1);
    default:
      return false; // We should never get here!
    }
  }


  bool W32InputDriver::getMouseInput(InputSpec const& spec, InputDetails& details)
  {
    int button = -1;

    switch (spec.control_id)
    {
    case MOUSE_LEFTBUTTON:
      button = L;
      details.type = InputType::INPUT_TYPE_BOOL;
      break;
    case MOUSE_RIGHTBUTTON:
      button = R;
      details.type = InputType::INPUT_TYPE_BOOL;
      break;
    case MOUSE_MIDBUTTON:
      button = M;
      details.type = InputType::INPUT_TYPE_BOOL;
      break;
    case MOUSE_WHEEL:
      details.type = InputType::INPUT_TYPE_1D;
      break;
    case MOUSE_MOVEMENT:
      details.type = InputType::INPUT_TYPE_2D;
      break;
    default:
      return false; // We should never get here!
    }

    if (button >= 0)
    {
      // A CONSUMED BUTTON HIDES ITS EDGES AND NOT ITS HELD STATE, which is the
      // one place this mask is narrower than the key mask, and deliberately.
      // What the UI consumes is a CLICK, and a click is an edge — while UI code
      // itself still polls `mouse left pressed` to know the button is down over
      // it. EclipseLMousePressed is read that way by the landscape editor, the
      // input scrollers and the profile window, and blanking it would break
      // every one of them. It matches the suppression this replaces:
      // suppressEvent named ControlEclipseLMouseDown alone and left
      // ControlEclipseLMousePressed answering.
      const bool consumed = m_consumedButtons[button];

      switch (static_cast<InputCondition>(spec.condition))
      {
      case InputCondition::COND_DOWN:
        return !consumed && (m_state.m_mbDeltas[button] == 1);
      case InputCondition::COND_UP:
        return !consumed && (m_state.m_mbDeltas[button] == -1);
      case InputCondition::COND_PRESSED:
        return (m_state.m_mb[button]);
      default:
        return false; // We should never get here!
      }
    }

    bool reading = false;
    switch (static_cast<InputCondition>(spec.condition))
    {
    case InputCondition::COND_READ:
      reading = true;
      [[fallthrough]];
    case InputCondition::COND_MOVED:
      if (MOUSE_MOVEMENT == spec.control_id)
      {
        // RAW WHEN THERE IS RAW, and the difference is what this task exists
        // for: a client-position difference saturates at the edge of the
        // screen, so pushing the mouse right with the pointer already against
        // the right-hand edge produced zero and the camera stopped turning
        // while the hand kept moving. Raw deltas are unclamped.
        //
        // The fallback is not decoration. RegisterRawInputDevices can fail, and
        // a device can report absolute coordinates instead of relative — a
        // tablet, some remote-desktop stacks — in which case no relative packet
        // ever arrives. Without a fallback either of those leaves the game with
        // no mouse aim whatsoever, which is a worse failure than the saturation
        // this replaces.
        if (UsingRawMouseMovement())
        {
          details.x = m_state.m_mouseRelative[X];
          details.y = m_state.m_mouseRelative[Y];
        }
        else
        {
          details.x = m_state.m_mouseVel[X];
          details.y = m_state.m_mouseVel[Y];
        }
        return reading || (details.x != 0 || details.y != 0);
      }
      else if (MOUSE_WHEEL == spec.control_id)
      {
        details.x = m_state.m_mouseVel[Z];
        return reading || (details.x != 0);
      }
      else
        return false; // We should never get here!

    default:
      return false; // We should never get here!
    }
  }


  // Is the mouse's motion coming from the device rather than from client-area
  // position differences? True once a relative raw packet has actually been
  // seen, which is a stronger claim than "the registration succeeded": a device
  // that reports ABSOLUTE coordinates registers fine and then never sends one.
  bool W32InputDriver::UsingRawMouseMovement() const { return m_rawMouseMovementSeen && g_windowManager->RawMouseInput(); }


  bool W32InputDriver::isIdle()
  {
    static const InputFrameState idle;

    // MOVEMENT IS READ WHEREVER MOVEMENT IS COMING FROM, and on the raw path
    // that is an improvement rather than a translation: m_mouseVel moves when
    // the GAME warps the cursor, which it does every frame in the camera modes,
    // so this could only ever have answered "idle" in a menu. A raw delta
    // arrives when a hand moves the mouse and at no other time.
    bool mouseStill = memcmp(idle.m_mouseVel, m_state.m_mouseVel, sizeof(idle.m_mouseVel)) == 0;
    if (UsingRawMouseMovement())
    {
      // The wheel still comes from m_mouseVel[Z]; only X and Y move over.
      mouseStill =
        memcmp(idle.m_mouseRelative, m_state.m_mouseRelative, sizeof(idle.m_mouseRelative)) == 0 && idle.m_mouseVel[Z] == m_state.m_mouseVel[Z];
    }

    return memcmp(idle.m_keys, m_state.m_keys, sizeof(idle.m_keys)) == 0 &&
           memcmp(idle.m_keyDeltas, m_state.m_keyDeltas, sizeof(idle.m_keyDeltas)) == 0 && memcmp(idle.m_mb, m_state.m_mb, sizeof(idle.m_mb)) == 0 &&
           mouseStill;
  }


  InputMode W32InputDriver::getInputMode() { return InputMode::INPUT_MODE_KEYBOARD; }


  void W32InputDriver::OnFocusLost()
  {
    // Windows sends no WM_KEYUP or WM_LBUTTONUP for anything held when focus
    // goes, so without this the game keeps believing those are down — which is
    // what the ALT special case in the event handler used to paper over, for
    // ALT alone.
    //
    // An EVENT, in the queue, in order with the messages around it. The
    // previous version wrote release edges straight into the state and had to
    // explain at length which array it was safe to write them into; now it is
    // the derivation's problem, and the derivation defers focus loss behind an
    // edge made in the same frame rather than overwriting it.
    Enqueue(InputEventType::FocusLost, [](InputEvent&) {});
  }


  void W32InputDriver::SetMousePosNoVelocity(int _x, int _y)
  {
    // THE POSITION IS ALL THIS STILL OWES ON THE RAW PATH. Raw deltas come from
    // the device, so a warp — and the camera modes warp the cursor every single
    // frame — produces none, and there is no phantom travel to suppress. The
    // queue surgery and the zeroed velocity below are load-bearing ONLY for the
    // WM_MOUSEMOVE fallback, where a queued MouseMove from before the warp
    // would report travel from the position this call just overwrote.
    if (!UsingRawMouseMovement())
    {
      // ONLY WHAT IS STILL QUEUED, never the events the current frame has
      // already derived: those are waiting for DispatchEvents to walk them, and
      // erasing from under it would shift the window it walks. The first
      // m_frameEventCount entries are the frame's, and they are past being a
      // warp's problem anyway — the phantom travel this suppresses is in the
      // messages that have NOT been read yet.
      const auto queued = m_events.begin() + static_cast<std::ptrdiff_t>(m_frameEventCount);
      m_events.erase(std::remove_if(queued, m_events.end(), [](InputEvent const& _event) { return _event.m_type == InputEventType::MouseMove; }),
                     m_events.end());

      m_state.m_mouseVel[X] = 0;
      m_state.m_mouseVel[Y] = 0;
    }

    m_state.m_mousePos[X] = _x;
    m_state.m_mousePos[Y] = _y;
  }


  bool W32InputDriver::getFirstActiveInput(InputSpec& spec, bool instant)
  {
    // Check for pressed mouse buttons
    // for ( unsigned i = 0; i < NUM_MB; ++i ) {
    //	if ( 1 == m_mbDeltas[i] ) { // Mouse button pressed
    //		spec.handler_id = MOUSE_DRIVER;
    //		switch ( i ) {
    //			case L: spec.control_id = MOUSE_LEFTBUTTON; break;
    //			case R: spec.control_id = MOUSE_RIGHTBUTTON; break;
    //			case M: spec.control_id = MOUSE_MIDBUTTON; break;
    //			default: return false; // We should never get here!
    //		}
    //		spec.condition = InputCondition::COND_DOWN;
    //		spec.type = INPUT_TYPE_BOOL;
    //		return true;
    //	}
    //}

    // Check for pressed keys
    for (unsigned i = 0; i < KEY_TILDE; ++i)
    {
      if (1 == m_state.m_keyDeltas[i] && // key was pressed
          strstr(getKeyNames()[i], " ") == nullptr)
      { // Key is bindable
        spec.handler_id = KEY_DRIVER;
        spec.control_id = i;
        if (instant)
          spec.condition = static_cast<condition_t>(InputCondition::COND_DOWN);
        else
          spec.condition = static_cast<condition_t>(InputCondition::COND_PRESSED);
        spec.type = InputType::INPUT_TYPE_BOOL;
        return true;
      }
    }

    // We found no active inputs
    return false;

  } // End of getFirstActiveInput


  void W32InputDriver::Advance()
  {
    // A control the UI took stays taken until it comes back up. This runs
    // BEFORE the derivation, so it reads the PREVIOUS frame's held state: a key
    // released last frame had its up edge suppressed last frame, and clearing
    // the mark now is the first moment at which nothing is left to hide.
    for (int key = 0; key < KEY_MAX; ++key)
    {
      if (m_consumedKeys[key] && m_state.m_keys[key] == 0)
        m_consumedKeys[key] = false;
    }
    for (int button = 0; button < NUM_MB; ++button)
    {
      if (m_consumedButtons[button] && !m_state.m_mb[button])
        m_consumedButtons[button] = false;
    }

    // Pump first, so everything Windows has for us is in the queue, then
    // derive one frame from it. What the frame cannot represent stays queued
    // and is the first thing the next frame sees.
    PollForEvents();

    m_frameEventCount = DeriveFrameState(m_events.data(), m_events.size(), m_state);
    ApplyConsumedEventSideEffects(m_frameEventCount);

    // THE EVENTS ARE NOT ERASED HERE ANY MORE. DispatchEvents still has to walk
    // them, and it runs later in the frame — after the cursor has moved — so
    // they are held until then. See DispatchEvents.
  }


  // The side effects a consumed event has on the WINDOW rather than on the
  // game: mouse capture, and nothing else since the Eclipse calls moved to the
  // router's sink. Separate from DeriveFrameState so that stays pure and
  // testable.
  void W32InputDriver::ApplyConsumedEventSideEffects(size_t _consumed)
  {
    for (size_t i = 0; i < _consumed; ++i)
    {
      switch (m_events[i].m_type)
      {
      case InputEventType::MouseButtonDown:
        // Capture at frame time rather than message time costs nothing: this
        // runs immediately after the pump, so no further message is delivered
        // in between, and capture only affects messages still to come.
        g_windowManager->CaptureMouse();
        break;

      case InputEventType::MouseButtonUp:
        g_windowManager->UncaptureMouse();
        break;

      default:
        break;
      }
    }
  }


  // THE UI GETS FIRST REFUSAL, and what it takes is then invisible to the
  // bindings for as long as the control is held. This is the whole of the
  // consume semantics: everything that used to be suppressEvent, the
  // AdvanceMenus poll-and-push, and T6's text capture is now this one loop.
  void W32InputDriver::DispatchEvents(InputRouter const& _router)
  {
    // Which key produced the character that is about to arrive. Windows sends
    // WM_CHAR from TranslateMessage on the WM_KEYDOWN before it, so the most
    // recent key down in this walk IS the one — and it is the key the mark has
    // to land on, because a character carries no key code of its own.
    int lastKeyDown = -1;

    for (size_t i = 0; i < m_frameEventCount; ++i)
    {
      InputEvent const& event = m_events[i];

      if (event.m_type == InputEventType::KeyDown && event.m_key >= 0 && event.m_key < KEY_MAX)
        lastKeyDown = event.m_key;

      if (_router.Dispatch(event) == EventDisposition::Consumed)
        MarkConsumed(event, lastKeyDown);
    }

    m_events.erase(m_events.begin(), m_events.begin() + static_cast<std::ptrdiff_t>(m_frameEventCount));
    m_frameEventCount = 0;
  }


  void W32InputDriver::MarkConsumed(InputEvent const& _event, int _lastKeyDown)
  {
    switch (_event.m_type)
    {
    case InputEventType::KeyDown:
    case InputEventType::KeyUp:
      if (_event.m_key >= 0 && _event.m_key < KEY_MAX)
        m_consumedKeys[_event.m_key] = true;
      break;

    case InputEventType::Char:
      // A character carries no key of its own, so the mark lands on the key
      // that produced it — which is why the walk above tracks the last key
      // down at all.
      if (_lastKeyDown >= 0)
        m_consumedKeys[_lastKeyDown] = true;
      break;

    case InputEventType::MouseButtonDown:
    case InputEventType::MouseButtonUp:
      if (_event.m_button >= 0 && _event.m_button < NUM_MB)
        m_consumedButtons[_event.m_button] = true;
      break;

    default:
      // Movement and the wheel are not consumable: no sink acts on them, and
      // there is no edge to hide even if one did.
      break;
    }
  }


  void W32InputDriver::PollForEvents() { g_windowManager->NastyPollForMessages(); }


  // ENQUEUE ONLY. Nothing here reads or writes the frame state, calls into
  // Eclipse, or touches the window -- all of that moved to Advance, where it
  // happens in event order with everything else. That is what makes a key
  // pressed and released between two frames survive: two messages become two
  // records rather than one overwriting the other.
  //
  // hWnd is unused because this driver never needed it; the event handler owns
  // the window.
  LRESULT CALLBACK W32InputDriver::WndProc(HWND, UINT message, WPARAM wParam, LPARAM lParam)
  {
    switch (message)
    {
    case WM_LBUTTONDOWN:
      Enqueue(InputEventType::MouseButtonDown, [](InputEvent& _e) { _e.m_button = L; });
      break;
    case WM_LBUTTONUP:
      Enqueue(InputEventType::MouseButtonUp, [](InputEvent& _e) { _e.m_button = L; });
      break;
    case WM_MBUTTONDOWN:
      Enqueue(InputEventType::MouseButtonDown, [](InputEvent& _e) { _e.m_button = M; });
      break;
    case WM_MBUTTONUP:
      Enqueue(InputEventType::MouseButtonUp, [](InputEvent& _e) { _e.m_button = M; });
      break;
    case WM_RBUTTONDOWN:
      Enqueue(InputEventType::MouseButtonDown, [](InputEvent& _e) { _e.m_button = R; });
      break;
    case WM_RBUTTONUP:
      Enqueue(InputEventType::MouseButtonUp, [](InputEvent& _e) { _e.m_button = R; });
      break;

    case WM_MOUSEWHEEL:
    {
      // The RAW delta. Turning it into detents needs the remainder carried
      // between messages, and that accumulation is part of the derivation so
      // that it can be tested — a high-resolution wheel sends fractions of
      // WHEEL_DELTA and dividing each one on its own threw all of them away.
      const int raw = GET_WHEEL_DELTA_WPARAM(wParam);
      Enqueue(InputEventType::Wheel, [raw](InputEvent& _e) { _e.m_wheelDelta = raw; });
      break;
    }

    case WM_INPUT:
    {
      // The mouse, as the device reports it: a relative step, unclamped by the
      // screen and untouched by the pointer acceleration Windows applies on the
      // way to WM_MOUSEMOVE.
      RAWINPUT raw;
      UINT size = sizeof(raw);
      if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) != static_cast<UINT>(-1) &&
          raw.header.dwType == RIM_TYPEMOUSE && (raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0)
      {
        // A device that reports ABSOLUTE coordinates — a tablet, some
        // remote-desktop stacks — is skipped rather than differenced, and that
        // is what m_rawMouseMovementSeen is for: until a RELATIVE packet has
        // actually arrived, the driver keeps aiming from WM_MOUSEMOVE, so such
        // a device is left with the aim it always had rather than with none.
        const int dx = raw.data.mouse.lLastX;
        const int dy = raw.data.mouse.lLastY;
        if (dx != 0 || dy != 0)
        {
          m_rawMouseMovementSeen = true;
          Enqueue(InputEventType::MouseRawMove,
                  [dx, dy](InputEvent& _e)
                  {
                    _e.m_x = dx;
                    _e.m_y = dy;
                  });
        }
      }

      // DELIBERATELY REPORTED AS NOT PROCESSED, which is what routes it on to
      // DefWindowProc — the system needs to see WM_INPUT to clean the raw input
      // up after us. This is the one message here that is both read and passed
      // along.
      return -1;
    }

    case WM_MOUSEMOVE:
    {
      const short newPosX = lParam & 0xFFFF;
      const short newPosY = short(lParam >> 16);
      Enqueue(InputEventType::MouseMove,
              [newPosX, newPosY](InputEvent& _e)
              {
                _e.m_x = newPosX;
                _e.m_y = newPosY;
              });
      break;
    }

    case WM_CHAR:
    {
      // Windows has already applied the keyboard layout and the dead keys, so
      // this is the character the user typed rather than a guess from a virtual
      // key code. The window class is registered with RegisterClassA, so it is
      // one byte in the active code page — the same encoding the edit buffers
      // and the 8-bit font downstream of them use.
      const unsigned int character = static_cast<unsigned int>(wParam);
      Enqueue(InputEventType::Char, [character](InputEvent& _e) { _e.m_char = character; });
      break;
    }

    case WM_SYSCHAR:
      // SWALLOWED, AND THE EMPTY BODY IS THE POINT: it enqueues nothing and
      // returns 0, so this never reaches DefWindowProc. Alt+key is a game
      // binding here and never text, and the default handling of a WM_SYSCHAR
      // with no menu to open is the system beep — one per keystroke for every
      // Alt combination the game binds.
      break;

    case WM_SYSKEYUP:
    case WM_KEYUP:
    {
      const int key = static_cast<int>(wParam);
      const bool systemKey = (message == WM_SYSKEYUP);
      Enqueue(InputEventType::KeyUp,
              [key, systemKey](InputEvent& _e)
              {
                _e.m_key = key;
                _e.m_systemKey = systemKey;
              });
      break;
    }

    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
    {
      const int key = static_cast<int>(wParam);

      // Bit 30 of lParam is set when the key was ALREADY down, which is
      // Windows auto-repeating. The old pipeline had no way to express the
      // difference and treated every repeat as a fresh press.
      const bool repeat = (lParam & (1 << 30)) != 0;

      // Alt-F4 still quits, and it is still handled here rather than as a
      // binding: it must work with the queue full, the frame loop stalled, or
      // the game refusing to advance.
      if (message == WM_SYSKEYDOWN && key == KEY_F4 && (HIWORD(lParam) & KF_ALTDOWN))
        g_requestQuit = true;

      const bool systemKey = (message == WM_SYSKEYDOWN);
      Enqueue(InputEventType::KeyDown,
              [key, repeat, systemKey](InputEvent& _e)
              {
                _e.m_key = key;
                _e.m_repeat = repeat;
                _e.m_systemKey = systemKey;
              });
      break;
    }

    default:
      return -1; // Not processed
    }

    return 0; // Processed

  } // End of WndProc


  bool W32InputDriver::acceptDriver(string const& name)
  {
    if (name == "key")
      lastAcceptedDriver = KEY_DRIVER;
    else if (name == "mouse")
      lastAcceptedDriver = MOUSE_DRIVER;
    else
      lastAcceptedDriver = NO_DRIVER;
    return (lastAcceptedDriver != NO_DRIVER);
  }


  control_id_t W32InputDriver::getControlID(string const& name)
  {
    switch (lastAcceptedDriver)
    {
    case KEY_DRIVER:
      return getKeyId(name);

    case MOUSE_DRIVER:
      if (name == "axes")
        return MOUSE_MOVEMENT;
      if (name == "left")
        return MOUSE_LEFTBUTTON;
      if (name == "right")
        return MOUSE_RIGHTBUTTON;
      if (name == "middle")
        return MOUSE_MIDBUTTON;
      if (name == "wheel")
        return MOUSE_WHEEL;
      if (name == "any")
        return MOUSE_ANY;

    default:
      return -1; // Error
    }
  }


  InputParserState W32InputDriver::writeExtraSpecInfo(InputSpec& spec)
  {
    spec.handler_id = lastAcceptedDriver;
    return InputParserState::STATE_DONE;
  }


  inputtype_t W32InputDriver::getControlType(control_id_t control_id)
  {
    switch (lastAcceptedDriver)
    {
    case KEY_DRIVER:
      return InputType::INPUT_TYPE_BOOL;

    case MOUSE_DRIVER:
      return getMouseControlType(control_id);

    default:
      // Should never get here!
      //
      // THE -1 IS PRESERVED EXACTLY RATHER THAN MAPPED TO INPUT_TYPE_FAIL, and
      // the difference is not cosmetic. InputType is a bit field: -1 is every
      // bit SET, so a `(type & x) == x` test accepts everything, while
      // INPUT_TYPE_FAIL is zero and accepts nothing. Those are opposites at the
      // one place this value is read -- it reaches
      // InputDriver::getDefaultConditionID through InputSpec::type, which tests
      // it exactly that way.
      //
      // So this is an integer boundary, declared as one by language-hygiene
      // T10 rather than quietly given a nicer value. Whether -1 or FAIL is the
      // RIGHT answer on an unreachable path is a separate question with no
      // evidence behind it either way; see the task's notes.
      return static_cast<InputType>(-1);
    }
  }


  inputtype_t W32InputDriver::getMouseControlType(control_id_t control_id)
  {
    switch (control_id)
    {
    case MOUSE_MOVEMENT:
      return InputType::INPUT_TYPE_2D;
    case MOUSE_LEFTBUTTON:
      return InputType::INPUT_TYPE_BOOL;
    case MOUSE_RIGHTBUTTON:
      return InputType::INPUT_TYPE_BOOL;
    case MOUSE_MIDBUTTON:
      return InputType::INPUT_TYPE_BOOL;
    case MOUSE_WHEEL:
      return InputType::INPUT_TYPE_1D;
    case MOUSE_ANY:
      return InputType::INPUT_TYPE_BOOL;
    default:
      // Should never get here! Same preserved -1 as getControlType above, and
      // for the same reason — see the comment there.
      return static_cast<InputType>(-1);
    }
  }


  control_id_t W32InputDriver::getKeyId(string const& keyName)
  {
    for (control_id_t i = 0; i <= KEY_TILDE; ++i)
    {
      if (stricmp(keyName.c_str(), getKeyNames()[i]) == 0)
        return i;
    }

    if (stricmp(keyName.c_str(), "any") == 0)
      return KEY_ANY;

    return -1;
  }


  bool W32InputDriver::getInputDescription(InputSpec const& spec, InputDescription& desc)
  {
    if (KEY_DRIVER == spec.handler_id)
    {
      bool keyInRange = (0 <= spec.control_id && spec.control_id <= KEY_TILDE);
      if (keyInRange)
      {
        desc.noun = "control_";
        desc.noun += getKeyNames()[spec.control_id];
        if (ISLANGUAGEPHRASE(desc.noun.c_str()))
          desc.noun = RAWLANGUAGEPHRASE(desc.noun.c_str());
        else
          desc.noun = getKeyNames()[spec.control_id];
      }
      else
        desc.noun = "control_key_unknown";

      std::string prefCond;
      if (getDefaultVerb(spec.condition, InputType::INPUT_TYPE_BOOL, desc.verb) &&
          getDefaultPrefsString(spec.condition, InputType::INPUT_TYPE_BOOL, prefCond))
      {
        desc.pref = "key ";
        desc.pref += desc.noun + " " + prefCond;
        return keyInRange;
      }
      else
      {
        desc.pref = "unknown_key_pref_string";
        return false;
      }
    }
    else if (MOUSE_DRIVER == spec.handler_id)
    {
      bool ret = true;
      inputtype_t type = InputType::INPUT_TYPE_BOOL;
      desc.pref = "mouse ";
      switch (spec.control_id)
      {
      case MOUSE_LEFTBUTTON:
        desc.noun = "control_mouse_left_button";
        desc.pref += "left ";
        break;
      case MOUSE_RIGHTBUTTON:
        desc.noun = "control_mouse_right_button";
        desc.pref += "right ";
        break;
      case MOUSE_MIDBUTTON:
        desc.noun = "control_mouse_mid_button";
        desc.pref += "middle ";
        break;
      case MOUSE_WHEEL:
        desc.noun = "control_mouse_wheel";
        desc.pref += "wheel ";
        type = InputType::INPUT_TYPE_1D;
        break;
      case MOUSE_MOVEMENT:
        desc.noun = "control_mouse";
        desc.pref += "axes ";
        type = InputType::INPUT_TYPE_2D;
        break;
      default:
        desc.noun = "control_mouse_unknown";
        desc.pref += "unknown ";
        ret = false;
      }

      std::string mouseCondition;
      if (MOUSE_WHEEL == spec.control_id)
      {
        if (InputCondition::COND_MOVED == static_cast<InputCondition>(spec.condition))
        {
          desc.verb = "control_verb_scroll";
          getDefaultPrefsString(spec.condition, type, mouseCondition);
          desc.pref += mouseCondition;
          return true;
        }
        else
        {
          desc.verb = "control_verb_unknown";
          desc.pref += "unknown";
          return false;
        }
      }
      else
      {
        if (getDefaultVerb(spec.condition, type, desc.verb) && getDefaultPrefsString(spec.condition, type, mouseCondition))
        {
          desc.pref += mouseCondition;
          return ret;
        }
        else
          return false;
      }
    }
    else
    { // Shouldn't be here!
      desc.noun = "control_driver_unknown";
      desc.verb = "control_driver_unknown";
      desc.pref = "control_driver_unknown";
      return false;
    }
  }
} // namespace Neuron
