#include "pch.h"

#include <math.h>
#include <string.h>
#include <float.h>

#include "Debug.h"
#include "HiResTime.h"
#include "Input.h"
#include "Movement2d.h"
#include "TargetCursor.h"
#include "WindowManager.h"
#include "MathUtils.h"
#include "Profiler.h"
#include "DebugRender.h"

#include "Preferences.h"
#include "PrefsOtherWindow.h"

#include "Eclipse.h"

#include "ClientToServer.h"

#include "Camera.h"
#include "GlobalWorld.h"
#include "LevelFile.h"
#include "Location.h"
#include "Main.h"
#include "Renderer.h"
#include "TaskManager.h"
#include "TaskManagerInterface.h"
#include "Team.h"
#include "Unit.h"
#include "UserInput.h"
#include "Script.h"
#include "GameCursor.h"
#include "ControlHelp.h"

#include "Teleport.h"
#include "InsertionSquad.h"
#include "WorldPointers.h"
#include "AppState.h"

// Vector3::RotateAround(axis) took the ANGLE FROM THE AXIS VECTOR'S MAGNITUDE
// and did nothing at all below 1e-8 squared. Both halves are load-bearing here:
// the guard is what stops a cross product of two parallel vectors turning the
// camera's up vector into QNaN. Named the same as Citizen.cpp's copy because it
// is the same routine; NeuronMath.h refuses to grow an operator layer, so each
// file that rotates this way carries it.
static DirectX::XMVECTOR XM_CALLCONV RotateAroundScaledAxis(DirectX::FXMVECTOR _v, DirectX::FXMVECTOR _scaledAxis)
{
  float const lengthSquared = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(_scaledAxis));
  if (lengthSquared < 1e-8f)
    return _v;

  float const angle = sqrtf(lengthSquared);
  return DirectX::XMVector3Transform(_v, DirectX::XMMatrixRotationAxis(DirectX::XMVectorScale(_scaledAxis, 1.0f / angle), angle));
}


#define MIN_GROUND_CLEARANCE 10.0f // Minimum height relative to land
#define MIN_HEIGHT 10.0f           // Height above sea level (which is y=0)
#define MAX_HEIGHT 5000.0f         // Height above sea level (which is y=0)
#define MIN_TRACKING_HEIGHT 200.0f // Minimum height of the camera when tracking an entity

// ***************
// Private Methods
// ***************

void Camera::AdvanceDebugMode()
{
  if (strcmp(EclGetCurrentFocus(), "none") != 0 && !g_editing)
    return;

  if (g_editing)
    m_targetFov = 60.0f;

  float advanceTime = g_advanceTime;
  // front x up, which is the NEGATIVE of GetRight()'s up x front. The sign is
  // why the slide below SUBTRACTS this one; the rotate block further down
  // takes GetRight() and is a different vector, not a repeat of this.
  DirectX::XMVECTOR const right = DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&m_front), DirectX::XMLoadFloat3(&m_up));

  float speedSideways = g_globalWorld->GetSize() / 30.0f;
  if (g_locationId != -1)
    speedSideways = g_location->m_landscape.GetWorldSizeX() / 30.0f;
  float speedVertical = speedSideways;
  float speedForwards = speedSideways;

  if (g_inputManager->controlEvent(ControlCameraSpeedup))
  {
    speedSideways *= 10.0f;
    speedVertical *= 10.0f;
    speedForwards *= 10.0f;
  }
  else if (g_inputManager->controlEvent(ControlCameraSlowdown))
  {
    speedSideways *= 0.1f;
    speedVertical /= 10.0f;
    speedForwards /= 10.0f;
  }

  // speedForwards *= 20.0f;
  //  TODO: Support mouse/joystick
  // if( EclGetWindows()->size() == 0 )
  {
    static DPadMovement cam_slide(ControlCameraForwards, ControlCameraBackwards, ControlCameraLeft, ControlCameraRight, ControlCameraUp,
                                  ControlCameraDown, 1);
    cam_slide.Advance();
    DirectX::XMVECTOR pos = DirectX::XMLoadFloat3(&m_pos);
    pos = DirectX::XMVectorSubtract(pos, DirectX::XMVectorScale(right, cam_slide.signX() * advanceTime * speedSideways));
    pos = DirectX::XMVectorAdd(pos, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_up), cam_slide.signZ() * advanceTime * speedVertical));
    pos = DirectX::XMVectorAdd(pos, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_front), cam_slide.signY() * advanceTime * speedForwards));
    DirectX::XMStoreFloat3(&m_pos, pos);
  }

  int mx = g_target->dX();
  int my = g_target->dY();

  // TODO: Really?
  if (g_inputManager->controlEvent(ControlCameraDebugRotate))
  {
    // THE TWO MATRIX33 ROTATORS DISAGREE ABOUT SIGN, and only one of them
    // says so in its name. Applied as `v * mat` from identity:
    //
    //   RotateAroundY(a)          rotates v by +a about +Y  ==  XMMatrixRotationY(a)
    //   FastRotateAround(n, a)    rotates v by -a about n
    //
    // because FastRotateAround turns the matrix's three basis ROWS by +a, and
    // a matrix built by rotating its rows transposes into the inverse of one
    // that rotates vectors. Hence the flipped sign on the pitch below. Both
    // spellings compile and both are rotations; one of them pitches the debug
    // camera the wrong way when the mouse moves.
    DirectX::XMMATRIX const yaw = DirectX::XMMatrixRotationY(static_cast<float>(mx) * -0.005f);
    DirectX::XMStoreFloat3(&m_up, DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&m_up), yaw));
    DirectX::XMStoreFloat3(&m_front, DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&m_front), yaw));

    DirectX::XMFLOAT3 const rightAxis = GetRight();
    DirectX::XMMATRIX const pitch = DirectX::XMMatrixRotationAxis(DirectX::XMLoadFloat3(&rightAxis), static_cast<float>(my) * 0.005f);
    DirectX::XMStoreFloat3(&m_up, DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&m_up), pitch));
    DirectX::XMStoreFloat3(&m_front, DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&m_front), pitch));
  }
}

void Camera::Get2DScreenPos(DirectX::XMFLOAT3 const& _vector, float* _screenX, float* _screenY)
{
  double outX, outY, outZ;

  int viewport[4];
  double viewMatrix[16];
  double projMatrix[16];
  glGetIntegerv(GL_VIEWPORT, viewport);
  glGetDoublev(GL_MODELVIEW_MATRIX, viewMatrix);
  glGetDoublev(GL_PROJECTION_MATRIX, projMatrix);
  gluProject(_vector.x, _vector.y, _vector.z, viewMatrix, projMatrix, viewport, &outX, &outY, &outZ);

  *_screenX = outX;
  *_screenY = outY;
}

void Camera::SetHeight(float _height) { m_height = _height; }

void Camera::SetFOV(float _fov)
{
  m_targetFov = _fov;
  m_fov = _fov;
}

void Camera::SetTargetFOV(float _fov) { m_targetFov = _fov; }

void Camera::AdvanceSphereWorldMode()
{
  bool chatLog = false;
  if (chatLog)
    return;

  m_targetFov = 100.0f;

  const int screenH = g_renderer->ScreenH();
  const int screenW = g_renderer->ScreenW();

  DirectX::XMVECTOR const focusPos = DirectX::XMVectorSet(0.0f, m_height * -400.0f, 0.0f, 0.0f);

  // Set up viewing matrices
  glPushMatrix();
  SetupProjectionMatrix(g_renderer->GetNearPlane(), g_renderer->GetFarPlane());
  SetupModelviewMatrix();

  // Get the 2D mouse coordinates before we move the camera
  DirectX::XMFLOAT3 mousePos3D = TheUserInput()->GetMousePos3d();
  float oldMouseX, oldMouseY;
  Get2DScreenPos(mousePos3D, &oldMouseX, &oldMouseY);
  oldMouseY = screenH - oldMouseY;

  InputDetails details;
  if (g_inputManager->controlEvent(ControlCameraMove, details))
  {
    g_target->SetMousePos(g_target->X() + details.x, g_target->Y() + details.y);
    TheUserInput()->RecalcMousePos3d();
    mousePos3D = TheUserInput()->GetMousePos3d();
    Get2DScreenPos(mousePos3D, &oldMouseX, &oldMouseY);
    oldMouseY = screenH - oldMouseY;
  }

  float factor1 = 2.0f * g_advanceTime;
  float factor2 = 1.0f - factor1;

  // Update camera orientation
  if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(DirectX::XMLoadFloat3(&mousePos3D))) > 1.0f)
  {
    DirectX::XMVECTOR const desiredFront =
      DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&mousePos3D), DirectX::XMLoadFloat3(&m_pos)));
    DirectX::XMVECTOR const front = DirectX::XMVector3Normalize(
      DirectX::XMVectorAdd(DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_front), factor2), DirectX::XMVectorScale(desiredFront, factor1)));

    // g_upVector, which is (0,1,0). The right vector is derived from the NEW
    // front, not the one this block was entered with.
    DirectX::XMVECTOR const right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(front, DirectX::g_XMIdentityR1));
    DirectX::XMStoreFloat3(&m_front, front);
    DirectX::XMStoreFloat3(&m_up, DirectX::XMVector3Cross(right, front));
  }

  //
  // Move towards the idealPos

  factor1 /= 2.0f;
  factor2 = 1.0f - factor1;

  DirectX::XMVECTOR idealPos = DirectX::XMVectorSubtract(focusPos, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_front), 30000.0f));
  if (DirectX::XMVectorGetX(DirectX::XMVector3Length(idealPos)) > 35000.0f)
    // SetLength answered a zero-length input with (len,0,0). The test above is
    // what makes that branch unreachable, so no fallback is reproduced here.
    idealPos = DirectX::XMVectorScale(DirectX::XMVector3Normalize(idealPos), 35000.0f);
  DirectX::XMVECTOR const toIdealPos = DirectX::XMVectorSubtract(idealPos, DirectX::XMLoadFloat3(&m_pos));
  float const distToIdealPos = DirectX::XMVectorGetX(DirectX::XMVector3Length(toIdealPos));
  DirectX::XMStoreFloat3(
    &m_pos, DirectX::XMVectorAdd(DirectX::XMVectorScale(idealPos, factor1), DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_pos), factor2)));

  // Set up viewing matrices
  SetupModelviewMatrix();

  // Get the 2D mouse coordinates now that we have moved the camera
  float newMouseX, newMouseY;
  Get2DScreenPos(mousePos3D, &newMouseX, &newMouseY);
  newMouseY = screenH - newMouseY;

  // Calculate how much to move the cursor to make it look like it is
  // locked to a point on the landscape (we need to take account of
  // sub-pixel error from previous frames)
  static float mouseDeltaX = 0.0f;
  static float mouseDeltaY = 0.0f;
  mouseDeltaX += (newMouseX - oldMouseX) * 1.0f;
  mouseDeltaY += (newMouseY - oldMouseY) * 1.0f;
  int intMouseDeltaX = floorf(mouseDeltaX);
  int intMouseDeltaY = floorf(mouseDeltaY);
  mouseDeltaX -= intMouseDeltaX;
  mouseDeltaY -= intMouseDeltaY;
  newMouseX = g_target->X() + intMouseDeltaX;
  newMouseY = g_target->Y() + intMouseDeltaY;

  // Make sure these movements don't put the mouse cursor somewhere stupid
  if (newMouseX < 0)
    newMouseX = 0;
  if (newMouseX >= screenW)
    newMouseX = screenW - 1;
  if (newMouseY < 0)
    newMouseY = 0;
  if (newMouseY >= screenH)
    newMouseY = screenH - 1;

  // Apply the mouse cursor movement
  g_target->SetMousePos(newMouseX, newMouseY);
  glPopMatrix();
}

void Camera::AdvanceSphereWorldScriptedMode()
{
  int b = 10;

  m_height = 50.0f;

  const int screenH = g_renderer->ScreenH();
  const int screenW = g_renderer->ScreenW();

  DirectX::XMVECTOR const focusPos = DirectX::XMVectorSet(sinf(g_gameTime * 0.5f) * 4000.0f, m_height * -400.0f + cosf(g_gameTime * 0.4f) * 2000.0f,
                                                          sinf(g_gameTime * 0.3f) * 4000.0f, 0.0f);

  // Set up viewing matrices
  glPushMatrix();
  SetupProjectionMatrix(g_renderer->GetNearPlane(), g_renderer->GetFarPlane());
  SetupModelviewMatrix();

  // Get the 2D mouse coordinates before we move the camera
  DirectX::XMFLOAT3 mousePos3D = TheUserInput()->GetMousePos3d();
  float oldMouseX, oldMouseY;
  Get2DScreenPos(mousePos3D, &oldMouseX, &oldMouseY);
  oldMouseY = screenH - oldMouseY;

  float factor1 = 1.0f * g_advanceTime;
  float factor2 = 1.0f - factor1;

  // Update camera orientation
  DirectX::XMVECTOR const desiredFront =
    DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_targetPos), DirectX::XMLoadFloat3(&m_pos)));
  DirectX::XMVECTOR const front = DirectX::XMVector3Normalize(
    DirectX::XMVectorAdd(DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_front), factor2), DirectX::XMVectorScale(desiredFront, factor1)));

  // g_upVector, which is (0,1,0).
  DirectX::XMVECTOR const right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(front, DirectX::g_XMIdentityR1));
  DirectX::XMStoreFloat3(&m_front, front);
  DirectX::XMStoreFloat3(&m_up, DirectX::XMVector3Cross(right, front));

  //
  // Move towards the idealPos

  factor1 /= 2.0f;
  factor2 = 1.0f - factor1;

  DirectX::XMVECTOR idealPos = DirectX::XMVectorSubtract(focusPos, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_front), 30000.0f));
  if (DirectX::XMVectorGetX(DirectX::XMVector3Length(idealPos)) > 35000.0f)
    // SetLength answered a zero-length input with (len,0,0). The test above is
    // what makes that branch unreachable, so no fallback is reproduced here.
    idealPos = DirectX::XMVectorScale(DirectX::XMVector3Normalize(idealPos), 35000.0f);
  DirectX::XMVECTOR const toIdealPos = DirectX::XMVectorSubtract(idealPos, DirectX::XMLoadFloat3(&m_pos));
  float const distToIdealPos = DirectX::XMVectorGetX(DirectX::XMVector3Length(toIdealPos));
  DirectX::XMStoreFloat3(
    &m_pos, DirectX::XMVectorAdd(DirectX::XMVectorScale(idealPos, factor1), DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_pos), factor2)));

  glPopMatrix();
}

