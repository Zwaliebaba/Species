#pragma once

#include <memory>
#include <vector>

#include "Entity.h"
#include "WorldObject.h"

class Route;
class GlobalEventCondition;

// ============================================================================
// class Task


class Task
{
  public:
    enum
    {
      StateIdle,    // Not yet run
      StateStarted, // Running, not yet targetted
      StateRunning, // Targetted, running
      StateStopping // Stopping
    };

    int m_id;
    int m_type;
    int m_state;
    WorldObjectId m_objId;

    std::unique_ptr<Route> m_route; // Only used when this is a Controller task; owning

  public:
    Task();
    ~Task();

    void Start();
    bool Advance();
    void SwitchTo();
    void Stop();

    void Target(DirectX::XMFLOAT3 const& _pos);
    void TargetSquad(DirectX::XMFLOAT3 const& _pos);
    void TargetEngineer(DirectX::XMFLOAT3 const& _pos);
    void TargetOfficer(DirectX::XMFLOAT3 const& _pos);
    void TargetArmour(DirectX::XMFLOAT3 const& _pos);

    WorldObjectId Promote(WorldObjectId _id);
    static WorldObjectId Demote(WorldObjectId _id);
    static WorldObjectId FindCitizen(DirectX::XMFLOAT3 const& _pos);

    static char const* GetTaskName(int _type);
    static char const* GetTaskNameTranslated(int _type);
};


// ============================================================================
// class TaskTargetArea


class TaskTargetArea
{
  public:
    // TaskTargetArea is default-constructed then assigned field by field in
    // GetTargetArea, so this needs an initialiser of its own.
    DirectX::XMFLOAT3 m_centre{0.0f, 0.0f, 0.0f};
    float m_radius;
    bool m_stationary;
};


// ============================================================================
// class TaskManager


class TaskManager
{
  public:
    // The task manager owns its tasks. RunTask and RegisterTask take that
    // ownership in their signatures, so a task that fails to register is
    // destroyed rather than leaked.
    std::vector<std::unique_ptr<Task>> m_tasks;

    int m_nextTaskId;
    int m_currentTaskId;
    bool m_verifyTargetting;

  protected:
    void AdvanceTasks();

  public:
    TaskManager();

    int Capacity();
    int CapacityUsed();

    bool RunTask(std::unique_ptr<Task> _task); // Starts the task, registers it; takes ownership either way
    bool RunTask(int _type);
    bool RegisterTask(std::unique_ptr<Task> _task); // Assumes task is already started; takes ownership either way

    int MapGestureToTask(int _gestureId); // Maps a gesture to a task type

    Task* GetCurrentTask();
    Task* GetTask(int _id);
    Task* GetTask(WorldObjectId _id);
    void SelectTask(int _id);
    void SelectTask(WorldObjectId _id); // Selects the corrisponding task
    bool IsValidTargetArea(int _id, DirectX::XMFLOAT3 const& _pos);
    bool TargetTask(int _id, DirectX::XMFLOAT3 const& _pos);
    bool TerminateTask(int _id);

    void StopAllTasks();

    std::vector<TaskTargetArea>* GetTargetArea(int _id); // Returns all valid placement areas

    void Advance();
};
