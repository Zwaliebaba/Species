#include "pch.h"

#include <cstring>

#include "InputEvents.h"


namespace Neuron
{
  namespace
  {
    // Windows reports the wheel in WHEEL_DELTA units. The constant is spelled
    // here rather than included, so this file compiles and tests without
    // windows.h — the whole point of the derivation being separable.
    constexpr int WheelDeltaPerDetent = 120;

    // X and Y are the mouse; Z is the wheel. The same indexing the driver has
    // always used, named so the arithmetic below reads.
    constexpr int AxisX = 0;
    constexpr int AxisY = 1;
    constexpr int AxisWheel = 2;

    bool KeyInRange(int _key) { return _key >= 0 && _key < KEY_MAX; }

    bool ButtonInRange(int _button) { return _button >= 0 && _button < NUM_MB; }
  } // namespace


  size_t DeriveFrameState(InputEvent const* _events, size_t _count, InputFrameState& _state)
  {
    // Everything per-frame resets; everything held persists. m_keys, m_mb,
    // m_mousePos and m_wheelRemainder are state and carry over.
    std::memset(_state.m_keyDeltas, 0, sizeof(_state.m_keyDeltas));
    std::memset(_state.m_mbDeltas, 0, sizeof(_state.m_mbDeltas));
    std::memset(_state.m_mouseRelative, 0, sizeof(_state.m_mouseRelative));

    const int startX = _state.m_mousePos[AxisX];
    const int startY = _state.m_mousePos[AxisY];
    const int startWheel = _state.m_mousePos[AxisWheel];

    size_t consumed = 0;
    for (; consumed < _count; ++consumed)
    {
      InputEvent const& event = _events[consumed];

      switch (event.m_type)
      {
      case InputEventType::KeyDown:
      {
        if (!KeyInRange(event.m_key))
          break;

        // A repeat is not an edge. Windows sends one every few tens of
        // milliseconds while a key is held, and treating each as a press would
        // make every held key look like a machine-gun of down edges — which is
        // exactly what the old code did, minus the ability to tell the
        // difference. The key is already down, so there is nothing to record.
        if (event.m_repeat)
          break;

        // Already edged this frame. Stop here and leave this event, and
        // everything behind it, for the next frame — ordering is preserved
        // because nothing after it is consumed either.
        if (_state.m_keyDeltas[event.m_key] != 0)
          return consumed;

        if (_state.m_keys[event.m_key] != 1)
        {
          _state.m_keys[event.m_key] = 1;
          _state.m_keyDeltas[event.m_key] = 1;
        }
        break;
      }

      case InputEventType::KeyUp:
      {
        if (!KeyInRange(event.m_key))
          break;

        if (_state.m_keyDeltas[event.m_key] != 0)
          return consumed;

        if (_state.m_keys[event.m_key] != 0)
        {
          _state.m_keys[event.m_key] = 0;
          _state.m_keyDeltas[event.m_key] = -1;
        }
        break;
      }

      case InputEventType::Char:
        // Consumed, and it changes no state. A character is not a control: it
        // has no held-or-not, no edge, and nothing in the bindings reads one.
        // It is delivered to the focused widget by the driver's side-effect
        // walk over these same consumed events, which is what keeps it in order
        // with the key events either side of it.
        break;

      case InputEventType::MouseButtonDown:
      {
        if (!ButtonInRange(event.m_button))
          break;

        if (_state.m_mbDeltas[event.m_button] != 0)
          return consumed;

        if (!_state.m_mb[event.m_button])
        {
          _state.m_mb[event.m_button] = true;
          _state.m_mbDeltas[event.m_button] = 1;
        }
        break;
      }

      case InputEventType::MouseButtonUp:
      {
        if (!ButtonInRange(event.m_button))
          break;

        if (_state.m_mbDeltas[event.m_button] != 0)
          return consumed;

        if (_state.m_mb[event.m_button])
        {
          _state.m_mb[event.m_button] = false;
          _state.m_mbDeltas[event.m_button] = -1;
        }
        break;
      }

      case InputEventType::MouseMove:
        // Coalesced by overwriting: the last position in the frame is where the
        // mouse is, and the velocity below is computed against where it was at
        // the start, so intermediate positions add nothing.
        _state.m_mousePos[AxisX] = event.m_x;
        _state.m_mousePos[AxisY] = event.m_y;
        break;

      case InputEventType::MouseRawMove:
        // SUMMED, not overwritten — the opposite of MouseMove above, and for
        // the obvious reason: two absolute positions in one frame mean the
        // second one is where the mouse is, and two relative steps mean it
        // travelled the total of them.
        _state.m_mouseRelative[AxisX] += event.m_x;
        _state.m_mouseRelative[AxisY] += event.m_y;
        break;

      case InputEventType::Wheel:
      {
        _state.m_wheelRemainder += event.m_wheelDelta;

        // Truncation toward zero is what is wanted in both directions: a
        // partial detent stays in the remainder rather than rounding into a
        // scroll that the user did not make.
        const int detents = _state.m_wheelRemainder / WheelDeltaPerDetent;
        if (detents != 0)
        {
          _state.m_wheelRemainder -= detents * WheelDeltaPerDetent;
          _state.m_mousePos[AxisWheel] += detents;
        }
        break;
      }

      case InputEventType::FocusLost:
      {
        // Windows sends no release for anything held when focus goes, so the
        // release edges are produced here. One per key, so the one-edge rule is
        // not violated — but a key that has ALREADY edged this frame would be,
        // so this event defers exactly like the others.
        for (int key = 0; key < KEY_MAX; ++key)
        {
          if (_state.m_keys[key] && _state.m_keyDeltas[key] != 0)
            return consumed;
        }
        for (int button = 0; button < NUM_MB; ++button)
        {
          if (_state.m_mb[button] && _state.m_mbDeltas[button] != 0)
            return consumed;
        }

        for (int key = 0; key < KEY_MAX; ++key)
        {
          if (_state.m_keys[key])
          {
            _state.m_keys[key] = 0;
            _state.m_keyDeltas[key] = -1;
          }
        }
        for (int button = 0; button < NUM_MB; ++button)
        {
          if (_state.m_mb[button])
          {
            _state.m_mb[button] = false;
            _state.m_mbDeltas[button] = -1;
          }
        }

        // A partial scroll is not owed to anyone after focus goes.
        _state.m_wheelRemainder = 0;
        break;
      }
      }
    }

    _state.m_mouseVel[AxisX] = _state.m_mousePos[AxisX] - startX;
    _state.m_mouseVel[AxisY] = _state.m_mousePos[AxisY] - startY;
    _state.m_mouseVel[AxisWheel] = _state.m_mousePos[AxisWheel] - startWheel;

    return consumed;
  }
} // namespace Neuron
