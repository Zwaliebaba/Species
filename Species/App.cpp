#include "pch.h"
#include "App.h"
#include "Camera.h"
#include "ClientToServer.h"
#include "ControlHelp.h"
#include "FilesysUtils.h"
#include "GameCursor.h"
#include "GameMenu.h"
#include "GlobalWorld.h"
#include "LanguageTable.h"
#include "LevelFile.h"
#include "Location.h"
#include "LocationInput.h"
#include "ParticleSystem.h"
#include "Preferences.h"
#include "PrefsOtherWindow.h"
#include "Profiler.h"
#include "Renderer.h"
#include "Resource.h"
#include "Script.h"
#include "SoundStreamDecoder.h"
#include "SoundSystem.h"
#include "SystemInfo.h"
#include "TaskManager.h"
#include "TaskManagerInterfaceIcons.h"
#include "TextRenderer.h"
#include "TextStreamReaders.h"
#include "UserInput.h"
#include "WorldPointers.h"
#include "AppState.h"

// Overlays GameData/DefaultPreferences.txt onto the built-in defaults, and is
// installed on PrefsManager before the first one is constructed. It runs only
// when there is no preferences file yet, because that is the only time
// CreateDefaultValues runs.
//
// This lives here rather than in Preferences.cpp because reaching the file means
// going through the resource system, which decrypts and strips comments — a
// settings store in NeuronCore has no business knowing that exists.
static void ApplyShippedPreferenceDefaults(PrefsManager& _prefs)
{
  if (!g_app || !g_app->m_resource)
    return;

  TextReader* reader = g_app->m_resource->GetTextReader("DefaultPreferences.txt");
  if (reader && reader->IsOpen())
  {
    while (reader->ReadLine())
    {
      _prefs.AddLine(reader->GetRestOfLine(), true);
    }
  }
}

// Drains the graphics pipeline so a render timing measures work that has actually
// happened rather than work still queued. Installed on Profiler, which cannot
// make the call itself without dragging OpenGL into NeuronCore and with it every
// binary that links the foundation — including the headless server.
static void ProfilerRenderSync() { glFinish(); }

App* g_app = nullptr;

#define GAMEDATAFILE "Game.txt"

App::App()
  : m_resource(nullptr),
    m_soundSystem(nullptr),
    m_langTable(nullptr),
    m_profiler(nullptr),
    m_clientToServer(nullptr),
    m_locationInput(nullptr),
    m_startSequence(nullptr),
    m_attractMode(nullptr),
    m_gameMenu(nullptr),
    m_negativeRenderer(false),
    m_paused(false),
    m_levelReset(false)
{
  g_app = this;

  // Load resources

  m_resource = new Resource();
  g_resource = m_resource;

  PrefsManager::SetDefaultsProvider(&ApplyShippedPreferenceDefaults);
  g_prefsManager = new PrefsManager(GetPreferencesPath());

  m_negativeRenderer = g_prefsManager->GetInt("RenderNegative", 0) ? true : false;
  if (m_negativeRenderer)
    m_backgroundColour.Set(255, 255, 255, 255);
  else
    m_backgroundColour.Set(0, 0, 0, 0);

  UpdateDifficultyFromPreferences();

#ifdef PROFILER_ENABLED
  m_profiler = new Profiler();
  g_profiler = m_profiler;
  Profiler::SetRenderSyncHook(&ProfilerRenderSync);
#endif

  g_renderer = new Renderer();
  g_renderer->Initialise();

  // Make sure that resources are now available - either the .dat files
  // or the data directory must exist

  SoundStreamDecoder* ssd = m_resource->GetSoundStreamDecoder("Sounds/ABlaster");
  ASSERT_TEXT(ssd, "Couldn't find sound resources. This is probably because\n" "sounds.dat isn't in the working directory.");
  delete ssd;

  int textureId = m_resource->GetTexture("Textures/EditorFontNormal.bmp");

  m_gameCursor = new GameCursor();
  m_soundSystem = new SoundSystem();
  g_soundSystem = m_soundSystem;
  m_clientToServer = new ClientToServer();
  g_clientToServer = m_clientToServer;
  g_userInput = new UserInput();
  //    g_location          = new Location();
  //    m_locationInput		= new LocationInput();

  g_camera = new Camera();
  m_gameMenu = new GameMenu();

  strcpy(m_gameDataFile, "Game.txt");

  //
  // Determine default language if possible

  const char* language = g_prefsManager->GetString("TextLanguage");
  if (stricmp(language, "unknown") == 0)
  {
    char* defaultLang = g_systemInfo->m_localeInfo.m_language;
    char langFilename[512];
    sprintf(langFilename, "Language/%s.txt", defaultLang);
    if (DoesFileExist(langFilename))
      g_prefsManager->SetString("TextLanguage", defaultLang);
    else
      g_prefsManager->SetString("TextLanguage", "English");
  }
  language = g_prefsManager->GetString("TextLanguage");

  SetLanguage(language, g_prefsManager->GetInt("TextLanguageTest", 0));

  SetProfileName(g_prefsManager->GetString("UserProfile", "none"));

  g_particleSystem = new ParticleSystem();
  g_taskManager = new TaskManager();
  g_script = new Script();
#ifdef ATTRACTMODE_ENABLED
  m_attractMode = new AttractMode();
#endif
  g_controlHelpSystem = new ControlHelpSystem();

  g_taskManagerInterface = new TaskManagerInterfaceIcons();

  m_soundSystem->Initialise();

  int menuOption = g_prefsManager->GetInt(OTHER_LARGEMENUS, 0);
  if (menuOption == 2) // (todo) or is running in media center and tenFootMode == -1
    g_largeMenus = true;

  //
  // Load save games

  bool profileLoaded = LoadProfile();
}