/*
void Camera::AdvanceSphereWorldIntroMode()
{
    m_targetFov = 100.0f;

    static float fixMeUp = 45000.0f;

    if( g_keys[KEY_G] )
    {
        fixMeUp = 45000.0;
    }

    if( fixMeUp == 45000.0f )
    {
        m_pos.Set( 0, 0, -1000000.0f );
        m_front.Set( 1, 0, -1 );
        m_up.Set( 1, 0, 0 );
    }

    fixMeUp -= g_advanceTime * 500.0f;

    Vector3 targetFront = Vector3(200, 200, 200) - m_pos;
    float distance = targetFront.Mag();

    float forwardSpeed = 3000.0f;

    targetFront.Normalise();

    float rotateSpeed = forwardSpeed / (fixMeUp*0.66f);
    float factor1 = g_advanceTime * rotateSpeed;
    float factor2 = 1.0f - factor1;
    m_front = m_front * factor2 + targetFront * factor1;
    m_up.RotateAround( m_front * g_advanceTime * rotateSpeed * 0.2f );

    m_pos += m_front * forwardSpeed;
}*/

void Camera::AdvanceSphereWorldIntroMode()
{
  m_targetFov = 70.0f;

  static bool fixMeUp = true;
  static float startTime = GetHighResTime();
  static float lastFrame = startTime;

  float timeNow = GetHighResTime();
  float frameTime = timeNow - lastFrame;
  float runningTime = timeNow - startTime;
  lastFrame = timeNow;

#ifdef _DEBUG
  if (g_inputManager->controlEvent(ControlDebugCameraFixUp))
    fixMeUp = true;
#endif

  if (fixMeUp)
  {
    startTime = GetHighResTime();

    m_pos = DirectX::XMFLOAT3(-965852.0f, 1720000.0f, 2600000.0f);
    // (-1,0,0) is already unit length, so the Normalise it used to go through
    // was a no-op. (0.15, 0.93, 0.31) is not, so that one is kept.
    m_front = DirectX::XMFLOAT3(-1.0f, 0.0f, 0.0f);
    DirectX::XMStoreFloat3(&m_up, DirectX::XMVector3Normalize(DirectX::XMVectorSet(0.15f, 0.93f, 0.31f, 0.0f)));

    fixMeUp = false;
  }

  DirectX::XMVECTOR targetPos = DirectX::XMVectorSet(1000.0f, 500.0f, 1000.0f, 0.0f);
  if (runningTime < 30.0f)
    targetPos = DirectX::XMVectorSet(-1000000.0f, 4000000.0f, 397000.0f, 0.0f);
  DirectX::XMVECTOR targetFront = DirectX::XMVectorSubtract(targetPos, DirectX::XMLoadFloat3(&m_pos));
  float distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(targetFront));

  float forwardSpeed = 3000.0f;

  if (distance < 2000000 && runningTime > 30)
    forwardSpeed = (distance / 1070000.0f) * 3000.0f;

  targetFront = DirectX::XMVector3Normalize(targetFront);

  float rotateSpeed = 4000.0f / 30000.0f;
  float factor1 = frameTime * rotateSpeed;
  float factor2 = 1.0f - factor1;
  // The two lines below read the NEW front, which is why it is held rather
  // than reloaded, and it is deliberately not normalised — nor was it.
  DirectX::XMVECTOR const front =
    DirectX::XMVectorAdd(DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_front), factor2), DirectX::XMVectorScale(targetFront, factor1));
  DirectX::XMStoreFloat3(&m_front, front);
  DirectX::XMStoreFloat3(&m_up,
                         RotateAroundScaledAxis(DirectX::XMLoadFloat3(&m_up),
                                                DirectX::XMVectorScale(front, frameTime * rotateSpeed * sinf(runningTime * 0.1f + 0.4f) * 1.6f)));

  DirectX::XMStoreFloat3(&m_pos,
                         DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMVectorScale(front, forwardSpeed * frameTime * 62.0f)));
}

void Camera::AdvanceSphereWorldOutroMode()
{
  m_targetFov = 70;

  static bool fixMeUp = true;
  static float startTime = GetHighResTime();
  static float lastFrame = startTime;

  float timeNow = GetHighResTime();
  float frameTime = timeNow - lastFrame;
  float runningTime = timeNow - startTime;
  lastFrame = timeNow;

#ifdef _DEBUG
  if (g_inputManager->controlEvent(ControlDebugCameraFixUp))
    fixMeUp = true;
#endif

  if (fixMeUp)
  {
    startTime = GetHighResTime();

    // All three are already unit length, so the Normalise calls that followed
    // them were no-ops.
    m_pos = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_front = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
    m_up = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);

    m_vel = m_front;

    fixMeUp = false;
  }

  DirectX::XMVECTOR targetPos = DirectX::XMVectorSet(-1000000.0f, 4000000.0f, 397000.0f, 0.0f);

  if (runningTime > 30.0f)
    targetPos = DirectX::XMVectorSet(-965852.0f, 1720000.0f, 2600000.0f, 0.0f);

  DirectX::XMVECTOR targetFront = DirectX::XMVectorScale(DirectX::XMVectorSubtract(targetPos, DirectX::XMLoadFloat3(&m_pos)), -1.0f);
  float distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(targetFront));

  float forwardSpeed = sqrtf(DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&m_pos)))) * 4;
  forwardSpeed = std::max(forwardSpeed, 1000.0f);
  forwardSpeed = std::min(forwardSpeed, 2000.0f);

  targetFront = DirectX::XMVector3Normalize(targetFront);

  float rotateSpeed = 4000.0f / 30000.0f;
  float factor1 = frameTime * rotateSpeed;
  float factor2 = 1.0f - factor1;
  DirectX::XMVECTOR const vel =
    DirectX::XMVectorAdd(DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), factor2), DirectX::XMVectorScale(targetFront, factor1));
  DirectX::XMStoreFloat3(&m_vel, vel);
  // The rotation axis is the OLD front — m_front is not written until the end
  // of this function — so the order of these statements is load-bearing.
  DirectX::XMStoreFloat3(&m_up, RotateAroundScaledAxis(DirectX::XMLoadFloat3(&m_up),
                                                       DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_front),
                                                                              frameTime * rotateSpeed * sinf(runningTime * 0.1f + 0.4f) * 0.6f)));

  DirectX::XMVECTOR const pos =
    DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMVectorScale(vel, forwardSpeed * frameTime * 62.0f));
  DirectX::XMStoreFloat3(&m_pos, pos);

  targetFront = DirectX::XMVectorNegate(pos);
  if (runningTime > 30.0f)
    targetFront = vel;

  targetFront = DirectX::XMVector3Normalize(targetFront);
  factor1 *= 5.0f;
  DirectX::XMStoreFloat3(
    &m_front, DirectX::XMVectorAdd(DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_front), factor2), DirectX::XMVectorScale(targetFront, factor1)));

  if (runningTime > 70.0f)
  {
    RequestMode(ModeSphereWorld);
    m_pos = DirectX::XMFLOAT3(0.0f, 0.0f, 100.0f);
  }
}

void Camera::RequestSphereFocusMode()
{
  m_framesInThisMode = 0;
  m_mode = ModeSphereWorldFocus;
  m_targetPos = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
  m_trackRange = 100000.0f;
  m_trackHeight = 0.0f;
  m_trackTimer = GetHighResTime();

  m_trackVector = m_pos;
  m_trackVector.y = 0.0f;
  DirectX::XMStoreFloat3(&m_trackVector, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&m_trackVector)));
}

void Camera::AdvanceSphereWorldFocusMode()
{
  m_targetFov = 100;

  DirectX::XMFLOAT3 oldPos = m_pos;

  m_trackRange = 100000;
  m_trackHeight = 50000;

  //
  // Target a point that slowly rotates around the building

  // Vector3::RotateAroundY(a) went through FastRotateAround(g_upVector, a),
  // which on a VECTOR is the ordinary right-handed rotation about +Y -- the
  // same one XMMatrixRotationY(a) performs under `v * M`. It is only the
  // MATRIX form of FastRotateAround that inverts; see AdvanceDebugMode.
  DirectX::XMVECTOR trackVector =
    DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&m_trackVector), DirectX::XMMatrixRotationY(g_advanceTime * -0.15f));
  trackVector = DirectX::XMVector3Normalize(trackVector);
  DirectX::XMStoreFloat3(&m_trackVector, trackVector);
  float height = sinf(g_gameTime * 0.2f) * m_trackHeight;
  float trackRange = fabs(m_trackRange) + sinf(g_gameTime * 0.3f) * fabs(m_trackRange) * 0.4f;
  DirectX::XMFLOAT3 realTargetPos;
  DirectX::XMStoreFloat3(&realTargetPos,
                         DirectX::XMVectorMultiplyAdd(trackVector, DirectX::XMVectorReplicate(trackRange), DirectX::XMLoadFloat3(&m_targetPos)));
  realTargetPos.y = -110000 + height;

  // Calculate a Movement Factor, so we start moving slowly towards our target,
  // then eventually reach full speed
  float timeSinceBegin = GetHighResTime() - m_trackTimer;
  float moveFactor = timeSinceBegin * 0.2f;
  moveFactor = std::min(moveFactor, 1.0f);

  float factor1 = moveFactor * 0.5f * g_advanceTime;
  float factor2 = 1.0f - factor1;
  DirectX::XMStoreFloat3(&m_pos, DirectX::XMVectorAdd(DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_pos), factor2),
                                                      DirectX::XMVectorScale(DirectX::XMLoadFloat3(&realTargetPos), factor1)));

  m_targetPos.y = 10000;

  DirectX::XMFLOAT3 targetFront;
  DirectX::XMStoreFloat3(&targetFront,
                         DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_targetPos), DirectX::XMLoadFloat3(&m_pos))));
  if (m_trackRange < 0.0f)
  {
    targetFront.x *= -1.0f;
    targetFront.z *= -1.0f;
  }
  factor1 = moveFactor * 1.0f * g_advanceTime * 0.5f;
  factor2 = 1.0f - factor1;
  DirectX::XMVECTOR const front = DirectX::XMVectorAdd(DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_front), factor2),
                                                       DirectX::XMVectorScale(DirectX::XMLoadFloat3(&targetFront), factor1));
  DirectX::XMStoreFloat3(&m_front, front);

  // m_up is written to (0,-1,0) and immediately used to derive the right
  // vector, so the intermediate never survives the function.
  DirectX::XMVECTOR const right = DirectX::XMVector3Cross(DirectX::XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f), front);
  DirectX::XMStoreFloat3(&m_up, DirectX::XMVector3Cross(right, front));
}

// Returns the number of meters to the nearest blockage that the camera would
// experience if it travelled in the specified direction. A blockage is defined
// as a piece of land more than 10 metres higher than the camera's current
// height. If there is no blockage FLT_MAX is returned.
float XM_CALLCONV Camera::DistanceToBlockage(DirectX::FXMVECTOR _dir, const float _maxDist)
{
  if (!g_location)
    return FLT_MAX;

  DirectX::XMFLOAT3 dir;
  DirectX::XMStoreFloat3(&dir, _dir);

  constexpr unsigned int numSteps = 40;
  const float distStep = _maxDist / static_cast<float>(numSteps);
  for (int i = 1; i <= numSteps; ++i)
  {
    float x = m_pos.x + dir.x * distStep * static_cast<float>(i);
    float z = m_pos.z + dir.z * distStep * static_cast<float>(i);
    float landHeight = g_location->m_landscape.m_heightMap->GetValue(x, z);
    if (landHeight + MIN_GROUND_CLEARANCE > m_height)
      return static_cast<float>(i) * distStep;
  }

  return _maxDist;
}

