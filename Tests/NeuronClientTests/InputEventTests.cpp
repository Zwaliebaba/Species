#include "pch.h"

#include <vector>

#include "InputEvents.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Neuron;

// DeriveFrameState is the whole reason input events exist as a separate file:
// it is the rule that decides what a frame saw, it is where the information the
// old pipeline threw away is preserved, and it touches no window, no Windows API
// and no global — so it can be tested here rather than only by playing the game.
//
// The case that named the task is FastKeyPressAndRelease below. A key pressed and
// released between two frames used to report NEITHER edge: the window procedure
// wrote a +1 into m_keyNewDeltas and then a -1 over the top of it, and the -1 was
// indistinguishable from a key that had been up all along.
namespace NeuronClientTests
{
  namespace
  {
    InputEvent KeyEvent(InputEventType _type, int _key, bool _repeat = false)
    {
      InputEvent event;
      event.m_type = _type;
      event.m_key = _key;
      event.m_repeat = _repeat;
      return event;
    }

    InputEvent ButtonEvent(InputEventType _type, int _button)
    {
      InputEvent event;
      event.m_type = _type;
      event.m_button = _button;
      return event;
    }

    InputEvent MoveEvent(int _x, int _y)
    {
      InputEvent event;
      event.m_type = InputEventType::MouseMove;
      event.m_x = _x;
      event.m_y = _y;
      return event;
    }

    InputEvent WheelEvent(int _rawDelta)
    {
      InputEvent event;
      event.m_type = InputEventType::Wheel;
      event.m_wheelDelta = _rawDelta;
      return event;
    }

    InputEvent CharEvent(unsigned int _character)
    {
      InputEvent event;
      event.m_type = InputEventType::Char;
      event.m_char = _character;
      return event;
    }

    InputEvent FocusLostEvent()
    {
      InputEvent event;
      event.m_type = InputEventType::FocusLost;
      return event;
    }

    // One frame. Consumes what the derivation accepted and leaves the rest,
    // exactly as the driver does.
    size_t Frame(std::vector<InputEvent>& _queue, InputFrameState& _state)
    {
      const size_t consumed = DeriveFrameState(_queue.data(), _queue.size(), _state);
      _queue.erase(_queue.begin(), _queue.begin() + consumed);
      return consumed;
    }

    constexpr int TestKeyA = 65;
    constexpr int TestKeyB = 66;
    constexpr int WheelDeltaPerDetent = 120;
  } // namespace


