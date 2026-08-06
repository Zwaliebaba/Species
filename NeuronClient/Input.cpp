#include "pch.h"

#include <algorithm>
#include <vector>
#include <iostream>
#include <fstream>

#include "Input.h"
#include "TargetCursor.h"
#include "WindowManager.h"
#include "LanguageTable.h"
#include "HiResTime.h"

#define Q(x) "\"" << x << "\""

using namespace std;


namespace Neuron
{
  InputManager* g_inputManager = nullptr;


  ControlSubscription::ControlSubscription(InputManager* _owner, unsigned _id)
    : m_owner(_owner),
      m_id(_id)
  {
  }


  ControlSubscription::~ControlSubscription() { reset(); }


  ControlSubscription::ControlSubscription(ControlSubscription&& _other) noexcept
    : m_owner(_other.m_owner),
      m_id(_other.m_id)
  {
    _other.m_owner = nullptr;
    _other.m_id = 0;
  }


  ControlSubscription& ControlSubscription::operator=(ControlSubscription&& _other) noexcept
  {
    if (this != &_other)
    {
      // The subscription this token already held goes first. Overwriting it
      // would leak a handler that nothing can ever cancel.
      reset();

      m_owner = _other.m_owner;
      m_id = _other.m_id;
      _other.m_owner = nullptr;
      _other.m_id = 0;
    }
    return *this;
  }


  void ControlSubscription::reset()
  {
    if (m_owner)
      m_owner->unsubscribe(m_id);

    m_owner = nullptr;
    m_id = 0;
  }


  InputManager::InputManager()
    : drivers(),
      m_idle(true)
  {
    // FIRST IN, FIRST OFFERED. The UI is the only sink today and it is added
    // here, before any driver exists, so the ordering cannot depend on
    // construction order elsewhere.
    m_router.AddSink(&m_eclipseSink);
  }


  InputManager::~InputManager()
  {
    for (unsigned i = 0; i < drivers.size(); ++i)
      if (drivers[i])
        delete drivers[i];
  }


