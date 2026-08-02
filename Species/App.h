#pragma once

#include "AppState.h"

#include "RgbColour.h"

class Camera;
class Location;
class Server;
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



class App
{
public:
	// Library Code Objects
    Resource            *m_resource;
    SoundSystem         *m_soundSystem;
	LangTable			*m_langTable;
    Profiler            *m_profiler;

	// Things that are the world

	// Everything else
    ClientToServer      *m_clientToServer;          // Clients connection to Server
	LocationInput		*m_locationInput;
    GameCursor          *m_gameCursor;
    StartSequence       *m_startSequence;
	AttractMode			*m_attractMode;
    GameMenu            *m_gameMenu;

    bool                m_negativeRenderer;

	// State flags
    bool                m_paused;

	// Requested state flags

    char                m_requestedMission[256];
    char                m_requestedMap[256];
    bool                m_levelReset;
    char                m_gameDataFile[256];

	RGBAColour			m_backgroundColour;



public:
    App ();
	~App();


    void    SetProfileName  ( char const *_profileName );
    bool    LoadProfile     ();

    void    SetLanguage     ( char const *_language, bool _test );

	bool	HasBoughtGame	();

    void    LoadPrologue    ();
    void    LoadCampaign    ();

	static const char *GetProfileDirectory();
	static const char *GetPreferencesPath();
    static const char *GetScreenshotDirectory();

	void	UpdateDifficultyFromPreferences();

};

extern App *g_app;

