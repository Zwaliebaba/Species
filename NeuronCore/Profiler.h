#pragma once

#include "SortingHashTable.h"

#ifdef PROFILER_ENABLED

//*****************************************************************************
// Class ProfiledElement
//*****************************************************************************

class ProfiledElement
{
  public:
    // Values used for accumulating the profile for the current second
    double m_currentTotalTime; // In seconds
    int m_currentNumCalls;

    // Values used for storing the profile for the previous second
    double m_lastTotalTime; // In seconds
    int m_lastNumCalls;

    // Values used for storing history data (accumulation of all m_lastTotalTime
    // and m_lastNumCalls values since last reset)
    double m_historyTotalTime; // In seconds
    double m_historyNumSeconds;
    int m_historyNumCalls;

    // Values used for storing the longest and shortest duration spent inside
    // this elements profile. These values are reset when the history is reset
    double m_shortest;
    double m_longest;

    double m_callStartTime;
    char* m_name;

    SortingHashTable<ProfiledElement*> m_children;
    ProfiledElement* m_parent;

    bool m_isExpanded; // Bit of data that a tree view display can use
    bool m_wasExpanded;

    ProfiledElement(const char* _name, ProfiledElement* _parent);
    ~ProfiledElement();

    void Start();
    void End();
    void Advance();
    void ResetHistory();

    double GetMaxChildTime();
};

//*****************************************************************************
// Class Profiler
//*****************************************************************************

class Profiler
{
    bool m_insideRenderSection; // Used to decide whether to do a render sync for each call to EndProfile

  public:
    // Draining the graphics pipeline before taking a timestamp is what makes a
    // render timing mean anything — otherwise the work is still queued when the
    // clock is read. But it is a graphics call, and this is the foundation: a
    // headless server links NeuronCore and must not need OpenGL to do it.
    //
    // So the client installs the call and the profiler makes it through here.
    // With no hook installed, m_doGlFinish simply does nothing, which is the
    // right behaviour for anything that has no pipeline to drain.
    using RenderSyncHook = void (*)();
    static void SetRenderSyncHook(RenderSyncHook _hook);
    ProfiledElement* m_currentElement; // Stores the currently active profiled element
    ProfiledElement* m_rootElement;
    bool m_doGlFinish; // Only ever set by the debug profile window

  private:
    static RenderSyncHook sm_renderSync;

  public:
    double m_endOfSecond;
    double m_lengthOfLastSecond; // Will be somewhere between 1.0 and (1.0 + g_advanceTime)

    Profiler();
    ~Profiler();

    void Advance();

    void RenderStarted();
    void RenderEnded();

    void StartProfile(const char* _name);
    void EndProfile(const char* _name);

    void ResetHistory();
};

#define SET_PROFILE(profiler, itemName, value) profiler->SetProfile(itemName, value)
#define START_PROFILE(profiler, itemName) profiler->StartProfile(itemName)
#define END_PROFILE(profiler, itemName) profiler->EndProfile(itemName)

#else // PROFILER_ENABLED
#define SET_PROFILE(profiler, name, value)
#define START_PROFILE(profiler, itemName)
#define END_PROFILE(profiler, itemName)
#endif // PROFILER_ENABLED

// Owned by App, which assigns this during startup. Declared here so the layers
// below Species can reach the subsystem without including App.h — see
// tasks/layering-inversion.yaml T8.
extern Profiler* g_profiler;
