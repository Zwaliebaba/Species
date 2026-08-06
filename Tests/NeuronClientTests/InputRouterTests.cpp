#include "pch.h"

#include <vector>

#include "InputEvents.h"
#include "InputRouter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Neuron;

// The router is the ordering rule that replaced suppressEvent, EclUpdateMouse's
// boolean-to-event synthesis and the AdvanceMenus poll-and-push seam. What it
// owes is small enough to state in one line and worth pinning anyway, because
// getting it wrong is silent in both directions: a sink that never gets offered
// an event makes the UI stop responding, and a chain that keeps going after a
// sink consumed makes a click the UI handled ALSO reach the game.
//
// EclipseInputSink is deliberately not exercised here. It reaches Eclipse's
// window list and the game's cursor, so it is not a unit; what it decides is
// covered where the decision is made, in Eclipse and InputField.
namespace NeuronClientTests
{
  namespace
  {
    class RecordingSink : public InputSink
    {
      public:
        explicit RecordingSink(EventDisposition _answer)
          : m_answer(_answer)
        {
        }

        EventDisposition OnInputEvent(InputEvent const& _event) override
        {
          m_seen.push_back(_event.m_key);
          return m_answer;
        }

        EventDisposition m_answer;
        std::vector<int> m_seen;
    };

    InputEvent KeyDown(int _key)
    {
      InputEvent event;
      event.m_type = InputEventType::KeyDown;
      event.m_key = _key;
      return event;
    }
  } // namespace

  TEST_CLASS(InputRouterTests)
  {
    public:
      TEST_METHOD(AnEventNobodyWantsIsReportedIgnored)
      {
        RecordingSink sink(EventDisposition::Ignored);
        InputRouter router;
        router.AddSink(&sink);

        Assert::IsTrue(router.Dispatch(KeyDown(65)) == EventDisposition::Ignored);
        Assert::AreEqual(size_t(1), sink.m_seen.size(), L"it was still offered");
      }

      TEST_METHOD(DispatchWithNoSinksIsIgnoredRatherThanUndefined)
      {
        InputRouter router;
        Assert::IsTrue(router.Dispatch(KeyDown(65)) == EventDisposition::Ignored);
      }

      // The reason sinks are a list rather than a single handler: the first
      // added is the first offered, and the UI is added first.
      TEST_METHOD(SinksAreOfferedInTheOrderTheyWereAdded)
      {
        RecordingSink first(EventDisposition::Ignored);
        RecordingSink second(EventDisposition::Ignored);
        InputRouter router;
        router.AddSink(&first);
        router.AddSink(&second);

        router.Dispatch(KeyDown(65));
        Assert::AreEqual(1, first.m_seen.empty() ? 0 : 1);
        Assert::AreEqual(1, second.m_seen.empty() ? 0 : 1);
      }

      // THE RULE THE WHOLE DESIGN RESTS ON. A click the UI handles must not
      // also reach the game, so consumption stops the chain dead.
      TEST_METHOD(ConsumptionStopsTheChainAndIsReportedUpwards)
      {
        RecordingSink ui(EventDisposition::Consumed);
        RecordingSink game(EventDisposition::Ignored);
        InputRouter router;
        router.AddSink(&ui);
        router.AddSink(&game);

        Assert::IsTrue(router.Dispatch(KeyDown(65)) == EventDisposition::Consumed);
        Assert::AreEqual(size_t(1), ui.m_seen.size());
        Assert::IsTrue(game.m_seen.empty(), L"nothing behind a consuming sink is offered the event");
      }

      TEST_METHOD(ARemovedSinkStopsBeingOffered)
      {
        RecordingSink sink(EventDisposition::Consumed);
        InputRouter router;
        router.AddSink(&sink);
        router.RemoveSink(&sink);

        Assert::IsTrue(router.Dispatch(KeyDown(65)) == EventDisposition::Ignored);
        Assert::IsTrue(sink.m_seen.empty());
      }

      // AddSink takes a pointer, and a null one would be a crash on the next
      // event rather than at the call that made the mistake.
      TEST_METHOD(ANullSinkIsRefusedRatherThanStored)
      {
        InputRouter router;
        router.AddSink(nullptr);
        Assert::IsTrue(router.Dispatch(KeyDown(65)) == EventDisposition::Ignored);
      }
  };
} // namespace NeuronClientTests
