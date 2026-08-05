#pragma once

#include "NeuronMath.h"
#include "WorldObjectId.h"

namespace Species
{
  class CameraAnimation;
} // namespace Species

// What the layers below Species ask the camera for.
//
// Camera itself lives in Species, so entity render code, the debug windows and
// the level editor windows reaching it had to include Camera.h upward — 47
// allowlist entries, the largest remaining cluster after Renderer's. This is
// the surface those files actually use, measured rather than guessed:
// `grep -rhoE "g_camera->[A-Za-z_]+" GameLogic NeuronClient` finds exactly
// these twenty members and nothing else.
//
// The seam is the pointer's TYPE rather than a second global, exactly as
// RendererAccess.h does it: g_camera in WorldPointers.h is a CameraAccess*, so
// dereferencing it needs only this header. Code that constructs a Camera, or
// reaches a member not listed here, still includes Camera.h and is still
// recorded in the allowlist.
//
// Moving Camera down instead was considered and rejected in T1: Camera.h
// includes GameLogic's Entity.h and WorldObject.h for its tracking members, so
// the move would invert the violation rather than remove it. See
// tasks/layering-inversion.yaml T9.


namespace Neuron
{
  class CameraAccess
  {
    public:
      virtual ~CameraAccess() = default;

      // Declared here rather than in Camera so that a caller naming a mode does
      // not need Camera.h for the enumerator. Camera derives from this class, so
      // `Camera::Mode::ModeFreeMovement` still resolves for the Species-side code that
      // spells it that way.
      //
      // Scoped by language-hygiene T12. Camera stored m_mode as an int, compared
      // it against ints and indexed a string table with it; all three are typed
      // now, and the one remaining integer boundary is GetModeName's index,
      // which spells the conversion Neuron::I(). The values are pinned because
      // they were explicit before and a level file's camera-animation node
      // writes a transition mode as a number.
      enum class Mode : int
      {
        ModeReplay = 0,
        ModeSphereWorld = 1,
        ModeFreeMovement = 2,  // Remember to update the static string table
        ModeBuildingFocus = 3, // at the end of camera.cpp when you update this
        ModeEntityTrack = 4,
        ModeRadarAim = 5,
        ModeFirstPerson = 6,
        ModeMoveToTarget = 7,
        ModeDoNothing = 8,
        ModeEntityFollow = 9,
        ModeTurretAim = 10,
        ModeSphereWorldScripted = 11,
        ModeSphereWorldIntro = 12,
        ModeSphereWorldOutro = 13,
        ModeSphereWorldFocus = 14,
        ModeMainMenu = 15,
        ModeNumModes
      };

      // The enumerators are spelled Mode::ModeFreeMovement now. Code below
      // Species that named one as CameraAccess::Mode::ModeFreeMovement says
      // CameraAccess::Mode::ModeFreeMovement.

      enum
      {
        DebugModeNever,
        DebugModeAlways,
        DebugModeAuto,
        DebugModeNumStates
      };

      // Position and orientation. Not const, because Camera's are not, and this
      // change is a seam rather than a cleanup.
      virtual DirectX::XMFLOAT3 GetPos() = 0;
      virtual DirectX::XMFLOAT3 GetFront() = 0;
      virtual DirectX::XMFLOAT3 GetUp() = 0;
      virtual DirectX::XMFLOAT3 GetRight() = 0;
      virtual DirectX::XMFLOAT3 GetVel() = 0;
      virtual DirectX::XMFLOAT3 GetControlVector() = 0;

      // Culling, called per object per frame from entity render bodies.
      virtual bool PosInViewFrustum(DirectX::XMFLOAT3 const& _pos) = 0;
      virtual bool SphereInViewFrustum(DirectX::XMFLOAT3 const& _centre, float _radius) = 0;

      virtual void SetupProjectionMatrix(float _nearPlane, float _farPlane) = 0;

      // World point to screen point. Used by entity render code to place labels
      // and health bars, so it moves with the cluster in
      // tasks/layering-inversion.yaml T15.
      virtual void Get2DScreenPos(DirectX::XMFLOAT3 const& _vector, float* _screenX, float* _screenY) = 0;

      virtual void CreateCameraShake(float _intensity) = 0;

      virtual bool IsInteractive() = 0;
      virtual bool IsInMode(Mode _mode) = 0;
      virtual void RequestMode(Mode _mode) = 0;

      // Follow a particular object. Asked for by the task manager, which moves
      // down in tasks/layering-inversion.yaml T15.
      virtual void RequestEntityTrackMode(WorldObjectId const& _id) = 0;

      virtual int GetDebugMode() = 0;
      virtual void SetNextDebugMode() = 0;

      virtual void PlayAnimation(Species::CameraAnimation* _anim) = 0;
      virtual void StopAnimation() = 0;

      // Camera declares the three-argument form with a default for _up. It is not
      // repeated here: a call through this interface passes all three, which is
      // what the one caller below Species does.
      virtual void SetTarget(DirectX::XMFLOAT3 const& _pos, DirectX::XMFLOAT3 const& _front, DirectX::XMFLOAT3 const& _up) = 0;
      virtual void CutToTarget() = 0;

      // Screen-space picking, used by the level editor windows.
      virtual void GetClickRay(int _x, int _y, DirectX::XMFLOAT3* _rayStart, DirectX::XMFLOAT3* _rayDir) = 0;
  };
} // namespace Neuron
