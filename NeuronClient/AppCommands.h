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
class AppCommands
{
  public:
    virtual ~AppCommands() = default;

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
