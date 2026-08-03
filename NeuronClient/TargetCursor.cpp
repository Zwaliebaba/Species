#include "pch.h"

#include "TargetCursor.h"
#include "Input.h"
#include "Preferences.h"

#include "WindowManager.h"


#include "Eclipse.h"
#include "WorldPointers.h"
#include "AppState.h"

#define AXIS_X 0
#define AXIS_Y 1
#define AXIS_Z 2


TargetCursor* g_target = nullptr;


TargetCursor::TargetCursor()
{
  m_screenCoords[AXIS_X] = 0;
  m_screenCoords[AXIS_Y] = 0;
  m_screenCoords[AXIS_Z] = 0;
  m_velocity[AXIS_X] = 0;
  m_velocity[AXIS_Y] = 0;
  m_velocity[AXIS_Z] = 0;
}


void TargetCursor::SetMousePos(int x, int y)
{
  m_screenCoords[AXIS_X] = x;
  m_screenCoords[AXIS_Y] = y;
  g_windowManager->NastySetMousePos(x, y);
}


void TargetCursor::MoveCursor(int x, int y)
{
  m_velocity[AXIS_X] += x;
  m_velocity[AXIS_Y] += y;
  m_screenCoords[AXIS_X] += x;
  m_screenCoords[AXIS_Y] += y;
  g_windowManager->NastyMoveMouse(x, y);
}


int TargetCursor::X() const { return m_screenCoords[AXIS_X]; }


int TargetCursor::Y() const { return m_screenCoords[AXIS_Y]; }


int TargetCursor::Z() const { return m_screenCoords[AXIS_Z]; }


int TargetCursor::dX() const { return m_velocity[AXIS_X]; }


int TargetCursor::dY() const { return m_velocity[AXIS_Y]; }


int TargetCursor::dZ() const { return m_velocity[AXIS_Z]; }


bool secondaryInputEnabled() { return (EclGetWindows()->size() == 0) && !g_taskManagerInterface->IsVisible(); }

void TargetCursor::Advance()
{
  InputDetails details;
  if ((g_inputManager->controlEvent(ControlTargetMove, details) ||
       (secondaryInputEnabled() && g_inputManager->controlEvent(ControlTargetMoveSecondary, details))) &&
      INPUT_TYPE_2D == details.type)
  {
    m_velocity[AXIS_X] = details.x;
    m_velocity[AXIS_Y] = details.y;
    m_screenCoords[AXIS_X] += m_velocity[AXIS_X];
    m_screenCoords[AXIS_Y] += m_velocity[AXIS_Y];

    if (g_camera->IsInMode(CameraAccess::ModeFreeMovement))
      g_controlHelpSystem->RecordCondUsed(ControlHelpAccess::CondCameraAim);
  }
  else
    m_velocity[AXIS_X] = m_velocity[AXIS_Y] = 0;

  if (g_inputManager->controlEvent(ControlTargetMoveZ, details) && INPUT_TYPE_1D == details.type)
  {
    m_velocity[AXIS_Z] = details.x;
    m_screenCoords[AXIS_Z] += m_velocity[AXIS_Z];
  }
  else
    m_velocity[AXIS_Z] = 0;
}
