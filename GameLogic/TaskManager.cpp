#include "pch.h"
#include "TextRenderer.h"
#include "MathUtils.h"
#include "Debug.h"
#include "Input.h"
#include "InputTypes.h"
#include "Resource.h"
#include "Bitmap.h"
#include "Profiler.h"
#include "HiResTime.h"
#include "LanguageTable.h"
#include "DebugRender.h"
#include "OglExtensions.h"
#include "BinaryStreamReaders.h"
#include "Preferences.h"
#include "TargetCursor.h"

#include "ClientToServer.h"

#include "GlobalWorld.h"
#include "Location.h"
#include "TaskManager.h"
#include "Team.h"
#include "Unit.h"
#include "LevelFile.h"
#include "GameTime.h"
#include "RoutingSystem.h"
#include "EntityGrid.h"
#include "ParticleSystem.h"

#include "SoundSystem.h"

#include "PrefsOtherWindow.h"

#include "InsertionSquad.h"
#include "Officer.h"
#include "Citizen.h"
#include "ResearchItem.h"
#include "TrunkPort.h"
#include "Engineer.h"
#include "WorldPointers.h"


namespace Species
{
  Task::Task()
    : m_type(GlobalResearch::TypeSquad),
      m_id(-1),
      m_state(StateIdle),
      m_route()
  {
  }


  Task::~Task() = default;


  void Task::Start() { m_state = StateStarted; }


  void Task::Target(DirectX::XMFLOAT3 const& _pos)
  {
    switch (m_type)
    {
    case GlobalResearch::TypeSquad:
      TargetSquad(_pos);
      break;
    case GlobalResearch::TypeEngineer:
      TargetEngineer(_pos);
      break;
    case GlobalResearch::TypeOfficer:
      TargetOfficer(_pos);
      break;
    case GlobalResearch::TypeArmour:
      TargetArmour(_pos);
      break;
    }
  }


  void Task::TargetSquad(DirectX::XMFLOAT3 const& _pos)
  {
    int teamId = g_globalWorld->m_myTeamId;

    int numEntities = 2 + g_globalWorld->m_research->CurrentLevel(GlobalResearch::TypeSquad);

    int unitId;
    g_location->m_teams[teamId].NewUnit(Entity::TypeInsertionSquadie, numEntities, &unitId, _pos);
    g_location->SpawnEntities(_pos, teamId, unitId, Entity::TypeInsertionSquadie, numEntities, g_zeroVector, 10);

    g_location->m_teams[teamId].SelectUnit(unitId, -1, -1);
    m_objId.Set(teamId, unitId, -1, -1);

    m_state = StateRunning;

    g_soundSystem->TriggerOtherEvent("GestureSuccess", SoundSourceBlueprint::TypeGesture);

    int trackEntity = g_prefsManager->GetInt(OTHER_AUTOMATICCAM, 0);
    if (trackEntity == 0)
    {
      // work out if player is using control pad
      if (g_inputManager->getInputMode() == InputMode::INPUT_MODE_GAMEPAD)
        trackEntity = 2;
    }

    if (trackEntity == 2)
    {
      g_camera->RequestEntityTrackMode(m_objId);
    }
  }


  void Task::TargetEngineer(DirectX::XMFLOAT3 const& _pos)
  {
    int teamId = g_globalWorld->m_myTeamId;

    DirectX::XMFLOAT3 pos = _pos;
    pos.y += 10.0f;
    m_objId = g_location->SpawnEntities(pos, teamId, -1, Entity::TypeEngineer, 1, g_zeroVector, 0);
    g_location->m_teams[teamId].SelectUnit(-1, m_objId.GetIndex(), -1);

    m_state = StateRunning;
    g_soundSystem->TriggerOtherEvent("GestureSuccess", SoundSourceBlueprint::TypeGesture);
  }


  void Task::TargetArmour(DirectX::XMFLOAT3 const& _pos)
  {
    int teamId = g_globalWorld->m_myTeamId;

    m_objId = g_location->SpawnEntities(_pos, teamId, -1, Entity::TypeArmour, 1, g_zeroVector, 0);
    g_location->m_teams[teamId].SelectUnit(-1, m_objId.GetIndex(), -1);

    m_state = StateRunning;

    g_soundSystem->TriggerOtherEvent("GestureSuccess", SoundSourceBlueprint::TypeGesture);
  }


