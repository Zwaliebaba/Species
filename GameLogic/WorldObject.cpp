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
  Vector3 lastPos = m_pos; // - m_vel * g_advanceTime;
  Vector3 impactPos = (m_pos + lastPos) * 0.5f;
  m_pos = impactPos;
  m_pos.y = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);

  Vector3 normal = g_location->m_landscape.m_normalMap->GetValue(m_pos.x, m_pos.z);
  Vector3 incomingVel = m_vel * -1.0f;
  float dotProd = normal * incomingVel;
  m_vel = 2.0f * dotProd * normal - incomingVel;
  m_vel *= COEF_OF_RESTITUTION;
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

  SetFront(Vector3(0, 0, 1));
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


void Light::SetFront(Vector3 front)
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