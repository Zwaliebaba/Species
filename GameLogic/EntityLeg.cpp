#include "pch.h"

#include "Debug.h"
#include "PersistingDebugRender.h"
#include "MathUtils.h"
#include "Resource.h"
#include "Shape.h"
#include "DebugRender.h"

#include "Entity.h"
#include "EntityLeg.h"

#include "Location.h"
#include "GameTime.h"
#include "WorldPointers.h"

EntityLeg::EntityLeg(int _legNum, Entity* _parent, const char* _shapeNameUpper, const char* _shapeNameLower, const char* _rootMarkerName)
  : m_legNum(_legNum),
    m_parent(_parent)
{
  m_shapeUpper = g_resource->GetShape(_shapeNameUpper);
  m_shapeLower = g_resource->GetShape(_shapeNameLower);
  ASSERT_TEXT(m_shapeUpper, "EntityLeg: Couldn't load leg shape {}", _shapeNameUpper);
  ASSERT_TEXT(m_shapeLower, "EntityLeg: Couldn't load leg shape {}", _shapeNameLower);

  // ShapeMarker::GetWorldMatrix still returns a legacy matrix -- T10 left that
  // signature deliberately, because 43 sites in fourteen GameLogic files read
  // .pos or .f off it. Reading .pos here is that seam, not an oversight.
  DirectX::XMFLOAT4X4 const identity(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);

  ShapeMarker* endMarker = m_shapeUpper->m_rootFragment->LookupMarker("MarkerEnd");
  m_thighLen = endMarker->GetWorldMatrix(identity).pos.Mag();

  endMarker = m_shapeLower->m_rootFragment->LookupMarker("MarkerEnd");
  m_shinLen = endMarker->GetWorldMatrix(identity).pos.Mag();

  m_rootMarker = m_parent->m_shape->m_rootFragment->LookupMarker(_rootMarkerName);
  ASSERT_TEXT(m_rootMarker, "EntityLeg: Couldn't find root marker {}", _rootMarkerName);

  m_foot.m_state = EntityFoot::FootState::OnGround;
}

DirectX::XMFLOAT3 EntityLeg::GetLegRootPos()
{
  DirectX::XMFLOAT4X4 rootMat;
  DirectX::XMStoreFloat4x4(
    &rootMat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_parent->m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_parent->m_pos)));

  // Legacy return type, per the GetWorldMatrix seam noted in the constructor.
  return m_rootMarker->GetWorldMatrix(rootMat).pos;
}

DirectX::XMFLOAT3 EntityLeg::CalcFootHomePos(float _targetHoverHeight)
{
  DirectX::XMFLOAT3 const rootWorldPos = GetLegRootPos();
  DirectX::XMVECTOR const root = DirectX::XMLoadFloat3(&rootWorldPos);

  // HorizontalAndNormalise: flatten to the xz plane, then normalise. The legacy
  // one divided by an unguarded sqrtf(x*x + z*z), so a leg root exactly above
  // the body centre already produced infinities; the native normalise answers
  // zero instead. Degenerate either way -- see the normalise decision in
  // NeuronMath.h, and T1's audit, which names this function.
  DirectX::XMVECTOR fromCentreToRoot =
    DirectX::XMVector3Normalize(DirectX::XMVectorSetY(DirectX::XMVectorSubtract(root, DirectX::XMLoadFloat3(&m_parent->m_pos)), 0.0f));

  // The landscape converts in T18, so its normal map still yields a legacy vector.
  DirectX::XMFLOAT3 const groundNormal = g_location->m_landscape.m_normalMap->GetValue(m_parent->m_pos.x, m_parent->m_pos.z);
  fromCentreToRoot = DirectX::XMVectorScale(fromCentreToRoot, groundNormal.y);

  DirectX::XMFLOAT3 returnVal;
  DirectX::XMStoreFloat3(&returnVal,
                         DirectX::XMVectorMultiplyAdd(fromCentreToRoot, DirectX::XMVectorReplicate(m_idealLegSlope * _targetHoverHeight), root));
  returnVal.y -= _targetHoverHeight;

  return returnVal;
}