void Camera::AdvanceFreeMovementMode()
{
  UpdateEntityTrackingMode();

  // Check to see whether we should switch to entity tracking mode
  WorldObjectId selection;
  if (m_entityTrack && GetEntityToTrack(selection))
  {
    Entity* entity = g_location->GetEntity(selection);
    if (entity->m_type == Entity::TypeInsertionSquadie)
    {
      RequestEntityTrackMode(selection);
      return;
    }
    m_objectId = WorldObjectId();
  }

  int screenW = g_renderer->ScreenW();
  int screenH = g_renderer->ScreenH();
  InputManager* im = g_inputManager;

  // Set up viewing matrices
  glPushMatrix();
  SetupProjectionMatrix(g_renderer->GetNearPlane(), g_renderer->GetFarPlane());
  SetupModelviewMatrix();

  // Get the 2D mouse coordinates before we move the camera
  DirectX::XMFLOAT3 mousePos3D = TheUserInput()->GetMousePos3d();
  float oldMouseX, oldMouseY;
  Get2DScreenPos(mousePos3D, &oldMouseX, &oldMouseY);
  oldMouseY = screenH - oldMouseY;

  // Allow quake keys to move us
  if (!TheTaskManagerInterface()->m_visible)
  {
    float moveRate = 250.0f;
    // Flattened onto the horizontal plane before normalising, both of them.
    // A camera looking straight down makes the forward one zero-length, and
    // XMVector3Normalize answers that with zero where Vector3::Normalise
    // answered (0,0,1) -- but the result only ever scales a movement delta,
    // so a zero is a stopped camera rather than a QNaN position. Left native.
    DirectX::XMFLOAT3 accelForwardStore = m_front;
    accelForwardStore.y = 0.0f;
    DirectX::XMVECTOR const accelForward = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&accelForwardStore));
    DirectX::XMFLOAT3 accelRightStore = GetRight();
    accelRightStore.y = 0.0f;
    DirectX::XMVECTOR const accelRight = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&accelRightStore));

    // TODO: Support mouse/joystick
    if (g_inputManager->controlEvent(ControlCameraSpeedup))
      moveRate *= 4.0f;

    bool keyForward = g_inputManager->controlEvent(ControlCameraForwards);
    bool keyBackward = g_inputManager->controlEvent(ControlCameraBackwards);
    bool keyLeft = g_inputManager->controlEvent(ControlCameraLeft);
    bool keyRight = g_inputManager->controlEvent(ControlCameraRight);

    DirectX::XMVECTOR targetPos = DirectX::XMLoadFloat3(&m_targetPos);
    if (keyLeft)
      targetPos = DirectX::XMVectorAdd(targetPos, DirectX::XMVectorScale(accelRight, g_advanceTime * moveRate));
    if (keyRight)
      targetPos = DirectX::XMVectorSubtract(targetPos, DirectX::XMVectorScale(accelRight, g_advanceTime * moveRate));
    if (keyForward)
      targetPos = DirectX::XMVectorAdd(targetPos, DirectX::XMVectorScale(accelForward, g_advanceTime * moveRate));
    if (keyBackward)
      targetPos = DirectX::XMVectorSubtract(targetPos, DirectX::XMVectorScale(accelForward, g_advanceTime * moveRate));

    if (m_mode == ModeFreeMovement)
    {
      InputDetails details;
      if (g_inputManager->controlEvent(ControlCameraMove, details))
      {
        targetPos = DirectX::XMVectorSubtract(targetPos, DirectX::XMVectorScale(accelRight, g_advanceTime * details.x * 10.0f));
        targetPos = DirectX::XMVectorSubtract(targetPos, DirectX::XMVectorScale(accelForward, g_advanceTime * details.y * 10.0f));

        TheControlHelp()->RecordCondUsed(ControlHelpSystem::CondMoveCameraOrUnit);
      }
    }

    if (keyForward || keyBackward || keyLeft || keyRight)
    {
    }

    DirectX::XMStoreFloat3(&m_targetPos, targetPos);

    //		if (m_pos.x < m_minX)	m_targetPos.x -= (m_pos.x - m_minX);
    //		if (m_pos.x > m_maxX)	m_targetPos.x -= (m_pos.x - m_maxX);
    //		if (m_pos.z < m_minX)	m_targetPos.z -= (m_pos.z - m_minZ);
    //		if (m_pos.z > m_maxX)	m_targetPos.z -= (m_pos.z - m_maxZ);

    // Stop camera getting too close to cliffs
    {
      DirectX::XMFLOAT3 horiDirStore;
      DirectX::XMStoreFloat3(&horiDirStore, DirectX::XMVectorSubtract(targetPos, DirectX::XMLoadFloat3(&m_pos)));
      horiDirStore.y = 0.0f;
      DirectX::XMVECTOR const horiDir = DirectX::XMLoadFloat3(&horiDirStore);
      if (DirectX::XMVectorGetX(DirectX::XMVector3Length(horiDir)) > 1e-4)
      {
        const float distToBlockage = DistanceToBlockage(DirectX::XMVector3Normalize(horiDir), 100.0f);
        if (distToBlockage < 100.0f)
        {
          DirectX::XMVECTOR const targetVector = DirectX::XMVectorSubtract(targetPos, DirectX::XMLoadFloat3(&m_pos));
          DirectX::XMFLOAT3 targetVectorHori;
          DirectX::XMStoreFloat3(&targetVectorHori, targetVector);
          targetVectorHori.y = 0.0f;
          float maxSpeed = (distToBlockage - 30.0f) / (100.0f - 30.0f);
          if (maxSpeed < 0.0f)
            maxSpeed = 0.0f;
          DirectX::XMStoreFloat3(&m_targetPos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMVectorScale(targetVector, maxSpeed)));
          m_height += (1.0f - maxSpeed) * 1.0f * DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&targetVectorHori)));
        }
      }
    }

    // Make sure we haven't set the height too low
    constexpr float hitDownRadius = 10.0f;
    float landheight = g_location->m_landscape.m_heightMap->GetValue(m_targetPos.x, m_targetPos.z);
    float landHeight2 = g_location->m_landscape.m_heightMap->GetValue(m_targetPos.x - hitDownRadius, m_targetPos.z - hitDownRadius);
    if (landHeight2 > landheight)
      landheight = landHeight2;
    landHeight2 = g_location->m_landscape.m_heightMap->GetValue(m_targetPos.x + hitDownRadius, m_targetPos.z - hitDownRadius);
    if (landHeight2 > landheight)
      landheight = landHeight2;
    landHeight2 = g_location->m_landscape.m_heightMap->GetValue(m_targetPos.x - hitDownRadius, m_targetPos.z + hitDownRadius);
    if (landHeight2 > landheight)
      landheight = landHeight2;
    landHeight2 = g_location->m_landscape.m_heightMap->GetValue(m_targetPos.x + hitDownRadius, m_targetPos.z + hitDownRadius);
    if (landHeight2 > landheight)
      landheight = landHeight2;

    if (landheight < MIN_HEIGHT)
      landheight = MIN_HEIGHT;

    if (landheight + MIN_GROUND_CLEARANCE > m_height)
    {
      m_targetPos.y = landheight + MIN_GROUND_CLEARANCE;
      m_height = m_targetPos.y;
    }
    else
      m_targetPos.y = m_height;

    // Update our position
    float factor1 = 4.0f * g_advanceTime;
    float factor2 = 1.0f - factor1;
    DirectX::XMVECTOR const oldPos = DirectX::XMLoadFloat3(&m_pos);
    DirectX::XMVECTOR const newPos =
      DirectX::XMVectorAdd(DirectX::XMVectorScale(oldPos, factor2), DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_targetPos), factor1));
    DirectX::XMStoreFloat3(&m_pos, newPos);

    // deltaPos started as -m_pos and had the new m_pos added to it, so it is
    // the frame's movement. Written out rather than kept in two steps.
    DirectX::XMVECTOR const deltaPos = DirectX::XMVectorSubtract(newPos, oldPos);
    // operator/ multiplied by the reciprocal once, which is what XMVectorScale does.
    DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(deltaPos, 1.0f / g_advanceTime));

    // Update camera orientation
    bool chatLog = false;

    if (!TheTaskManagerInterface()->m_visible && !chatLog)
    {
      if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(DirectX::XMLoadFloat3(&mousePos3D))) > 1.0f)
      {
        DirectX::XMVECTOR const desiredFront =
          DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&mousePos3D), DirectX::XMLoadFloat3(&m_pos)));
        DirectX::XMVECTOR const oldFront = DirectX::XMLoadFloat3(&m_front);
        float amountToRotate = sqrtf(g_advanceTime) * 1.0f;
        if (amountToRotate > 1.0f)
          amountToRotate = 1.0f;
        // The cross product IS the rotation: its direction is the axis and its
        // magnitude the angle, which is what RotateAround took.
        DirectX::XMVECTOR const desiredRotation = DirectX::XMVectorScale(DirectX::XMVector3Cross(oldFront, desiredFront), amountToRotate);
        DirectX::XMVECTOR const front = DirectX::XMVector3Normalize(RotateAroundScaledAxis(oldFront, desiredRotation));

        // g_upVector, which is (0,1,0).
        DirectX::XMVECTOR const right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(front, DirectX::g_XMIdentityR1));
        DirectX::XMStoreFloat3(&m_front, front);
        DirectX::XMStoreFloat3(&m_up, DirectX::XMVector3Cross(right, front));
      }
    }

    // Set up viewing matrices
    SetupModelviewMatrix();

    // Get the 2D mouse coordinates now that we have moved the camera
    float newMouseX, newMouseY;
    DirectX::XMStoreFloat3(&mousePos3D, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&mousePos3D), deltaPos));
    Get2DScreenPos(mousePos3D, &newMouseX, &newMouseY);
    newMouseY = screenH - newMouseY;

    // Calculate how much to move the cursor to make it look like it is
    // locked to a point on the landscape (we need to take account of
    // sub-pixel error from previous frames)
    static float mouseDeltaX = 0.0f;
    static float mouseDeltaY = 0.0f;
    mouseDeltaX += (newMouseX - oldMouseX) * 1.0f;
    mouseDeltaY += (newMouseY - oldMouseY) * 1.0f;
    int intMouseDeltaX = floorf(mouseDeltaX);
    int intMouseDeltaY = floorf(mouseDeltaY);
    mouseDeltaX -= intMouseDeltaX;
    mouseDeltaY -= intMouseDeltaY;
    // g_inputManager->PollForEvents();
    newMouseX = g_target->X() + intMouseDeltaX;
    newMouseY = g_target->Y() + intMouseDeltaY;

    // Make sure these movements don't put the mouse cursor somewhere stupid
    if (newMouseX < 0)
      newMouseX = 0;
    if (newMouseX >= screenW)
      newMouseX = screenW - 1;
    if (newMouseY < 0)
      newMouseY = 0;
    if (newMouseY >= screenH)
      newMouseY = screenH - 1;

    // Apply the mouse cursor movement
    g_target->SetMousePos(newMouseX, newMouseY);
    glPopMatrix();
  }
}

void Camera::Render()
{
  //    RenderArrow( m_targetPos, m_targetPos + m_trackVector * m_trackRange, 1.0f );
  //    RenderSphere( m_targetPos + m_trackVector * m_trackRange, 20.0f );
}

void Camera::AdvanceBuildingFocusMode()
{
  DirectX::XMFLOAT3 oldPos = m_pos;

  //
  // Target a point that slowly rotates around the building

  // See AdvanceSphereWorldFocusMode: the VECTOR form of RotateAroundY is the
  // ordinary right-handed rotation, and XMMatrixRotationY under `v * M` is it.
  DirectX::XMVECTOR trackVector =
    DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&m_trackVector), DirectX::XMMatrixRotationY(g_advanceTime * -0.15f));
  trackVector = DirectX::XMVector3Normalize(trackVector);
  DirectX::XMStoreFloat3(&m_trackVector, trackVector);
  float height = m_trackHeight + sinf(g_gameTime * 0.3f) * m_trackHeight * 0.5f;
  float trackRange = fabs(m_trackRange) + sinf(g_gameTime * 0.3f) * fabs(m_trackRange) * 0.3f;
  DirectX::XMFLOAT3 realTargetPos;
  DirectX::XMStoreFloat3(&realTargetPos,
                         DirectX::XMVectorMultiplyAdd(trackVector, DirectX::XMVectorReplicate(trackRange), DirectX::XMLoadFloat3(&m_targetPos)));
  realTargetPos.y = m_targetPos.y + height;

  // Calculate a Movement Factor, so we start moving slowly towards our target,
  // then eventually reach full speed
  float timeSinceBegin = GetHighResTime() - m_trackTimer;
  float moveFactor = timeSinceBegin * 1.0f;
  moveFactor = std::min(moveFactor, 1.0f);

  if (timeSinceBegin < 2.0f)
  {
    // Make the camera lift up when first moving towards a building
    float distance = DirectX::XMVectorGetX(
      DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&realTargetPos))));
    realTargetPos.y += distance * 0.75f * (2.0f - timeSinceBegin);
    realTargetPos.y = std::min(realTargetPos.y, 1000.0f);
  }

  float factor1 = moveFactor * 0.5f * g_advanceTime;
  float factor2 = 1.0f - factor1;
  DirectX::XMStoreFloat3(&m_pos, DirectX::XMVectorAdd(DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_pos), factor2),
                                                      DirectX::XMVectorScale(DirectX::XMLoadFloat3(&realTargetPos), factor1)));

  DirectX::XMFLOAT3 targetFront;
  DirectX::XMStoreFloat3(&targetFront,
                         DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_targetPos), DirectX::XMLoadFloat3(&m_pos))));
  if (m_trackRange < 0.0f)
  {
    targetFront.x *= -1.0f;
    targetFront.z *= -1.0f;
  }
  factor1 = moveFactor * 1.0f * g_advanceTime;
  factor2 = 1.0f - factor1;
  DirectX::XMVECTOR const front = DirectX::XMVectorAdd(DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_front), factor2),
                                                       DirectX::XMVectorScale(DirectX::XMLoadFloat3(&targetFront), factor1));
  DirectX::XMStoreFloat3(&m_front, front);

  DirectX::XMVECTOR const right = DirectX::XMVector3Cross(DirectX::XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f), front);
  DirectX::XMStoreFloat3(&m_up, DirectX::XMVector3Cross(right, front));

  float landHeight = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);
  if (m_pos.y < landHeight + 10.0f)
    m_pos.y = landHeight + 10.0f;
}

