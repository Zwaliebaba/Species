#include "pch.h"

#include <math.h>
#include <stdio.h>

#include "Debug.h"
#include "MathUtils.h"

#include "Location.h"
#include "WorldObject.h"
#include "GameTime.h"
#include "WorldPointers.h"


#define COEF_OF_RESTITUTION 0.85f


// ****************************************************************************
//  Class WorldObject
// ****************************************************************************

// *** Constructor
WorldObject::WorldObject()
  : m_onGround(false),
    m_enabled(true),
    m_type(0)
{
}


WorldObject::~WorldObject() {}


// *** BounceOffLandscape
void WorldObject::BounceOffLandscape()
{
  // Assume that we were above the landscape last frame and that we are not
  // now. We know that we must have impacted the landscape somewhere we
  // are now and where we were last frame. Let's use the midpoint of our last
  // and our current position as the point of impact (it will be correct on
  // average)
  DirectX::XMVECTOR const pos = DirectX::XMLoadFloat3(&m_pos);
  DirectX::XMVECTOR const lastPos = pos; // - m_vel * g_advanceTime;
  DirectX::XMVECTOR const impactPos = DirectX::XMVectorScale(DirectX::XMVectorAdd(pos, lastPos), 0.5f);
  DirectX::XMStoreFloat3(&m_pos, impactPos);
  m_pos.y = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);

  // vector. Copy-initialising the native type takes the seam's conversion
  // without naming the legacy one; a reference would bind to the temporary the
  // conversion operator returns and dangle.
  DirectX::XMFLOAT3 const normalValue = g_location->m_landscape.m_normalMap->GetValue(m_pos.x, m_pos.z);

  DirectX::XMVECTOR const normal = DirectX::XMLoadFloat3(&normalValue);
  DirectX::XMVECTOR const incomingVel = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), -1.0f);
  float const dotProd = DirectX::XMVectorGetX(DirectX::XMVector3Dot(normal, incomingVel));
  DirectX::XMVECTOR const bounced = DirectX::XMVectorSubtract(DirectX::XMVectorScale(normal, 2.0f * dotProd), incomingVel);
  DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(bounced, COEF_OF_RESTITUTION));
}


bool WorldObject::Advance() { return false; }


void WorldObject::Render(float _time) {}


bool WorldObject::RenderPixelEffect(float predictionTime) { return false; }


// ****************************************************************************
//  Class Light
// ****************************************************************************

// *** Constructor
Light::Light()
{
  m_colour[0] = 1.3f;
  m_colour[1] = 1.3f;
  m_colour[2] = 1.3f;
  m_colour[3] = 0.0f;

  SetFront(DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f));
}


void Light::SetColour(float colour[4])
{
  m_colour[0] = colour[0];
  m_colour[1] = colour[1];
  m_colour[2] = colour[2];
  m_colour[3] = colour[3];
}


void Light::SetFront(float front[4])
{
  m_front[0] = front[0];
  m_front[1] = front[1];
  m_front[2] = front[2];
  m_front[3] = front[3];
}


void Light::SetFront(DirectX::XMFLOAT3 front)
{
  m_front[0] = front.x;
  m_front[1] = front.y;
  m_front[2] = front.z;
  m_front[3] = 0.0f;
}


void Light::Normalise()
{
  float mag = sqrtf(m_front[0] * m_front[0] + m_front[1] * m_front[1] + m_front[2] * m_front[2]);
  m_front[0] /= mag;
  m_front[1] /= mag;
  m_front[2] /= mag;
}