#pragma once

//*****************************************************************************
// Input events, and the per-frame state derived from them
//*****************************************************************************
//
// The window procedure used to write straight into the driver's state arrays,
// which meant the state WAS the record: anything two messages said about the
// same key in one frame collapsed to whatever the second one left behind. A key
// pressed and released between two frames reported neither edge, because the
// down was overwritten by the up and the up was then indistinguishable from
// nothing having happened.
//
// So messages become a queue of events, and the per-frame state is DERIVED from
// that queue by DeriveFrameState below. The rule that makes an edge survivable
// is one line of it: at most one edge per control per frame, and an event that
// would need a second one is left in the queue for the next frame. Press and
// release inside a single frame now reports a down edge, then an up edge on the
// frame after.
//
// DeriveFrameState is a free function over plain data and touches no Windows
// API, no globals and no window — which is what lets InputEventTests cover the
// rules above on a machine with no window at all.

#include <vector>

#include "KeyDefs.h"

#define NUM_MB 3
#define NUM_AXES 3


namespace Neuron
{
  enum class InputEventType
  {
    KeyDown,
    KeyUp,
    Char,
    MouseButtonDown,
    MouseButtonUp,
    MouseMove,
    Wheel,
    FocusLost
  };

  // One Windows message, decoded. Deliberately a flat struct rather than a
  // variant: it is memcpy-able, it is what a queue of them wants to be, and
  // the fields a given type does not use are simply zero.
  struct InputEvent
  {
      InputEventType m_type{InputEventType::FocusLost};

      // KeyDown, KeyUp. A virtual key code, always < KEY_MAX.
      int m_key{0};

      // KeyDown only. True when Windows is auto-repeating a held key, which the
      // old pipeline could not distinguish from a fresh press at all.
      bool m_repeat{false};

      // KeyDown, KeyUp. True for WM_SYSKEYDOWN/WM_SYSKEYUP — a key pressed with
      // ALT held, or ALT itself. It changes no state derivation; it is carried
      // so the driver can keep NOT forwarding those to Eclipse, which is what
      // the old window procedure did by only hooking WM_KEYDOWN.
      bool m_systemKey{false};

      // Char only. The character Windows decoded for the current keyboard
      // layout — which is the point of carrying it, because a virtual key code
      // is not a character on any layout but US.
      unsigned int m_char{0};

      // MouseButtonDown, MouseButtonUp. An index into the NUM_MB arrays.
      int m_button{0};

      // MouseMove. Client-area pixels.
      int m_x{0};
      int m_y{0};

      // Wheel. The RAW delta from the message, in WHEEL_DELTA units, not
      // detents — a high-resolution wheel sends fractions and the accumulation
      // that turns them into detents is part of the derivation, so that it can
      // be tested.
      int m_wheelDelta{0};
  };


  // What the drivers read. One instance lives in the W32 driver; the tests
  // build their own.
  struct InputFrameState
  {
      signed char m_keys[KEY_MAX]{};      // 1 while held
      signed char m_keyDeltas[KEY_MAX]{}; // +1 pressed this frame, -1 released, 0 neither

      bool m_mb[NUM_MB]{};      // Mouse button held
      int m_mbDeltas[NUM_MB]{}; // +1 / -1 / 0, same convention as the keys

      // X, Y in client pixels; Z is the accumulated wheel position in detents,
      // which is how the wheel has always been expressed to the bindings.
      int m_mousePos[NUM_AXES]{};
      int m_mouseVel[NUM_AXES]{};

      // Wheel movement too small to be a whole detent, carried between frames.
      // Dividing each message's delta on its own threw all of them away on a
      // high-resolution wheel.
      int m_wheelRemainder{0};

      // Characters decoded this frame, in order. Nothing consumes this yet — T6
      // routes it to text input — and it is here rather than in a later task
      // because the event that fills it is produced by the same window procedure.
      std::vector<unsigned int> m_chars;
  };


  // Advances _state by one frame from the front of _events, and returns HOW
  // MANY events it consumed. The caller erases exactly that many and keeps the
  // rest for the next frame.
  //
  // Consumption stops early at the first event that would put a second edge on
  // a control this frame. That is the whole mechanism by which a fast press and
  // release survives, and the reason this returns a count rather than draining.
  //
  // Pure: no globals, no Windows calls, no allocation beyond m_chars.
  size_t DeriveFrameState(InputEvent const* _events, size_t _count, InputFrameState& _state);
} // namespace Neuron
