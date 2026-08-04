#include "pch.h"
#include "TextRenderer.h"
#include "MathUtils.h"
#include "Vector2.h"
#include "Debug.h"
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

#include "GameCursor.h"
#include "GlobalWorld.h"
#include "Location.h"
#include "Renderer.h"
#include "TaskManager.h"
#include "TaskManagerInterface.h"
#include "Team.h"
#include "Unit.h"
#include "LevelFile.h"
#include "Main.h"
#include "RoutingSystem.h"
#include "EntityGrid.h"
#include "ParticleSystem.h"
#include "Camera.h"
#include "Script.h"
#include "LocationInput.h"

#include "SoundSystem.h"

#include "InsertionSquad.h"
#include "Officer.h"
#include "Citizen.h"
#include "ResearchItem.h"
#include "TrunkPort.h"
#include "Engineer.h"
#include "WorldPointers.h"


// ============================================================================


ScreenZone::ScreenZone(char const* _name, char const* _tooltip, float _x, float _y, float _w, float _h, int _data)
  : m_x(_x),
    m_y(_y),
    m_w(_w),
    m_h(_h),
    m_data(_data),
    m_scrollZone(-1),
    m_subZones(false)
{
  ASSERT_TEXT(strlen(_name) < sizeof(m_name), "Button name too long : {}", _name);
  ASSERT_TEXT(strlen(_tooltip) < sizeof(m_toolTip), "Tooltip too long : {}", _tooltip);
  strcpy(m_name, _name);
  strcpy(m_toolTip, _tooltip);
}


// ============================================================================

KeyboardShortcut::KeyboardShortcut(std::string _name, int _data, ControlType _controltype)
  : ControlEventFunctor(_controltype),
    m_name(_name),
    m_data(_data)
{
}


const char* KeyboardShortcut::name() { return m_name.c_str(); }


int KeyboardShortcut::data() { return m_data; }


// ============================================================================


TaskManagerInterface::TaskManagerInterface()
  : m_visible(false),
    m_highlightedTaskId(-1),
    m_lockTaskManager(false),
    m_quickUnitVisible(false)
{
}


void TaskManagerInterface::SetCurrentMessage(int _messageType, int _taskType, float _timer)
{
  m_currentMessageType = _messageType;
  m_currentTaskType = _taskType;
  m_messageTimer = GetHighResTime() + _timer;
}


void TaskManagerInterface::RunDefaultObjective(GlobalEventCondition* _cond)
{
  switch (_cond->m_type)
  {
  case GlobalEventCondition::BuildingOnline:
  case GlobalEventCondition::BuildingOffline:
  {
    Building* building = g_location->GetBuilding(_cond->m_id);
    if (building)
    {
      TheCamera()->RequestBuildingFocusMode(building, 250.0f, 75.0f);
      m_viewingDefaultObjective = true;
    }
    break;
  }

  case GlobalEventCondition::ResearchOwned:
  {
    Building* building = nullptr;
    for (int i = 0; i < g_location->m_buildings.Size(); ++i)
    {
      if (g_location->m_buildings.ValidIndex(i))
      {
        Building* thisBuilding = g_location->m_buildings[i];
        if (thisBuilding->m_type == Building::TypeResearchItem && ((ResearchItem*)thisBuilding)->m_researchType == _cond->m_id)
        {
          building = thisBuilding;
          break;
        }
      }
    }

    if (building)
    {
      TheCamera()->RequestBuildingFocusMode(building, 100.0f, 75.0f);
      m_viewingDefaultObjective = true;
    }
    break;
  }
  }
}

void TaskManagerInterface::SetVisible(bool _visible) { m_visible = _visible; }

void TaskManagerInterface::AdvanceTab()
{
  if (!m_visible || g_inputManager->getInputMode() == InputMode::INPUT_MODE_KEYBOARD)
  {
    int taskId = -1;
    int index = -1;
    bool changeTask = false;
    bool gesturesCycle = false;
    if (g_inputManager->controlEvent(ControlGesturesSwitchUnit) && g_prefsManager->GetInt("ControlMethod", 0) == 0)
    {
      gesturesCycle = true;
    }
    if (g_inputManager->controlEvent(ControlUnitCycleRight) || gesturesCycle)
    {
      changeTask = true;
      for (int i = 0; i < static_cast<int>(g_taskManager->m_tasks.size()); ++i)
      {
        if (ValidIndex(g_taskManager->m_tasks, i))
        {
          if (g_taskManager->m_tasks[i]->m_id == g_taskManager->m_currentTaskId)
          {
            if (ValidIndex(g_taskManager->m_tasks, i + 1))
            {
              index = i + 1;
            }
            else if (ValidIndex(g_taskManager->m_tasks, 0))
            {
              index = 0;
            }
            break;
          }
        }
      }
    }

    if (g_inputManager->controlEvent(ControlUnitCycleLeft))
    {
      changeTask = true;
      for (int i = 0; i < static_cast<int>(g_taskManager->m_tasks.size()); ++i)
      {
        if (ValidIndex(g_taskManager->m_tasks, i))
        {
          if (g_taskManager->m_tasks[i]->m_id == g_taskManager->m_currentTaskId)
          {
            if (ValidIndex(g_taskManager->m_tasks, i - 1))
            {
              index = i - 1;
            }
            else if (ValidIndex(g_taskManager->m_tasks, static_cast<int>(g_taskManager->m_tasks.size()) - 1))
            {
              index = static_cast<int>(g_taskManager->m_tasks.size()) - 1;
            }
            break;
          }
        }
      }
    }

    if (changeTask)
    {
      if (index == -1 && ValidIndex(g_taskManager->m_tasks, 0))
      {
        index = 0;
      }

      if (ValidIndex(g_taskManager->m_tasks, index))
      {
        if (g_taskManager->m_tasks[index]->m_type == GlobalResearch::TypeSquad)
        {
          TheCamera()->RequestEntityTrackMode(g_taskManager->m_tasks[index]->m_objId);
        }
        else
        {
          TheCamera()->RequestMode(Camera::ModeFreeMovement);
        }
        taskId = g_taskManager->m_tasks[index]->m_id;
        g_taskManager->SelectTask(taskId);
      }
    }
  }
}
