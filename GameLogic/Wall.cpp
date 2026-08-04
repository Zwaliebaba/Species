#include "pch.h"

#include "DebugRender.h"
#include "MathUtils.h"
#include "Profiler.h"
#include "Resource.h"
#include "Shape.h"

#include "Wall.h"

#include "Location.h"
#include "ParticleSystem.h"
#include "WorldPointers.h"
#include "AppState.h"


Wall::Wall()
  : m_damage(0.0f),
    m_fallSpeed(0.0f)
{
  m_type = TypeWall;

  SetShape(g_resource->GetShape("Wall.shp"));
}

bool Wall::Advance()
{
  float landHeight = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);
  float targetY = landHeight + m_damage / 10 + 0.01f;

  if (m_pos.y > targetY)
  {
    m_fallSpeed += 10.0f * SERVER_ADVANCE_PERIOD;
    m_pos.y -= m_fallSpeed * SERVER_ADVANCE_PERIOD;

    if (m_pos.y < targetY)
    {
      m_fallSpeed = 0.0f;
      m_pos.y = targetY;

      Vector3 right = AsLegacy(m_front) ^ g_upVector;
      for (int i = -5; i < 5; ++i)
      {
        Vector3 particlePos = AsLegacy(m_pos) + right * i * frand(10.0f);
        particlePos.y = g_location->m_landscape.m_heightMap->GetValue(particlePos.x, particlePos.z) + 10.0f;
        Vector3 particleVel(sfrand(10.0f), frand(10.0f), sfrand(10.0f));

        g_particleSystem->CreateParticle(particlePos, particleVel, Particle::TypeRocketTrail, 50.0f);
      }
    }
  }

  return false;
}

void Wall::Damage(float _damage) { m_damage -= _damage; }

void Wall::Render(float _predictionTime)
{
#ifdef DEBUG_RENDER_ENABLED
  if (g_editing)
  {
    Vector3 pos(m_pos);
    pos.y += 5.0f;
    RenderArrow(pos, pos + AsLegacy(m_front) * 20.0f, 4.0f);
  }
#endif

  Vector3 predictedPos = AsLegacy(m_pos) - Vector3(0, m_fallSpeed, 0) * _predictionTime;
  Matrix34 mat(m_front, g_upVector, predictedPos);
  m_shape->Render(_predictionTime, mat);
}
