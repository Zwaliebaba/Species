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
#include "WorldTypeRoster.h"
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
  if (!g_app || !g_resource)
    return;

  TextReader* reader = g_resource->GetTextReader("DefaultPreferences.txt");
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
    m_locationInput(nullptr),
    m_startSequence(nullptr),
    m_gameMenu(nullptr),
    m_levelReset(false)
{
  g_app = this;
  g_appCommands = this;

  // Load resources

  m_resource = new Resource();
  g_resource = m_resource;

  PrefsManager::SetDefaultsProvider(&ApplyShippedPreferenceDefaults);
  g_prefsManager = new PrefsManager(GetPreferencesPath());

  g_negativeRenderer = g_prefsManager->GetInt("RenderNegative", 0) ? true : false;
  if (g_negativeRenderer)
    g_backgroundColour.Set(255, 255, 255, 255);
  else
    g_backgroundColour.Set(0, 0, 0, 0);

  UpdateDifficultyFromPreferences();

#ifdef PROFILER_ENABLED
  m_profiler = std::make_unique<Profiler>();
  g_profiler = m_profiler.get();
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

  // Before the sound system exists, and well before Initialise() reads a
  // blueprint that has to resolve an entity or building type name.
  InstallWorldTypeRoster();

  g_gameCursor = new GameCursor();
  m_soundSystem = std::make_unique<SoundSystem>();
  g_soundSystem = m_soundSystem.get();
  m_clientToServer = std::make_unique<ClientToServer>();
  g_clientToServer = m_clientToServer.get();
  g_userInput = new UserInput();
  //    g_location          = new Location();
  //    m_locationInput		= new LocationInput();

  g_camera = new Camera();
  m_gameMenu = new GameMenu();

  m_gameDataFile = "Game.txt";

  //
  // Determine default language if possible

  const char* language = g_prefsManager->GetString("TextLanguage");
  if (stricmp(language, "unknown") == 0)
  {
    char* defaultLang = g_systemInfo->m_localeInfo.m_language;
    const std::string langFilename = std::format("Language/{}.txt", defaultLang);
    if (DoesFileExist(langFilename.c_str()))
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
  m_attractMode = std::make_unique<AttractMode>();
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

// The two factories AppCommands declares. GameLogic's preferences windows
// rebuild these objects when a setting changes and cannot name the concrete
// types; this is where `new` happens on their behalf.
RendererAccess* App::CreateRenderer() { return new Renderer(); }

TaskManagerInterfaceAccess* App::CreateTaskManagerInterface() { return new TaskManagerInterfaceIcons(); }


// THIS DESTRUCTOR NEVER RUNS. Species/Main.cpp calls Finalise() and then
// exit(0), which does not unwind the stack, and nothing anywhere deletes
// g_app. Every line below is unreached today. That is not a reason to leave it
// wrong -- it is the reason the conversion is safe to make, and the reason the
// smoke test cannot confirm it. Recorded on ownership T6.
//
// THE ORDER BELOW IS BYTE-FOR-BYTE THE SEQUENCE THE SAFE_DELETEs HAD. The
// unique_ptr members are reset explicitly, in place, rather than left to the
// implicit destructor -- which would run them in reverse declaration order and
// interleave them differently with the raw deletes that remain.
App::~App()
{
  SAFE_DELETE(g_globalWorld);

  m_langTable.reset();
  g_langTable = nullptr;

  SAFE_DELETE(g_taskManagerInterface);
  SAFE_DELETE(g_controlHelpSystem);
#ifdef ATTRACTMODE_ENABLED
  m_attractMode.reset();
#endif
  SAFE_DELETE(g_script);
  SAFE_DELETE(g_taskManager);
  SAFE_DELETE(g_particleSystem);
  SAFE_DELETE(g_camera);
  SAFE_DELETE(g_userInput);

  m_clientToServer.reset();
  g_clientToServer = nullptr;

  m_soundSystem.reset();
  g_soundSystem = nullptr;

  SAFE_DELETE(g_gameCursor);
  SAFE_DELETE(g_renderer);
#ifdef PROFILER_ENABLED
  m_profiler.reset();
  g_profiler = nullptr;
#endif
  SAFE_DELETE(g_prefsManager);

  // NOT deleted here, and this is a behaviour change: SAFE_DELETE(m_resource)
  // used to run. Species/Main.cpp's Finalise() also does `delete g_resource`,
  // and THAT is the one that actually executes, so the two together were a
  // latent double delete that only the unreachability of this destructor kept
  // from firing. Ownership is stated where it really lives rather than
  // duplicated here.
  m_resource = nullptr;
}

void App::SetProfileName(const char* _profileName)
{
  g_userProfileName = _profileName;

  if (stricmp(_profileName, "AttractMode") != 0)
  {
    g_prefsManager->SetString("UserProfile", g_userProfileName.c_str());
    g_prefsManager->Save();
  }
}

void App::SetLanguage(const char* _language, bool _test)
{
  //
  // Delete existing language data

  if (m_langTable)
  {
    m_langTable.reset();
    g_langTable = nullptr;
  }

  //
  // Load the language text file

  std::string langFilename = std::format("Language/{}.txt", _language);

  m_langTable = std::make_unique<LangTable>(langFilename.c_str());
  g_langTable = m_langTable.get();

  if (_test)
    m_langTable->TestAgainstEnglish();

  //
  // Load the MOD language file if it exists

  langFilename = std::format("strings_{}.txt", _language);
  TextReader* modLangFile = g_resource->GetTextReader(langFilename.c_str());
  if (!modLangFile)
  {
    langFilename = "strings_default.txt";
    modLangFile = g_resource->GetTextReader(langFilename.c_str());
  }

  if (modLangFile)
  {
    delete modLangFile;
    m_langTable->ParseLanguageFile(langFilename.c_str());
  }

  //
  // Load localised fonts if they exist

  std::string fontFilename = std::format("Textures/SpeccyFont{}.bmp", _language);
  if (!g_resource->DoesTextureExist(fontFilename.c_str()))
    fontFilename = "Textures/SpeccyFontNormal.bmp";
  g_gameFont.Initialise(fontFilename.c_str());

  fontFilename = std::format("Textures/EditorFont{}.bmp", _language);
  if (!g_resource->DoesTextureExist(fontFilename.c_str()))
    fontFilename = "Textures/EditorFontNormal.bmp";
  g_editorFont.Initialise(fontFilename.c_str());

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
  // Built once and cached: the return type is const char* and callers hold it.
  static std::string path;

  if (path.empty())
  {
    path = std::format("{}preferences.txt", GetProfileDirectory());
  }

  return path.c_str();
}

const char* App::GetScreenshotDirectory()
{
  return "";
}

bool App::LoadProfile()
{
  DebugTrace("Loading profile {}\n", g_userProfileName);

  if ((stricmp(g_userProfileName.c_str(), "AccessAllAreas") == 0 || stricmp(g_userProfileName.c_str(), "AttractMode") == 0) &&
      g_gameMode != GameModePrologue)
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
    for (auto const& building : g_globalWorld->m_buildings)
    {
      if (building && building->m_type == Building::TypeTrunkPort)
        building->m_online = true;
    }
    for (auto const& loc : g_globalWorld->m_locations)
    {
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
    g_globalWorld->LoadGame(m_gameDataFile.c_str());
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

  m_gameDataFile = "game_demo2.txt";
  LoadProfile();

  g_requestedLocationId = g_globalWorld->GetLocationId("launchpad");
  GlobalLocation* gloc = g_globalWorld->GetLocation(g_requestedLocationId);
  g_requestedMap = gloc->m_mapFilename;
  g_requestedMission = gloc->m_missionFilename;

  g_atMainMenu = false;

  g_prefsManager->SetInt("RenderSpecialLighting", 1);
  g_prefsManager->SetInt("CurrentGameMode", 0);
  g_prefsManager->Save();
}

void App::LoadCampaign()
{
  m_soundSystem->StopAllSounds(WorldObjectId(), "Music");

  // g_atMainMenu = false;

  m_gameDataFile = "Game.txt";
  LoadProfile();
  g_gameMode = GameModeCampaign;
  g_requestedLocationId = -1;
  g_prefsManager->SetInt("RenderSpecialLighting", 0);
  g_prefsManager->SetInt("CurrentGameMode", 1);
  g_prefsManager->Save();
}
