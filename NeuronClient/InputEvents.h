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

// For size_t, in the DeriveFrameState signature below. It used to arrive
// transitively through <vector>, which T6 removed along with the character
// collection that needed it.
#include <cstddef>

#include "KeyDefs.h"

#define NUM_MB 3
#define NUM_AXES 3


namespace Neuron
{
  // The mouse button indices carried in InputEvent::m_button. The driver has
  // always had these as three file-scope L, R and M macros; they are named here
  // so that a reader of an event, in another file, can say which button it is
  // holding without redefining single letters.
  constexpr int MouseButtonLeft = 0;
  constexpr int MouseButtonRight = 1;
  constexpr int MouseButtonMiddle = 2;


  enum class InputEventType
  {
    KeyDown,
    KeyUp,
    Char,
    MouseButtonDown,
    MouseButtonUp,
    MouseMove,
    MouseRawMove,
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

      // MouseButtonDown, MouseButtonUp. An index into the NUM_MB arrays; see
      // MouseButtonLeft and friends below.
      int m_button{0};

      // MouseMove: client-area pixels, an absolute position.
      // MouseRawMove: a RELATIVE step straight off the device, in whatever
      // units it counts in. The two are different quantities in the same
      // fields, and which one a reader is looking at is decided by m_type.
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

      // THIS FRAME'S RELATIVE MOUSE TRAVEL, summed from Raw Input, and the
      // reason it is a separate pair rather than reusing m_mouseVel: they are
      // not the same measurement.
      //
      // m_mouseVel[X] and [Y] are the difference between two CLIENT-AREA
      // positions, so they stop at the edge of the screen — push the mouse
      // right at the right-hand edge and the position cannot go any further, so
      // the velocity reads zero and the camera stops turning while the hand is
      // still moving. They also include Windows' pointer acceleration, and they
      // move when the game WARPS the cursor, which is what the whole
      // suppress-the-velocity dance existed to undo.
      //
      // Raw deltas have none of those properties: they are what the device
      // reported, unclamped, unaccelerated, and silent when a warp moves the
      // pointer without anybody touching it.
      int m_mouseRelative[2]{};

      // Wheel movement too small to be a whole detent, carried between frames.
      // Dividing each message's delta on its own threw all of them away on a
      // high-resolution wheel.
      int m_wheelRemainder{0};

      // NO CHARACTERS HERE. T5 collected them into an m_chars vector as a
      // placeholder for T6, and T6 retired it: a character has to reach the
      // focused widget INTERLEAVED with the key events around it, because
      // typing "ab" and then pressing enter must append both letters before the
      // enter commits the field. A per-frame collection cannot express that
      // ordering, so the driver dispatches Char events from its side-effect
      // walk instead, where they sit in event order with EclUpdateKeyboard.
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
