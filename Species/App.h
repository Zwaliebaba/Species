#pragma once

#include <memory>

#include "AppCommands.h"
#include "AppState.h"

#include "RgbColour.h"

class Profiler;
class AttractMode;
class PrefsManager;

namespace Neuron
{
  class ClientToServer;
  class Resource;
  class SoundSystem;
  class LangTable;
  class BitmapRGBA;
} // namespace Neuron


namespace Species
{
  class Camera;
  class Location;
  class Renderer;
  class UserInput;
  class LocationInput;
  class GlobalWorld;
  class ParticleSystem;
  class TaskManager;
  class TaskManagerInterface;
  class TaskManagerInterfaceIcons;
  class Script;
  class LocationEditor;
  class MouseCursor;
  class GameCursor;
  class GameMenu;
  class StartSequence;
  class GameMenu;

  class App : public AppCommands
  {
    public:
      // Library Code Objects
      //
      // m_resource is NOT owned here. Species/Main.cpp's Finalise() deletes
      // g_resource, and that is the delete that actually executes -- see ~App.
      Resource* m_resource;
      std::unique_ptr<SoundSystem> m_soundSystem;
      std::unique_ptr<LangTable> m_langTable;
      std::unique_ptr<Profiler> m_profiler;

      // Things that are the world

      // Everything else
      std::unique_ptr<ClientToServer> m_clientToServer; // Clients connection to Server

      // Not owned either: Species/Main.cpp deletes both of these, and nothing
      // anywhere deletes m_gameMenu.
      LocationInput* m_locationInput;
      StartSequence* m_startSequence;

      // Guarded exactly like its construction and destruction in App.cpp.
      // AttractMode has NO HEADER anywhere in the tree and ATTRACTMODE_ENABLED
      // is defined nowhere, so the type is never completed -- an unguarded
      // unique_ptr member instantiates a deleter for it and fails with
      // "can't delete an incomplete type". A raw pointer hid that; an owner
      // cannot.
#ifdef ATTRACTMODE_ENABLED
    std::unique_ptr<AttractMode> m_attractMode;
#endif
    GameMenu* m_gameMenu;

    // Owned here since ownership T6, and reachable only through App: GameLogic
    // used to delete g_renderer and g_taskManagerInterface directly, which is
    // what DestroyRenderer/CreateRenderer/ReplaceTaskManagerInterface replace.
    // g_renderer and g_taskManagerInterface observe these.
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<TaskManagerInterfaceIcons> m_taskManagerInterface;

    // The rest of the subsystem graph, owned here since ownership T6. Each has
    // a global beside it in WorldPointers.h or AppState.h that observes it --
    // several of those are typed as the *Access interface rather than the
    // concrete class, which is dependency inversion doing its job and not a
    // reason for the owner to be typed that way too.
    //
    // Nothing outside App.cpp deletes or reassigns any of these; that was
    // checked before they moved, and it is what separates them from
    // m_renderer and m_taskManagerInterface above.
    std::unique_ptr<PrefsManager> m_prefsManager;
    std::unique_ptr<GameCursor> m_gameCursor;
    std::unique_ptr<UserInput> m_userInput;
    std::unique_ptr<Camera> m_camera;
    std::unique_ptr<ParticleSystem> m_particleSystem;
    std::unique_ptr<TaskManager> m_taskManager;
    std::unique_ptr<Script> m_script;
    std::unique_ptr<GlobalWorld> m_globalWorld;


    // State flags

    // Requested state flags

    bool m_levelReset;
    std::string m_gameDataFile;


  public:
    App();
    ~App();


    void DestroyRenderer() override;
    void CreateRenderer() override;
    void ReplaceTaskManagerInterface() override;

    void SetProfileName(char const* _profileName) override;
    bool LoadProfile() override;

    void SetLanguage(char const* _language, bool _test) override;

    bool HasBoughtGame() override;

    void LoadPrologue() override;
    void LoadCampaign() override;

    static const char* GetProfileDirectory();
    char const* ProfileDirectory() override { return GetProfileDirectory(); }
    static const char* GetPreferencesPath();
    static const char* GetScreenshotDirectory();

    void UpdateDifficultyFromPreferences() override;
  };

extern App* g_app;
} // namespace Species