// Determine whether there is an entity or unit that has been selected
// by the player, and if so, return true and fill in the object id.
bool Camera::GetEntityToTrack(WorldObjectId& selection)
{
  if (!g_location)
    return false;

  Team* team = g_location->GetMyTeam();

  if (!team)
    return false;

  if (team->GetMyEntity())
  {
    selection = team->GetMyEntity()->m_id;
    return true;
  }

  Task* currentTask = g_taskManager->GetCurrentTask();
  // if the task has just been ended or killed, it isnt valid
  if (currentTask && currentTask->m_state == Task::StateStopping)
    return false;

  if (!currentTask)
    return false;

  Unit* unit = team->GetMyUnit();
  if (!unit)
    return false;

  if (unit->m_troopType == Entity::TypeInsertionSquadie)
  {
    auto squad = static_cast<InsertionSquad*>(unit);
    Entity* pointMan = squad->GetPointMan();
    if (pointMan)
    {
      selection = pointMan->m_id;
      return true;
    }
  }

  return false;
}

void Camera::AdvanceAutomaticTracking()
{
  float factor1 = g_advanceTime * 4.0f;
  float factor2 = 1.0f - factor1;

  // Braced to zero: this accumulates six optional contributions and
  // Vector3's default constructor is what used to make that start from the
  // origin. cameraTarget is written by each callee before it is read.
  DirectX::XMFLOAT3 avgCameraTarget{0.0f, 0.0f, 0.0f};
  int numInputs = 0;

  DirectX::XMFLOAT3 cameraTarget{0.0f, 0.0f, 0.0f};

  DirectX::XMVECTOR sum = DirectX::XMLoadFloat3(&avgCameraTarget);

  if (AdvanceRopeModel(cameraTarget))
  {
    sum = DirectX::XMVectorAdd(sum, DirectX::XMLoadFloat3(&cameraTarget));
    numInputs++;
  }

  if (AdvanceCanSeeUnits(cameraTarget))
  {
    sum = DirectX::XMVectorAdd(sum, DirectX::XMLoadFloat3(&cameraTarget));
    numInputs++;
  }

  if (AdvanceNotTooLow(cameraTarget))
  {
    sum = DirectX::XMVectorAdd(sum, DirectX::XMLoadFloat3(&cameraTarget));
    numInputs++;
  }

  if (AdvanceNotTooFarAway(cameraTarget))
  {
    sum = DirectX::XMVectorAdd(sum, DirectX::XMLoadFloat3(&cameraTarget));
    numInputs++;
  }

  if (AdvanceManualRotateCamera(cameraTarget))
  {
    sum = DirectX::XMVectorAdd(sum, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&cameraTarget), 3.0f));
    numInputs += 3;
  }

  if (AdvanceManualCameraHeight(cameraTarget))
  {
    sum = DirectX::XMVectorAdd(sum, DirectX::XMLoadFloat3(&cameraTarget));
    numInputs++;
  }

  if (numInputs > 0)
    // operator/ multiplied by the reciprocal once.
    sum = DirectX::XMVectorScale(sum, 1.0f / static_cast<float>(numInputs));
  else
    sum = DirectX::XMLoadFloat3(&m_pos);

  DirectX::XMStoreFloat3(&avgCameraTarget, sum);

  DirectX::XMVECTOR const cameraTargetVec =
    DirectX::XMVectorAdd(DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_cameraTarget), factor2), DirectX::XMVectorScale(sum, factor1));
  DirectX::XMStoreFloat3(&m_cameraTarget, cameraTargetVec);
  DirectX::XMStoreFloat3(
    &m_pos, DirectX::XMVectorAdd(DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_pos), factor2), DirectX::XMVectorScale(cameraTargetVec, factor1)));

  // Finally face the unit
  RotateTowardsEntity(m_trackingEntity);
  UpdateControlVector();
}

void Camera::UpdateControlVector()
{
  constexpr float angleThreshold = 0.98f;

  bool recalc = false;
  if (g_inputManager->controlEvent(ControlUnitMoveDirectionChange))
  {
    m_skipDirectionCalculation = false;
    recalc = true;
  }

  // operator* between two Vector3s was the DOT product, not a scale. Taking
  // cosf of a dot is what the original did and is kept exactly.
  DirectX::XMFLOAT3 const upStore = GetUp();
  float angle = cosf(DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&upStore))));
  if ((fabs(angle) < angleThreshold && !m_skipDirectionCalculation) || !g_inputManager->controlEvent(ControlUnitMove) || recalc)
  {
    m_controlVector = GetRight();
    m_skipDirectionCalculation = false;
  }
  else if (fabs(angle) >= angleThreshold)
    m_skipDirectionCalculation = true;
}

bool Camera::AdvanceManualRotateCamera(DirectX::XMFLOAT3& cameraTarget)
{
  if ((g_inputManager->controlEvent(ControlCameraRotateLeft) || g_inputManager->controlEvent(ControlCameraRotateRight)) &&
      !g_inputManager->controlEvent(ControlUnitPrimaryFireDirected))
  {
    m_trackHeight = 0.0f;

    int rotSpeed = 100;

    int halfWidth = g_renderer->ScreenW() / 2;
    int deltaX = g_target->X() - halfWidth;
    if (g_inputManager->controlEvent(ControlCameraRotateLeft))
      deltaX = rotSpeed;

    if (g_inputManager->controlEvent(ControlCameraRotateRight))
      deltaX = -rotSpeed;

    float rotY = static_cast<float>(deltaX) * 0.015f;

    // Disable vertical camera adjustment, for now

    int halfHeight = g_renderer->ScreenH() / 2;
    int deltaY = g_target->Y() - halfHeight;
    float rotRight = static_cast<float>(deltaY) * -0.01f;

    DirectX::XMVECTOR front = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&m_front), DirectX::XMMatrixRotationY(rotY));

    // g_upVector, which is (0,1,0).
    DirectX::XMVECTOR const right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(front, DirectX::g_XMIdentityR1));
    DirectX::XMVECTOR const scaledAxis = DirectX::XMVectorScale(right, rotRight);

    DirectX::XMVECTOR const newUp = RotateAroundScaledAxis(DirectX::XMLoadFloat3(&m_up), scaledAxis);

    if (DirectX::XMVectorGetY(newUp) > 0.1f)
      front = RotateAroundScaledAxis(front, scaledAxis);

    DirectX::XMStoreFloat3(&cameraTarget,
                           DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_targetPos), DirectX::XMVectorScale(front, m_currentDistance)));
    return true;
  }
  m_currentDistance = DirectX::XMVectorGetX(
    DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&m_predictedEntityPos))));
  return false;
}

bool Camera::AdvanceManualCameraHeight(DirectX::XMFLOAT3& cameraTarget)
{
  constexpr float heightScale = 0.05f;

  bool camDown = false;

  if (EclGetWindows()->size() == 0)
  {
    if (g_inputManager->controlEvent(ControlCameraUp))
    {
      m_heightMultiplier += heightScale;
      TheControlHelp()->RecordCondUsed(ControlHelpSystem::CondCameraUp);
      TheControlHelp()->RecordCondUsed(ControlHelpSystem::CondCameraDown);
    }

    if (g_inputManager->controlEvent(ControlCameraDown))
    {
      m_heightMultiplier -= heightScale;
      camDown = true;
      TheControlHelp()->RecordCondUsed(ControlHelpSystem::CondCameraUp);
      TheControlHelp()->RecordCondUsed(ControlHelpSystem::CondCameraDown);
    }

    m_heightMultiplier = std::min(2.0f, m_heightMultiplier);
    m_heightMultiplier = std::max(m_heightMultiplier, 0.25f);

    if (camDown)
    {
      cameraTarget = m_pos;
      cameraTarget.y -= 25.0f;
      return true;
    }
  }

  return false;
}

bool Camera::AdvanceRopeModel(DirectX::XMFLOAT3& cameraTarget)
{
  // Follow along as if the camera is attached to the entity
  // by a rope on the floor.

  DirectX::XMFLOAT3 C = m_pos;
  C.y = 0.0f;

  DirectX::XMFLOAT3 E2 = m_predictedEntityPos;
  E2.y = 0.0f;

  DirectX::XMVECTOR const flatEntityPos = DirectX::XMLoadFloat3(&E2);
  DirectX::XMVECTOR const B = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&C), flatEntityPos);

  // Distance to stay within
  const float d = m_distFromEntity;
  const float dSquared = d * d;

  // Stay within a certain distance of the entity
  if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(B)) > dSquared)
  {
    DirectX::XMStoreFloat3(&cameraTarget, DirectX::XMVectorMultiplyAdd(DirectX::XMVector3Normalize(B), DirectX::XMVectorReplicate(d), flatEntityPos));
    cameraTarget.y = m_pos.y;
    return true;
  }

  // Distance to stay outside of

  float cameraHeight = m_pos.y - m_predictedEntityPos.y;

  // outsideD should be dependent on the camera height
  // (oustideD = h / tan theta, where theta is the angle of elevation of the camera from the entity's point of view)
  // Important that outsideD < d;

  constexpr float outsideD = 120.0; // cameraHeight / 0.36;
  const float outsideDSquared = outsideD * outsideD;

  // DebugTrace("2dcamdist = %f, outsideD = %f\n", B.Mag(), outsideD);
  if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(B)) < outsideDSquared)
  {
    DirectX::XMStoreFloat3(&cameraTarget,
                           DirectX::XMVectorMultiplyAdd(DirectX::XMVector3Normalize(B), DirectX::XMVectorReplicate(outsideD), flatEntityPos));
    cameraTarget.y = m_pos.y;
    return true;
  }

  return false;
}

bool Camera::AdvanceCanSeeUnits(DirectX::XMFLOAT3& targetCamera)
{
  DirectX::XMVECTOR const pos = DirectX::XMLoadFloat3(&m_pos);
  DirectX::XMVECTOR const entityPos = DirectX::XMLoadFloat3(&m_predictedEntityPos);
  float distToEntity = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(entityPos, pos)));

  if (DirectDistanceToBlockage(pos, entityPos, distToEntity) < distToEntity - 1.0f)
  {
    // Braced to zero: GetHighestTangentPoint writes this only if it finds a
    // gradient steeper than the one it starts from, and Vector3's default
    // constructor is what made the miss case the origin rather than stack junk.
    DirectX::XMFLOAT3 blockage{0.0f, 0.0f, 0.0f};
    GetHighestTangentPoint(entityPos, pos, distToEntity, blockage);

    DirectX::XMVECTOR const ray = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&blockage), entityPos));
    DirectX::XMStoreFloat3(&targetCamera, DirectX::XMVectorMultiplyAdd(ray, DirectX::XMVectorReplicate(distToEntity), entityPos));
    return true;
  }

  return false;
}

bool Camera::AdvanceNotTooLow(DirectX::XMFLOAT3& targetCamera)
{
  // Code to check if the camera is not too low of the ground
  float cameraHeight = m_pos.y - g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);

  if (m_pos.y < MIN_HEIGHT || cameraHeight < MIN_TRACKING_HEIGHT * m_heightMultiplier)
  {
    targetCamera = m_pos;
    targetCamera.y += 20;
    return true;
  }

  return false;
}

bool Camera::AdvanceNotTooFarAway(DirectX::XMFLOAT3& targetCamera)
{
  DirectX::XMVECTOR const entityPos = DirectX::XMLoadFloat3(&m_predictedEntityPos);
  DirectX::XMVECTOR const entityToCamera = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), entityPos);

  float maxDist = m_distFromEntity * 1.2f;

  if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(entityToCamera)) > maxDist * maxDist)
  {
    DirectX::XMStoreFloat3(&targetCamera,
                           DirectX::XMVectorMultiplyAdd(DirectX::XMVector3Normalize(entityToCamera), DirectX::XMVectorReplicate(maxDist), entityPos));
    return true;
  }

  return false;
}

