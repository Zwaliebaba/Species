#pragma once

// The handful of application-level actions the layers below Species ask for:
// loading a profile or a campaign, changing language, checking whether the
// game was bought.
//
// Unlike the state in AppState.h and the subsystems in WorldPointers.h, these
// are behaviour that only App can perform, so they cannot become globals. They
// are also the reason App.h cannot simply be dropped from GameLogic: wrapping
// them as free functions defined in Species would leave GameLogicTests unable
// to link, and Tests/GameLogicTests/LinkStubs.cpp may only ever shrink.
//
// So the dependency inverts instead. App installs itself here during startup;
// the pointer is null in a test DLL, which links because the definition lives
// in NeuronClient rather than in the executable. Callers that can run before
// startup must null-check. See tasks/layering-inversion.yaml T14.
class RendererAccess;
class TaskManagerInterfaceAccess;

class AppCommands
{
  public:
    virtual ~AppCommands() = default;

    // Construction is the one thing a pure-virtual seam cannot express, so
    // these are factories rather than ordinary interface members. The
    // preferences windows rebuild the renderer when the screen mode changes,
    // and the keybindings window rebuilds the task manager interface when the
    // control method changes; both windows live in GameLogic, and neither can
    // say `new Renderer()` without including Species. Everything else they do
    // to the objects goes through RendererAccess and
    // TaskManagerInterfaceAccess. See tasks/layering-inversion.yaml T12.
    //
    // THESE USED TO RETURN THE OBJECT AND LEAVE THE CALLER TO INSTALL IT, and
    // ownership T6 changed that: App owns both, so a caller that did
    // `delete g_renderer` was destroying something App held a unique_ptr to.
    // They install what they build, and the matching destroy is a call here
    // too. g_renderer and g_taskManagerInterface are observers now.
    //
    // THE RENDERER KEEPS TWO CALLS RATHER THAN ONE, deliberately, and the
    // reason is the same one that argued against a Recreate() before
    // ownership moved: its call sites differ in what they do between
    // destroying the old object and building the new one. One destroys a
    // window in between; one builds a renderer only to throw it away. A
    // single Replace() would have to guess which sequence it was being asked
    // for. The task manager interface has no such gap at either of its call
    // sites, so it gets the single call.
    //
    // CreateRenderer also Initialise()s what it builds, because all three call
    // sites did that on the next line and none of them can use it before.
    virtual void DestroyRenderer() = 0;
    virtual void CreateRenderer() = 0;
    virtual void ReplaceTaskManagerInterface() = 0;

    virtual void SetProfileName(char const* _profileName) = 0;
    virtual bool LoadProfile() = 0;
    virtual void SetLanguage(char const* _language, bool _test) = 0;
    virtual bool HasBoughtGame() = 0;
    virtual void LoadPrologue() = 0;
    virtual void LoadCampaign() = 0;

    // Re-reads the difficulty preference and pushes it into the world. Called
    // from the level file loader, which moves to GameLogic in T15.
    virtual void UpdateDifficultyFromPreferences() = 0;

    // Named differently from App's static GetProfileDirectory, which cannot be
    // virtual; the implementation forwards to it.
    virtual char const* ProfileDirectory() = 0;
};

extern AppCommands* g_appCommands;
