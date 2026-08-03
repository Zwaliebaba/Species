#pragma once

#include <vector>

#include "Unit.h"
#include "Shape.h"

#define GAP_BETWEEN_MEN 10.0f

//*****************************************************************************
// Class HistoricWayPoint
//
// Stores positions where the user clicked.
//*****************************************************************************

class HistoricWayPoint
{
  public:
    Vector3 m_pos;
    unsigned int m_id;
    static unsigned int s_lastId;

    HistoricWayPoint(const Vector3& _pos)
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
    std::vector<HistoricWayPoint*> m_positionHistory; // A list of all the places the user has clicked. Most recent first

  public:
    int m_weaponType;   // Indexes into GlobalResearch
    int m_controllerId; // Task ID of controller if this squad is running one
    int m_teleportId;   // Id of teleport build we wish to enter, or -1

    InsertionSquad(int teamId, int _unitId, int numEntities, const Vector3& _pos);
    ~InsertionSquad() override;

    void SetWayPoint(const Vector3& _pos) override;
    Vector3 GetTargetPos(float _distFromPointMan);
    Entity* GetPointMan();
    void SetWeaponType(int _weaponType); // Indexes into GlobalResearch
    void Attack(Vector3 pos, bool withGrenade) override;

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
    void Attack(const Vector3& _pos) override;

    void Render(float _predictionTime) override;
    bool RenderPixelEffect(float _predictionTime) override;

    bool HasSecondaryWeapon();
    void FireSecondaryWeapon(const Vector3& _pos);

    void ListSoundEvents(std::vector<const char*>* _list) override;

    Vector3 GetCameraFocusPoint() override;

    Vector3 GetSecondaryWeaponTarget();
};