  void InputManager::parseInputPrefs(TextReader& reader, bool replace)
  {
    int line = 1;
#ifdef TARGET_DEBUG
  ofstream derr("inputprefs_debug.txt");
#else
  ostream& derr = cout;
#endif
  while (reader.ReadLine())
  {
    // derr << "Line " << line++ << ": ";
    char* control = reader.GetNextToken();
    if (control)
    {
      // THE `~` ICON-LINE BRANCH IS GONE. `Control ~ path/to/icon.bmp` parsed
      // into a map nothing ever read, and no file under GameData/Input has
      // carried such a line since T2 removed the eighteen gamepad glyphs. A
      // `~` in somebody's own prefs file is now reported as an unparseable
      // line, which is what it is.
      char* eq = reader.GetNextToken();
      if (!eq || strcmp(eq, "=") != 0)
      {
        derr << "Assignment not found." << endl;
        continue;
      }

      string inputspec = reader.GetRestOfLine();
      std::optional<ControlType> const control_id = getControlID(control);
      if (control_id.has_value())
      {
        InputSpec spec;
        string err;
        if (PARSE_SUCCESS(parseInputSpecString(inputspec, spec, err)))
        {
          if (bindings.bind(*control_id, spec, replace))
            derr << "Success. Driver = " << drivers[spec.driver]->getName() << endl;
          else
            derr << "Binding failed." << endl;
        }
        else
          derr << "Parse failed - " << err << endl;
      }
      else
        derr << "Control ID not found." << endl;
    }
    else
      derr << "Blank line." << endl;
  }
  }


InputParserState InputManager::parseInputSpecString(string const& description, InputSpec& spec, string& err)
{
  InputSpecTokens tokens(description);
  return parseInputSpecTokens(tokens, spec, err);
}


InputParserState InputManager::parseInputSpecTokens(InputSpecTokens const& tokens, InputSpec& spec, string& err)
{
  InputParserState state = InputParserState::STATE_ERROR;
  InputParserState curr = InputParserState::STATE_ERROR;
  err = "";
  if (tokens.length() == 0)
    return state;

  for (unsigned i = 0; i < drivers.size(); ++i)
    if (PARSE_SUCCESS(curr = drivers[i]->parseInputSpecification(tokens, spec)))
    {
      spec.driver = i;
      return curr;
    }
    else
    {
      if (curr >= state)
      {
        state = curr;
        err = drivers[i]->getLastParseError(state);
      }
    }

  return state;
}


bool InputManager::controlEventA(ControlType type, InputDetails& details)
{
  // The isActive() guard that used to wrap this went with suppressEvent. It
  // asked whether something had taken this control back AFTER the fact; the
  // driver now answers the underlying input as absent from the moment a sink
  // consumed it, which is one frame earlier and per INPUT rather than per
  // control name — so a consumed click hides from every control bound to it
  // instead of from the one the caller happened to name.
  const InputSpecList& specs = bindings[type];

  for (unsigned i = 0; i < specs.size(); ++i)
    if (checkInput(*(specs[i]), details))
      return true;

  details.type = InputType::INPUT_TYPE_FAIL;
  return false;
}


bool InputManager::controlEvent(ControlType type, InputDetails& details) { return controlEventA(type, details); }


bool InputManager::controlEvent(ControlType type)
{
  InputDetails details;
  return controlEvent(type, details);
}


void InputManager::Advance()
{
  // THE MESSAGE PUMP, ONCE, HERE. It used to run twice a frame down two paths:
  // five game loops called InputManager::PollForEvents, which walked every
  // driver asking it to pump, and the W32 driver then pumped again at the top
  // of its own Advance. Doing it here rather than in the loops is deliberate —
  // three of those loops, MainMenuLoop among them, never called PollForEvents
  // at all and were served entirely by the driver's second pump, so a pump that
  // lives in the loops is a pump a loop can forget.
  g_windowManager->PumpMessages();

  bool idleNext = true;

  for (unsigned i = 0; i < drivers.size(); ++i)
  {
    InputDriver* driver = drivers[i];
    driver->Advance();
    idleNext = idleNext && driver->isIdle();
  }

  m_idle = idleNext;

  // THE CURSOR MOVES BEFORE THE EVENTS ARE DISPATCHED, and the order is
  // load-bearing rather than tidy. The UI sink asks TargetCursor where the
  // pointer is when it handles a click, because that is the cursor the player
  // sees — TargetCursor does not read the OS pointer, it accumulates the
  // movement control into its own coordinates and warps the OS one to match.
  // Dispatching before this line would hand Eclipse the PREVIOUS frame's
  // position and land every click one frame behind the arrow.
  //
  // It is also the order the code being replaced had: AdvanceMenus ran after
  // g_inputManager->Advance() and passed g_target->X() and Y() straight in.
  if (g_target)
    g_target->Advance();

  for (unsigned i = 0; i < drivers.size(); ++i)
    drivers[i]->DispatchEvents(m_router);

  // After the dispatch, so a control the UI consumed never reaches a
  // subscriber either — the two mechanisms answer to the same rule rather than
  // each having their own.
  FireSubscriptions();
}


ControlSubscription InputManager::subscribe(ControlType type, std::function<void()> handler)
{
  if (!handler)
    return ControlSubscription();

  const unsigned id = m_nextSubscriptionId++;
  m_subscriptions.push_back(Subscription{id, type, std::move(handler)});
  return ControlSubscription(this, id);
}


void InputManager::unsubscribe(unsigned id)
{
  std::erase_if(m_subscriptions, [id](Subscription const& _subscription) { return _subscription.m_id == id; });
}


void InputManager::FireSubscriptions()
{
  if (m_subscriptions.empty())
    return;

  // WHICH ONES FIRED IS DECIDED BEFORE ANY HANDLER RUNS, because a handler is
  // allowed to do both of the things that would otherwise corrupt this walk:
  // unsubscribe — its token is typically a member of the very object it just
  // destroyed — and subscribe, which a window opened by this keystroke will do,
  // and which must not then see the same keystroke.
  std::vector<unsigned> firing;
  for (Subscription const& subscription : m_subscriptions)
  {
    if (controlEvent(subscription.m_control))
      firing.push_back(subscription.m_id);
  }

  for (unsigned const id : firing)
  {
    auto const found =
      std::find_if(m_subscriptions.begin(), m_subscriptions.end(), [id](Subscription const& _subscription) { return _subscription.m_id == id; });
    if (found == m_subscriptions.end())
      continue; // an earlier handler cancelled this one

    // A COPY OF THE HANDLER, and it is not defensiveness. The common case for
    // a window's subscription is a handler that closes that window — which
    // destroys the token, which erases the std::function the call is executing
    // inside. Copying it out first means the closure that runs is a local and
    // the stored one can be destroyed underneath it harmlessly.
    std::function<void()> const handler = found->m_handler;
    handler();
  }
}


bool InputManager::checkInput(InputSpec const& spec, InputDetails& details)
{
  InputDriver* driver = drivers[spec.driver];
  bool ans = driver->getInput(spec, details);
  if (!ans)
    details.type = InputType::INPUT_TYPE_FAIL;
  return ans;
}


bool InputManager::getFirstActiveInput(InputSpec& spec, bool instant)
{
  for (unsigned i = 0; i < drivers.size(); ++i)
  {
    InputDriver* driver = drivers[i];
    if (driver->getFirstActiveInput(spec, instant))
    {
      spec.driver = i;
      return true;
    }
  }
  return false;
}


void InputManager::addDriver(InputDriver* driver)
{
  if (driver)
    drivers.push_back(driver);
}


bool InputManager::getBoundInputDescription(ControlType type, InputDescription& desc)
{
  const InputSpecList& specs = bindings[type];
  if (specs.size() >= 1)
    for (InputSpecIt i = specs.begin(); i != specs.end(); ++i)
      if (getInputDescription(**i, desc))
      {
        desc.translate();
        return true;
      }
  return false;
}


bool InputManager::getInputDescription(InputSpec const& spec, InputDescription& desc)
{
  InputDriver* driver = drivers[spec.driver];
  return driver->getInputDescription(spec, desc);
}


std::optional<ControlType> InputManager::getControlID(string const& control) { return bindings.getControlID(control); }


void InputManager::getControlString(ControlType type, std::string& name) { bindings.getControlString(type, name); }


void InputManager::replacePrimaryBinding(ControlType type, string const& prefString)
{
  InputSpec spec;
  string err;
  if (PARSE_SUCCESS(parseInputSpecString(prefString, spec, err)))
  {
    // bindings.replacePrimaryBinding( type, spec );
    bindings.bind(type, spec, true);
  }
}


void InputManager::Clear() { bindings.Clear(); }


bool InputManager::isIdle() { return m_idle; }


void InputManager::printNumBindings()
{
  // ENUM_HELPER's range rather than an int loop: it walks
  // [ControlMenuLeft, ControlNull], which is the same 0..NumControlTypes-1 the
  // int loop walked, without a value that can be compared against a foreign
  // enum on the way.
  for (ControlType const type : RangeControlType())
  {
    const InputSpecList& specs = bindings[type];
    cout << "There are " << specs.size() << " binding for type " << Neuron::I(type) << endl;
  }
}
} // namespace Neuron
