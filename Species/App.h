#pragma once

#include <memory>

#include "AppCommands.h"
#include "AppState.h"

#include "RgbColour.h"

class Camera;
class Location;
class ClientToServer;
class Renderer;
class UserInput;
class Resource;
class SoundSystem;
class LocationInput;
class LangTable;
class GlobalWorld;
class ParticleSystem;
class TaskManager;
class TaskManagerInterface;
class Script;
class Profiler;
class LocationEditor;
class MouseCursor;
class GameCursor;
class GameMenu;
class StartSequence;
class AttractMode;
class ControlHelpSystem;
class BitmapRGBA;
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

    std::unique_ptr<AttractMode> m_attractMode;
    GameMenu* m_gameMenu;


    // State flags

    // Requested state flags

    bool m_levelReset;
    std::string m_gameDataFile;


  public:
    App();
    ~App();


    RendererAccess* CreateRenderer() override;
    TaskManagerInterfaceAccess* CreateTaskManagerInterface() override;

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
