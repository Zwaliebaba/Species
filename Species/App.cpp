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
#include "UserInput.h"

void SetPreferenceOverrides(); // See main.cpp

App* g_app = nullptr;

#ifdef DEMO2
#define GAMEDATAFILE "game_demo2.txt"
#else
#ifdef DEMOBUILD
#define GAMEDATAFILE "game_demo.txt"
#else
#define GAMEDATAFILE "Game.txt"
#endif
#endif

App::App()
  : m_userInput(nullptr),
    m_resource(nullptr),
    m_soundSystem(nullptr),
    m_particleSystem(nullptr),
    m_langTable(nullptr),
    m_profiler(nullptr),
    m_globalWorld(nullptr),
    m_location(nullptr),
    m_locationId(-1),
    m_camera(nullptr),
    m_server(nullptr),
    m_clientToServer(nullptr),
    m_renderer(nullptr),
    m_locationInput(nullptr),
    m_locationEditor(nullptr),
    m_taskManager(nullptr),
    m_script(nullptr),
    m_testHarness(nullptr),
    m_startSequence(nullptr),
    m_attractMode(nullptr),
    m_controlHelpSystem(nullptr),
    m_gameMenu(nullptr),
    m_negativeRenderer(false),
    m_difficultyLevel(0),
    m_largeMenus(false),
    m_paused(false),
    m_editing(false),
    m_requestedLocationId(-1),
    m_requestToggleEditing(false),
    m_requestQuit(false),
    m_levelReset(false),
    m_atMainMenu(false),
    m_gameMode(GameModeNone)
{
  g_app = this;

  // Load resources

  m_resource = new Resource();

  g_prefsManager = new PrefsManager(GetPreferencesPath());
  SetPreferenceOverrides();


  m_negativeRenderer = g_prefsManager->GetInt("RenderNegative", 0) ? true : false;
  if (m_negativeRenderer)
    m_backgroundColour.Set(255, 255, 255, 255);
  else
    m_backgroundColour.Set(0, 0, 0, 0);

  UpdateDifficultyFromPreferences();

#ifdef PROFILER_ENABLED
  m_profiler = new Profiler();
#endif

  m_renderer = new Renderer();
  m_renderer->Initialise();

  // Make sure that resources are now available - either the .dat files
  // or the data directory must exist

  SoundStreamDecoder* ssd = m_resource->GetSoundStreamDecoder("Sounds/ABlaster");
  ASSERT_TEXT(ssd, "Couldn't find sound resources. This is probably because\n" "sounds.dat isn't in the working directory.");
  delete ssd;

  int textureId = m_resource->GetTexture("Textures/EditorFontNormal.bmp");

  m_gameCursor = new GameCursor();
  m_soundSystem = new SoundSystem();
  m_clientToServer = new ClientToServer();
  m_userInput = new UserInput();
  //    m_location          = new Location();
  //    m_locationInput		= new LocationInput();

  m_camera = new Camera();
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

#ifdef TARGET_OS_VISTA
  if (strlen(g_saveFile) > 0) { SetProfileName(g_saveFile); }
#endif

  m_particleSystem = new ParticleSystem();
  m_taskManager = new TaskManager();
  m_script = new Script();
#ifdef ATTRACTMODE_ENABLED
  m_attractMode = new AttractMode();
#endif
  m_controlHelpSystem = new ControlHelpSystem();

  m_taskManagerInterface = new TaskManagerInterfaceIcons();

  m_soundSystem->Initialise();

  int menuOption = g_prefsManager->GetInt(OTHER_LARGEMENUS, 0);
  if (menuOption == 2) // (todo) or is running in media center and tenFootMode == -1
    m_largeMenus = true;

  //
  // Load save games

  bool profileLoaded = LoadProfile();
}

App::~App()
{
  SAFE_DELETE(m_globalWorld);
  SAFE_DELETE(m_langTable);
#ifdef DEMOBUILD
#endif
  SAFE_DELETE(m_taskManagerInterface);
  SAFE_DELETE(m_controlHelpSystem);
#ifdef ATTRACTMODE_ENABLED
  SAFE_DELETE(m_attractMode);
#endif
  SAFE_DELETE(m_script);
  SAFE_DELETE(m_taskManager);
  SAFE_DELETE(m_particleSystem);
  SAFE_DELETE(m_camera);
  SAFE_DELETE(m_userInput);
  SAFE_DELETE(m_clientToServer);
  SAFE_DELETE(m_soundSystem);
  SAFE_DELETE(m_gameCursor);
  SAFE_DELETE(m_renderer);
#ifdef PROFILER_ENABLED
  SAFE_DELETE(m_profiler);
#endif
  SAFE_DELETE(g_prefsManager);
  SAFE_DELETE(m_resource);
}

void App::SetProfileName(const char* _profileName)
{
  strcpy(m_userProfileName, _profileName);

  if (stricmp(_profileName, "AttractMode") != 0)
  {
    g_prefsManager->SetString("UserProfile", m_userProfileName);
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
  }

  //
  // Load the language text file

  char langFilename[256];
#if defined(TARGET_OS_LINUX) && defined(TARGET_DEMOGAME)
  sprintf(langFilename, "Language/%sDemo.txt", _language);
#else
  sprintf(langFilename, "Language/%s.txt", _language);
#endif

  m_langTable = new LangTable(langFilename);

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
  // used to control the game play (g_app->m_difficultyLevel) is
  // consistent with the user preferences.

  // Preferences value is 1-based, m_difficultyLevel is 0-based.
  m_difficultyLevel = g_prefsManager->GetInt(OTHER_DIFFICULTY, 1) - 1;
  if (m_difficultyLevel < 0)
    m_difficultyLevel = 0;
}