float EntityLeg::CalcFootsDesireToMove(float _targetHoverHeight)
{
  DirectX::XMFLOAT3 const homePos = CalcFootHomePos(_targetHoverHeight);
  DirectX::XMVECTOR const delta = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_foot.m_pos), DirectX::XMLoadFloat3(&homePos));

  // deltaHoriNorm was computed and never read -- the only use is inside the
  // commented-out block below. Left out rather than computed and discarded.
  float scoreDueToDirection = 1.0f;
  //	if (m_parent->m_vel.Mag() > 0.1f)
  //	{
  //		Vector3 velHoriNorm = m_parent->m_vel;
  //		velHoriNorm.HorizontalAndNormalise();
  //		scoreDueToDirection = -(deltaHoriNorm * velHoriNorm);
  //	}
  float scoreDueToDist = DirectX::XMVectorGetX(DirectX::XMVector3Length(delta));
  float score = scoreDueToDirection * scoreDueToDist;

  return score;
}

DirectX::XMFLOAT3 EntityLeg::CalcDesiredFootPos(float _targetHoverHeight)
{
  DirectX::XMFLOAT3 rv = CalcFootHomePos(_targetHoverHeight);

  float expectedRotation = 1.2f * m_parent->m_angVel.y;

  // Vector3::RotateAroundY(a) built its own sin/cos rotation about +y. The
  // native equivalent is a rotation matrix about the same axis; row-vector
  // order, so the vector goes on the left.
  DirectX::XMVECTOR const averageExpectedVel =
    DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&m_parent->m_vel), DirectX::XMMatrixRotationY(expectedRotation * 0.5f));

  DirectX::XMStoreFloat3(&rv,
                         DirectX::XMVectorMultiplyAdd(averageExpectedVel, DirectX::XMVectorReplicate(m_lookAheadCoef), DirectX::XMLoadFloat3(&rv)));

  return rv;
}

DirectX::XMFLOAT3 EntityLeg::CalcKneePos(DirectX::XMFLOAT3 const& _footPos, DirectX::XMFLOAT3 const& _rootPos, DirectX::XMFLOAT3 const& _centrePos)
{
  DirectX::XMVECTOR const footPos = DirectX::XMLoadFloat3(&_footPos);
  DirectX::XMVECTOR const rootToFoot = DirectX::XMVectorSubtract(footPos, DirectX::XMLoadFloat3(&_rootPos));
  float rootToFootLen = DirectX::XMVectorGetX(DirectX::XMVector3Length(rootToFoot));

  // rootToFootHoriNorm was computed and never used. Dropped rather than carried.
  DirectX::XMVECTOR const centreToRoot = DirectX::XMVector3Normalize(
    DirectX::XMVectorSetY(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&_rootPos), DirectX::XMLoadFloat3(&_centrePos)), 0.0f));
  DirectX::XMVECTOR const axis = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(centreToRoot, DirectX::g_XMIdentityR1));

  float cosTheta = (rootToFootLen * 0.4f) / m_thighLen;
  // FIXME
  // cosTheta should never be greater than one, yet sometimes it is
  if (cosTheta > 1.0f)
    cosTheta = 1.0f;
  float theta = -acosf(cosTheta);

  // SetLength(m_shinLen) on the reversed leg vector, then RotateAround(axis *
  // theta) -- whose ANGLE IS THE VECTOR'S MAGNITUDE, and which returned early
  // without rotating when that magnitude squared was below 1e-8. Both halves
  // are reproduced: the guard because a degenerate axis would otherwise give a
  // NaN knee, and the negative theta because it sets the direction of bend.
  DirectX::XMVECTOR footToKnee = DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMVectorNegate(rootToFoot)), m_shinLen);

  DirectX::XMVECTOR const rotation = DirectX::XMVectorScale(axis, theta);
  float const rotationLengthSquared = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(rotation));
  if (rotationLengthSquared >= 1e-8f)
  {
    float const angle = sqrtf(rotationLengthSquared);
    footToKnee = DirectX::XMVector3Transform(footToKnee, DirectX::XMMatrixRotationAxis(DirectX::XMVectorScale(rotation, 1.0f / angle), angle));
  }

  DirectX::XMFLOAT3 kneePos;
  DirectX::XMStoreFloat3(&kneePos, DirectX::XMVectorAdd(footPos, footToKnee));
  return kneePos;
}

