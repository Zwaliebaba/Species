#include "pch.h"

#include <utility>

#include "Input.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Neuron;

// ControlSubscription is the lifetime half of the subscription API, and it is
// the half that is invisible when it goes wrong: a token that fails to cancel
// leaves a handler holding a pointer to a destroyed window, and the game keeps
// running until the day that control is pressed.
//
// The InputManager here has no drivers and no bindings, so no control ever
// fires. That is deliberate — what these pin is the bookkeeping, which needs no
// input at all. Whether a firing handler may destroy its own subscription is a
// separate question, answered where the firing is.
namespace NeuronClientTests
{
  TEST_CLASS(ControlSubscriptionTests)
  {
    public:
      TEST_METHOD(SubscribingRegistersAndTheTokenIsActive)
      {
        InputManager manager;
        ControlSubscription token = manager.subscribe(ControlType::ControlGamePause, [] {});

        Assert::AreEqual(size_t(1), manager.subscriptionCount());
        Assert::IsTrue(token.active());
      }

      // THE WHOLE POINT OF THE TYPE. A window that subscribes in its
      // constructor is deleted without telling anybody, and the subscription
      // has to go with it.
      TEST_METHOD(DestroyingTheTokenUnsubscribes)
      {
        InputManager manager;
        {
          ControlSubscription token = manager.subscribe(ControlType::ControlGamePause, [] {});
          Assert::AreEqual(size_t(1), manager.subscriptionCount());
        }
        Assert::AreEqual(size_t(0), manager.subscriptionCount(), L"the subscription went with the token");
      }

      TEST_METHOD(SubscriptionsAreIndependentOfEachOther)
      {
        InputManager manager;
        ControlSubscription kept = manager.subscribe(ControlType::ControlGamePause, [] {});
        {
          ControlSubscription temporary = manager.subscribe(ControlType::ControlMenuActivate, [] {});
          Assert::AreEqual(size_t(2), manager.subscriptionCount());
        }
        Assert::AreEqual(size_t(1), manager.subscriptionCount(), L"only the one that went out of scope was cancelled");
      }

      // Moving transfers the ONE subscription rather than duplicating the duty
      // to cancel it: the moved-from token must not also unsubscribe when it
      // dies, or it would cancel whatever had taken that slot.
      TEST_METHOD(MovingTransfersOwnershipWithoutCancelling)
      {
        InputManager manager;
        ControlSubscription first = manager.subscribe(ControlType::ControlGamePause, [] {});

        {
          ControlSubscription second = std::move(first);
          Assert::IsFalse(first.active(), L"the moved-from token owns nothing");
          Assert::IsTrue(second.active());
          Assert::AreEqual(size_t(1), manager.subscriptionCount(), L"a move is not a subscribe");
        }

        Assert::AreEqual(size_t(0), manager.subscriptionCount(), L"the destination cancelled it");
      }

      TEST_METHOD(AMovedFromTokenGoingOutOfScopeCancelsNothing)
      {
        InputManager manager;
        ControlSubscription kept = manager.subscribe(ControlType::ControlGamePause, [] {});
        {
          ControlSubscription source = manager.subscribe(ControlType::ControlMenuActivate, [] {});
          ControlSubscription moved = std::move(source);
          Assert::AreEqual(size_t(2), manager.subscriptionCount());
        }
        Assert::AreEqual(size_t(1), manager.subscriptionCount(), L"two destructors, one cancellation");
      }

      // Assigning over a live token has to cancel what it was holding, or that
      // subscription is leaked with nothing left able to reach it.
      TEST_METHOD(MoveAssigningOverALiveTokenCancelsTheOldSubscription)
      {
        InputManager manager;
        ControlSubscription token = manager.subscribe(ControlType::ControlGamePause, [] {});
        ControlSubscription replacement = manager.subscribe(ControlType::ControlMenuActivate, [] {});
        Assert::AreEqual(size_t(2), manager.subscriptionCount());

        token = std::move(replacement);
        Assert::AreEqual(size_t(1), manager.subscriptionCount(), L"the token's previous subscription was cancelled");
        Assert::IsTrue(token.active());
      }

      TEST_METHOD(ResetIsIdempotent)
      {
        InputManager manager;
        ControlSubscription token = manager.subscribe(ControlType::ControlGamePause, [] {});

        token.reset();
        Assert::AreEqual(size_t(0), manager.subscriptionCount());
        Assert::IsFalse(token.active());

        token.reset();
        Assert::AreEqual(size_t(0), manager.subscriptionCount(), L"a second reset is not a second unsubscribe");
      }

      TEST_METHOD(ADefaultTokenOwnsNothingAndIsSafeToDestroy)
      {
        InputManager manager;
        {
          ControlSubscription token;
          Assert::IsFalse(token.active());
        }
        Assert::AreEqual(size_t(0), manager.subscriptionCount());
      }

      // An empty std::function would be called and crash on the frame the
      // control fires, which is arbitrarily far from the code that made the
      // mistake. It is refused at the call instead.
      TEST_METHOD(ANullHandlerIsRefusedRatherThanStored)
      {
        InputManager manager;
        ControlSubscription token = manager.subscribe(ControlType::ControlGamePause, nullptr);

        Assert::AreEqual(size_t(0), manager.subscriptionCount());
        Assert::IsFalse(token.active());
      }
  };
} // namespace NeuronClientTests
