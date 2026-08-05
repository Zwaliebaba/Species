#pragma once

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
    Resource* m_resource;
    SoundSystem* m_soundSystem;
    LangTable* m_langTable;
    Profiler* m_profiler;

    // Things that are the world

    // Everything else
    ClientToServer* m_clientToServer; // Clients connection to Server
    LocationInput* m_locationInput;
    StartSequence* m_startSequence;
    AttractMode* m_attractMode;
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