#if defined(TARGET_OS_LINUX) || defined(TARGET_OS_MACOSX)
#include <sys/types.h>
#include <sys/stat.h>
#endif

const char* App::GetProfileDirectory()
{
#if defined(TARGET_OS_LINUX)

  static char userdir[256]; const char* home = getenv("HOME"); if (home != NULL)
  {
    sprintf(userdir, "%s/.darwinia", home);
    mkdir(userdir, 0777);

    sprintf(userdir, "%s/.darwinia/%s/", home, SPECIES_GAMETYPE);
    mkdir(userdir, 0777);
    return userdir;
  }
  else // Current directory if no home
    return "";

#elif defined(TARGET_OS_MACOSX)

  static char userdir[256]; const char* home = getenv("HOME"); if (home != NULL)
  {
    sprintf(userdir, "%s/Library", home);
    mkdir(userdir, 0777);

    sprintf(userdir, "%s/Library/Application Support", home);
    mkdir(userdir, 0777);

    sprintf(userdir, "%s/Library/Application Support/Darwinia", home);
    mkdir(userdir, 0777);

    sprintf(userdir, "%s/Library/Application Support/Darwinia/%s/", home, SPECIES_GAMETYPE);
    mkdir(userdir, 0777);

    return userdir;
  }
  else // Current directory if no home
    return "";

#else
#ifdef TARGET_OS_VISTA
  if (IsRunningVista())
  {
    static char userdir[256];

    PWSTR path;
    SHGetKnownFolderPath(FOLDERID_SavedGames, 0, NULL, &path);
    wcstombs(userdir, path, sizeof(userdir));
    CoTaskMemFree(path);

#ifdef TARGET_VISTA_DEMO2
  const char* subdir = "\\Darwinia Demo 2\\";
#else
  const char* subdir = "\\Darwinia\\";
#endif
  strncat(userdir, subdir, sizeof(userdir)); CreateDirectory(userdir); return userdir;
    }
    else
#endif // TARGET_OS_VISTA
  {
    return "";
  }
#endif
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
#ifdef TARGET_OS_VISTA
  static char dir[MAX_PATH]; SHGetFolderPath(NULL, CSIDL_DESKTOP, NULL, SHGFP_TYPE_CURRENT, dir); sprintf(dir, "%s\\", dir); return dir;
#else
  return "";
#endif
}

bool App::LoadProfile()
{
  DebugTrace("Loading profile %s\n", m_userProfileName);

  if ((stricmp(m_userProfileName, "AccessAllAreas") == 0 || stricmp(m_userProfileName, "AttractMode") == 0) && g_app->m_gameMode !=
    GameModePrologue)
  {
    // Cheat username that opens all locations
    // aimed at beta testers who've completed the game already

    if (m_globalWorld)
    {
      delete m_globalWorld;
      m_globalWorld = nullptr;
    }

    m_globalWorld = new GlobalWorld();
    m_globalWorld->LoadGame("GameUnlockAll.txt");
    for (int i = 0; i < m_globalWorld->m_buildings.Size(); ++i)
    {
      GlobalBuilding* building = m_globalWorld->m_buildings[i];
      if (building && building->m_type == Building::TypeTrunkPort)
        building->m_online = true;
    }
    for (int i = 0; i < m_globalWorld->m_locations.Size(); ++i)
    {
      GlobalLocation* loc = m_globalWorld->m_locations[i];
      loc->m_available = true;
    }
  }
  else
  {
    if (m_globalWorld)
    {
      delete m_globalWorld;
      m_globalWorld = nullptr;
    }

    m_globalWorld = new GlobalWorld();
    m_globalWorld->LoadGame(m_gameDataFile);
  }

  return true;
}

bool App::HasBoughtGame()
{
#if defined(DEMOBUILD)
  return false;
#else
  return true;
#endif
}

void App::LoadPrologue()
{
  m_gameMode = GameModePrologue;

  m_soundSystem->StopAllSounds(WorldObjectId(), "Music");

  strcpy(m_gameDataFile, "game_demo2.txt");
  LoadProfile();

  m_requestedLocationId = m_globalWorld->GetLocationId("launchpad");
  GlobalLocation* gloc = m_globalWorld->GetLocation(m_requestedLocationId);
  strcpy(m_requestedMap, gloc->m_mapFilename);
  strcpy(m_requestedMission, gloc->m_missionFilename);

  m_atMainMenu = false;

  g_prefsManager->SetInt("RenderSpecialLighting", 1);
  g_prefsManager->SetInt("CurrentGameMode", 0);
  g_prefsManager->Save();
}

void App::LoadCampaign()
{
  m_soundSystem->StopAllSounds(WorldObjectId(), "Music");

  //m_atMainMenu = false;

  strcpy(m_gameDataFile, "Game.txt");
  LoadProfile();
  m_gameMode = GameModeCampaign;
  m_requestedLocationId = -1;
  g_prefsManager->SetInt("RenderSpecialLighting", 0);
  g_prefsManager->SetInt("CurrentGameMode", 1);
  g_prefsManager->Save();
}
