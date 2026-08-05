#pragma once


#include "NeuronMath.h"

class Entity;
namespace Neuron
{
  class ShapeMarker;
  class Shape;
} // namespace Neuron


//*****************************************************************************
// Class EntityFoot
//*****************************************************************************

class EntityFoot
{
  public:
    // Scoped. Held in m_state and never serialised.
    enum class FootState
    {
      OnGround,
      Swinging,
      Pouncing
    };

    // Braced to zero throughout: EntityFoot has no constructor, so Vector3's
    // default one was doing the work and XMFLOAT3's does not.
    DirectX::XMFLOAT3 m_pos{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 m_targetPos{0.0f, 0.0f, 0.0f};
    FootState m_state;
    float m_leftGroundTimeStamp;
    DirectX::XMFLOAT3 m_lastGroundPos{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 m_bodyToFoot{0.0f, 0.0f, 0.0f}; // Used whilst pouncing
};


//*****************************************************************************
// Class EntityLeg
//*****************************************************************************

class EntityLeg
{
  public:
    float m_legLift;       // How high to lift the foot (in metres)
    float m_idealLegSlope; // 0.0 means vertical 1.0 means 45 degrees.
    float m_legSwingDuration;
    float m_lookAheadCoef; // Amount of velocity to add on to foot home pos to calc foot target pos

    int m_legNum;
    EntityFoot m_foot;
    ShapeMarker* m_rootMarker;
    Shape* m_shapeUpper;
    Shape* m_shapeLower;
    float m_thighLen;
    float m_shinLen;
    Entity* m_parent;

  protected:
    DirectX::XMFLOAT3 GetLegRootPos();
    DirectX::XMFLOAT3 CalcFootHomePos(float _targetHoverHeight);
    DirectX::XMFLOAT3 CalcDesiredFootPos(float _targetHoverHeight);
    DirectX::XMFLOAT3 CalcKneePos(DirectX::XMFLOAT3 const& _footPos, DirectX::XMFLOAT3 const& _rootPos, DirectX::XMFLOAT3 const& _centrePos);
    DirectX::XMFLOAT3 GetIdealSwingingFootPos(float _fractionComplete);

  public:
    EntityLeg(int _legNum, Entity* _parent, char const* _shapeNameUpper, char const* _shapeNameLower, char const* _rootMarkerName);

    float CalcFootsDesireToMove(float _targetHoverHeight);
    void LiftFoot(float _targetHoverHeight);
    void PlantFoot();

    bool Advance(); // Returns true if the foot was planted this frame
    void AdvanceSpiderPounce(float _fractionComplete);
    void Render(float _predictionTime, DirectX::XMFLOAT3 const& _predictedMovement);
    bool RenderPixelEffect(float _predictionTime, DirectX::XMFLOAT3 const& _predictedMovement);
};