void Camera::RotateTowardsEntity(Entity* entity)
{
  float factor1 = g_advanceTime * 2.0f;
  float factor2 = 1.0f - factor1;

  // We deliberately overshoot the target pos?

  DirectX::XMFLOAT3 newTargetPos = entity->GetCameraFocusPoint();
  DirectX::XMVECTOR const targetPos = DirectX::XMVectorAdd(DirectX::XMVectorScale(DirectX::XMLoadFloat3(&newTargetPos), factor1),
                                                           DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_targetPos), factor2));
  DirectX::XMStoreFloat3(&m_targetPos, targetPos);

  DirectX::XMVECTOR const targetFront = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(targetPos, DirectX::XMLoadFloat3(&m_pos)));
  DirectX::XMVECTOR const front =
    DirectX::XMVectorAdd(DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_front), factor2), DirectX::XMVectorScale(targetFront, factor1));
  DirectX::XMStoreFloat3(&m_front, front);

  DirectX::XMVECTOR const right = DirectX::XMVector3Cross(DirectX::XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f), front);
  DirectX::XMStoreFloat3(&m_up, DirectX::XMVector3Cross(right, front));
}

void Camera::AdvanceEntityTrackMode()
{
  if (TheTaskManagerInterface()->m_visible)
    return;

  UpdateEntityTrackingMode();

  // Declared up front so that the jumps to finishMode don't skip over any
  // initialisation.
  Entity* entity = nullptr;
  Task* currentTask = nullptr;
  int halfHeight = 0;
  int halfWidth = 0;

  if (!g_location || !m_entityTrack)
    goto finishMode;

  entity = g_location->GetEntity(m_objectId);
  if (!entity || entity->m_dead)
  {
    WorldObjectId id;
    GetEntityToTrack(id);
    m_objectId = id;
  }

  entity = g_location->GetEntity(m_objectId);
  if (!entity || entity->m_dead)
    goto finishMode;

  currentTask = g_taskManager->GetCurrentTask();
  if (currentTask && currentTask->m_state != Task::StateRunning)
    goto finishMode;

  if (!currentTask && entity->m_type != Entity::TypeOfficer)
    goto finishMode;

  if (!m_objectId.IsValid())
    goto finishMode;

  // Calculate the predicated position of the entity (where it should be at
  // the next frame). This is used by the auxiliary functions.
  DirectX::XMStoreFloat3(&m_predictedEntityPos,
                         DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&entity->m_vel), DirectX::XMVectorReplicate(g_advanceTime),
                                                      DirectX::XMLoadFloat3(&entity->m_pos)));
  m_trackingEntity = entity;

  AdvanceAutomaticTracking();

  // Ensure that the target cursor remains in the centre of the screen
  halfHeight = g_renderer->ScreenH() / 2;
  halfWidth = g_renderer->ScreenW() / 2;
  g_target->SetMousePos(halfWidth, halfHeight);

  return;

finishMode:
  RequestMode(ModeFreeMovement);
}

void XM_CALLCONV Camera::GetHighestTangentPoint(DirectX::FXMVECTOR _from, DirectX::FXMVECTOR _to, float _maxDist, DirectX::XMFLOAT3& location)
{
  constexpr unsigned int numSteps = 40;
  const float distStep = _maxDist / static_cast<float>(numSteps);

  DirectX::XMFLOAT3 from;
  DirectX::XMStoreFloat3(&from, _from);
  DirectX::XMFLOAT3 to;
  DirectX::XMStoreFloat3(&to, _to);

  DirectX::XMFLOAT3 fromAtSameHeight = from;
  fromAtSameHeight.y = to.y;
  DirectX::XMFLOAT3 dir;
  DirectX::XMStoreFloat3(&dir, DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(_to, DirectX::XMLoadFloat3(&fromAtSameHeight))));

  float x = from.x;
  float z = from.z;

  const float deltaX = distStep * dir.x;
  const float deltaZ = distStep * dir.z;

  float maxGradient = 0.0f;
  float distanceTravelled = 0.0f;

  for (int i = 1; i <= numSteps; ++i)
  {
    x += deltaX;
    z += deltaZ;
    distanceTravelled += distStep;

    float landHeight = g_location->m_landscape.m_heightMap->GetValue(x, z);

    float gradient = (landHeight - from.y) / distanceTravelled;

    if (gradient > maxGradient)
    {
      maxGradient = gradient;
      location = DirectX::XMFLOAT3(x, landHeight, z);
    }
  }
}

void XM_CALLCONV Camera::GetHighestPoint(DirectX::FXMVECTOR _from, DirectX::FXMVECTOR _to, float _maxDist, DirectX::XMFLOAT3& location)
{
  constexpr unsigned int numSteps = 40;
  const float distStep = _maxDist / static_cast<float>(numSteps);

  DirectX::XMFLOAT3 from;
  DirectX::XMStoreFloat3(&from, _from);
  DirectX::XMFLOAT3 dir;
  DirectX::XMStoreFloat3(&dir, DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(_to, _from)));

  float maxHeight = 0.0f;

  for (int i = 1; i <= numSteps; ++i)
  {
    float x = from.x + dir.x * distStep * static_cast<float>(i);
    float z = from.z + dir.z * distStep * static_cast<float>(i);
    float landHeight = g_location->m_landscape.m_heightMap->GetValue(x, z);
    if (landHeight > maxHeight)
    {
      maxHeight = landHeight;
      location = DirectX::XMFLOAT3(x, landHeight, z);
    }
  }
}

// Returns the number of meters to the nearest blockage that the camera would
// experience if it travelled in the specified direction. A blockage is defined
// as a piece of land more than 10 metres higher than the camera's current
// height. If there is no blockage FLT_MAX is returned.
float XM_CALLCONV Camera::DirectDistanceToBlockage(DirectX::FXMVECTOR _from, DirectX::FXMVECTOR _to, const float _maxDist)
{
  if (!g_location)
    return FLT_MAX;

  constexpr unsigned int numSteps = 40;
  const float distStep = _maxDist / static_cast<float>(numSteps);

  DirectX::XMFLOAT3 from;
  DirectX::XMStoreFloat3(&from, _from);

  float x = from.x;
  float y = from.y;
  float z = from.z;

  DirectX::XMFLOAT3 dir;
  DirectX::XMStoreFloat3(&dir, DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(_to, _from)));

  const float deltaX = distStep * dir.x;
  const float deltaY = distStep * dir.y;
  const float deltaZ = distStep * dir.z;

  for (int i = 1; i <= numSteps; ++i)
  {
    x += deltaX;
    y += deltaY;
    z += deltaZ;

    float landHeight = g_location->m_landscape.m_heightMap->GetValue(x, z);

    if (landHeight > y)
      return static_cast<float>(i) * distStep;
  }

  return _maxDist;
}

// Expects m_targetPos to have been set
void Camera::AdvanceRadarAimMode()
{
  const int screenH = g_renderer->ScreenH();
  const int screenW = g_renderer->ScreenW();

  DirectX::XMFLOAT3 groundPos = m_targetPos;
  groundPos.y = g_location->m_landscape.m_heightMap->GetValue(groundPos.x, groundPos.z);

  DirectX::XMFLOAT3 focusPos = groundPos;
  focusPos.y += m_height;

  // Set up viewing matrices
  glPushMatrix();
  SetupProjectionMatrix(g_renderer->GetNearPlane(), g_renderer->GetFarPlane());
  SetupModelviewMatrix();

  // Get the 2D mouse coordinates before we move the camera
  DirectX::XMFLOAT3 mousePos3D = TheUserInput()->GetMousePos3d();
  float oldMouseX, oldMouseY;
  Get2DScreenPos(mousePos3D, &oldMouseX, &oldMouseY);
  oldMouseY = screenH - oldMouseY;

  float factor1 = 4.0f * g_advanceTime;
  float factor2 = 1.0f - factor1;

  // Update camera orientation
  if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(DirectX::XMLoadFloat3(&mousePos3D))) > 1.0f)
  {
    DirectX::XMVECTOR const desiredFront =
      DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&mousePos3D), DirectX::XMLoadFloat3(&m_pos)));
    DirectX::XMVECTOR const front = DirectX::XMVector3Normalize(
      DirectX::XMVectorAdd(DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_front), factor2), DirectX::XMVectorScale(desiredFront, factor1)));

    // g_upVector, which is (0,1,0). The right vector is derived from the NEW
    // front, not the one this block was entered with.
    DirectX::XMVECTOR const right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(front, DirectX::g_XMIdentityR1));
    DirectX::XMStoreFloat3(&m_front, front);
    DirectX::XMStoreFloat3(&m_up, DirectX::XMVector3Cross(right, front));
  }

  //
  // Move towards the idealPos

  DirectX::XMFLOAT3 idealPosStore = groundPos;
  idealPosStore.y += m_height;
  DirectX::XMFLOAT3 horiCamFront = m_front;
  horiCamFront.y = 0.0f;
  DirectX::XMVECTOR const idealPos =
    DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&idealPosStore),
                              DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&horiCamFront)), 0.0f + m_height * 1.5f));
  DirectX::XMVECTOR const toIdealPos = DirectX::XMVectorSubtract(idealPos, DirectX::XMLoadFloat3(&m_pos));
  float const distToIdealPos = DirectX::XMVectorGetX(DirectX::XMVector3Length(toIdealPos));
  DirectX::XMStoreFloat3(
    &m_pos, DirectX::XMVectorAdd(DirectX::XMVectorScale(idealPos, factor1), DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_pos), factor2)));

  // Set up viewing matrices
  SetupModelviewMatrix();

  // Get the 2D mouse coordinates now that we have moved the camera
  float newMouseX, newMouseY;
  Get2DScreenPos(mousePos3D, &newMouseX, &newMouseY);
  newMouseY = screenH - newMouseY;

  // Calculate how much to move the cursor to make it look like it is
  // locked to a point on the landscape (we need to take account of
  // sub-pixel error from previous frames)
  static float mouseDeltaX = 0.0f;
  static float mouseDeltaY = 0.0f;
  mouseDeltaX += (newMouseX - oldMouseX) * 1.0f;
  mouseDeltaY += (newMouseY - oldMouseY) * 1.0f;
  int intMouseDeltaX = floorf(mouseDeltaX);
  int intMouseDeltaY = floorf(mouseDeltaY);
  mouseDeltaX -= intMouseDeltaX;
  mouseDeltaY -= intMouseDeltaY;
  newMouseX = g_target->X() + intMouseDeltaX;
  newMouseY = g_target->Y() + intMouseDeltaY;

  // Make sure these movements don't put the mouse cursor somewhere stupid
  if (newMouseX < 0)
    newMouseX = 0;
  if (newMouseX >= screenW)
    newMouseX = screenW - 1;
  if (newMouseY < 0)
    newMouseY = 0;
  if (newMouseY >= screenH)
    newMouseY = screenH - 1;

  // Apply the mouse cursor movement
  // alleg    position_mouse(newMouseX, newMouseY);
  g_target->SetMousePos(newMouseX, newMouseY);
  glPopMatrix();
}

void Camera::AdvanceTurretAimMode()
{
  const int screenH = g_renderer->ScreenH();
  const int screenW = g_renderer->ScreenW();

  DirectX::XMFLOAT3 groundPos = m_targetPos;
  groundPos.y += 20.0f;
  float minY = g_location->m_landscape.m_heightMap->GetValue(groundPos.x, groundPos.z);

  DirectX::XMStoreFloat3(
    &groundPos, DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&groundPos), DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_front), m_height)));
  // groundPos.y = max( groundPos.y, minY );

  // groundPos.y = ;
  // groundPos.y -= 10.0f;

  DirectX::XMFLOAT3 focusPos = groundPos;
  focusPos.y += m_height;

  // Set up viewing matrices
  glPushMatrix();
  SetupProjectionMatrix(g_renderer->GetNearPlane(), g_renderer->GetFarPlane());
  SetupModelviewMatrix();

  // Get the 2D mouse coordinates before we move the camera
  DirectX::XMFLOAT3 mousePos3D = TheUserInput()->GetMousePos3d();
  float oldMouseX, oldMouseY;
  Get2DScreenPos(mousePos3D, &oldMouseX, &oldMouseY);
  oldMouseY = screenH - oldMouseY;

  float factor1 = 4.0f * g_advanceTime;
  float factor2 = 1.0f - factor1;

  // Update camera orientation
  if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(DirectX::XMLoadFloat3(&mousePos3D))) > 1.0f)
  {
    DirectX::XMVECTOR const desiredFront =
      DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&mousePos3D), DirectX::XMLoadFloat3(&m_pos)));
    DirectX::XMVECTOR const front = DirectX::XMVector3Normalize(
      DirectX::XMVectorAdd(DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_front), factor2), DirectX::XMVectorScale(desiredFront, factor1)));

    // g_upVector, which is (0,1,0). The right vector is derived from the NEW
    // front, not the one this block was entered with.
    DirectX::XMVECTOR const right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(front, DirectX::g_XMIdentityR1));
    DirectX::XMStoreFloat3(&m_front, front);
    DirectX::XMStoreFloat3(&m_up, DirectX::XMVector3Cross(right, front));
  }

  //
  // Move towards the idealPos

  DirectX::XMFLOAT3 idealPosStore = groundPos;
  idealPosStore.y += m_height;
  DirectX::XMFLOAT3 horiCamFront = m_front;
  horiCamFront.y = 0.0f;
  DirectX::XMVECTOR const idealPos =
    DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&idealPosStore),
                              DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&horiCamFront)), 0.0f + m_height * 0.4f));
  DirectX::XMVECTOR const toIdealPos = DirectX::XMVectorSubtract(idealPos, DirectX::XMLoadFloat3(&m_pos));
  float const distToIdealPos = DirectX::XMVectorGetX(DirectX::XMVector3Length(toIdealPos));
  DirectX::XMStoreFloat3(
    &m_pos, DirectX::XMVectorAdd(DirectX::XMVectorScale(idealPos, factor1), DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_pos), factor2)));

  // Set up viewing matrices
  SetupModelviewMatrix();

  // Get the 2D mouse coordinates now that we have moved the camera
  float newMouseX, newMouseY;
  Get2DScreenPos(mousePos3D, &newMouseX, &newMouseY);
  newMouseY = screenH - newMouseY;

  // Calculate how much to move the cursor to make it look like it is
  // locked to a point on the landscape (we need to take account of
  // sub-pixel error from previous frames)
  static float mouseDeltaX = 0.0f;
  static float mouseDeltaY = 0.0f;
  mouseDeltaX += (newMouseX - oldMouseX) * 1.0f;
  mouseDeltaY += (newMouseY - oldMouseY) * 1.0f;
  int intMouseDeltaX = floorf(mouseDeltaX);
  int intMouseDeltaY = floorf(mouseDeltaY);
  mouseDeltaX -= intMouseDeltaX;
  mouseDeltaY -= intMouseDeltaY;
  newMouseX = g_target->X() + intMouseDeltaX;
  newMouseY = g_target->Y() + intMouseDeltaY;

  // Make sure these movements don't put the mouse cursor somewhere stupid
  if (newMouseX < 0)
    newMouseX = 0;
  if (newMouseX >= screenW)
    newMouseX = screenW - 1;
  if (newMouseY < 0)
    newMouseY = 0;
  if (newMouseY >= screenH)
    newMouseY = screenH - 1;

  // Apply the mouse cursor movement
  // alleg    position_mouse(newMouseX, newMouseY);
  g_target->SetMousePos(newMouseX, newMouseY);
  glPopMatrix();
}

