#pragma once

#include "NeuronMath.h"

#include "CameraAccess.h"
#include "Entity.h"
#include "WorldObject.h"
#include "WorldPointers.h"




// Mode and the debug-mode enumerators are inherited from CameraAccess rather
// than declared here, so that code below Species can name a mode without this
// header. `Camera::Mode::ModeFreeMovement` still resolves through the base, which is
// why the Species-side spellings did not have to change.


namespace Species
{
  class Building;
  class Teleport;
  class CameraAnimation;

  class Camera : public CameraAccess
  {
    protected:
      void AdvanceAnim();

      void AdvanceComponentZoom();
      void AdvanceComponentMouseWheelHeight();
      void AdvanceDebugMode();
      void AdvanceSphereWorldMode();
      void AdvanceFreeMovementMode();
      void AdvanceBuildingFocusMode();
      void AdvanceEntityTrackMode();
      void AdvanceRadarAimMode();
      void AdvanceFirstPersonMode();
      void AdvanceMoveToTargetMode();
      void AdvanceEntityFollowMode();
      void AdvanceTurretAimMode();
      void AdvanceSphereWorldScriptedMode();
      void AdvanceSphereWorldIntroMode();
      void AdvanceSphereWorldOutroMode();
      void AdvanceSphereWorldFocusMode();
      void AdvanceMainMenuMode();

      float XM_CALLCONV DistanceToBlockage(DirectX::FXMVECTOR _dir, float _maxDist);
      float XM_CALLCONV DirectDistanceToBlockage(DirectX::FXMVECTOR _from, DirectX::FXMVECTOR _to, float const _maxDist);
      void XM_CALLCONV GetHighestPoint(DirectX::FXMVECTOR _from, DirectX::FXMVECTOR _to, float _maxDist, DirectX::XMFLOAT3& location);
      void XM_CALLCONV GetHighestTangentPoint(DirectX::FXMVECTOR _from, DirectX::FXMVECTOR _to, float _maxDist, DirectX::XMFLOAT3& location);

      bool GetEntityToTrack(WorldObjectId& selection);
      void AdvanceAutomaticTracking();
      void RotateTowardsEntity(Entity* entity);

      bool AdvanceNotTooLow(DirectX::XMFLOAT3& targetCamera);
      bool AdvanceCanSeeUnits(DirectX::XMFLOAT3& targetCamera);
      bool AdvanceNotTooFarAway(DirectX::XMFLOAT3& targetCamera);
      bool AdvanceRopeModel(DirectX::XMFLOAT3& cameraTarget);
      bool AdvanceManualRotateCamera(DirectX::XMFLOAT3& cameraTarget);
      bool AdvanceManualCameraHeight(DirectX::XMFLOAT3& cameraTarget);

    private:
      // Braced to zero throughout: Vector3's default constructor did it and
      // XMFLOAT3's does not. The constructor writes m_pos, m_front and m_up
      // itself; every other member below was reaching a zeroed
      // default and several are read before their first write — m_targetPos on
      // the frame RequestMode(Mode::ModeFreeMovement) has not run yet, m_cameraTarget
      // in AdvanceAutomaticTracking, and all three m_*BeforeAnim if a script
      // cuts to a mount before anything has recorded a position.
      DirectX::XMFLOAT3 m_pos{0.0f, 0.0f, 0.0f};
      DirectX::XMFLOAT3 m_front{0.0f, 0.0f, 0.0f};
      DirectX::XMFLOAT3 m_up{0.0f, 0.0f, 0.0f};
      float m_fov;
      float m_cosFov;        // Updated once per frame from m_fov in SetupProjectionMatrix
      float m_maxFovRadians; // Updated once per frame from m_fov in SetupProjectionMatrix
      float m_height;        // Distance above ground (in metres)
      DirectX::XMFLOAT3 m_vel{0.0f, 0.0f, 0.0f};

      float m_minX, m_maxX; // Bounds of camera movement
      float m_minZ, m_maxZ;
      DirectX::XMFLOAT3 m_targetPos{0.0f, 0.0f, 0.0f};
      DirectX::XMFLOAT3 m_targetFront{0.0f, 0.0f, 0.0f};
      DirectX::XMFLOAT3 m_targetUp{0.0f, 0.0f, 0.0f};
      float m_targetFov;
      DirectX::XMFLOAT3 m_cameraTarget{0.0f, 0.0f, 0.0f};       // Target Position of camera for automatic entity tracking
      DirectX::XMFLOAT3 m_predictedEntityPos{0.0f, 0.0f, 0.0f}; // Predicted Position of entity for automatic entity tracking
      Entity* m_trackingEntity;                                 // The entity we are tracking

      DirectX::XMFLOAT3 m_startPos{0.0f, 0.0f, 0.0f}; // Camera pos and orientation at the start of a "MoveToTarget"
      DirectX::XMFLOAT3 m_startFront{0.0f, 0.0f, 0.0f};
      DirectX::XMFLOAT3 m_startUp{0.0f, 0.0f, 0.0f};
      float m_startTime;
      float m_moveDuration;

      float m_distFromEntity;  // Used in EntityFollowMode and MicroUnitMode
      float m_currentDistance; // Used in Manual Camera Rotation when tracking entities
      float m_heightMultiplier;

      Mode m_mode;
      int m_debugMode;
      int m_framesInThisMode;

