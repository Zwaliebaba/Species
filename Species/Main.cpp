#include "pch.h"
#include "Globals.h"
#include "Main.h"
#include "App.h"
#include "Camera.h"
#include "ClientToServer.h"
#include "ControlHelp.h"
#include "Debug.h"
#include "DebugMenu.h"
#include "Eclipse.h"
#include "Explosion.h"
#include "FilePaths.h"
#include "GameMenu.h"
#include "GlobalWorld.h"
#include "GlobalWorldEditorWindow.h"
#include "HiResTime.h"
#include "Input.h"
#include "InputDriverChord.h"
#include "InputDriverConjoin.h"
#include "InputDriverIdle.h"
#include "InputDriverInvert.h"
#include "InputDriverWin32.h"
#include "Landscape.h"
#include "LanguageTable.h"
#include "Location.h"
#include "LocationEditor.h"
#include "LocationInput.h"
#include "MainMenus.h"
#include "MathUtils.h"
#include "ParticleSystem.h"
#include "Preferences.h"
#include "Profiler.h"
#include "Renderer.h"
#include "Resource.h"
#include "Script.h"
#include "Server.h"
#include "ServerToClientLetter.h"
#include "ServerUpdates.h"
#include "SoundLibrary3d.h"
#include "SoundSystem.h"
#include "StartSequence.h"
#include "SystemInfo.h"
#include "TargetCursor.h"
#include "TaskManager.h"
#include "TaskManagerInterface.h"
#include "TaskManagerInterfaceIcons.h"
#include "Team.h"
#include "TextStreamReaders.h"
#include "Unit.h"
#include "UpdateAvailableWindow.h"
#include "UserInput.h"
#include "Water.h"
#include "Win32EventHandler.h"
#include "WindowManager.h"
#include "WorldPointers.h"
#include "AppState.h"

#define TARGET_FRAME_RATE_INCREMENT 0.25f


namespace Species
{
  static void Finalise();

  // ******************
  //  Global Variables
  // ******************

  // g_startTime moved to ClientToServer::m_startTime — it was derived entirely from
  // arriving letters, and this was the only translation unit that read it.

  // ******************
  //  Local Functions
  // ******************

  void UpdateAdvanceTime()
  {
    int recordDemo = g_prefsManager->GetInt("RecordDemo");
    if (recordDemo == 1 || recordDemo == 2)
    {
      int demoFrameRate = g_prefsManager->GetInt("DemoFrameRate", 25);
      g_advanceTime = 1.0f / static_cast<float>(demoFrameRate);
      IncrementFakeTime(1.0f / static_cast<double>(demoFrameRate));
      // g_gameTime += g_advanceTime;
      g_gameTime = GetHighResTime();
      g_predictionTime = static_cast<float>(g_gameTime - g_lastServerAdvance) - 0.07f;
    }
    else
    {
      double realTime = GetHighResTime();
      g_advanceTime = static_cast<float>(realTime - g_gameTime);
      if (g_advanceTime > 0.25f)
        g_advanceTime = 0.25f;
      g_gameTime = realTime;

      float prevPredictionTime = g_predictionTime;
      g_predictionTime = static_cast<float>(realTime - g_lastServerAdvance) - 0.07f;

      // DebugTrace( "Change = %6.3f\n", g_predictionTime - prevPredictionTime );
    }
  }

  double GetNetworkTime() { return g_lastProcessedSequenceId * 0.1f; }

  void UpdateTargetFrameRate(int _currentSlice)
  {
    int numUpdatesToProcess = g_app->m_clientToServer->m_lastValidSequenceIdFromServer - g_lastProcessedSequenceId;
    int numSlicesPending = numUpdatesToProcess * NUM_SLICES_PER_FRAME - _currentSlice;
    float timeSinceStartOfAdvance = g_gameTime - g_lastServerAdvance;
    int numSlicesThatShouldBePending = 10 - timeSinceStartOfAdvance * 10.0f;

    // Increase or lower the target frame rate, depending on how far behind schedule
    // we are
    //	if( numSlicesPending > NUM_SLICES_PER_FRAME/2 )
    float amountBehind = numSlicesPending - numSlicesThatShouldBePending;
    g_targetFrameRate -= 0.1f * amountBehind * TARGET_FRAME_RATE_INCREMENT;

    // Make sure the target frame rate is within sensible bounds
    if (g_targetFrameRate < 2.0f)
      g_targetFrameRate = 2.0f;
    else if (g_targetFrameRate > 85.0f)
      g_targetFrameRate = 85.0f;
  }

