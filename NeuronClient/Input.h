#pragma once

#include <functional>
#include <optional>
#include <vector>
#include <string>

#include "TextStreamReaders.h"
#include "InputTypes.h"
#include "InputSpec.h"
#include "InputDriver.h"
#include "InputRouter.h"
#include "ControlTypes.h"
#include "ControlBindings.h"


namespace Neuron
{
  class InputManager;


  // ===========================================================================
  // SUBSCRIBING TO A CONTROL
  // ===========================================================================
  //
  // A window or a button says once which control it cares about and is called
  // when that control fires, instead of asking every frame forever. That is the
  // shape the owner asked for on 2026-08-06.
  //
  // THE RULE THAT GOVERNS WHO MAY USE IT, and a reviewer should reject a change
  // that breaks it rather than debating it:
  //
  //     SIMULATION CODE MUST NOT SUBSCRIBE.
  //
  // Multiplayer is deterministic lockstep. Simulation input reaches the wire
  // only through TeamControls and the 10Hz IAmAlive letter, so every machine
  // advances from the same orders. A handler that reaches into game state when
  // a key is pressed runs on ONE client, at a frame boundary the others never
  // saw, and the result is a desync that every build stays green through. Any
  // subscriber under GameLogic that touches an entity, a building, a team or
  // anything reachable from Location::Advance is wrong even if it works.
  //
  // Subscriptions are for the UI and for discrete client-local actions -
  // opening a window, pausing, toggling an overlay. The camera, unit steering
  // and TeamControls keep reading the per-frame state through controlEvent().
  //
  // A subscription must not outlive the InputManager. That is free in practice
  // - the manager lives for the process - and stated because the token holds a
  // raw pointer back to it.
  class ControlSubscription
  {
    public:
      ControlSubscription() = default;
      ControlSubscription(InputManager* _owner, unsigned _id);
      ~ControlSubscription();

      // Move-only, because a subscription is a piece of ownership: copying the
      // token would leave two objects each believing they must unsubscribe, and
      // the second would cancel somebody else's subscription once the id had
      // been reused.
      ControlSubscription(ControlSubscription const&) = delete;
      ControlSubscription& operator=(ControlSubscription const&) = delete;
      ControlSubscription(ControlSubscription&& _other) noexcept;
      ControlSubscription& operator=(ControlSubscription&& _other) noexcept;

      // Unsubscribe early. The destructor does this; call it by hand only when
      // the subscription has to stop before its owner does.
      void reset();

      bool active() const { return m_owner != nullptr; }

    private:
      InputManager* m_owner{nullptr};
      unsigned m_id{0};
  };


  class InputManager
  {
    private:
      std::vector<InputDriver*> drivers;

      ControlBindings bindings;

      // EVENTS GO HERE FIRST, AND THE UI IS THE FIRST SINK. See InputRouter.h
      // for what that replaces. The Eclipse sink is a member rather than
      // something Species registers, because Eclipse is part of this layer and
      // the UI-before-game ordering is the design rather than a caller's
      // choice.
      InputRouter m_router;
      EclipseInputSink m_eclipseSink;

      struct Subscription
      {
          unsigned m_id;
          ControlType m_control;
          std::function<void()> m_handler;
      };

      std::vector<Subscription> m_subscriptions;

      // Never reused and never zero, so a stale token cannot cancel a
      // subscription that took its slot.
      unsigned m_nextSubscriptionId{1};

      // Calls the handlers whose control fired this frame. Once per Advance,
      // after the router has dispatched, so a control the UI consumed does not
      // reach a subscriber either.
      void FireSubscriptions();

      bool m_idle;

      // Does the work for controlEvent, below
      bool controlEventA(ControlType type, InputDetails& details);

    public:
      InputManager();

      ~InputManager();

      // Parse an input prefs file
      void parseInputPrefs(TextReader& reader, bool replace = false);