App::~App()
{
  SAFE_DELETE(g_globalWorld);
  SAFE_DELETE(m_langTable);
  SAFE_DELETE(g_taskManagerInterface);
  SAFE_DELETE(g_controlHelpSystem);
#ifdef ATTRACTMODE_ENABLED
  SAFE_DELETE(m_attractMode);
#endif
  SAFE_DELETE(g_script);
  SAFE_DELETE(g_taskManager);
  SAFE_DELETE(g_particleSystem);
  SAFE_DELETE(g_camera);
  SAFE_DELETE(g_userInput);
  SAFE_DELETE(m_clientToServer);
  SAFE_DELETE(m_soundSystem);
  SAFE_DELETE(m_gameCursor);
  SAFE_DELETE(g_renderer);
#ifdef PROFILER_ENABLED
  SAFE_DELETE(m_profiler);
#endif
  SAFE_DELETE(g_prefsManager);
  SAFE_DELETE(m_resource);
}

void App::SetProfileName(const char* _profileName)
{
  strcpy(g_userProfileName, _profileName);

  if (stricmp(_profileName, "AttractMode") != 0)
  {
    g_prefsManager->SetString("UserProfile", g_userProfileName);
    g_prefsManager->Save();
  }
}

void App::SetLanguage(const char* _language, bool _test)
{
  //
  // Delete existing language data

  if (m_langTable)
  {
    delete m_langTable;
    m_langTable = nullptr;
    g_langTable = m_langTable;
  }

  //
  // Load the language text file

  char langFilename[256];
  sprintf(langFilename, "Language/%s.txt", _language);

  m_langTable = new LangTable(langFilename);
  g_langTable = m_langTable;

  if (_test)
    m_langTable->TestAgainstEnglish();

  //
  // Load the MOD language file if it exists

  sprintf(langFilename, "strings_%s.txt", _language);
  TextReader* modLangFile = g_app->m_resource->GetTextReader(langFilename);
  if (!modLangFile)
  {
    sprintf(langFilename, "strings_default.txt");
    modLangFile = g_app->m_resource->GetTextReader(langFilename);
  }

  if (modLangFile)
  {
    delete modLangFile;
    m_langTable->ParseLanguageFile(langFilename);
  }

  //
  // Load localised fonts if they exist

  char fontFilename[256];
  sprintf(fontFilename, "Textures/SpeccyFont%s.bmp", _language);
  if (!g_app->m_resource->DoesTextureExist(fontFilename))
    sprintf(fontFilename, "Textures/SpeccyFontNormal.bmp");
  g_gameFont.Initialise(fontFilename);

  sprintf(fontFilename, "Textures/EditorFont%s.bmp", _language);
  if (!g_app->m_resource->DoesTextureExist(fontFilename))
    sprintf(fontFilename, "Textures/EditorFontNormal.bmp");
  g_editorFont.Initialise(fontFilename);

  if (g_inputManager)
    m_langTable->RebuildTables();
}