  /*
  int GetNumSlicesToAdvance()
  {
      int slicesPerSecond = SERVER_ADVANCE_FREQ * NUM_SLICES_PER_FRAME;
      float ratio = (float)slicesPerSecond / (float)g_targetFrameRate;


      static float accumulator = 0.0f;
      accumulator += ratio;

      int returnVal = floorf(accumulator);

      accumulator -= (float)returnVal;

      return returnVal;
  }*/

  int GetNumSlicesToAdvance()
  {
    int numUpdatesToProcess = g_app->m_clientToServer->m_lastValidSequenceIdFromServer - g_lastProcessedSequenceId;
    int numSlicesPending = numUpdatesToProcess * NUM_SLICES_PER_FRAME;
    if (g_sliceNum != -1)
      numSlicesPending -= g_sliceNum;
    else if (g_sliceNum == -1)
      numSlicesPending -= 10;

    float timeSinceStartOfAdvance = g_gameTime - g_lastServerAdvance;
    // int numSlicesThatShouldBePending = 10 - timeSinceStartOfAdvance * 10.0f;

    int numSlicesToAdvance = timeSinceStartOfAdvance * 100;
    if (g_sliceNum != -1)
      numSlicesToAdvance -= g_sliceNum;
    if (g_sliceNum == -1)
      numSlicesToAdvance -= 10;

    // DEBUG_ASSERT( numSlicesToAdvance >= 0 );
    numSlicesToAdvance = std::max(numSlicesToAdvance, 0);
    numSlicesToAdvance = std::min(numSlicesToAdvance, 10);

    return numSlicesToAdvance;
  }

  bool ProcessServerLetters(ServerToClientLetter* letter)
  {
    switch (letter->m_type)
    {
    case ServerToClientLetter::LetterType::HelloClient:
      if (letter->m_ip == g_app->m_clientToServer->GetOurIP_Int())
        DebugTrace("CLIENT : Received HelloClient from Server\n");
      return true;

    case ServerToClientLetter::LetterType::GoodbyeClient:
      // g_location->RemoveTeam( letter->m_teamId );
      return true;

    case ServerToClientLetter::LetterType::TeamAssign:

      if (letter->m_ip == g_app->m_clientToServer->GetOurIP_Int())
        g_location->InitialiseTeam(letter->m_teamId, letter->m_teamType);
      else
        g_location->InitialiseTeam(letter->m_teamId, Team::TeamTypeRemotePlayer);
      return true;

    default:
      return false;
    }
  }

  bool WindowsOnScreen() { return EclGetWindows()->size() > 0; }

  void RemoveAllWindows()
  {
    std::vector<std::unique_ptr<EclWindow>>* windows = EclGetWindows();
    while (windows->size() > 0)
    {
      EclWindow* w = (*windows)[0].get();
      EclRemoveWindow(w->m_name);
    }
  }

  bool HandleCommonConditions()
  {
    bool curWindowHasFocus = g_eventHandler->WindowHasFocus();
    static bool oldWindowFocus = true;

    if (!curWindowHasFocus)
    {
      TheUserInput()->Advance();
      g_soundSystem->Advance();

      // Render twice to avoid double buffering artefacts
      TheRenderer()->Render();
      TheRenderer()->Render();
      return true;
    }

    if (g_requestQuit)
    {
      Finalise();
      exit(0);
    }

    return false;
  }