  WorldObjectId Task::Promote(WorldObjectId _id)
  {
    int teamId = g_globalWorld->m_myTeamId;

    Entity* entity = g_location->GetEntity(_id);
    DEBUG_ASSERT(entity);


    //
    // Spawn an Officer

    WorldObjectId spawnedId = g_location->SpawnEntities(entity->m_pos, teamId, -1, Entity::TypeOfficer, 1, entity->m_vel, 0);
    Officer* officer = (Officer*)g_location->GetEntity(spawnedId);
    DEBUG_ASSERT(officer);


    //
    // Particle effect

    int numFlashes = 5 + speciesRandom() % 5;
    for (int i = 0; i < numFlashes; ++i)
    {
      DirectX::XMFLOAT3 vel(sfrand(5.0f), frand(15.0f), sfrand(5.0f));
      g_particleSystem->CreateParticle(entity->m_pos, vel, Particle::TypeControlFlash);
    }


    Citizen* citizen = (Citizen*)entity;
    citizen->m_promoted = true;


    return spawnedId;
  }


  WorldObjectId Task::Demote(WorldObjectId _id)
  {
    // Make demoted officers return to green
    // int teamId = g_globalWorld->m_myTeamId;
    int teamId = 0;

    Entity* entity = g_location->GetEntity(_id);
    DEBUG_ASSERT(entity);


    //
    // Spawn a Citizen

    WorldObjectId spawnedId = g_location->SpawnEntities(entity->m_pos, teamId, -1, Entity::TypeCitizen, 1, entity->m_vel, 0);


    //
    // Particle effect

    int numFlashes = 5 + speciesRandom() % 5;
    for (int i = 0; i < numFlashes; ++i)
    {
      DirectX::XMFLOAT3 vel(sfrand(5.0f), frand(15.0f), sfrand(5.0f));
      g_particleSystem->CreateParticle(entity->m_pos, vel, Particle::TypeControlFlash);
    }


    return spawnedId;
  }