  TEST_CLASS(InputEventTests)
  {
    public:
      // THE TASK'S NAMED CASE. Down on one frame, up on the next, from a press
      // and release that both arrived before a single frame ran.
      TEST_METHOD(FastKeyPressAndReleaseReportsBothEdgesOnConsecutiveFrames)
      {
        InputFrameState state;
        std::vector<InputEvent> queue{KeyEvent(InputEventType::KeyDown, TestKeyA), KeyEvent(InputEventType::KeyUp, TestKeyA)};

        Assert::AreEqual(size_t(1), Frame(queue, state), L"the up must not be consumed in the same frame as the down");
        Assert::AreEqual(1, static_cast<int>(state.m_keyDeltas[TestKeyA]), L"frame 1 reports the down edge");
        Assert::AreEqual(1, static_cast<int>(state.m_keys[TestKeyA]), L"and the key reads as held");

        Assert::AreEqual(size_t(1), Frame(queue, state));
        Assert::AreEqual(-1, static_cast<int>(state.m_keyDeltas[TestKeyA]), L"frame 2 reports the up edge");
        Assert::AreEqual(0, static_cast<int>(state.m_keys[TestKeyA]), L"and the key reads as released");
      }

      TEST_METHOD(AHeldKeyEdgesOnceAndThenStaysDown)
      {
        InputFrameState state;
        std::vector<InputEvent> queue{KeyEvent(InputEventType::KeyDown, TestKeyA)};

        Frame(queue, state);
        Assert::AreEqual(1, static_cast<int>(state.m_keyDeltas[TestKeyA]));

        Frame(queue, state);
        Assert::AreEqual(0, static_cast<int>(state.m_keyDeltas[TestKeyA]), L"no second edge from holding");
        Assert::AreEqual(1, static_cast<int>(state.m_keys[TestKeyA]), L"still held");
      }

      // Windows repeats a held key every few tens of milliseconds. The old
      // pipeline could not tell a repeat from a fresh press, because the
      // message carried the distinction and the state did not.
      TEST_METHOD(AutoRepeatIsNotAnEdge)
      {
        InputFrameState state;
        std::vector<InputEvent> queue{KeyEvent(InputEventType::KeyDown, TestKeyA), KeyEvent(InputEventType::KeyDown, TestKeyA, true),
                                      KeyEvent(InputEventType::KeyDown, TestKeyA, true)};

        Assert::AreEqual(size_t(3), Frame(queue, state), L"repeats do not defer anything");
        Assert::AreEqual(1, static_cast<int>(state.m_keyDeltas[TestKeyA]), L"one edge, not three");
      }

      // Stopping early must not reorder: an unrelated key behind the deferred
      // event waits with it rather than jumping the queue.
      TEST_METHOD(DeferringAnEdgeDefersEverythingBehindIt)
      {
        InputFrameState state;
        std::vector<InputEvent> queue{KeyEvent(InputEventType::KeyDown, TestKeyA), KeyEvent(InputEventType::KeyUp, TestKeyA),
                                      KeyEvent(InputEventType::KeyDown, TestKeyB)};

        Assert::AreEqual(size_t(1), Frame(queue, state));
        Assert::AreEqual(0, static_cast<int>(state.m_keyDeltas[TestKeyB]), L"B has not been seen yet");

        Assert::AreEqual(size_t(2), Frame(queue, state));
        Assert::AreEqual(-1, static_cast<int>(state.m_keyDeltas[TestKeyA]));
        Assert::AreEqual(1, static_cast<int>(state.m_keyDeltas[TestKeyB]));
      }

      TEST_METHOD(AClickInsideOneFrameReportsBothEdges)
      {
        InputFrameState state;
        std::vector<InputEvent> queue{ButtonEvent(InputEventType::MouseButtonDown, 0), ButtonEvent(InputEventType::MouseButtonUp, 0)};

        Assert::AreEqual(size_t(1), Frame(queue, state));
        Assert::AreEqual(1, state.m_mbDeltas[0]);
        Assert::IsTrue(state.m_mb[0]);

        Frame(queue, state);
        Assert::AreEqual(-1, state.m_mbDeltas[0]);
        Assert::IsFalse(state.m_mb[0]);
      }

      TEST_METHOD(MouseMovesCoalesceAndVelocityIsMeasuredAcrossTheFrame)
      {
        InputFrameState state;
        std::vector<InputEvent> queue{MoveEvent(10, 10), MoveEvent(20, 30), MoveEvent(25, 35)};

        Frame(queue, state);
        Assert::AreEqual(25, state.m_mousePos[0], L"the last position in the frame is where the mouse is");
        Assert::AreEqual(35, state.m_mousePos[1]);
        Assert::AreEqual(25, state.m_mouseVel[0], L"velocity is the whole frame's travel, not the last step");
        Assert::AreEqual(35, state.m_mouseVel[1]);

        queue.push_back(MoveEvent(30, 35));
        Frame(queue, state);
        Assert::AreEqual(5, state.m_mouseVel[0], L"velocity is per-frame");
        Assert::AreEqual(0, state.m_mouseVel[1]);
      }

      // A high-resolution wheel sends fractions of a detent. Dividing each
      // message on its own threw every one of them away, so such a wheel
      // scrolled nothing until a message happened to carry a whole detent.
      TEST_METHOD(PartialWheelDeltasAccumulateIntoOneDetent)
      {
        InputFrameState state;
        const int twelfth = WheelDeltaPerDetent / 12;

        for (int i = 0; i < 11; ++i)
        {
          InputEvent event = WheelEvent(twelfth);
          DeriveFrameState(&event, 1, state);
        }
        Assert::AreEqual(0, state.m_mousePos[2], L"eleven twelfths is not a scroll");

        InputEvent event = WheelEvent(twelfth);
        DeriveFrameState(&event, 1, state);
        Assert::AreEqual(1, state.m_mousePos[2], L"the twelfth completes exactly one detent");
        Assert::AreEqual(1, state.m_mouseVel[2]);
      }

      TEST_METHOD(AWholeWheelDetentBackwardsScrollsOneBackwards)
      {
        InputFrameState state;
        InputEvent event = WheelEvent(-WheelDeltaPerDetent);

        DeriveFrameState(&event, 1, state);
        Assert::AreEqual(-1, state.m_mousePos[2]);
        Assert::AreEqual(-1, state.m_mouseVel[2]);
      }

      // Windows sends no release for anything held when focus goes.
      TEST_METHOD(FocusLossReleasesEverythingHeldWithAnEdgeForEach)
      {
        InputFrameState state;
        std::vector<InputEvent> queue{KeyEvent(InputEventType::KeyDown, TestKeyA), ButtonEvent(InputEventType::MouseButtonDown, 1)};
        Frame(queue, state);

        queue.push_back(FocusLostEvent());
        Frame(queue, state);

        Assert::AreEqual(0, static_cast<int>(state.m_keys[TestKeyA]));
        Assert::AreEqual(-1, static_cast<int>(state.m_keyDeltas[TestKeyA]));
        Assert::IsFalse(state.m_mb[1]);
        Assert::AreEqual(-1, state.m_mbDeltas[1]);
      }

      // Focus loss obeys the same one-edge rule as everything else, or it would
      // erase the press that the frame had just reported.
      TEST_METHOD(FocusLossWaitsForAPressMadeInTheSameFrame)
      {
        InputFrameState state;
        std::vector<InputEvent> queue{KeyEvent(InputEventType::KeyDown, TestKeyA), FocusLostEvent()};

        Assert::AreEqual(size_t(1), Frame(queue, state));
        Assert::AreEqual(1, static_cast<int>(state.m_keyDeltas[TestKeyA]), L"the press is reported");

        Frame(queue, state);
        Assert::AreEqual(-1, static_cast<int>(state.m_keyDeltas[TestKeyA]), L"the release lands the frame after");
        Assert::AreEqual(0, static_cast<int>(state.m_keys[TestKeyA]));
      }

      // T5 collected characters into an m_chars vector and T6 removed it: a
      // character has to reach the focused widget interleaved with the key
      // events around it, so the driver dispatches it from its side-effect walk
      // and the derivation has nothing to say about it. What the derivation
      // still owes is that a character never DEFERS — it edges no control, so it
      // can never be the second edge that stops a frame, and a burst of typing
      // must not hold the keys behind it back.
      TEST_METHOD(CharactersAreConsumedWithoutEdgingAnythingOrDeferring)
      {
        InputFrameState state;
        std::vector<InputEvent> queue{KeyEvent(InputEventType::KeyDown, TestKeyA), CharEvent('a'), CharEvent('b'),
                                      KeyEvent(InputEventType::KeyDown, TestKeyB)};

        Assert::AreEqual(size_t(4), Frame(queue, state), L"characters consume without deferring what is behind them");
        Assert::AreEqual(1, static_cast<int>(state.m_keyDeltas[TestKeyA]));
        Assert::AreEqual(1, static_cast<int>(state.m_keyDeltas[TestKeyB]));
      }

      // The key comes off wParam and the button off a message id, so neither is
      // trusted. Indexing m_keys with a bad wParam would be a write past the end.
      TEST_METHOD(OutOfRangeKeysAndButtonsAreIgnoredRatherThanIndexed)
      {
        InputFrameState state;
        std::vector<InputEvent> queue{KeyEvent(InputEventType::KeyDown, 9999), KeyEvent(InputEventType::KeyDown, -1),
                                      ButtonEvent(InputEventType::MouseButtonDown, 77)};

        Assert::AreEqual(size_t(3), Frame(queue, state), L"consumed and discarded, not deferred forever");
      }

      TEST_METHOD(AnEmptyFrameClearsTheDeltasAndKeepsTheState)
      {
        InputFrameState state;
        std::vector<InputEvent> queue{KeyEvent(InputEventType::KeyDown, TestKeyA), MoveEvent(7, 8)};
        Frame(queue, state);

        DeriveFrameState(nullptr, 0, state);
        Assert::AreEqual(0, static_cast<int>(state.m_keyDeltas[TestKeyA]), L"deltas are per-frame");
        Assert::AreEqual(1, static_cast<int>(state.m_keys[TestKeyA]), L"held state is not");
        Assert::AreEqual(7, state.m_mousePos[0], L"position is not");
        Assert::AreEqual(0, state.m_mouseVel[0], L"velocity is");
      }
  };
} // namespace NeuronClientTests