void EntityLeg::LiftFoot(float _targetHoverHeight)
{
  m_foot.m_targetPos = CalcDesiredFootPos(_targetHoverHeight);
  m_foot.m_targetPos.y = g_location->m_landscape.m_heightMap->GetValue(m_foot.m_targetPos.x, m_foot.m_targetPos.z);
  m_foot.m_state = EntityFoot::FootState::Swinging;
  m_foot.m_leftGroundTimeStamp = g_gameTime;
  m_foot.m_lastGroundPos = m_foot.m_pos;
}

void EntityLeg::PlantFoot()
{
  m_foot.m_pos = m_foot.m_targetPos;
  m_foot.m_state = EntityFoot::FootState::OnGround;
}

DirectX::XMFLOAT3 EntityLeg::GetIdealSwingingFootPos(float _fractionComplete)
{
  float fractionIncomplete = 1.0f - _fractionComplete;

  DirectX::XMFLOAT3 pos;
  DirectX::XMStoreFloat3(&pos,
                         DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_foot.m_lastGroundPos), DirectX::XMVectorReplicate(fractionIncomplete),
                                                      DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_foot.m_targetPos), _fractionComplete)));

  if (_fractionComplete < 0.33f)
    pos.y += _fractionComplete * 3.0f * m_legLift;
  else if (fractionIncomplete < 0.33f)
    pos.y += fractionIncomplete * 3.0f * m_legLift;
  else
    pos.y += m_legLift;

  return pos;
}

// Returns true if the foot was planted this frame
bool EntityLeg::Advance()
{
  if (m_foot.m_state == EntityFoot::FootState::Swinging)
  {
    float fractionComplete = RampUpAndDown(m_foot.m_leftGroundTimeStamp, m_legSwingDuration, g_gameTime);
    if (fractionComplete > 1.0f)
    {
      PlantFoot();
      return true;
    }
    m_foot.m_pos = GetIdealSwingingFootPos(fractionComplete);
  }

  return false;
}

void EntityLeg::AdvanceSpiderPounce(float _fractionComplete)
{
  DirectX::XMVECTOR offset = DirectX::XMLoadFloat3(&m_foot.m_bodyToFoot);
  if (_fractionComplete < 0.5f)
    offset = DirectX::XMVectorScale(offset, 1.0f - _fractionComplete);
  else
    offset = DirectX::XMVectorScale(offset, _fractionComplete);

  DirectX::XMVECTOR const drift = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_parent->m_vel), _fractionComplete * 0.05f);
  DirectX::XMStoreFloat3(&m_foot.m_pos, DirectX::XMVectorAdd(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_parent->m_pos), offset), drift));
  m_foot.m_lastGroundPos = m_foot.m_pos;
  m_foot.m_targetPos = m_foot.m_pos;
}