      // THE COMPATIBILITY PATH, and new code should use subscribe() instead.
      //
      // controlEvent asks, once per caller per frame, whether a control fired -
      // which means every consumer has to remember to ask, every frame,
      // forever, and a consumer that forgets simply stops working with nothing
      // to see. It is reimplemented over the same per-frame state the router
      // dispatches from, so it and a subscriber agree about what happened; it
      // stays because 150-odd call sites across 29 files read it, and they
      // migrate one at a time rather than in one unreviewable diff.
      //
      // It is still the RIGHT door for anything that wants a CONTINUOUS answer
      // - is this held, how far has the mouse moved - and for the camera, unit
      // steering and TeamControls, which must sample rather than react. See the
      // rule above ControlSubscription for why.
      bool controlEvent(ControlType type, InputDetails& details);
      bool controlEvent(ControlType type); // Not interested in details

      // Call _handler once each frame the control fires. The returned token
      // owns the subscription: destroy it and the handler stops being called,
      // which is what makes a window that subscribes in its constructor safe to
      // delete without telling anybody.
      //
      // WHAT "FIRES" MEANS IS WHATEVER THE BINDING SAYS. A control bound to
      // `key p down` calls the handler on the press; one bound to `key p
      // pressed` calls it on EVERY frame the key is held, which is almost never
      // what a subscriber wants. Subscribe to edges.
      //
      // A handler may unsubscribe itself and may destroy the object holding its
      // own token - closing the window it belongs to is the obvious case - and
      // may subscribe something new, which will not fire until the next frame.
      [[nodiscard]] ControlSubscription subscribe(ControlType type, std::function<void()> handler);

      // Cancels a subscription by id. Called by ControlSubscription; there is
      // no reason to call it directly, because the token is the handle.
      void unsubscribe(unsigned id);

      // How many subscriptions are live. Nothing in the game asks; it exists so
      // ControlSubscriptionTests can see that a destroyed token really did
      // cancel, which is the whole promise of the type and is otherwise
      // invisible from outside.
      size_t subscriptionCount() const { return m_subscriptions.size(); }

      // suppressEvent IS GONE. It existed so UserInput::AdvanceMenus could
      // notice, after the fact, that a click it had already read out of the
      // bindings had really been meant for a window, and take it back for the
      // rest of the frame. A sink that consumes the event says so before
      // anything reads it; see InputRouter.h.

      // ONE FRAME OF INPUT, and the only place the OS message pump runs. It
      // drains Windows' queue, has each driver derive its frame, moves the
      // cursor, offers the events to the router and then fires subscriptions —
      // in that order, for reasons written at each step.
      void Advance();

      // Add a new driver to the drivers collection. The driver will be deleted
      // at the end of the life of this InputManager, so must have been created
      // with "new". Never do this twice for the same driver.
      void addDriver(InputDriver* driver);

      // Returns a description of the first bound input of a control event
      bool getBoundInputDescription(ControlType type, InputDescription& desc);

      // Returns a description of the first bound input of a control event
      bool getInputDescription(InputSpec const& spec, InputDescription& desc);

      // Get the ID corresponding to a control name, or nothing if there is no
      // such control. Returned -1 until language-hygiene T11; see
      // ControlBindings::getControlID for why that did not become an enumerator.
      std::optional<ControlType> getControlID(std::string const& control);

      // Inverse of getControlID
      void getControlString(ControlType type, std::string& name);

      // Check to see if this input spec has been triggered this time step
      // Not for general use outside the input system
      bool checkInput(InputSpec const& spec, InputDetails& details);

      // Returns true if there was an "active" input event this frame. Fills spec with
      // the details of the input. Active inputs are primarily things like button presses
      // but not buttons held down or released or 2D analog events.
      bool getFirstActiveInput(InputSpec& spec, bool instant = false);

      // Turn a prefs file string (right hand side) into an InputSpec, if
      // one of the drivers can handle it. Not for general use outside
      // of the input system
      InputParserState parseInputSpecString(std::string const& description, InputSpec& spec, std::string& err);

      // Turn a set of tokens into an InputSpec. Not for general use outside of the input system
      InputParserState parseInputSpecTokens(InputSpecTokens const& tokens, InputSpec& spec, std::string& err);

      // Replace primary binding with this one. Used when overriding default bindings
      // with user specified ones
      void replacePrimaryBinding(ControlType type, std::string const& prefString);

      // Remove all bindings
      void Clear();

      // Returns true if all of the InputDrivers report being idle
      bool isIdle();

      void printNumBindings();
  };


  extern InputManager* g_inputManager;
} // namespace Neuron