void Camera::AdvanceFirstPersonMode()
{
  if (g_inputManager->controlEvent(ControlCameraFreeMovement))
  {
    RequestMode(ModeFreeMovement);
    return;
  }

  // JAMES_TODO: Support directional firing mode
  if (g_inputManager->controlEvent(ControlUnitPrimaryFireTarget))
  {
    static float lastFire = 0.0f;
    if (GetHighResTime() > lastFire)
    {
      DirectX::XMFLOAT3 const rightStore = GetRight();
      DirectX::XMFLOAT3 const upStore = GetUp();
      DirectX::XMFLOAT3 from;
      DirectX::XMStoreFloat3(&from, DirectX::XMVectorAdd(DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos),
                                                                              DirectX::XMVectorScale(DirectX::XMLoadFloat3(&rightStore), -2.0f)),
                                                         DirectX::XMVectorScale(DirectX::XMLoadFloat3(&upStore), -3.0f)));
      DirectX::XMFLOAT3 laserDir;
      DirectX::XMStoreFloat3(&laserDir, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_front), 200.0f));
      g_location->FireLaser(from, laserDir, 3);
      lastFire = GetHighResTime() + 0.1f;
    }
  }

  //
  // Allow quake keys to move us

  // Both are dead: every use of them below is commented out, and has been
  // since the code was inherited. Kept because the commented lines are the
  // only record of what this mode was meant to do.
  DirectX::XMFLOAT3 accelForward = m_front;
  accelForward.y = 0.0f;
  DirectX::XMStoreFloat3(&accelForward, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&accelForward)));
  DirectX::XMFLOAT3 accelRight = GetRight();
  accelRight.y = 0.0f;
  DirectX::XMStoreFloat3(&accelRight, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&accelRight)));
  float moveRate = 100.0f;
  //    if (g_controlBindings->CameraLeft())		m_vel += accelRight * g_advanceTime * moveRate;
  //	if (g_controlBindings->CameraRight())		m_vel -= accelRight * g_advanceTime * moveRate;
  //	if (g_controlBindings->CameraForwards())	m_vel += accelForward * g_advanceTime * moveRate * 2.0f;
  //	if (g_controlBindings->CameraBackwards())	m_vel -= accelForward * g_advanceTime * moveRate;

  //
  // Update our position and orientation

  DirectX::XMVECTOR const vel = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), 0.9993f);
  DirectX::XMStoreFloat3(&m_vel, vel);
  DirectX::XMStoreFloat3(&m_pos, DirectX::XMVectorMultiplyAdd(vel, DirectX::XMVectorReplicate(g_advanceTime), DirectX::XMLoadFloat3(&m_pos)));

  int mx = 0, my = 0;
  mx = g_target->dX();
  my = g_target->dY();

  // Same sign trap as AdvanceDebugMode, and this one reaches it through
  // RotateAround(norm, angle) rather than FastRotateAround -- the two differ
  // only in whether they normalise the axis first, so the inversion is the
  // same and the angle flips sign here too.
  DirectX::XMMATRIX const yaw = DirectX::XMMatrixRotationY(static_cast<float>(mx) * -0.01f);
  DirectX::XMStoreFloat3(&m_up, DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&m_up), yaw));
  DirectX::XMStoreFloat3(&m_front, DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&m_front), yaw));

  DirectX::XMFLOAT3 const rightAxis = GetRight();
  DirectX::XMMATRIX const pitch = DirectX::XMMatrixRotationAxis(DirectX::XMLoadFloat3(&rightAxis), static_cast<float>(my) * -0.01f);
  DirectX::XMStoreFloat3(&m_up, DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&m_up), pitch));
  DirectX::XMStoreFloat3(&m_front, DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&m_front), pitch));

  float landHeight = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);
  m_pos.y = landHeight + 3.0f;
}

void Camera::AdvanceMoveToTargetMode()
{
  double currentTimeFraction = (g_gameTime - m_startTime) / m_moveDuration;
  // static_cast rather than 0.2, because the two are different numbers: the max
  // macro promoted 0.2f to double through the conditional operator, giving
  // 0.20000000298023224, and the double literal 0.2 is 0.2000000000000000111.
  // Writing 0.2 here would be a changed computed value dressed up as a tidy-up.
  currentTimeFraction = std::max(currentTimeFraction, static_cast<double>(0.2f));

  // Pos
  DirectX::XMVECTOR const startFront = DirectX::XMLoadFloat3(&m_startFront);
  DirectX::XMVECTOR const targetFront = DirectX::XMLoadFloat3(&m_targetFront);
  DirectX::XMVECTOR direction = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_targetPos), DirectX::XMLoadFloat3(&m_startPos));
  float dist = DirectX::XMVectorGetX(DirectX::XMVector3Length(direction));
  // operator/= multiplied by the reciprocal once, so this is not XMVector3Normalize.
  direction = DirectX::XMVectorScale(direction, 1.0f / dist);
  float maxSpeed = (2.0f * dist) / m_moveDuration;

  // Orientation
  DirectX::XMVECTOR const fullRotationDirection = DirectX::XMVector3Cross(startFront, targetFront);
  DirectX::XMVECTOR const middleFront = RotateAroundScaledAxis(startFront, DirectX::XMVectorScale(fullRotationDirection, 0.5f));

  if (currentTimeFraction < 0.5)
  {
    DirectX::XMStoreFloat3(
      &m_pos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos),
                                   DirectX::XMVectorScale(direction, static_cast<float>(maxSpeed * 2.0 * currentTimeFraction * g_advanceTime))));
    float amount2 = RampUpAndDown(m_startTime, m_moveDuration, g_gameTime) * 2.0f;
    float amount1 = 1.0f - amount2;
    DirectX::XMStoreFloat3(&m_front, DirectX::XMVector3Normalize(DirectX::XMVectorAdd(DirectX::XMVectorScale(startFront, amount1),
                                                                                      DirectX::XMVectorScale(middleFront, amount2))));
  }
  else if (currentTimeFraction < 1.0)
  {
    DirectX::XMStoreFloat3(
      &m_pos,
      DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos),
                           DirectX::XMVectorScale(direction, static_cast<float>(maxSpeed * 2.0 * (1.0 - currentTimeFraction) * g_advanceTime))));
    float amount2 = (RampUpAndDown(m_startTime, m_moveDuration, g_gameTime) - 0.5f) * 2.0f;
    float amount1 = 1.0f - amount2;
    DirectX::XMStoreFloat3(&m_front, DirectX::XMVector3Normalize(DirectX::XMVectorAdd(DirectX::XMVectorScale(middleFront, amount1),
                                                                                      DirectX::XMVectorScale(targetFront, amount2))));
  }
  else
  {
    RequestMode(ModeDoNothing);
    m_front = m_targetFront;
  }

  DirectX::XMStoreFloat3(&m_front, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&m_front)));
  m_up = s_defaultUp; // was g_upVector
  // Vector3 right = m_front ^ m_up;
  // right.Normalise();
  // m_up = right ^ m_front;
  // m_up.Normalise();

  // float dot = m_front * m_up;
  // dot *= 1.0f;
}

void Camera::AdvanceEntityFollowMode()
{
  auto obj = g_location->GetEntity(m_objectId);
  if (!obj)
  {
    RequestMode(ModeFreeMovement);
    return;
  }

  //
  // Get X and Y mouse move

  int halfHeight = g_renderer->ScreenH() / 2;
  int halfWidth = g_renderer->ScreenW() / 2;
  int deltaX = g_target->X() - halfWidth;
  int deltaY = g_target->Y() - halfHeight;
  g_target->SetMousePos(halfWidth, halfHeight);

  //
  // Get Z mouse move

  m_distFromEntity *= 1.0f + (static_cast<float>(g_target->dZ()) * -0.1f);
  float radius = 15.0f;
  if (m_distFromEntity < radius)
    m_distFromEntity = radius;
  else if (m_distFromEntity > 5000.0f)
    m_distFromEntity = 5000.0f;

  //
  // Do rotation

  float rotY = static_cast<float>(deltaX) * -0.015f;
  float rotRight = static_cast<float>(deltaY) * -0.01f;

  DirectX::XMVECTOR front = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&m_front), DirectX::XMMatrixRotationY(rotY));
  // g_upVector, which is (0,1,0).
  DirectX::XMVECTOR const right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(front, DirectX::g_XMIdentityR1));
  DirectX::XMVECTOR const scaledAxis = DirectX::XMVectorScale(right, rotRight);
  DirectX::XMVECTOR const newUp = RotateAroundScaledAxis(DirectX::XMLoadFloat3(&m_up), scaledAxis);
  if (DirectX::XMVectorGetY(newUp) > 0.1f)
    front = RotateAroundScaledAxis(front, scaledAxis);
  DirectX::XMStoreFloat3(&m_front, front);
  DirectX::XMStoreFloat3(&m_up, DirectX::XMVector3Normalize(DirectX::XMVector3Cross(right, front)));

  //
  // Update position

  float factor1 = g_advanceTime * 5.0f;
  float factor2 = 1.0f - factor1;
  DirectX::XMVECTOR const newTargetPos = DirectX::XMVectorMultiplyAdd(
    DirectX::XMLoadFloat3(&obj->m_vel), DirectX::XMVectorReplicate(g_predictionTime), DirectX::XMLoadFloat3(&obj->m_pos));
  DirectX::XMVECTOR const targetPos =
    DirectX::XMVectorAdd(DirectX::XMVectorScale(newTargetPos, factor1), DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_targetPos), factor2));
  DirectX::XMStoreFloat3(&m_targetPos, targetPos);
  DirectX::XMStoreFloat3(&m_pos, DirectX::XMVectorSubtract(targetPos, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_front), m_distFromEntity)));
}

// **************
// Public Methods
// **************

// *** Constructor
Camera::Camera()
  : m_fov(60.0f),
    m_height(50.0f),
    m_vel(0.0f, 0.0f, 0.0f),
    m_targetFov(60.0f),
    m_trackingEntity(nullptr),
    m_distFromEntity(100.0f),
    m_currentDistance(0.0f),
    m_heightMultiplier(1.0f),
    m_mode(ModeDoNothing),
    m_debugMode(DebugModeAuto),
    m_framesInThisMode(0),
    m_objectId(),
    m_anim(nullptr),
    m_cameraShake(0.0f),
    m_entityTrack(false),
    m_skipDirectionCalculation(false)
{
  m_cosFov = cos(m_fov / 180.0f * M_PI);
  m_pos = DirectX::XMFLOAT3(1000.0f,          // g_location->m_landscape.GetWorldSizeX() / 2.0f,
                            500.0f, 1000.0f); // g_location->m_landscape.GetWorldSizeZ() / 2.0f);

  m_minX = -1e6;
  m_maxX = 1e6;
  m_minZ = -1e6;
  m_maxZ = 1e6;

  // m_front = (0, -0.7f, 1);
  DirectX::XMVECTOR const front = DirectX::XMVector3Normalize(DirectX::XMVectorSet(0.0f, -0.5f, -1.0f, 0.0f));
  DirectX::XMStoreFloat3(&m_front, front);

  // m_up was assigned g_upVector purely to feed the cross product below, and
  // the result overwrites it two lines later.
  DirectX::XMVECTOR const right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::g_XMIdentityR1, front));
  DirectX::XMStoreFloat3(&m_up, DirectX::XMVector3Normalize(DirectX::XMVector3Cross(front, right)));
  DirectX::XMStoreFloat3(&m_controlVector, right);
}