void EntityLeg::Render(float _predictionTime, DirectX::XMFLOAT3 const& _predictedMovement)
{
  DirectX::XMVECTOR const movement = DirectX::XMLoadFloat3(&_predictedMovement);

  DirectX::XMFLOAT3 predictedPos;
  DirectX::XMStoreFloat3(&predictedPos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_parent->m_pos), movement));

  DirectX::XMFLOAT3 const legRootPos = GetLegRootPos();
  DirectX::XMFLOAT3 rootPos;
  DirectX::XMStoreFloat3(&rootPos, DirectX::XMVectorAdd(movement, DirectX::XMLoadFloat3(&legRootPos)));

  DirectX::XMFLOAT3 footPos{0.0f, 0.0f, 0.0f};

  switch (m_foot.m_state)
  {
  case EntityFoot::FootState::OnGround:
    footPos = m_foot.m_pos;
    break;

  case EntityFoot::FootState::Swinging:
  {
    float fractionComplete = RampUpAndDown(m_foot.m_leftGroundTimeStamp, m_legSwingDuration, g_gameTime);
    footPos = GetIdealSwingingFootPos(fractionComplete);
    break;
  }

  case EntityFoot::FootState::Pouncing:
    DirectX::XMStoreFloat3(&footPos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_foot.m_pos), movement));
    break;
  }

  DirectX::XMFLOAT3 const kneePos = CalcKneePos(footPos, rootPos, predictedPos);

  DirectX::XMVECTOR const foot = DirectX::XMLoadFloat3(&footPos);
  DirectX::XMVECTOR const knee = DirectX::XMLoadFloat3(&kneePos);
  DirectX::XMVECTOR const root = DirectX::XMLoadFloat3(&rootPos);

  {
    DirectX::XMVECTOR const up = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(knee, foot));
    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMVector3Cross(up, DirectX::g_XMIdentityR1), up, foot));
    m_shapeLower->Render(_predictionTime, mat);

    // RenderArrow( kneePos, footPos, 1.0f );
  }

  {
    DirectX::XMVECTOR const up = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(root, knee));
    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMVector3Cross(up, DirectX::g_XMIdentityR1), up, knee));
    m_shapeUpper->Render(_predictionTime, mat);

    // RenderArrow( rootPos, kneePos, 1.0f );
  }
}

bool EntityLeg::RenderPixelEffect(float _predictionTime, DirectX::XMFLOAT3 const& _predictedMovement)
{
  DirectX::XMVECTOR const movement = DirectX::XMLoadFloat3(&_predictedMovement);

  DirectX::XMFLOAT3 predictedPos;
  DirectX::XMStoreFloat3(&predictedPos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_parent->m_pos), movement));

  DirectX::XMFLOAT3 const legRootPos = GetLegRootPos();
  DirectX::XMFLOAT3 rootPos;
  DirectX::XMStoreFloat3(&rootPos, DirectX::XMVectorAdd(movement, DirectX::XMLoadFloat3(&legRootPos)));

  DirectX::XMFLOAT3 footPos{0.0f, 0.0f, 0.0f};

  switch (m_foot.m_state)
  {
  case EntityFoot::FootState::OnGround:
    footPos = m_foot.m_pos;
    break;

  case EntityFoot::FootState::Swinging:
  {
    float fractionComplete = RampUpAndDown(m_foot.m_leftGroundTimeStamp, m_legSwingDuration, g_gameTime);
    footPos = GetIdealSwingingFootPos(fractionComplete);
    break;
  }

  case EntityFoot::FootState::Pouncing:
    DirectX::XMStoreFloat3(&footPos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_foot.m_pos), movement));
    break;
  }

  DirectX::XMFLOAT3 const kneePos = CalcKneePos(footPos, rootPos, predictedPos);

  DirectX::XMVECTOR const foot = DirectX::XMLoadFloat3(&footPos);
  DirectX::XMVECTOR const knee = DirectX::XMLoadFloat3(&kneePos);
  DirectX::XMVECTOR const root = DirectX::XMLoadFloat3(&rootPos);

  {
    DirectX::XMVECTOR const up = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(knee, foot));
    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMVector3Cross(up, DirectX::g_XMIdentityR0), up, foot));
    g_renderer->MarkUsedCells(m_shapeLower, mat);
  }

  {
    DirectX::XMVECTOR const up = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(root, knee));
    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMVector3Cross(up, DirectX::g_XMIdentityR0), up, knee));
    g_renderer->MarkUsedCells(m_shapeUpper, mat);
  }

  return true;
}
