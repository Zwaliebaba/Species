#include "pch.h"
#include "Profiler.h"
#include "HiResTime.h"

Profiler* g_profiler = nullptr;

#ifdef PROFILER_ENABLED

#define PROFILE_HISTORY_LENGTH  10

// ****************************************************************************
//  Class ProfiledElement
// ****************************************************************************

// *** Constructor
ProfiledElement::ProfiledElement(const char* _name, ProfiledElement* _parent)
  : m_currentTotalTime(0.0),
    m_currentNumCalls(0),
    m_lastTotalTime(0.0),
    m_lastNumCalls(0),
    m_historyTotalTime(0.0),
    m_historyNumSeconds(0.0),
    m_historyNumCalls(0),
    m_shortest(DBL_MAX),
    m_longest(DBL_MIN),
    m_callStartTime(0.0),
    m_parent(_parent),
    m_isExpanded(false)
{
  m_name = strdup(_name);
}

// *** Destructor
ProfiledElement::~ProfiledElement()
{
  free(m_name);
}

// *** Start
void ProfiledElement::Start() { m_callStartTime = GetHighResTime(); }

// *** End
void ProfiledElement::End()
{
  const double timeNow = GetHighResTime();

  m_currentNumCalls++;
  const double duration = timeNow - m_callStartTime;
  m_currentTotalTime += duration;

  if (duration > m_longest)
    m_longest = duration;
  if (duration < m_shortest)
    m_shortest = duration;
}

// *** Advance
void ProfiledElement::Advance()
{
  m_lastTotalTime = m_currentTotalTime;
  m_lastNumCalls = m_currentNumCalls;
  m_currentTotalTime = 0.0;
  m_currentNumCalls = 0;
  m_historyTotalTime += m_lastTotalTime;
  m_historyNumSeconds += 1.0;
  m_historyNumCalls += m_lastNumCalls;

  for (auto& entry : m_children)
  {
    entry.second->Advance();
  }
}

// *** ResetHistory
void ProfiledElement::ResetHistory()
{
  m_historyTotalTime = 0.0;
  m_historyNumSeconds = 0.0;
  m_historyNumCalls = 0;
  m_longest = DBL_MIN;
  m_shortest = DBL_MAX;

  for (auto& entry : m_children)
  {
    entry.second->ResetHistory();
  }
}

double ProfiledElement::GetMaxChildTime()
{
  if (m_children.empty())
    return 0.0;

  double rv = 0.0;
  for (const auto& entry : m_children)
  {
    // Deliberately float, not double: m_historyTotalTime is a double and the
    // narrowing is what the comparison below has always seen. Widening it here
    // would change which child wins a near-tie.
    const float val = entry.second->m_historyTotalTime;
    if (val > rv)
      rv = val;
  }

  // The divisor is the FIRST child's history length, not the winner's, which
  // is why the ordering has to be preserved exactly. It reads like an
  // oversight; it is left alone because changing it changes a number the
  // profile window puts on screen.
  return rv / m_children.begin()->second->m_historyNumSeconds;
}

// ****************************************************************************
//  Class Profiler
// ****************************************************************************

#define MAIN_THREAD_ONLY {}

// *** Constructor
Profiler::Profiler()
  : m_insideRenderSection(false),
    m_currentElement(nullptr),
    m_doGlFinish(false)
{
  m_rootElement = std::make_unique<ProfiledElement>("Root", nullptr);
  m_rootElement->m_isExpanded = true;
  m_currentElement = m_rootElement.get();
  m_endOfSecond = GetHighResTime() + 1.0f;
}

// *** Destructor
Profiler::~Profiler() = default;

// *** Advance
void Profiler::Advance()
{
  double timeNow = GetHighResTime();
  if (timeNow > m_endOfSecond)
  {
    m_lengthOfLastSecond = timeNow - (m_endOfSecond - 1.0);
    m_endOfSecond = timeNow + 1.0;

    m_rootElement->Advance();
  }
}

// *** RenderStarted
Profiler::RenderSyncHook Profiler::sm_renderSync = nullptr;

void Profiler::SetRenderSyncHook(RenderSyncHook _hook) { sm_renderSync = _hook; }

void Profiler::RenderStarted() { m_insideRenderSection = true; }

// *** RenderEnded
void Profiler::RenderEnded() { m_insideRenderSection = false; }

// *** ResetHistory
void Profiler::ResetHistory() { m_rootElement->ResetHistory(); }

static bool s_expanded = false;

// *** StartProfile
void Profiler::StartProfile(const char* _name)
{
  MAIN_THREAD_ONLY;

  auto& children = m_currentElement->m_children;
  const auto entry = children.find(_name);
  ProfiledElement* pe = (entry == children.end()) ? nullptr : entry->second.get();
  if (!pe)
  {
    // The observer is taken before the owner moves into the map, because
    // everything below this point works through pe.
    auto created = std::make_unique<ProfiledElement>(_name, m_currentElement);
    pe = created.get();
    children[_name] = std::move(created);
  }

  ASSERT_TEXT(m_rootElement->m_isExpanded, "Profiler root element has been un-expanded");

  bool wasExpanded = m_currentElement->m_isExpanded;

  if (m_currentElement->m_isExpanded)
  {
    if (m_doGlFinish && m_insideRenderSection && sm_renderSync)
      sm_renderSync();
    pe->Start();
  }
  m_currentElement = pe;

  m_currentElement->m_wasExpanded = wasExpanded;
}

// *** EndProfile
void Profiler::EndProfile(const char* _name)
{
  MAIN_THREAD_ONLY;

  DEBUG_ASSERT(m_currentElement->m_wasExpanded == m_currentElement->m_parent->m_isExpanded);

  if (m_currentElement->m_parent->m_isExpanded)
  {
    if (m_doGlFinish && m_insideRenderSection && sm_renderSync)
      sm_renderSync();

    DEBUG_ASSERT(m_currentElement != m_rootElement.get());
    DEBUG_ASSERT(stricmp(_name, m_currentElement->m_name) == 0);

    m_currentElement->End();
  }

  DEBUG_ASSERT(strcmp(m_currentElement->m_name, m_currentElement->m_parent->m_name) != 0);
  m_currentElement = m_currentElement->m_parent;
}

#endif // PROFILER_ENABLED