void App::UpdateDifficultyFromPreferences()
{
  // This method is called to make sure that the difficulty setting
  // used to control the game play (g_difficultyLevel) is
  // consistent with the user preferences.

  // Preferences value is 1-based, g_difficultyLevel is 0-based.
  g_difficultyLevel = g_prefsManager->GetInt(OTHER_DIFFICULTY, 1) - 1;
  if (g_difficultyLevel < 0)
    g_difficultyLevel = 0;
}

const char* App::GetProfileDirectory()
{
  {
    return "";
  }
}

const char* App::GetPreferencesPath()
{
  // good leak #1
  static char* path = nullptr;

  if (path == nullptr)
  {
    const char* profileDir = GetProfileDirectory();
    path = new char[strlen(profileDir) + 32];
    sprintf(path, "%spreferences.txt", profileDir);
  }

  return path;
}

const char* App::GetScreenshotDirectory()
{
  return "";
}

bool App::LoadProfile()
{
  DebugTrace("Loading profile %s\n", g_userProfileName);

  if ((stricmp(g_userProfileName, "AccessAllAreas") == 0 || stricmp(g_userProfileName, "AttractMode") == 0) && g_gameMode != GameModePrologue)
  {
    // Cheat username that opens all locations
    // aimed at beta testers who've completed the game already

    if (g_globalWorld)
    {
      delete g_globalWorld;
      g_globalWorld = nullptr;
    }

    g_globalWorld = new GlobalWorld();
    g_globalWorld->LoadGame("GameUnlockAll.txt");
    for (int i = 0; i < g_globalWorld->m_buildings.Size(); ++i)
    {
      GlobalBuilding* building = g_globalWorld->m_buildings[i];
      if (building && building->m_type == Building::TypeTrunkPort)
        building->m_online = true;
    }
    for (int i = 0; i < g_globalWorld->m_locations.Size(); ++i)
    {
      GlobalLocation* loc = g_globalWorld->m_locations[i];
      loc->m_available = true;
    }
  }
  else
  {
    if (g_globalWorld)
    {
      delete g_globalWorld;
      g_globalWorld = nullptr;
    }

    g_globalWorld = new GlobalWorld();
    g_globalWorld->LoadGame(m_gameDataFile);
  }

  return true;
}

bool App::HasBoughtGame()
{
  return true;
}

void App::LoadPrologue()
{
  g_gameMode = GameModePrologue;

  m_soundSystem->StopAllSounds(WorldObjectId(), "Music");

  strcpy(m_gameDataFile, "game_demo2.txt");
  LoadProfile();

  g_requestedLocationId = g_globalWorld->GetLocationId("launchpad");
  GlobalLocation* gloc = g_globalWorld->GetLocation(g_requestedLocationId);
  strcpy(m_requestedMap, gloc->m_mapFilename);
  strcpy(m_requestedMission, gloc->m_missionFilename);

  g_atMainMenu = false;

  g_prefsManager->SetInt("RenderSpecialLighting", 1);
  g_prefsManager->SetInt("CurrentGameMode", 0);
  g_prefsManager->Save();
}

void App::LoadCampaign()
{
  m_soundSystem->StopAllSounds(WorldObjectId(), "Music");

  // g_atMainMenu = false;

  strcpy(m_gameDataFile, "Game.txt");
  LoadProfile();
  g_gameMode = GameModeCampaign;
  g_requestedLocationId = -1;
  g_prefsManager->SetInt("RenderSpecialLighting", 0);
  g_prefsManager->SetInt("CurrentGameMode", 1);
  g_prefsManager->Save();
}
