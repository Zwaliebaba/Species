#pragma once

//*****************************************************************************
// The input router - one ordering rule where there used to be three hacks
//*****************************************************************************
//
// Every frame's events are offered to a list of SINKS in priority order, UI
// first, and a sink that acts on an event says so. What it said travels back to
// the driver, which stops that control answering the bindings for the rest of
// the frame. That is the whole mechanism, and it replaces three separate
// contrivances that existed because polled state has no way to say "the UI took
// that click":
//
//   * InputManager::suppressEvent, called from UserInput::AdvanceMenus after
//     the fact once it had noticed the click was over a window;
//   * EclUpdateMouse's boolean-to-event synthesis, which reconstructed
//     MouseDown and MouseUp by comparing this frame's polled booleans with last
//     frame's;
//   * the poll-and-push seam in AdvanceMenus, which read ControlEclipse* out of
//     the bindings and pushed the answers into Eclipse.
//
// WHAT DOES NOT SUBSCRIBE, and it is the boundary the whole plan is written
// around: THE SIMULATION. Lockstep determinism requires simulation input to
// reach the wire only through TeamControls and the 10Hz IAmAlive letter, so an
// input handler that mutates game state directly is a desync generator. Sinks
// drive the UI. The camera, unit steering and TeamControls keep reading the
// per-frame state through controlEvent().

#include <vector>


namespace Neuron
{
  // FORWARD DECLARED RATHER THAN INCLUDED, and it is not fastidiousness.
  // Input.h includes this header and is itself included by some thirty files
  // across GameLogic and Species; InputEvents.h brings KeyDefs.h and its
  // hundred KEY_* macros with it, plus NUM_MB and NUM_AXES. Pulling that into
  // every one of those translation units is how a macro named KEY_STOP or
  // NUM_AXES starts silently rewriting somebody else's identifier. A reference
  // in a signature needs no definition.
  struct InputEvent;


  // What a sink did with an event. Consumed means the event has been ACTED ON
  // and must not also reach anything further down; Ignored means the sink did
  // not want it and the next sink, and ultimately the bindings, still may.
  enum class EventDisposition
  {
    Ignored,
    Consumed
  };


  class InputSink
  {
    public:
      virtual ~InputSink() = default;

      // Offered one event. Answer Consumed only when the sink actually did
      // something with it: answering Consumed for an event it merely LOOKED at
      // silently deletes that input from the game.
      virtual EventDisposition OnInputEvent(InputEvent const& _event) = 0;
  };


  class InputRouter
  {
    public:
      // Sinks are offered events in the order they were added, so the first
      // added is the first to get a say. There is one today - the UI - and the
      // order is the design rather than an accident: a click the UI handles
      // must never reach the game.
      void AddSink(InputSink* _sink);
      void RemoveSink(InputSink* _sink);

      EventDisposition Dispatch(InputEvent const& _event) const;

    private:
      std::vector<InputSink*> m_sinks;
  };


  // The UI's sink. Turns events into the Eclipse entry points and reports what
  // Eclipse did with them.
  class EclipseInputSink : public InputSink
  {
    public:
      EventDisposition OnInputEvent(InputEvent const& _event) override;

    private:
      // Tracked from the events themselves rather than read off the frame
      // state, because Eclipse needs the modifiers AS THEY WERE at each
      // keystroke: reading the finished frame would give every key in a frame
      // the same, final answer, so shift-then-letter and letter-then-shift
      // would look identical. The driver used to do this walk itself.
      bool m_shift{false};
      bool m_control{false};
      bool m_alt{false};

      // Did the UI take the press this release belongs to? A click the UI
      // handled is wholly the UI's, so its release is consumed too - otherwise
      // a click that starts on a button finishes in the game.
      bool m_leftPressConsumed{false};
      bool m_rightPressConsumed{false};
  };
} // namespace Neuron