  WorldObjectId Task::FindCitizen(DirectX::XMFLOAT3 const& _pos)
  {
    int teamId = g_globalWorld->m_myTeamId;

    int numFound;
    WorldObjectId* ids = g_location->m_entityGrid->GetFriends(_pos.x, _pos.z, 10.0f, &numFound, teamId);
    WorldObjectId nearestId;
    float nearest = 99999.9f;

    for (int i = 0; i < numFound; ++i)
    {
      WorldObjectId id = ids[i];
      Entity* entity = g_location->GetEntity(id);
      if (entity && entity->m_type == Entity::TypeCitizen)
      {
        float distance = DirectX::XMVectorGetX(
          DirectX::XMVector3LengthSq(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&entity->m_pos), DirectX::XMLoadFloat3(&_pos))));
        if (distance < nearest)
        {
          nearestId = id;
          nearest = distance;
        }
      }
    }

    return nearestId;
  }


  void Task::TargetOfficer(DirectX::XMFLOAT3 const& _pos)
  {
    int teamId = g_globalWorld->m_myTeamId;

    //
    // We will not upgrade people if we're controlling something right now

    Team* myTeam = g_location->GetMyTeam();
    if (myTeam->m_currentUnitId != -1 || myTeam->m_currentEntityId != -1 || myTeam->m_currentBuildingId != -1)
    {
      return;
    }


    //
    // Find the nearest friendly Citizen to upgrade to an Officer
    // If we found someone, promote them
    // Then shutdown this task
    // Then select them

    WorldObjectId nearestId = FindCitizen(_pos);

    if (nearestId.IsValid())
    {
      WorldObjectId id = Promote(nearestId);
      g_taskManager->TerminateTask(m_id);
      g_location->m_teams[id.GetTeamId()].SelectUnit(id.GetUnitId(), id.GetIndex(), -1);
      g_taskManagerInterface->SetCurrentMessage(TaskManagerInterfaceAccess::MessageSuccess, GlobalResearch::TypeOfficer, 2.5f);

      g_soundSystem->TriggerOtherEvent("GestureSuccess", SoundSourceBlueprint::TypeGesture);
    }
  }


  bool Task::Advance()
  {
    if (m_state == StateRunning)
    {
      switch (m_type)
      {
      case GlobalResearch::TypeSquad:
      case GlobalResearch::TypeController:
      {
        Unit* unit = g_location->GetUnit(m_objId);
        if (!unit || unit->NumAliveEntities() == 0)
        {
          if (g_taskManager->m_currentTaskId == m_id)
          {
            g_taskManagerInterface->SetCurrentMessage(TaskManagerInterfaceAccess::MessageShutdown, m_type, 3.0f);
          }
          return true;
        }
        break;
      }

      case GlobalResearch::TypeEngineer:
      case GlobalResearch::TypeArmour:
      {
        Entity* entity = g_location->GetEntity(m_objId);
        if (!entity || entity->m_dead)
        {
          if (g_taskManager->m_currentTaskId == m_id)
          {
            g_taskManagerInterface->SetCurrentMessage(TaskManagerInterfaceAccess::MessageShutdown, m_type, 3.0f);
          }
          return true;
        }
        break;
      }

      case GlobalResearch::TypeOfficer:
      {
        m_state = StateStarted;
        break;
      }
      }
    }

    return (m_state == StateStopping);
  }


  void Task::SwitchTo()
  {
    if (g_camera->IsInMode(CameraAccess::Mode::ModeRadarAim) || g_camera->IsInMode(CameraAccess::Mode::ModeTurretAim))
    {
      g_camera->RequestMode(CameraAccess::Mode::ModeFreeMovement);
    }

    int teamId = g_globalWorld->m_myTeamId;

    switch (m_type)
    {
    case GlobalResearch::TypeSquad:
    {
      g_location->m_teams[teamId].SelectUnit(m_objId.GetUnitId(), -1, -1);
      break;
    }

    case GlobalResearch::TypeEngineer:
    case GlobalResearch::TypeArmour:
    {
      g_location->m_teams[teamId].SelectUnit(-1, m_objId.GetIndex(), -1);
      break;
    }

    case GlobalResearch::TypeController:
    {
      for (int i = 0; i < static_cast<int>(g_taskManager->m_tasks.size()); ++i)
      {
        Task* task = g_taskManager->m_tasks[i].get();
        if (task->m_type == GlobalResearch::TypeSquad && task->m_objId == m_objId)
        {
          g_taskManager->SelectTask(task->m_id);
          break;
        }
      }
      break;
    }

    case GlobalResearch::TypeOfficer:
    {
      g_location->m_teams[teamId].SelectUnit(-1, -1, -1);
      break;
    }
    }
  }


  void Task::Stop()
  {
    switch (m_type)
    {
    case GlobalResearch::TypeSquad:
    {
      Unit* unit = g_location->GetUnit(m_objId);
      if (unit)
      {
        for (int i = 0; i < unit->m_entities.Size(); ++i)
        {
          if (unit->m_entities.ValidIndex(i))
          {
            Entity* entity = unit->m_entities[i];
            int health = entity->m_stats[Entity::StatHealth];
            entity->ChangeHealth(-1000);
          }
        }
      }
      break;
    }

    case GlobalResearch::TypeEngineer:
    case GlobalResearch::TypeArmour:
    {
      Entity* entity = (Entity*)g_location->GetEntity(m_objId);
      if (entity)
      {
        int health = entity->m_stats[Entity::StatHealth];
        entity->ChangeHealth(-1000);
      }
      break;
    }
    }

    m_state = StateStopping;
  }


  char const* Task::GetTaskName(int _type) { return GlobalResearch::GetTypeName(_type); }


  char const* Task::GetTaskNameTranslated(int _type) { return GlobalResearch::GetTypeNameTranslated(_type); }


  // ============================================================================


  TaskManager::TaskManager()
    : m_nextTaskId(0),
      m_currentTaskId(-1),
      m_verifyTargetting(true)
  {
  }


  bool TaskManager::RunTask(std::unique_ptr<Task> _task)
  {
    if (CapacityUsed() < Capacity())
    {
      _task->m_id = m_nextTaskId;
      ++m_nextTaskId;
      Task* task = _task.get();
      m_tasks.push_back(std::move(_task));
      task->Start();
      m_currentTaskId = task->m_id;

      return true;
    }
    else
    {
      g_taskManagerInterface->SetCurrentMessage(TaskManagerInterfaceAccess::MessageFailure, -1, 2.5f);
    }

    return false;
  }


  bool TaskManager::RunTask(int _type)
  {
    switch (_type)
    {
    case GlobalResearch::TypeSquad:
    case GlobalResearch::TypeEngineer:
    case GlobalResearch::TypeOfficer:
    case GlobalResearch::TypeArmour:
    {
      auto task = std::make_unique<Task>();
      task->m_type = _type;
      bool success = RunTask(std::move(task));
      if (success)
      {
        int teamId = g_globalWorld->m_myTeamId;
        g_location->m_teams[teamId].SelectUnit(-1, -1, -1);
      }
      return success;
    }

    case GlobalResearch::TypeController:
    {
      Task* task = GetCurrentTask();
      if (task && task->m_type == GlobalResearch::TypeSquad)
      {
        Unit* unit = g_location->GetUnit(task->m_objId);
        if (unit && unit->m_troopType == Entity::TypeInsertionSquadie)
        {
          InsertionSquad* squad = (InsertionSquad*)unit;
          squad->SetWeaponType(_type);

          if (!GetTask(squad->m_controllerId))
          {
            auto controllerOwned = std::make_unique<Task>();
            Task* controller = controllerOwned.get();
            controller->m_type = _type;
            controller->m_objId = WorldObjectId(squad->m_teamId, squad->m_unitId, -1, -1);
            controller->m_route = std::make_unique<Route>(-1);
            controller->m_route->AddWayPoint(squad->m_centrePos);
            bool success = RunTask(std::move(controllerOwned));
            if (success)
            {
              squad->m_controllerId = controller->m_id;
              SelectTask(task->m_id);
              g_taskManagerInterface->SetCurrentMessage(TaskManagerInterfaceAccess::MessageSuccess, _type, 2.5f);
            }
            return success;
          }

          return true;
        }
      }

      g_taskManagerInterface->SetCurrentMessage(TaskManagerInterfaceAccess::MessageFailure, -1, 2.5f);
      return false;
    }

    case GlobalResearch::TypeGrenade:
    case GlobalResearch::TypeRocket:
    case GlobalResearch::TypeAirStrike:
    {
      Task* task = GetCurrentTask();
      if (task && task->m_type == GlobalResearch::TypeSquad)
      {
        Unit* unit = g_location->GetUnit(task->m_objId);
        if (unit && unit->m_troopType == Entity::TypeInsertionSquadie)
        {
          InsertionSquad* squad = (InsertionSquad*)unit;
          squad->SetWeaponType(_type);
          g_taskManagerInterface->SetCurrentMessage(TaskManagerInterfaceAccess::MessageSuccess, _type, 2.5f);
          return true;
        }
      }

      g_taskManagerInterface->SetCurrentMessage(TaskManagerInterfaceAccess::MessageFailure, -1, 2.5f);
      return false;
    }
    }

    return false;
  }


  bool TaskManager::RegisterTask(std::unique_ptr<Task> _task)
  {
    if (CapacityUsed() < Capacity())
    {
      _task->m_id = m_nextTaskId;
      ++m_nextTaskId;
      m_tasks.push_back(std::move(_task));
      return true;
    }

    return false;
  }


  bool TaskManager::TerminateTask(int _id)
  {
    for (int i = 0; i < static_cast<int>(m_tasks.size()); ++i)
    {
      Task* task = m_tasks[i].get();
      if (task->m_id == _id)
      {
        g_taskManagerInterface->SetCurrentMessage(TaskManagerInterfaceAccess::MessageShutdown, task->m_type, 3.0f);
        // Held alive across Stop(), exactly as the erase-then-delete pair did.
        std::unique_ptr<Task> const dying = std::move(m_tasks[i]);
        m_tasks.erase(m_tasks.begin() + i);
        task->Stop();

        if (m_currentTaskId == _id)
        {
          m_currentTaskId = -1;
        }

        return true;
      }
    }

    return false;
  }


  int TaskManager::Capacity()
  {
    int taskManagerResearch = g_globalWorld->m_research->CurrentLevel(GlobalResearch::TypeTaskManager);
    int capacity = 1 + taskManagerResearch;
    return capacity;
  }


  int TaskManager::CapacityUsed() { return static_cast<int>(m_tasks.size()); }


  int TaskManager::MapGestureToTask(int _gestureId)
  {
    switch (_gestureId)
    {
    case 0:
      return GlobalResearch::TypeSquad;
    case 1:
      return GlobalResearch::TypeEngineer;
    case 2:
      return GlobalResearch::TypeGrenade;
    case 3:
      return GlobalResearch::TypeRocket;
    case 4:
      return GlobalResearch::TypeAirStrike;
    case 5:
      return GlobalResearch::TypeController;
    case 6:
      return GlobalResearch::TypeOfficer;
    case 7:
      return GlobalResearch::TypeArmour;

    default:
      return -1;
    }
  }


  void TaskManager::AdvanceTasks()
  {
    //
    // Are we currently placing a task?

    Task* currentTask = GetCurrentTask();
    if (currentTask && currentTask->m_state == Task::StateStarted)
    {
      g_taskManagerInterface->SetCurrentMessage(TaskManagerInterfaceAccess::MessageSuccess, currentTask->m_type, 2.5f);
    }


    //
    // Advance all other tasks

    for (int i = 0; i < static_cast<int>(m_tasks.size()); ++i)
    {
      Task* task = m_tasks[i].get();
      bool amIDone = task->Advance();
      if (amIDone)
      {
        if (m_currentTaskId == task->m_id)
        {
          m_currentTaskId = -1;
          int teamId = g_globalWorld->m_myTeamId;
          g_location->m_teams[teamId].SelectUnit(-1, -1, -1);
        }

        m_tasks.erase(m_tasks.begin() + i);
        --i;
      }
    }
  }


  void TaskManager::StopAllTasks()
  {
    m_tasks.clear();
    m_currentTaskId = -1;
  }


  void TaskManager::Advance()
  {
    AdvanceTasks();

    g_globalWorld->m_research->AdvanceResearch();
  }


  void TaskManager::SelectTask(int _id)
  {
    m_currentTaskId = _id;
    if (m_currentTaskId != -1)
    {
      int currentIndex = -1;
      for (int i = 0; i < static_cast<int>(m_tasks.size()); ++i)
      {
        Task* task = m_tasks[i].get();
        if (task->m_id == m_currentTaskId)
        {
          currentIndex = i;
          break;
        }
      }

      ASSERT_TEXT(currentIndex != -1, "Error in TaskManager::SelectTask. Tried to select a task that doesn't exist.");

      Task* task = m_tasks[currentIndex].get();
      // m_tasks.erase(m_tasks.begin() + currentIndex);
      // m_tasks.insert(m_tasks.begin(),  task );
      task->SwitchTo();

      g_gameCursor->BoostSelectionArrows(2.0f);
    }
  }


  void TaskManager::SelectTask(WorldObjectId _id)
  {
    for (int i = 0; i < static_cast<int>(m_tasks.size()); ++i)
    {
      Task* task = m_tasks[i].get();
      if (task->m_objId.GetTeamId() == _id.GetTeamId() && task->m_objId.GetUnitId() == _id.GetUnitId() && task->m_objId.GetIndex() == _id.GetIndex())
      {
        SelectTask(task->m_id);
        break;
      }
    }
  }


  Task* TaskManager::GetCurrentTask() { return GetTask(m_currentTaskId); }


  Task* TaskManager::GetTask(int _id)
  {
    for (int i = 0; i < static_cast<int>(m_tasks.size()); ++i)
    {
      Task* task = m_tasks[i].get();
      if (task->m_id == _id)
      {
        return task;
      }
    }

    return nullptr;
  }


  Task* TaskManager::GetTask(WorldObjectId _id)
  {
    for (int i = 0; i < static_cast<int>(m_tasks.size()); ++i)
    {
      Task* task = m_tasks[i].get();
      if (task->m_objId == _id)
      {
        return task;
      }
    }

    return nullptr;
  }


  bool TaskManager::TargetTask(int _id, DirectX::XMFLOAT3 const& _pos)
  {
    if (IsValidTargetArea(_id, _pos))
    {
      Task* task = GetTask(_id);
      task->Target(_pos);
      return true;
    }

    return false;
  }


  bool TaskManager::IsValidTargetArea(int _id, DirectX::XMFLOAT3 const& _pos)
  {
    Task* task = g_taskManager->GetTask(_id);
    if (!task)
      return false;

    if (!g_location || !g_location->m_landscape.IsInLandscape(_pos))
      return false;

    if (m_verifyTargetting)
    {
      if (task->m_type == GlobalResearch::TypeOfficer)
      {
        int numFound;
        WorldObjectId* ids = g_location->m_entityGrid->GetFriends(_pos.x, _pos.z, 10.0f, &numFound, g_globalWorld->m_myTeamId);
        bool foundCitizen = false;
        for (int i = 0; i < numFound; ++i)
        {
          WorldObjectId id = ids[i];
          Entity* ent = g_location->GetEntity(id);
          if (ent && ent->m_type == Entity::TypeCitizen)
          {
            foundCitizen = true;
            break;
          }
        }
        return foundCitizen;
      }
      else
      {
        std::vector<TaskTargetArea>* targetAreas = GetTargetArea(_id);
        bool success = false;

        for (int i = 0; i < static_cast<int>(targetAreas->size()); ++i)
        {
          TaskTargetArea* targetArea = &(*targetAreas)[i];
          float const toCentre = DirectX::XMVectorGetX(
            DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&_pos), DirectX::XMLoadFloat3(&targetArea->m_centre))));
          if (toCentre <= targetArea->m_radius)
          {
            success = true;
            break;
          }
        }

        delete targetAreas;
        return success;
      }
    }
    else
    {
      return true;
    }

    return false;
  }


  std::vector<TaskTargetArea>* TaskManager::GetTargetArea(int _id)
  {
    auto result = new std::vector<TaskTargetArea>();

    Task* task = GetTask(_id);
    if (task)
    {
      switch (task->m_type)
      {
      case GlobalResearch::TypeArmour:
      {
        for (int i = 0; i < g_location->m_buildings.Size(); ++i)
        {
          if (g_location->m_buildings.ValidIndex(i))
          {
            Building* building = g_location->m_buildings[i];
            if (building && building->m_type == Building::TypeTrunkPort && ((TrunkPort*)building)->m_openTimer > 0.0f)
            {
              TaskTargetArea tta;
              tta.m_centre = building->m_pos;
              tta.m_radius = 120.0f;
              tta.m_stationary = true;
              result->push_back(tta);
            }
          }
        }
        break;
      }

      case GlobalResearch::TypeEngineer:
      {
        Team* team = g_location->GetMyTeam();
        for (int i = 0; i < team->m_units.Size(); ++i)
        {
          if (team->m_units.ValidIndex(i))
          {
            Unit* unit = team->m_units[i];
            if (unit->m_troopType == Entity::TypeInsertionSquadie)
            {
              TaskTargetArea tta;
              tta.m_centre = unit->m_centrePos;
              tta.m_radius = 100.0f;
              tta.m_stationary = false;
              result->push_back(tta);
            }
          }
        }
        // break;                // DELIBERATE FALL THROUGH
      }

      case GlobalResearch::TypeSquad:
        for (int i = 0; i < g_location->m_buildings.Size(); ++i)
        {
          if (g_location->m_buildings.ValidIndex(i))
          {
            Building* building = g_location->m_buildings[i];
            if (building && building->m_type == Building::TypeControlTower && building->m_id.GetTeamId() == g_location->GetMyTeam()->m_teamId)
            {
              TaskTargetArea tta;
              tta.m_centre = building->m_pos;
              tta.m_radius = 75.0f;
              tta.m_stationary = true;
              result->push_back(tta);
            }
          }
        }
        break;

      case GlobalResearch::TypeOfficer:
      {
        TaskTargetArea tta;
        tta.m_centre = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
        tta.m_radius = 99999.9f;
        result->push_back(tta);
        break;
      }
      }
    }

    return result;
  }
} // namespace Species
