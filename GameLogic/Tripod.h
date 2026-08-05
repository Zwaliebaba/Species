#pragma once

#include "NeuronMath.h"


#include "Entity.h"



//*****************************************************************************
// Class TripodNavData
//*****************************************************************************


namespace Species
{
  class Unit;
  class EntityLeg;
  class Tripod;

  class TripodNavData
  {
    public:
      DirectX::XMFLOAT2 m_directions[6];
      int m_dir; // Index into m_directions
      DirectX::XMFLOAT2 m_targetPos{0.0f, 0.0f};

      TripodNavData();
  };


  //*****************************************************************************
  // Class Tripod
  //*****************************************************************************

  class Tripod : public Entity
  {
    public:
      enum
      {
        ModeWalking,
        ModePreAttack,
        ModeAttacking,
        ModePostAttack
      };

      int m_mode;

    protected:
      EntityLeg* m_legs[3];
      unsigned int m_nextLegToMove; // Not certain to be true - just used to influence the DesireToMove score
      float m_speed;
      float m_targetHoverHeight;
      // Braced to zero: Vector3's default constructor did it, XMFLOAT3's does
      // not. m_up and m_bodyVel are assigned in the constructor list.
      DirectX::XMFLOAT3 m_up{0.0f, 0.0f, 0.0f};
      TripodNavData m_navData;
      DirectX::XMFLOAT3 m_bodyVel{0.0f, 0.0f, 0.0f};
      DirectX::XMFLOAT3 m_attackTarget{0.0f, 0.0f, 0.0f};
      float m_modeStartTime;

      void ChangeHealth(int _amount);

      int CalcWhichFootToMove();
      DirectX::XMFLOAT3 CalcAttackUpVector();
      void DoFallForTwoLegs();
      DirectX::XMFLOAT2 ChooseDestination();
      void DoNavigation();
      WorldObjectId FindEntityToAttack();

      void AdvanceWalk();
      void AdvancePreAttack();
      void AdvanceAttack();
      void AdvancePostAttack();

    public:
      Tripod();
      ~Tripod();

      bool Advance(Unit* _unit);
      void Render(float _predictionTime);
      void Begin();
  };
} // namespace Species
