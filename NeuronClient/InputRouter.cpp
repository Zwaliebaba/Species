#include "pch.h"

#include <algorithm>

#include "Eclipse.h"
#include "InputEvents.h"
#include "InputRouter.h"
#include "TargetCursor.h"


namespace Neuron
{
  void InputRouter::AddSink(InputSink* _sink)
  {
    if (_sink)
      m_sinks.push_back(_sink);
  }


  void InputRouter::RemoveSink(InputSink* _sink) { std::erase(m_sinks, _sink); }


  EventDisposition InputRouter::Dispatch(InputEvent const& _event) const
  {
    for (InputSink* sink : m_sinks)
    {
      if (sink->OnInputEvent(_event) == EventDisposition::Consumed)
        return EventDisposition::Consumed;
    }

    return EventDisposition::Ignored;
  }


  EventDisposition EclipseInputSink::OnInputEvent(InputEvent const& _event)
  {
    switch (_event.m_type)
    {
    case InputEventType::KeyDown:
    case InputEventType::KeyUp:
    {
      const bool down = (_event.m_type == InputEventType::KeyDown);
      if (_event.m_key == KEY_SHIFT)
        m_shift = down;
      else if (_event.m_key == KEY_CONTROL)
        m_control = down;
      else if (_event.m_key == KEY_ALT)
        m_alt = down;

      // NOT for a system key. The old window procedure hooked WM_KEYDOWN and
      // not WM_SYSKEYDOWN, so Eclipse has never been told about Alt+anything;
      // moving the dispatch into a sink is not the change that starts.
      if (down && !_event.m_systemKey)
        EclUpdateKeyboard(_event.m_key, m_shift, m_control, m_alt);

      // IGNORED, ALWAYS, and this is the one place the rule is deliberately
      // weaker than it could be. EclUpdateKeyboard reaches the focused window's
      // Keypress, which returns void and has done since before any of this - so
      // there is no answer to relay, and claiming one would be a guess. Text
      // capture is handled where the evidence actually exists: a CHARACTER the
      // focused field takes, below, which is what T6 established.
      return EventDisposition::Ignored;
    }

    case InputEventType::Char:
      // The focused text field, if there is one, and it says whether it took
      // the character. That answer is what stops the key that produced it from
      // also firing a game binding.
      return EclUpdateChar(_event.m_char) ? EventDisposition::Consumed : EventDisposition::Ignored;

    case InputEventType::MouseButtonDown:
    {
      // THE POSITION COMES FROM THE GAME'S CURSOR, not from the event and not
      // from Windows. TargetCursor does not read the OS pointer: it accumulates
      // the movement control into its own coordinates and warps the OS cursor
      // to match, so its position is the one the player sees under the arrow.
      // AdvanceMenus passed exactly this before the sink existed.
      const bool right = (_event.m_button == MouseButtonRight);
      const bool consumed = EclMouseDown(g_target->X(), g_target->Y(), right);

      if (right)
        m_rightPressConsumed = consumed;
      else
        m_leftPressConsumed = consumed;

      return consumed ? EventDisposition::Consumed : EventDisposition::Ignored;
    }

    case InputEventType::MouseButtonUp:
    {
      const bool right = (_event.m_button == MouseButtonRight);
      EclMouseUp(g_target->X(), g_target->Y(), right);

      // The release follows the press rather than being judged on its own. A
      // window that was clicked and then closed by that very click would
      // otherwise let the release through into the game, which is how a menu
      // button ends up also giving a unit an order.
      const bool consumed = right ? m_rightPressConsumed : m_leftPressConsumed;
      if (right)
        m_rightPressConsumed = false;
      else
        m_leftPressConsumed = false;

      return consumed ? EventDisposition::Consumed : EventDisposition::Ignored;
    }

    case InputEventType::FocusLost:
      // Nothing was clicked, so nothing is owed a release.
      m_leftPressConsumed = false;
      m_rightPressConsumed = false;
      m_shift = m_control = m_alt = false;
      return EventDisposition::Ignored;

    default:
      // Movement, raw movement and the wheel. Eclipse's hover, tooltip and drag
      // handling is a PER-FRAME tick rather than an event handler - a
      // stationary mouse produces no events at all, and a tooltip has to appear
      // after a second of not moving - so it is driven from EclMouseMove in
      // UserInput::AdvanceMenus and there is nothing to do here.
      return EventDisposition::Ignored;
    }
  }
} // namespace Neuron