void Camera::CreateCameraShake(float _intensity) { m_cameraShake = std::max(m_cameraShake, _intensity); }

void Camera::SetupProjectionMatrix(float _nearPlane, float _farPlane)
{
  // DEBUG_ASSERT(m_fov > 0.0f);
  // DEBUG_ASSERT(m_fov < 180.0f);

  ClampInPlace(m_fov, 1, 180);

  TheRenderer()->SetNearAndFar(_nearPlane, _farPlane);
  TheRenderer()->SetupProjMatrixFor3D();

  float fovRadians = m_fov * M_PI / 180.0f;
  m_cosFov = cosf(fovRadians);

  // m_fov is actually the vertical fov. We need a fov covering
  // the long diagonal of the screen for visibility checking.

  float screenW = g_renderer->ScreenW();
  float screenWHalf = screenW / 2.0;
  float screenH = g_renderer->ScreenH();
  float screenHHalf = screenH / 2.0;

  // Distance from camera to top-centre and bottom-centre of screen
  float dtc = screenHHalf / sin(fovRadians / 2.0);

  // Distance from camera to any corner of the screen
  float dc = sqrt(dtc * dtc + screenWHalf * screenWHalf);

  // Half distance from one corner of the screen to the diagonally opposite corner
  float ddHalf = sqrt(screenW * screenW + screenH * screenH) / 2.0;

  m_maxFovRadians = 2.0 * asin(ddHalf / dc);
}

void Camera::SetupModelviewMatrix()
{
  DirectX::XMVECTOR const frontVec = DirectX::XMLoadFloat3(&m_front);
  DirectX::XMVECTOR const upVec = DirectX::XMLoadFloat3(&m_up);
  DirectX::XMVECTOR const posVec = DirectX::XMLoadFloat3(&m_pos);

  // operator* between two Vector3s was the DOT product.
  float dot = DirectX::XMVectorGetX(DirectX::XMVector3Dot(frontVec, upVec));
  DEBUG_ASSERT(NearlyEquals(DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(frontVec)), 1.0f));
  DEBUG_ASSERT(NearlyEquals(DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(upVec)), 1.0f));
  DEBUG_ASSERT(NearlyEquals(dot, 0.0f));

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  // gluLookAt owns the view matrix here, not DirectXMath. NeuronMath.h is
  // explicit that projection and view construction stay in renderer code
  // because the depth range differs between GL and D3D, so this stays a
  // gluLookAt call fed three points rather than becoming XMMatrixLookAtRH.
  float magOfPos = DirectX::XMVectorGetX(DirectX::XMVector3Length(posVec));
  DirectX::XMFLOAT3 up;
  DirectX::XMStoreFloat3(&up, DirectX::XMVectorScale(upVec, magOfPos));
  DirectX::XMFLOAT3 forwards;
  DirectX::XMStoreFloat3(&forwards, DirectX::XMVectorMultiplyAdd(frontVec, DirectX::XMVectorReplicate(magOfPos), posVec));
  gluLookAt(m_pos.x, m_pos.y, m_pos.z, forwards.x, forwards.y, forwards.z, up.x, up.y, up.z);
}

bool Camera::PosInViewFrustum(DirectX::XMFLOAT3 const& _pos)
{
  DirectX::XMVECTOR const dirToPos =
    DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&_pos), DirectX::XMLoadFloat3(&m_pos)));
  // operator* between two Vector3s was the DOT product.
  float angle = DirectX::XMVectorGetX(DirectX::XMVector3Dot(dirToPos, DirectX::XMLoadFloat3(&m_front)));
  float fovInRadians = m_cosFov;
  float tolerance = 0.2f;
  if (angle < fovInRadians - tolerance)
    return false;
  return true;
}

bool Camera::SphereInViewFrustum(DirectX::XMFLOAT3 const& _centre, float _radius)
{
  DirectX::XMVECTOR const dirToPos = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&_centre), DirectX::XMLoadFloat3(&m_pos));
  float distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(dirToPos));
  if (distance < _radius)
    return true;

  float angularSize = asinf(_radius / distance);
  float angle = acosf(DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMVector3Normalize(dirToPos), DirectX::XMLoadFloat3(&m_front))));

  return angle - angularSize <= m_maxFovRadians / 2;
}

Building* Camera::GetBestBuildingInView()
{
  static float s_recalculateTimer = 0;
  static int s_buildingId = -1;

  DirectX::XMFLOAT3 const velStore = GetVel();
  if (DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&velStore))) > 5.0f)
  {
    // We are moving too fast to be focussing on a building
    s_buildingId = -1;
  }
  else if (!g_location)
  {
    // We aren't in a location
    s_buildingId = -1;
  }
  else
  {
    if (GetHighResTime() > s_recalculateTimer + 1.0f)
    {
      // GetClickRay writes both unconditionally, so these need no initialiser
      // beyond the braces that replace Vector3's zeroing constructor.
      DirectX::XMFLOAT3 rayStart{0.0f, 0.0f, 0.0f};
      DirectX::XMFLOAT3 rayDir{0.0f, 0.0f, 0.0f};
      GetClickRay(g_renderer->ScreenW() / 2, g_renderer->ScreenH() / 2, &rayStart, &rayDir);

      float nearest = 200.0f;
      s_buildingId = -1;

      for (int i = 0; i < g_location->m_buildings.Size(); ++i)
      {
        if (g_location->m_buildings.ValidIndex(i))
        {
          Building* building = g_location->m_buildings[i];
          if (building->DoesRayHit(rayStart, rayDir))
          {
            float distance = DirectX::XMVectorGetX(
              DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&building->m_pos), DirectX::XMLoadFloat3(&m_pos))));
            if (distance < nearest)
            {
              nearest = distance;
              s_buildingId = building->m_id.GetUniqueId();
            }
          }
        }
      }

      s_recalculateTimer = GetHighResTime();
    }
  }

  return g_location->GetBuilding(s_buildingId);
}

void Camera::AdvanceComponentZoom()
{
  // No zoom inside the task manager
  if (TheTaskManagerInterface()->m_visible || IsInMode(ModeEntityTrack))
    return;

  float change = 30.0f;
  float adjustedTargetFov = m_targetFov;

  // JAMES CHECK:
  if (g_inputManager->controlEvent(ControlCameraZoom))
  {
    adjustedTargetFov /= 4.0f;
    change = 100.0f;
  }

  if (m_mode == ModeSphereWorldScripted)
    change = 10.0f;

  if (m_mode == ModeMoveToTarget || m_mode == ModeDoNothing || m_mode == ModeBuildingFocus)
    change = 1.0f;

  //    if( m_fov < adjustedTargetFov )
  //    {
  //        m_fov += change * g_advanceTime;
  //        if( m_fov > adjustedTargetFov )
  //        {
  //            m_fov = adjustedTargetFov;
  //        }
  //    }
  //    else if( m_fov > adjustedTargetFov )
  //    {
  //        m_fov -= change * g_advanceTime;
  //        if( m_fov < adjustedTargetFov )
  //        {
  //            m_fov = adjustedTargetFov;
  //        }
  //    }

  float factor = g_advanceTime * change * 0.1f;
  m_fov = m_fov * (1 - factor) + adjustedTargetFov * factor;

  //    if( m_cameraShake > 0.0f )
  //    {
  //        m_fov += sfrand( m_cameraShake * 10.0f );
  //        m_cameraShake -= SERVER_ADVANCE_PERIOD;
  //    }
}

void Camera::AdvanceComponentMouseWheelHeight()
{
  float delta = g_target->dZ();

  if (EclGetWindows()->size() == 0)
  {
    bool keyUp = g_inputManager->controlEvent(ControlCameraUp);
    bool keyDown = g_inputManager->controlEvent(ControlCameraDown);
    if (keyUp)
      delta += g_advanceTime * 7.0f;
    if (keyDown)
      delta -= g_advanceTime * 7.0f;
    if (keyUp || keyDown)
    {
      TheControlHelp()->RecordCondUsed(ControlHelpSystem::CondCameraUp);
      TheControlHelp()->RecordCondUsed(ControlHelpSystem::CondCameraDown);
    }
  }

  if (g_location)
  {
    float landheight = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);
    if (landheight < MIN_HEIGHT)
      landheight = MIN_HEIGHT;
    float altitude = m_height - landheight;
    m_height += delta * 2.0f * sqrtf(fabsf(altitude));

    if (m_mode == ModeTurretAim)
      m_height = std::max(m_height, MIN_GROUND_CLEARANCE);
    else
    {
      if (landheight + MIN_GROUND_CLEARANCE > m_height)
        m_height = landheight + MIN_GROUND_CLEARANCE;
    }

    if (m_height > MAX_HEIGHT)
      m_height = MAX_HEIGHT;
  }
  else
  {
    m_height += delta * (m_height * 0.1f + 5.0f);
    if (m_height < 0.0f)
      m_height = 0.1f;
    else if (m_height > 1000.0f)
      m_height = 1000.0f;
  }
}

void Camera::AdvanceAnim()
{
  CamAnimNode* node = m_anim->m_nodes[m_animCurrentNode].get();
  float finishTime = m_animNodeStartTime + node->m_duration;

  if (g_gameTime > finishTime)
  {
    switch (node->m_transitionMode)
    {
    case CamAnimNode::TransitionMove:
      break;
    case CamAnimNode::TransitionCut:
      CutToTarget();
      break;
    }

    m_animCurrentNode++;
    if (m_animCurrentNode < static_cast<int>(m_anim->m_nodes.size()))
    {
      node = m_anim->m_nodes[m_animCurrentNode].get();
      SetTarget(node->m_mountName);
      m_animNodeStartTime = g_gameTime;

      switch (node->m_transitionMode)
      {
      case CamAnimNode::TransitionMove:
        RequestMode(ModeMoveToTarget);
        SetMoveDuration(node->m_duration);
        break;
      case CamAnimNode::TransitionCut:
        RequestMode(ModeDoNothing);
        break;
      }
    }
    else
    {
      RequestMode(m_modeBeforeAnim);
      m_anim = nullptr;
    }
  }
}

void Camera::AdvanceMainMenuMode()
{
  // The vector forms of RotateAroundZ/Y are ordinary right-handed rotations,
  // which XMMatrixRotationZ/Y are under `v * M`. Applied in the same order.
  DirectX::XMVECTOR pos = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&m_pos), DirectX::XMMatrixRotationZ(0.0005f));
  pos = DirectX::XMVector3Transform(pos, DirectX::XMMatrixRotationY(0.0005f));
  DirectX::XMStoreFloat3(&m_pos, pos);
  float factor1 = g_advanceTime * 2.0f;
  float factor2 = 1.0f - factor1;

  // targetFront is the zero vector, so the second term contributes nothing and
  // this is a plain decay of m_front. Left written out because the commented
  // (50000, 50000, 50000) above it is what the line is for.
  DirectX::XMVECTOR const front = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_front), factor2);
  DirectX::XMStoreFloat3(&m_front, front);
  DirectX::XMVECTOR const right = DirectX::XMVector3Cross(DirectX::XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f), front);
  DirectX::XMStoreFloat3(&m_up, DirectX::XMVector3Cross(right, front));
}

void Camera::Advance()
{
  START_PROFILE(g_profiler, "Advance Camera");

  if (m_anim)
    AdvanceAnim();

  // Toggle entity tracking
  /*if (g_inputManager->controlEvent( ControlCameraSwitchMode ))
    m_entityTrack = !m_entityTrack;*/

  //	switch (m_mode)
  //	{
  //		case ModeReplay:
  //		case ModeSphereWorld:
  //		case ModeFreeMovement:
  //		case ModeBuildingFocus:
  //		case ModeEntityTrack:
  //		case ModeRadarAim:
  //		case ModeFirstPerson:
  //		case ModeEntityFollow:
  //        case ModeTurretAim:
  //        case ModeMoveToTarget:
  //        case ModeSphereWorldScripted:
  AdvanceComponentZoom();
  //	}

  switch (m_mode)
  {
  case ModeSphereWorld:
  case ModeFreeMovement:
  case ModeBuildingFocus:
  case ModeEntityTrack:
  case ModeRadarAim:
  case ModeFirstPerson:
  case ModeTurretAim:
    AdvanceComponentMouseWheelHeight();
  }

  //
  // Pick an advancer

  if (m_anim == nullptr &&
      (m_debugMode == DebugModeAlways || (m_debugMode == DebugModeAuto && EclGetWindows()->size() > 0) || m_framesInThisMode < 2))
  {
    g_windowManager->EnsureMouseUncaptured();
    AdvanceDebugMode();
  }
  else
  {
    g_windowManager->EnsureMouseCaptured();
    switch (m_mode)
    {
    case ModeSphereWorld:
      AdvanceSphereWorldMode();
      break;
    case ModeFreeMovement:
      AdvanceFreeMovementMode();
      break;
    case ModeBuildingFocus:
      AdvanceBuildingFocusMode();
      break;
    case ModeEntityTrack:
      AdvanceEntityTrackMode();
      break;
    case ModeRadarAim:
      AdvanceRadarAimMode();
      break;
    case ModeFirstPerson:
      AdvanceFirstPersonMode();
      break;
    case ModeMoveToTarget:
      AdvanceMoveToTargetMode();
      break;
    case ModeEntityFollow:
      AdvanceEntityFollowMode();
      break;
    case ModeTurretAim:
      AdvanceTurretAimMode();
      break;
    case ModeSphereWorldScripted:
      AdvanceSphereWorldScriptedMode();
      break;
    case ModeSphereWorldIntro:
      AdvanceSphereWorldIntroMode();
      break;
    case ModeSphereWorldOutro:
      AdvanceSphereWorldOutroMode();
      break;
    case ModeSphereWorldFocus:
      AdvanceSphereWorldFocusMode();
      break;
    case ModeMainMenu:
      AdvanceMainMenuMode();
      break;
    }
  }

  if (m_cameraShake > 0.0f)
  {
    DirectX::XMVECTOR front = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&m_front), DirectX::XMMatrixRotationY(sfrand(m_cameraShake * 0.05f)));
    // g_upVector, rotated about the freshly yawed front.
    DirectX::XMVECTOR const up =
      RotateAroundScaledAxis(DirectX::g_XMIdentityR1, DirectX::XMVectorScale(DirectX::XMVectorScale(front, sfrand(m_cameraShake)), 0.3f));
    DirectX::XMVECTOR const right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(front, up));
    front = DirectX::XMVector3Normalize(front);
    DirectX::XMStoreFloat3(&m_front, front);
    DirectX::XMStoreFloat3(&m_up, DirectX::XMVector3Cross(right, front));
    m_cameraShake -= g_advanceTime;
  }

  Normalise();

  ASSERT_VECTOR3_IS_SANE(m_pos);
  // operator* between two Vector3s was the DOT product.
  float dot = DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMLoadFloat3(&m_front), DirectX::XMLoadFloat3(&m_up)));
  DEBUG_ASSERT(NearlyEquals(dot, 0.0f));

  TheUserInput()->RecalcMousePos3d();

  m_framesInThisMode++;

  END_PROFILE(g_profiler, "Advance Camera");
}

