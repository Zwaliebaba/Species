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

#include "App.h"
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
#include "Darwinian.h"
#include "ResearchItem.h"
#include "TrunkPort.h"
#include "Engineer.h"


// ============================================================================



ScreenZone::ScreenZone( char *_name, char *_tooltip,
                        float _x, float _y, float _w, float _h,
                        int _data )
:   m_x(_x),
    m_y(_y),
    m_w(_w),
    m_h(_h),
    m_data(_data),
    m_scrollZone(-1),
	m_subZones(false)
{
    ASSERT_TEXT( strlen(_name) < sizeof(m_name), "Button name too long : %s", _name );
    ASSERT_TEXT( strlen(_tooltip) < sizeof(m_toolTip), "Tooltip too long : %s", _tooltip );
    strcpy( m_name, _name );
    strcpy( m_toolTip, _tooltip );
}


// ============================================================================

KeyboardShortcut::KeyboardShortcut( std::string _name, int _data, ControlType _controltype )
: ControlEventFunctor( _controltype ), m_name( _name ), m_data( _data ) {}


const char * KeyboardShortcut::name() { return m_name.c_str(); }


int KeyboardShortcut::data() { return m_data; }


// ============================================================================


TaskManagerInterface::TaskManagerInterface()
:   m_visible(false),
    m_highlightedTaskId(-1),
    m_lockTaskManager(false),
    m_quickUnitVisible(false)
{
}


void TaskManagerInterface::SetCurrentMessage( int _messageType, int _taskType, float _timer )
{
    m_currentMessageType = _messageType;
    m_currentTaskType = _taskType;
    m_messageTimer = GetHighResTime() + _timer;
}


void TaskManagerInterface::RunDefaultObjective ( GlobalEventCondition *_cond )
{
    switch( _cond->m_type )
    {
        case GlobalEventCondition::BuildingOnline:
        case GlobalEventCondition::BuildingOffline:
        {
            Building *building = g_app->m_location->GetBuilding( _cond->m_id );
            if( building )
            {
                g_app->m_camera->RequestBuildingFocusMode( building, 250.0f, 75.0f );
                m_viewingDefaultObjective = true;
            }
            break;
        }

        case GlobalEventCondition::ResearchOwned:
        {
            Building *building = NULL;
            for( int i = 0; i < g_app->m_location->m_buildings.Size(); ++i )
            {
                if( g_app->m_location->m_buildings.ValidIndex(i) )
                {
                    Building *thisBuilding = g_app->m_location->m_buildings[i];
                    if( thisBuilding->m_type == Building::TypeResearchItem &&
                        ((ResearchItem *) thisBuilding)->m_researchType == _cond->m_id )
                    {
                        building = thisBuilding;
                        break;
                    }
                }
            }

            if( building )
            {
                g_app->m_camera->RequestBuildingFocusMode( building, 100.0f, 75.0f );
                m_viewingDefaultObjective = true;
            }
            break;
        }
    }
}

void TaskManagerInterface::SetVisible( bool _visible )
{
	m_visible = _visible;
}

void TaskManagerInterface::AdvanceTab()
{
    if( !m_visible ||
        g_inputManager->getInputMode() == INPUT_MODE_KEYBOARD )
    {
        int taskId = -1;
        int index = -1;
        bool changeTask = false;
        bool gesturesCycle = false;
        if( g_inputManager->controlEvent( ControlGesturesSwitchUnit ) &&
            g_prefsManager->GetInt( "ControlMethod", 0 ) == 0 )
        {
            gesturesCycle = true;
        }
        if(g_inputManager->controlEvent( ControlUnitCycleRight ) ||
            gesturesCycle )
        {
            changeTask = true;
            for( int i = 0; i < g_app->m_taskManager->m_tasks.Size(); ++i )
            {
                if( g_app->m_taskManager->m_tasks.ValidIndex(i) )
                {
                    if( g_app->m_taskManager->m_tasks[i]->m_id == g_app->m_taskManager->m_currentTaskId )
                    {
                        if( g_app->m_taskManager->m_tasks.ValidIndex(i+1) )
                        {
                            index = i+1;
                        }
                        else if( g_app->m_taskManager->m_tasks.ValidIndex(0) )
                        {
                            index = 0;
                        }
                        break;
                    }
                }
            }
        }

        if( g_inputManager->controlEvent( ControlUnitCycleLeft ) )
        {
            changeTask = true;
            for( int i = 0; i < g_app->m_taskManager->m_tasks.Size(); ++i )
            {
                if( g_app->m_taskManager->m_tasks.ValidIndex(i) )
                {
                    if( g_app->m_taskManager->m_tasks[i]->m_id == g_app->m_taskManager->m_currentTaskId )
                    {
                        if( g_app->m_taskManager->m_tasks.ValidIndex(i-1) )
                        {
                            index = i-1;
                        }
                        else if( g_app->m_taskManager->m_tasks.ValidIndex(g_app->m_taskManager->m_tasks.Size() - 1) )
                        {
                            index = g_app->m_taskManager->m_tasks.Size() - 1;
                        }
                        break;
                    }
                }
            }
        }

        if( changeTask )
        {
            if( index == -1 && g_app->m_taskManager->m_tasks.ValidIndex(0) )
            {
                index = 0;
            }

            if( g_app->m_taskManager->m_tasks.ValidIndex(index) )
            {
                if( g_app->m_taskManager->m_tasks[index]->m_type == GlobalResearch::TypeSquad )
                {
                    g_app->m_camera->RequestEntityTrackMode( g_app->m_taskManager->m_tasks[index]->m_objId );
                }
                else
                {
                    g_app->m_camera->RequestMode( Camera::ModeFreeMovement );
                }
                taskId = g_app->m_taskManager->m_tasks[index]->m_id;
                g_app->m_taskManager->SelectTask(taskId);
            }
        }
    }
}