  unsigned char GenerateSyncValue()
  {
#ifdef TARGET_DEBUG
  // THE ACCUMULATORS, and the one place in this function a conversion can go
  // wrong silently. Vector3's default constructor zeroed and XMFLOAT3's does
  // not; these sum thousands of positions and the total feeds the desync
  // assert below, so starting from uninitialised stack would fire it
  // spuriously. XMVECTOR accumulators are explicitly zeroed and cannot repeat
  // the mistake. Component-wise float addition either way, so the arithmetic
  // is unchanged as well as the order.
  DirectX::XMVECTOR unitPosition = DirectX::XMVectorZero();
  DirectX::XMVECTOR entityPosition = DirectX::XMVectorZero();
  DirectX::XMVECTOR laserPosition = DirectX::XMVectorZero();
  DirectX::XMVECTOR effectsPosition = DirectX::XMVectorZero();

  for (int t = 0; t < NUM_TEAMS; ++t)
  {
    Team* team = &g_location->m_teams[t];
    if (team->m_teamType != Team::TeamTypeUnused)
    {
      for (int u = 0; u < team->m_units.Size(); ++u)
      {
        if (team->m_units.ValidIndex(u))
        {
          Unit* unit = team->m_units[u];
          unitPosition = DirectX::XMVectorAdd(unitPosition, DirectX::XMLoadFloat3(&unit->m_centrePos));
          for (int e = 0; e < unit->m_entities.Size(); ++e)
          {
            if (unit->m_entities.ValidIndex(e))
            {
              Entity* ent = unit->m_entities[e];
              unitPosition = DirectX::XMVectorAdd(unitPosition, DirectX::XMLoadFloat3(&ent->m_pos));
              unitPosition = DirectX::XMVectorAdd(unitPosition, DirectX::XMLoadFloat3(&ent->m_vel));
            }
          }
        }
      }

      for (int e = 0; e < team->m_others.Size(); ++e)
      {
        if (team->m_others.ValidIndex(e))
        {
          Entity* entity = team->m_others[e];
          entityPosition = DirectX::XMVectorAdd(entityPosition, DirectX::XMLoadFloat3(&entity->m_pos));
          entityPosition = DirectX::XMVectorAdd(entityPosition, DirectX::XMLoadFloat3(&entity->m_vel));
        }
      }
    }
  }

  for (int l = 0; l < g_location->m_lasers.Size(); ++l)
  {
    if (g_location->m_lasers.ValidIndex(l))
    {
      Laser* laser = g_location->m_lasers.GetPointer(l);
      laserPosition = DirectX::XMVectorAdd(laserPosition, DirectX::XMLoadFloat3(&laser->m_pos));
      laserPosition = DirectX::XMVectorAdd(laserPosition, DirectX::XMLoadFloat3(&laser->m_vel));
    }
  }

  for (int e = 0; e < g_location->m_effects.Size(); ++e)
  {
    if (g_location->m_effects.ValidIndex(e))
    {
      WorldObject* wobj = g_location->m_effects[e];
      effectsPosition = DirectX::XMVectorAdd(effectsPosition, DirectX::XMLoadFloat3(&wobj->m_pos));
      effectsPosition = DirectX::XMVectorAdd(effectsPosition, DirectX::XMLoadFloat3(&wobj->m_vel));
    }
  }

  // Left-associated exactly as the original was -- ((u + e) + l) + f. Float
  // addition is not associative, and this total is compared across clients.
  DirectX::XMFLOAT3 position;
  DirectX::XMStoreFloat3(
    &position, DirectX::XMVectorAdd(DirectX::XMVectorAdd(DirectX::XMVectorAdd(unitPosition, entityPosition), laserPosition), effectsPosition));
  float totalValue = position.x + position.y + position.z;

  totalValue -= static_cast<int>(totalValue);
  DEBUG_ASSERT(totalValue >= 0.0f && totalValue <= 1.0f);
  unsigned char syncValue = totalValue * 255;

  //    float unitPositionSync = ( unitPosition.x + unitPosition.y + unitPosition.z );
  //    float entityPositionSync = ( entityPosition.x + entityPosition.y + entityPosition.z );
  //    float laserPositionSync = ( laserPosition.x + laserPosition.y + laserPosition.z );
  //    float effectsPositionSync = ( effectsPosition.x + effectsPosition.y + effectsPosition.z );
  //
  //    unitPositionSync -= (int) unitPositionSync;
  //    entityPositionSync -= (int) entityPositionSync;
  //    laserPositionSync -= (int) laserPositionSync;
  //    effectsPositionSync -= (int) effectsPositionSync;
  //
  //    DebugTrace( "Frame [%3d] Sync [%3d] unit[%3d] entity[%3d] laser[%3d] effects[%3d]\n",
  //            g_lastProcessedSequenceId, syncValue,
  //            (int)(unitPositionSync*255),
  //            (int)(entityPositionSync*255),
  //            (int)(laserPositionSync*255),
  //            (int)(effectsPositionSync*255) );

  return syncValue;

#else

  return 255 * syncfrand();

#endif
  }

void LocationGameLoop()
{
  bool iAmAClient = true;
  bool iAmAServer = g_prefsManager->GetInt("IAmAServer") ? true : false;

  double nextServerAdvanceTime = GetHighResTime();
  double nextIAmAliveMessage = GetHighResTime();
  double heavyWeightAdvanceStartTime = -1;
  double serverAdvanceStartTime = -1;
  double lastRenderTime = GetHighResTime();

  TeamControls teamControls;

  g_sliceNum = -1;

  TheRenderer()->StartFadeIn(0.6f);
  g_soundSystem->TriggerOtherEvent("EnterLocation", SoundSourceBlueprint::TypeAmbience);

  //
  // Main loop

  bool fadingOut = false;
  while (true)
  {
    if (!fadingOut)
    {
      if (g_requestedLocationId != g_locationId)
      {
        g_renderer->StartFadeOut();
        fadingOut = true;
      }
    }
    else
    {
      if (TheRenderer()->IsFadeComplete())
      {
        TheControlHelp()->Shutdown();
        break;
      }
    }

    g_inputManager->PollForEvents();
    if (g_inputManager->controlEvent(ControlType::ControlMenuEscape) && TheRenderer()->IsFadeComplete())
    {
      if (g_script && TheScript()->IsRunningScript())
      {
      }
      else
      {
        if (WindowsOnScreen())
          RemoveAllWindows();
        else if (TheTaskManagerInterface()->m_visible)
          TheTaskManagerInterface()->m_visible = false;
        else
        {
          TheCamera()->SetDebugMode(Camera::DebugModeAuto);
          EclRegisterWindow(std::make_unique<LocationWindow>());
        }
      }
      TheUserInput()->Advance();
    }

    if (HandleCommonConditions())
      continue;

    //
    // Get the time
    double timeNow = GetHighResTime();

    //
    // Advance the server
    if (iAmAServer)
    {
      if (timeNow > nextServerAdvanceTime)
      {
        g_server->Advance();
        nextServerAdvanceTime += SERVER_ADVANCE_PERIOD;
        if (timeNow > nextServerAdvanceTime)
          nextServerAdvanceTime = timeNow + SERVER_ADVANCE_PERIOD;
      }
    }

    if (!WindowsOnScreen())
      teamControls.Advance();

    if (iAmAClient)
    {
      START_PROFILE(g_profiler, "Client Main Loop");

      //
      // Send Client input to Server
      if (timeNow > nextIAmAliveMessage)
      {
        // Read the current teamControls from the inputManager

        bool chatLog = false;
        bool entityUnderMouse = false;
        int numMouseButtons = g_prefsManager->GetInt("ControlMouseButtons", 3);

        Team* team = g_location->GetMyTeam();
        if (team)
        {
          bool checkMouse = false;
          if (teamControls.m_unitMove)
            checkMouse = true;

          bool orderGiven = false;
          if (g_inputManager->getInputMode() == InputMode::INPUT_MODE_KEYBOARD && teamControls.m_primaryFireTarget)
            orderGiven = true;
          if (g_inputManager->getInputMode() == InputMode::INPUT_MODE_GAMEPAD && teamControls.m_secondaryFireDirected)
            orderGiven = true;

          if (team->GetMyEntity() && team->GetMyEntity()->m_type == Entity::TypeOfficer && orderGiven)
            checkMouse = true;

          if (checkMouse)
          {
            // We don't actually want to pass any left-clicks to the network system
            // If the user has left-clicked on another of his entities, because that
            // entity is about to be selected.  We don't want our original entity
            // walking up to him.
            WorldObjectId idUnderMouse;
            bool objectUnderMouse = g_app->m_locationInput->GetObjectUnderMouse(idUnderMouse, g_globalWorld->m_myTeamId);

            bool isCurrentEntity = (objectUnderMouse && idUnderMouse.GetUnitId() == -1 && idUnderMouse.GetIndex() == team->m_currentEntityId);
            bool isCurrentUnit = (objectUnderMouse && idUnderMouse.GetUnitId() != -1 && idUnderMouse.GetUnitId() == team->m_currentUnitId);

            entityUnderMouse = (objectUnderMouse && idUnderMouse.GetUnitId() != UNIT_BUILDINGS && !isCurrentEntity && !isCurrentUnit);

            if (idUnderMouse.GetUnitId() == UNIT_BUILDINGS)
            {
              // Focus the mouse on a Radar Dish if one exists under the mouse
              Building* building = g_location->GetBuilding(idUnderMouse.GetUniqueId());
              if (building && building->m_type == Building::TypeRadarDish)
                teamControls.m_mousePos = building->m_pos;
            }
          }
        }

        if (TheTaskManagerInterface()->m_visible || EclGetWindows()->size() != 0 || chatLog || entityUnderMouse)
          teamControls.ClearFlags();

        g_app->m_clientToServer->SendIAmAlive(g_globalWorld->m_myTeamId, teamControls);

        nextIAmAliveMessage += IAMALIVE_PERIOD;
        if (timeNow > nextIAmAliveMessage)
          nextIAmAliveMessage = timeNow + IAMALIVE_PERIOD;

        teamControls.Clear();
      }

      g_app->m_clientToServer->Advance();

      // UpdateTargetFrameRate(g_sliceNum);

      int slicesToAdvance = GetNumSlicesToAdvance();

      END_PROFILE(g_profiler, "Client Main Loop");

      // Do our heavy weight physics
      for (int i = 0; i < slicesToAdvance; ++i)
      {
        if (g_sliceNum == -1)
        {
          // Read latest update from Server
          std::unique_ptr<ServerToClientLetter> letter = g_app->m_clientToServer->GetNextLetter(g_lastProcessedSequenceId);

          if (letter)
          {
            DEBUG_ASSERT(letter->GetSequenceId() == g_lastProcessedSequenceId + 1);
            // g_app->m_clientToServer->m_lastServerLetterReceivedTime = GetHighResTime();

            // DebugTrace( "CLIENT : Processed update %d\n", letter->GetSequenceId() );
            // g_app->m_clientToServer->m_lastKnownSequenceIdFromServer = letter->GetSequenceId();
            bool handled = ProcessServerLetters(letter.get());
            if (handled == false)
              ProcessServerUpdates(letter.get());

            g_sliceNum = 0;
            heavyWeightAdvanceStartTime = timeNow;
            g_lastServerAdvance = static_cast<float>(letter->GetSequenceId()) * SERVER_ADVANCE_PERIOD + g_app->m_clientToServer->m_startTime;
            g_lastProcessedSequenceId = letter->GetSequenceId();
            // reset() at the point the delete was, not at the end of the block:
            // GenerateSyncValue() runs after it and the letter must already be
            // gone when it does, exactly as before.
            letter.reset();

            unsigned char sync = GenerateSyncValue();
            g_app->m_clientToServer->SendSyncronisation(g_lastProcessedSequenceId, sync);
          }
        }

        if (g_sliceNum != -1)
        {
          g_location->Advance(g_sliceNum);
          g_particleSystem->Advance(g_sliceNum);

          if (g_sliceNum < NUM_SLICES_PER_FRAME - 1)
            g_sliceNum++;
          else
          {
            g_sliceNum = -1;
            heavyWeightAdvanceStartTime = -1.0;
          }
        }
      }

      // Render
      UpdateAdvanceTime();
      lastRenderTime = GetHighResTime();
#ifdef PROFILER_ENABLED
      g_profiler->Advance();
#endif // PROFILER_ENABLED

      TheUserInput()->Advance();

      // The following are candidates for running in parallel
      // using something like OpenMP
      g_location->m_water->Advance();
      TheCamera()->Advance();
      g_app->m_locationInput->Advance();
      g_taskManager->Advance();
      TheTaskManagerInterface()->Advance();
      TheScript()->Advance();
      g_explosionManager.Advance();
      g_soundSystem->Advance();
      TheControlHelp()->Advance();

#ifdef ATTRACTMODE_ENABLED
      if (g_app->m_attractMode->m_running)
      {
        g_app->m_attractMode->Advance();
      }
#endif // ATTRACTMODE_ENABLED

      // DELETEME: for debug purposes only
      g_globalWorld->EvaluateEvents();

      TheRenderer()->Render();

      if (g_renderer->Fps() < 15)
        g_soundSystem->Advance();
    }
  }

  g_soundSystem->StopAllSounds(WorldObjectId(), "Ambience EnterLocation");
  g_soundSystem->TriggerOtherEvent("ExitLocation", SoundSourceBlueprint::TypeAmbience);

  g_explosionManager.Reset();

  if (g_globalWorld->GetLocationName(g_locationId))
    g_globalWorld->TransferSpirits(g_locationId);

  g_app->m_clientToServer->ClientLeave();
  // ClientLeave used to reset this itself, reaching down from the endpoint into
  // the game loop's own counter. It resets its own; this one is ours.
  g_lastProcessedSequenceId = -1;
  g_location->Empty();
  g_particleSystem->Empty();

  //	g_app->m_inLocation = false;
  //	g_requestedLocationId = false;

  delete g_location;
  g_location = nullptr;
  g_locationId = -1;

  delete g_app->m_locationInput;
  g_app->m_locationInput = nullptr;

  delete g_server;
  g_server = nullptr;

  g_taskManager->StopAllTasks();

  g_globalWorld->m_myTeamId = 255;
  g_globalWorld->EvaluateEvents();
}

#ifdef LOCATION_EDITOR
void LocationEditorLoop()
{
  while (!g_inputManager->controlEvent(ControlType::ControlMenuEscape))
  {
    g_inputManager->PollForEvents();

    if (HandleCommonConditions())
      continue;

    //
    // Get the time
    UpdateAdvanceTime();
    double timeNow = GetHighResTime();

    TheUserInput()->Advance();
    TheCamera()->Advance();
    TheLocationEditor()->Advance();
    g_soundSystem->Advance();
#ifdef PROFILER_ENABLED
    g_profiler->Advance();
#endif

    TheRenderer()->Render();
  }

  delete g_locationEditor;
  g_locationEditor = nullptr;

  g_location->Empty();
  delete g_location;
  g_location = nullptr;
  g_locationId = -1;
  g_requestedLocationId = -1;

  delete g_app->m_locationInput;
  g_app->m_locationInput = nullptr;
}
#endif // LOCATION_EDITOR

void GlobalWorldGameLoop()
{
  TheRenderer()->StartFadeIn(0.25f);

  g_soundSystem->TriggerOtherEvent("EnterGlobalWorld", SoundSourceBlueprint::TypeAmbience);

  while (g_requestedLocationId == -1 && !g_requestToggleEditing)
  {
    if (g_atMainMenu)
      break;

    g_inputManager->PollForEvents();

    if (g_inputManager->controlEvent(ControlType::ControlMenuEscape) && TheRenderer()->IsFadeComplete())
    {
      if (WindowsOnScreen())
        RemoveAllWindows();
      else
      {
        TheCamera()->SetDebugMode(Camera::DebugModeAuto);
        EclRegisterWindow(std::make_unique<MainMenuWindow>());
      }
      TheUserInput()->Advance();
    }

    if (HandleCommonConditions())
      continue;

    // Get the time
    UpdateAdvanceTime();
    double timeNow = GetHighResTime();

    TheScript()->Advance();
    g_globalWorld->Advance();
    TheUserInput()->Advance();
    TheCamera()->Advance();
    g_soundSystem->Advance();

#ifdef ATTRACTMODE_ENABLED
    g_app->m_attractMode->Advance();
#endif
#ifdef PROFILER_ENABLED
    g_profiler->Advance();
#endif // PROFILER_ENABLED

    g_globalWorld->EvaluateEvents();

    TheRenderer()->Render();
  }

  if (g_requestToggleEditing)
  {
    g_editing = true;
    g_requestToggleEditing = false;
  }

  g_soundSystem->StopAllSounds(WorldObjectId(), "Ambience EnterGlobalWorld");
}

// *** GlobalWorldEditorLoop
void GlobalWorldEditorLoop()
{
  TheCamera()->SetDebugMode(Camera::DebugModeAlways);

  auto ownedGwe = std::make_unique<GlobalWorldEditorWindow>();
  GlobalWorldEditorWindow* gweWindow = ownedGwe.get();
  EclRegisterWindow(std::move(ownedGwe));

  while (g_requestedLocationId == -1 && !g_requestToggleEditing)
  {
    g_inputManager->PollForEvents();

    if (g_inputManager->controlEvent(ControlType::ControlMenuEscape))
    {
      g_editing = false;
      return;
    }

    if (HandleCommonConditions())
      continue;

    //
    // Get the time
    UpdateAdvanceTime();
    double timeNow = GetHighResTime();

    g_globalWorld->Advance();
    TheUserInput()->Advance();
    TheCamera()->Advance();
    g_soundSystem->Advance();
#ifdef PROFILER_ENABLED
    g_profiler->Advance();
#endif // PROFILER_ENABLED

    TheRenderer()->Render();
  }

  if (g_requestToggleEditing)
  {
    g_editing = false;
    g_requestToggleEditing = false;
  }
}

void InitialiseInputManager()
{
  // ORDER IS LOAD-BEARING. parseInputSpecTokens offers a spec to each driver in
  // turn and takes the first that accepts it, so the three combinators have to
  // come before W32: each one scans for its own operator (&&, ++, not) and hands
  // the parts back round, and W32 would otherwise swallow the first part alone.
  g_inputManager = new InputManager;
  g_inputManager->addDriver(new ConjoinInputDriver());
  g_inputManager->addDriver(new ChordInputDriver());
  g_inputManager->addDriver(new InvertInputDriver());
  g_inputManager->addDriver(new IdleInputDriver());
  g_inputManager->addDriver(new W32InputDriver());
  {
    // Read Darwinia default input preferences file
    TextReader* inputPrefsReader = g_resource->GetTextReader(InputPrefs::GetSystemPrefsPath());
    if (inputPrefsReader)
    {
      ASSERT_TEXT(inputPrefsReader->IsOpen(), "Couldn't open input preferences file: {}\n", InputPrefs::GetSystemPrefsPath());
      g_inputManager->parseInputPrefs(*inputPrefsReader);
      delete inputPrefsReader;
    }

    // Override defaults with keyboard specific file, if applicable
    TextReader* localeInputPrefsReader = g_resource->GetTextReader(InputPrefs::GetLocalePrefsPath());
    if (localeInputPrefsReader)
    {
      if (localeInputPrefsReader->IsOpen())
        g_inputManager->parseInputPrefs(*localeInputPrefsReader, true);
      delete localeInputPrefsReader;
    }

    // Override again with user specified bindings
    TextReader* userInputPrefsReader = new TextFileReader(InputPrefs::GetUserPrefsPath());
    if (userInputPrefsReader)
    {
      if (userInputPrefsReader->IsOpen())
        g_inputManager->parseInputPrefs(*userInputPrefsReader, true);
      delete userInputPrefsReader;
    }
  }
}

void Initialise()
{
  //
  // Initialise all our basic objects

  g_systemInfo = new SystemInfo;
  InitialiseHighResTime();

  g_eventHandler = new W32EventHandler();
  g_app = new App();

  InitialiseInputManager();

  g_target = new TargetCursor();
  EntityBlueprint::Initialise();
  g_windowManager->HideMousePointer();

  //
  // Start on a specific level if the prefs file tells us to

  const char* startMap = g_prefsManager->GetString("StartMap");
  if (startMap && g_appCommands->HasBoughtGame())
  {
    int requestedLocationId = g_globalWorld->GetLocationId(startMap);
    GlobalLocation* gloc = g_globalWorld->GetLocation(requestedLocationId);

    if (gloc)
    {
      g_requestedLocationId = requestedLocationId;
      g_requestedMap = gloc->m_mapFilename;
      g_requestedMission = gloc->m_missionFilename;
    }
  }

  TheRenderer()->SetOpenGLState();
}

void Finalise()
{
  delete g_soundLibrary3d;
  g_soundLibrary3d = nullptr;

  delete g_resource;
  delete g_windowManager;
}

void RunBootLoaders()
{
  if (g_appCommands->HasBoughtGame() && g_prefsManager->GetInt("CurrentGameMode", 1) == 1)
  {
    const char* loaderName = g_prefsManager->GetString("BootLoader", "none");

    g_app->m_startSequence = new StartSequence();
    while (true)
    {
      UpdateAdvanceTime();
      bool amIDone = g_app->m_startSequence->Advance();
      if (amIDone)
        break;
    }

    delete g_app->m_startSequence;
    g_app->m_startSequence = nullptr;

    TheCamera()->SetTarget(DirectX::XMFLOAT3(1000, 500, 1000), DirectX::XMFLOAT3(0, -0.5f, -1));
    TheCamera()->CutToTarget();

    g_inputManager->Advance(); // clears g_keyDeltas[KEY_ESC]
    g_inputManager->Advance();
  }
}

void EnterLocation()
{
  bool iAmAServer = g_prefsManager->GetInt("IAmAServer") ? true : false;
  if (!g_editing)
  {
    if (iAmAServer)
    {
      g_server = new Server();
      g_server->Initialise(g_profiler);
    }

    g_app->m_clientToServer->ClientJoin();
  }

  g_location = new Location();
  g_app->m_locationInput = new LocationInput();
  g_location->Init(g_requestedMission.c_str(), g_requestedMap.c_str());
  g_locationId = g_requestedLocationId;

  TheCamera()->UpdateEntityTrackingMode();

  if (!g_editing)
  {
    if (iAmAServer)
    {
      g_app->m_clientToServer->RequestTeam(Team::TeamTypeCPU, -1);
      g_app->m_clientToServer->RequestTeam(Team::TeamTypeCPU, -1);
    }
    g_app->m_clientToServer->RequestTeam(Team::TeamTypeLocalPlayer, -1);
  }

  constexpr float borderSize = 200.0f;
  float minX = -borderSize;
  float maxX = g_location->m_landscape.GetWorldSizeX() + borderSize;
  TheCamera()->SetBounds(minX, maxX, minX, maxX);
  TheCamera()->SetTarget(DirectX::XMFLOAT3(maxX, 1000, maxX), DirectX::XMFLOAT3(-1, -0.7f, -1)); // Incase start doesn't exist
  TheCamera()->SetTarget("start");
  TheCamera()->CutToTarget();

  if (g_editing)
  {
#ifdef LOCATION_EDITOR
    g_locationEditor = new LocationEditor();
    TheCamera()->SetDebugMode(Camera::DebugModeAlways);

    LocationEditorLoop();
#endif // LOCATION_EDITOR
  }
  else
  {
    TheCamera()->SetDebugMode(Camera::DebugModeAuto);
    TheCamera()->RequestMode(Camera::Mode::ModeFreeMovement);

    LocationGameLoop();
  }
}

void EnterGlobalWorld()
{
  if (g_gameMode == GameModePrologue && !TheScript()->IsRunningScript())
  {
    // the only time you should see the world in prologue is during the cutscene
    // g_atMainMenu = true;
    g_requestedLocationId = g_globalWorld->GetLocationId("launchpad");
    GlobalLocation* gloc = g_globalWorld->GetLocation(g_requestedLocationId);
    g_requestedMap = gloc->m_mapFilename;
    g_requestedMission = gloc->m_missionFilename;
  }

  // Put the camera in a sensible place
  TheCamera()->SetDebugMode(Camera::DebugModeAuto);
  TheCamera()->RequestMode(Camera::Mode::ModeSphereWorld);
  TheCamera()->SetHeight(50.0f);

  if (g_editing)
    GlobalWorldEditorLoop();
  else
    GlobalWorldGameLoop();
}

void MainMenuLoop()
{
  TheCamera()->RequestMode(Camera::Mode::ModeMainMenu);
  while (g_atMainMenu)
  {
    UpdateAdvanceTime();
    TheRenderer()->Render();
    TheUserInput()->Advance();
    TheCamera()->Advance();
    g_soundSystem->Advance();
    HandleCommonConditions();

    if (!g_app->m_gameMenu->m_menuCreated)
    {
      if (TheRenderer()->IsFadeComplete())
        g_app->m_gameMenu->CreateMenu();
    }
  }

  g_app->m_gameMenu->DestroyMenu();
}

void RunTheGame()
{
  Initialise();
  RunBootLoaders();

  //
  // Do whatever mode was requested

  if (g_prefsManager->GetInt("CurrentGameMode", 1) == 0)
    g_app->LoadPrologue();
  else
    g_app->LoadCampaign();

  while (true)
  {
    if (g_requestedLocationId != -1)
      EnterLocation();
    else // Not in location
      EnterGlobalWorld();

    g_inputManager->Advance();
  }
}

// Main Function
void AppMain()
{
  wchar_t filename[MAX_PATH];
  GetModuleFileNameW(nullptr, filename, MAX_PATH);
  auto path = std::wstring(filename);
  path = path.substr(0, path.find_last_of('\\'));

  FileSys::SetHomeDirectory(path);

  RunTheGame();
}
} // namespace Species


// The process entry point, and the ONE thing in Species that stays at global
// scope: the CRT startup code looks up ::WinMain by name, so namespace-migration
// T5 left it out here and it calls into the namespace explicitly.
//
// It used to be in NeuronClient/WindowManager.cpp, where it called AppMain()
// below — a static library reaching up into the executable that links it.
// check_layering never saw it, because an include check cannot: NeuronClient
// DECLARED AppMain in its own header rather than including a Species one. The
// linker sees it, and said so the moment a test DLL pulled WindowManager.obj
// in without a game executable to satisfy it.
int WINAPI WinMain(HINSTANCE _hInstance, HINSTANCE _hPrevInstance, LPSTR _cmdLine, int _iCmdShow)
{
  Neuron::SetWin32InstanceHandle(_hInstance);

  Neuron::g_windowManager = new Neuron::WindowManager();

  Species::AppMain();

  return WM_QUIT;
}
