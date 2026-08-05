#pragma once

#include <memory>
#include <vector>

#include "Unit.h"
#include "Shape.h"

#define GAP_BETWEEN_MEN 10.0f

//*****************************************************************************
// Class HistoricWayPoint
//
// Stores positions where the user clicked.
//*****************************************************************************


namespace Species
{
  class HistoricWayPoint
  {
    public:
      DirectX::XMFLOAT3 m_pos{0.0f, 0.0f, 0.0f};
      unsigned int m_id;
      static unsigned int s_lastId;

      HistoricWayPoint(DirectX::XMFLOAT3 const& _pos)
        : m_pos(_pos)
      {
        s_lastId++;
        m_id = s_lastId;
      }
  };

  //*****************************************************************************
  // Class InsertionSquad
  //*****************************************************************************

  class InsertionSquad : public Unit
  {
    protected:
      std::vector<std::unique_ptr<HistoricWayPoint>> m_positionHistory; // A list of all the places the user has clicked. Most recent first

    public:
      int m_weaponType;   // Indexes into GlobalResearch
      int m_controllerId; // Task ID of controller if this squad is running one
      int m_teleportId;   // Id of teleport build we wish to enter, or -1

      InsertionSquad(int teamId, int _unitId, int numEntities, DirectX::XMFLOAT3 const& _pos);
      ~InsertionSquad() override;

      void SetWayPoint(DirectX::XMFLOAT3 const& _pos) override;
      DirectX::XMFLOAT3 GetTargetPos(float _distFromPointMan);
      Entity* GetPointMan();
      void SetWeaponType(int _weaponType); // Indexes into GlobalResearch
      void Attack(DirectX::XMFLOAT3 pos, bool withGrenade) override;

      void DirectControl(const TeamControls& _teamControls) override;
      // used when the squad is being directly controlled by the player using a control pad
  };

  //*****************************************************************************
  // Class Squadie
  //*****************************************************************************

  class Squadie : public Entity
  {
    public:
      bool m_justFired;
      float m_secondaryTimer;

    protected:
      ShapeMarker* m_laser;
      ShapeMarker* m_brass;
      ShapeMarker* m_eye1;
      ShapeMarker* m_eye2;

      WorldObjectId m_enemyId;
      float m_retargetTimer;
      void RunAI(); // Call this if the player isnt' controlling us

    public:
      Squadie();

      void Begin() override;
      bool Advance(Unit* _unit) override;
      void ChangeHealth(int _amount) override;
      void Attack(DirectX::XMFLOAT3 const& _pos) override;

      void Render(float _predictionTime) override;
      bool RenderPixelEffect(float _predictionTime) override;

      bool HasSecondaryWeapon();
      void FireSecondaryWeapon(DirectX::XMFLOAT3 const& _pos);

      void ListSoundEvents(std::vector<const char*>* _list) override;

      DirectX::XMFLOAT3 GetCameraFocusPoint() override;

      DirectX::XMFLOAT3 GetSecondaryWeaponTarget();
  };
} // namespace Species