int Camera::GetDebugMode() { return m_debugMode; }

void Camera::SetDebugMode(int _mode) { m_debugMode = _mode; }

void Camera::SetNextDebugMode()
{
  m_debugMode++;
  if (m_debugMode >= DebugModeNumStates)
    m_debugMode = 0;
}

void Camera::RequestMode(int _mode)
{
  DEBUG_ASSERT(_mode >= 0 && _mode < ModeNumModes);
  int screenW = g_renderer->ScreenW();
  int screenH = g_renderer->ScreenH();

  // m_targetFov = 60.0f;
  m_framesInThisMode = 0;
  m_mode = _mode;

  switch (_mode)
  {
  case ModeSphereWorld:
    g_target->SetMousePos(screenW / 2, screenH / 2);
    m_pos = DirectX::XMFLOAT3(1000.0f, 500.0f, 15000.0f);
    break;
  case ModeFreeMovement:
    m_targetPos = m_pos;
    m_height = m_pos.y;
    m_targetFov = 60.0f;
    g_target->SetMousePos(screenW / 2, screenH / 2);
    break;
  case ModeMoveToTarget:
    m_startPos = m_pos;
    m_startFront = m_front;
    m_startUp = m_up;
    m_startTime = g_gameTime;
    break;
  }
}

void Camera::RequestBuildingFocusMode(Building* _building, float _range, float _height)
{
  m_framesInThisMode = 0;
  m_mode = ModeBuildingFocus;
  m_targetPos = _building->m_centrePos;
  m_trackRange = _range;
  m_trackHeight = _height;
  m_trackTimer = GetHighResTime();

  DirectX::XMStoreFloat3(&m_trackVector, DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&_building->m_pos)));
  m_trackVector.y = 0.0f;
  DirectX::XMStoreFloat3(&m_trackVector, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&m_trackVector)));

  m_targetFov = 60.0f;
}

void Camera::RequestRadarAimMode(Building* _building)
{
  m_framesInThisMode = 0;
  m_mode = ModeRadarAim;
  m_targetPos = _building->m_pos;
}

void Camera::RequestTurretAimMode(Building* _building)
{
  m_framesInThisMode = 0;
  m_mode = ModeTurretAim;
  m_objectId = _building->m_id;
  m_targetPos = _building->m_pos;
}

void Camera::RequestEntityTrackMode(const WorldObjectId& _id)
{
  m_framesInThisMode = 0;
  m_mode = ModeEntityTrack;

  m_distFromEntity = 200.0f;
  m_objectId = _id;
  m_trackHeight = 0.0f;
  m_cameraTarget = m_pos;

  // Snap the camera to the look at the unit
  Entity* entity = g_location->GetEntity(_id);
  if (entity)
    m_targetPos = entity->m_pos;
}

void Camera::RequestEntityFollowMode(const WorldObjectId& _id)
{
  m_framesInThisMode = 0;
  m_mode = ModeEntityFollow;
  m_objectId = _id;
}

bool Camera::IsMoving() { return m_mode == ModeMoveToTarget; }

bool Camera::IsInteractive()
{
  // if( TheScript()->IsRunningScript() ) return false;

  return (m_mode == ModeSphereWorld || m_mode == ModeFreeMovement || m_mode == ModeRadarAim || m_mode == ModeTurretAim || m_mode == ModeEntityTrack);
}

bool Camera::IsInMode(int _mode) { return (m_mode == _mode); }

void Camera::SetTarget(DirectX::XMFLOAT3 const& _pos, DirectX::XMFLOAT3 const& _front, DirectX::XMFLOAT3 const& _up)
{
  m_targetPos = _pos;
  DirectX::XMVECTOR const targetFront = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&_front));
  DirectX::XMStoreFloat3(&m_targetFront, targetFront);

  DirectX::XMVECTOR const right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&_up), targetFront));
  DirectX::XMStoreFloat3(&m_targetUp, DirectX::XMVector3Cross(targetFront, right));
}

bool Camera::SetTarget(const char* _mountName)
{
  if (stricmp(_mountName, MAGIC_MOUNT_NAME_START_POS) == 0)
  {
    SetTarget(m_posBeforeAnim, m_frontBeforeAnim, m_upBeforeAnim);
    return true;
  }

  for (int i = 0; i < static_cast<int>(g_location->m_levelFile->m_cameraMounts.size()); ++i)
  {
    CameraMount* mount = g_location->m_levelFile->m_cameraMounts[i].get();
    if (stricmp(mount->m_name.c_str(), _mountName) == 0)
    {
      SetTarget(mount->m_pos, mount->m_front, mount->m_up);
      return true;
    }
  }

  return false;
}

void Camera::SetTarget(DirectX::XMFLOAT3 const& _focusPos, float _distance, float _height)
{
  DirectX::XMVECTOR const focusPos = DirectX::XMLoadFloat3(&_focusPos);
  DirectX::XMVECTOR const themToUs = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), focusPos);

  // SetLength answered a zero-length input with (_distance, 0, 0). The camera
  // sitting exactly on its focus point is reachable -- a script can cut to a
  // mount at the same position it is already focusing on -- and the native
  // normalise would hand a QNaN straight into m_targetPos, so the fallback is
  // reproduced here rather than taken natively.
  DirectX::XMVECTOR scaled;
  if (NearlyEquals(DirectX::XMVectorGetX(DirectX::XMVector3Length(themToUs)), 0.0f))
    scaled = DirectX::XMVectorSet(_distance, 0.0f, 0.0f, 0.0f);
  else
    scaled = DirectX::XMVectorScale(DirectX::XMVector3Normalize(themToUs), _distance);

  DirectX::XMFLOAT3 targetPos;
  DirectX::XMStoreFloat3(&targetPos, DirectX::XMVectorAdd(focusPos, scaled));
  targetPos.y = _focusPos.y + _height;

  DirectX::XMVECTOR const targetFrontVec = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(focusPos, DirectX::XMLoadFloat3(&targetPos)));
  DirectX::XMFLOAT3 targetFront;
  DirectX::XMStoreFloat3(&targetFront, targetFrontVec);

  // g_upVector, which is (0,1,0).
  DirectX::XMVECTOR const targetRight = DirectX::XMVector3Cross(targetFrontVec, DirectX::g_XMIdentityR1);
  DirectX::XMFLOAT3 targetUp;
  DirectX::XMStoreFloat3(&targetUp, DirectX::XMVector3Cross(targetRight, targetFrontVec));
  SetTarget(targetPos, targetFront, targetUp);
}

void Camera::SetMoveDuration(float _duration) { m_moveDuration = _duration; }

void Camera::CutToTarget()
{
  m_framesInThisMode = 0;
  m_pos = m_targetPos;
  m_height = m_targetPos.y;
  m_front = m_targetFront;
  m_up = m_targetUp;

  Normalise();
}

void Camera::Normalise()
{
  DirectX::XMVECTOR const front = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&m_front));
  DirectX::XMStoreFloat3(&m_front, front);
  DirectX::XMVECTOR const right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&m_up), front));
  DirectX::XMStoreFloat3(&m_up, DirectX::XMVector3Cross(front, right));
}

void Camera::GetClickRay(int _x, int _y, DirectX::XMFLOAT3* _rayStart, DirectX::XMFLOAT3* _rayDir)
{
  GLint viewport[4];
  GLdouble mvMatrix[16], projMatrix[16];
  GLdouble objx, objy, objz;

  glGetIntegerv(GL_VIEWPORT, viewport);
  glGetDoublev(GL_MODELVIEW_MATRIX, mvMatrix);
  glGetDoublev(GL_PROJECTION_MATRIX, projMatrix);
  int realY = viewport[3] - _y - 1;

  if ((mvMatrix[0] + mvMatrix[1] + mvMatrix[2] == 0) || (mvMatrix[5] + mvMatrix[6] + mvMatrix[7] == 0) ||
      (mvMatrix[9] + mvMatrix[10] + mvMatrix[11] == 0))
  {
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glGetDoublev(GL_MODELVIEW_MATRIX, mvMatrix);
  }

  gluUnProject(_x, realY, 0.0f, mvMatrix, projMatrix, viewport, &objx, &objy, &objz);
  *_rayStart = DirectX::XMFLOAT3(static_cast<float>(objx), static_cast<float>(objy), static_cast<float>(objz));

  gluUnProject(_x, realY, 1.0f, mvMatrix, projMatrix, viewport, &objx, &objy, &objz);
  DirectX::XMVECTOR const rayEnd = DirectX::XMVectorSet(static_cast<float>(objx), static_cast<float>(objy), static_cast<float>(objz), 0.0f);
  DirectX::XMStoreFloat3(_rayDir, DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(rayEnd, DirectX::XMLoadFloat3(_rayStart))));
}

void Camera::SetBounds(float _minX, float _maxX, float _minZ, float _maxZ)
{
  m_minX = _minX;
  m_maxX = _maxX;
  m_minZ = _minZ;
  m_maxZ = _maxZ;
}

void Camera::PlayAnimation(CameraAnimation* _anim)
{
  // The editor's preview button can hand over a null animation: the id it
  // holds is not renumbered when another animation is deleted.
  if (!_anim || _anim->m_nodes.empty())
    return;

  m_anim = _anim;
  m_animCurrentNode = 0;
  m_animNodeStartTime = g_gameTime;
  m_modeBeforeAnim = m_mode;
  m_posBeforeAnim = m_pos;
  m_upBeforeAnim = m_up;
  m_frontBeforeAnim = m_front;

  CamAnimNode* node = m_anim->m_nodes[0].get();
  SetTarget(node->m_mountName);

  switch (node->m_transitionMode)
  {
  case CamAnimNode::TransitionMove:
    RequestMode(ModeMoveToTarget);
    SetMoveDuration(node->m_duration);
    break;
  case CamAnimNode::TransitionCut:
    RequestMode(ModeDoNothing);
    break;
  }
}

void Camera::StopAnimation()
{
  if (m_anim)
  {
    RequestMode(m_modeBeforeAnim);
    m_anim = nullptr;
  }
}

bool Camera::IsAnimPlaying()
{
  if (m_anim)
    return true;

  if (m_mode == ModeMoveToTarget)
    return true;

  return false;
}

void Camera::RecordCameraPosition()
{
  m_posBeforeAnim = m_pos;
  m_upBeforeAnim = m_up;
  m_frontBeforeAnim = m_front;
}

void Camera::RestoreCameraPosition(bool _cut)
{
  SetTarget(m_posBeforeAnim, m_frontBeforeAnim, m_upBeforeAnim);

  if (_cut)
    CutToTarget();
  else
  {
    RequestMode(ModeMoveToTarget);
    SetMoveDuration(3);
  }
}

void Camera::SwitchEntityTracking(bool _onOrOff) { m_entityTrack = _onOrOff; }

DirectX::XMFLOAT3 Camera::GetControlVector() { return m_controlVector; }

void Camera::UpdateEntityTrackingMode()
{
  if (g_prefsManager && g_inputManager)
  {
    int camTracking = g_prefsManager->GetInt(OTHER_AUTOMATICCAM, 0);
    if (camTracking != 1 && camTracking != 2)
    {
      // do automatic option detection
      if (g_inputManager->getInputMode() == InputMode::INPUT_MODE_GAMEPAD)
        camTracking = 2;
    }
    SwitchEntityTracking(camTracking == 2);
  }
}

void Camera::WaterReflect()
{
  m_pos.y *= -1;
  m_front.y *= -1;
  m_up.y *= -1;
}
