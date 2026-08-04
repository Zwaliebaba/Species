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

      DirectX::XMVECTOR const right = DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1);
      DirectX::XMVECTOR const wallPos = DirectX::XMLoadFloat3(&m_pos);
      for (int i = -5; i < 5; ++i)
      {
        // frand is inside the loop and stays there: it advances the RNG once
        // per particle, and the migration may not change the call sequence.
        DirectX::XMFLOAT3 particlePos;
        DirectX::XMStoreFloat3(&particlePos, DirectX::XMVectorMultiplyAdd(right, DirectX::XMVectorReplicate(i * frand(10.0f)), wallPos));
        particlePos.y = g_location->m_landscape.m_heightMap->GetValue(particlePos.x, particlePos.z) + 10.0f;
        DirectX::XMFLOAT3 const particleVel(sfrand(10.0f), frand(10.0f), sfrand(10.0f));

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
    DirectX::XMFLOAT3 pos = m_pos;
    pos.y += 5.0f;

    DirectX::XMFLOAT3 arrowEnd;
    DirectX::XMStoreFloat3(
      &arrowEnd, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_front), DirectX::XMVectorReplicate(20.0f), DirectX::XMLoadFloat3(&pos)));
    RenderArrow(pos, arrowEnd, 4.0f);
  }
#endif

  DirectX::XMVECTOR const predictedPos =
    DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMVectorSet(0.0f, m_fallSpeed * _predictionTime, 0.0f, 0.0f));

  DirectX::XMFLOAT4X4 mat;
  DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, predictedPos));
  m_shape->Render(_predictionTime, mat);
}