      WorldObjectId m_objectId; // WorldObjectId of creature to track
      float m_trackRange;
      float m_trackHeight;
      float m_trackTimer;
      DirectX::XMFLOAT3 m_trackVector{0.0f, 0.0f, 0.0f}; // Used to rotate around tracking object

      CameraAnimation* m_anim;
      int m_animCurrentNode;
      float m_animNodeStartTime;
      Mode m_modeBeforeAnim;
      DirectX::XMFLOAT3 m_posBeforeAnim{0.0f, 0.0f, 0.0f};
      DirectX::XMFLOAT3 m_upBeforeAnim{0.0f, 0.0f, 0.0f};
      DirectX::XMFLOAT3 m_frontBeforeAnim{0.0f, 0.0f, 0.0f};

      float m_cameraShake;

      bool m_entityTrack; // the current state of automatic entity tracking

    public:
      Camera();

      DirectX::XMFLOAT3 GetPos() { return m_pos; }
      DirectX::XMFLOAT3 GetFront() { return m_front; }
      DirectX::XMFLOAT3 GetUp() { return m_up; }
      DirectX::XMFLOAT3 GetRight()
      {
        // operator^ was the cross product, and the operand order is the whole
        // meaning of this function: up x front, not front x up.
        DirectX::XMFLOAT3 right;
        DirectX::XMStoreFloat3(&right, DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&m_up), DirectX::XMLoadFloat3(&m_front)));
        return right;
      }
      DirectX::XMFLOAT3 GetVel() { return m_vel; }
      float GetFov() { return m_fov; }

      void SetupProjectionMatrix(float _nearPlane, float _farPlane);
      void SetupModelviewMatrix();

      bool PosInViewFrustum(DirectX::XMFLOAT3 const& _pos);
      bool SphereInViewFrustum(DirectX::XMFLOAT3 const& _centre, float _radius);

      Building* GetBestBuildingInView(); // Is the player currently looking at a building?

      void Advance();
      void Render();

      int GetDebugMode();
      void SetDebugMode(int _mode);
      void SetNextDebugMode();

      void RequestMode(Mode _mode);
      void RequestBuildingFocusMode(Building* _building, float _range, float _height);
      void RequestSphereFocusMode();
      void RequestRadarAimMode(Building* _building);
      void RequestEntityTrackMode(WorldObjectId const& _id);
      void RequestEntityFollowMode(WorldObjectId const& _id);
      void RequestTurretAimMode(Building* _building);

      void CreateCameraShake(float _intensity);

      bool IsMoving();
      bool IsInteractive();
      bool IsInMode(Mode _mode);

      void RecordCameraPosition(); // So you can return easily
      void RestoreCameraPosition(bool _cut = false);

      // SetTarget() only sets the target data. To make these changes take effect, call either
      // CutToTarget() or RequestMode(Camera::Mode::ModeMoveToTarget), depending on whether you
      // want an instant cut or a smooth transition
      // The default for _up was g_upVector, a Vector3 constant T25 retires.
      // s_defaultUp is the same (0,1,0) as an XMFLOAT3, kept as a default
      // argument rather than an overload because the three call sites that rely
      // on it read better for it.
      static constexpr DirectX::XMFLOAT3 s_defaultUp{0.0f, 1.0f, 0.0f};
      void SetTarget(DirectX::XMFLOAT3 const& _pos, DirectX::XMFLOAT3 const& _front, DirectX::XMFLOAT3 const& _up = s_defaultUp);
      bool SetTarget(char const* _mountName); // Returns false if mount not found
      void SetTarget(DirectX::XMFLOAT3 const& _focusPos, float _distance, float _height);
      void SetMoveDuration(float _duration);
      void CutToTarget();
      void SetHeight(float _height);
      void SetFOV(float _fov);
      void SetTargetFOV(float _fov);

      void Normalise(); // Needs to be called reasonably regularly to prevent the front
                        // and up vectors becoming non-orthogonal

      void GetClickRay(int _x, int _y, DirectX::XMFLOAT3* _rayStart, DirectX::XMFLOAT3* _rayDir);
      void Get2DScreenPos(DirectX::XMFLOAT3 const& _vector, float* _screenX, float* _screenY);

      void SetBounds(float _minX, float _maxX, float _minZ, float _maxZ);

      void PlayAnimation(CameraAnimation* _anim);
      void StopAnimation();
      bool IsAnimPlaying();

      void SwitchEntityTracking(bool _onOrOff);
      void UpdateEntityTrackingMode();

      void WaterReflect();
  };


  // g_camera is a CameraAccess* so the layers below Species need only the
  // interface. Species owns the concrete type and reaches the camera through
  // here — every call site in Species, not just the ones needing a member the
  // interface omits. The cast is safe because App is the only thing that ever
  // assigns g_camera, and it assigns a Camera.
  //
  // Uniform rather than mixed, unlike TheRenderer() in Renderer.h, and SetTarget
  // is why. Camera has three SetTarget overloads and CameraAccess declares one;
  // a Species call through the interface pointer sees only that one, so
  // SetTarget(pos, front) and SetTarget(mountName) stop compiling while
  // SetTarget(pos, front, up) keeps working. Splitting call sites by which
  // overload they happen to use is a trap. Species goes through here.
  inline Camera* TheCamera() { return static_cast<Camera*>(g_camera); }
} // namespace Species
