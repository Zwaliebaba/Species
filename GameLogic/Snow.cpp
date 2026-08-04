#include "pch.h"
#include "HiResTime.h"
#include "DebugRender.h"
#include "3dSprite.h"
#include "Resource.h"

#include "Snow.h"

#include "ProtocolLimits.h"
#include "Location.h"
#include "WorldPointers.h"


// ****************************************************************************
// Class Snow
// ****************************************************************************

Snow::Snow()
  : WorldObject()
{
  m_positionOffset = syncfrand(10.0f);
  m_xaxisRate = syncfrand(2.0f);
  m_yaxisRate = syncfrand(2.0f);
  m_zaxisRate = syncfrand(2.0f);

  m_timeSync = GetHighResTime();
  m_type = EffectSnow;
}


bool Snow::Advance()
{
  DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), 0.9f));

  //
  // Make me float around slowly

  m_positionOffset += SERVER_ADVANCE_PERIOD;
  m_xaxisRate += syncsfrand(1.0f);
  m_yaxisRate += syncsfrand(1.0f);
  m_zaxisRate += syncsfrand(1.0f);
  if (m_xaxisRate > 2.0f)
    m_xaxisRate = 2.0f;
  if (m_xaxisRate < 0.0f)
    m_xaxisRate = 0.0f;
  if (m_yaxisRate > 2.0f)
    m_yaxisRate = 2.0f;
  if (m_yaxisRate < 0.0f)
    m_yaxisRate = 0.0f;
  if (m_zaxisRate > 2.0f)
    m_zaxisRate = 2.0f;
  if (m_zaxisRate < 0.0f)
    m_zaxisRate = 0.0f;
  m_hover.x = sinf(m_positionOffset) * m_xaxisRate;
  m_hover.y = sinf(m_positionOffset) * m_yaxisRate;
  m_hover.z = sinf(m_positionOffset) * m_zaxisRate;

  float heightAboveGround = m_pos.y - g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);
  if (heightAboveGround > -10.0f)
  {
    float fractionAboveGround = heightAboveGround / 100.0f;
    fractionAboveGround = std::min(fractionAboveGround, 1.0f);
    fractionAboveGround = std::max(fractionAboveGround, 0.2f);
    m_hover.y = (-20.0f - syncfrand(20.0f)) * fractionAboveGround;
  }
  else
  {
    return true;
  }

  // Two separate steps, as the legacy code had them: velocity first, then
  // hover. One combined add would be a different rounding.
  DirectX::XMVECTOR const step = DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD);
  DirectX::XMVECTOR position = DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), step, DirectX::XMLoadFloat3(&m_pos));
  position = DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_hover), step, position);
  DirectX::XMStoreFloat3(&m_pos, position);

  float worldSizeX = g_location->m_landscape.GetWorldSizeX();
  float worldSizeZ = g_location->m_landscape.GetWorldSizeZ();
  if (m_pos.x < 0.0f)
    m_pos.x = 0.0f;
  if (m_pos.z < 0.0f)
    m_pos.z = 0.0f;
  if (m_pos.x >= worldSizeX)
    m_pos.x = worldSizeX;
  if (m_pos.z >= worldSizeZ)
    m_pos.z = worldSizeZ;

  return false;
}


float Snow::GetLife()
{
  float timePassed = GetHighResTime() - m_timeSync;
  float life = timePassed / 10.0f;
  life = std::min(life, 1.0f);
  return life;
}


void Snow::Render(float _predictionTime)
{
  _predictionTime -= SERVER_ADVANCE_PERIOD;

  DirectX::XMVECTOR const prediction = DirectX::XMVectorReplicate(_predictionTime);
  DirectX::XMVECTOR predictedPos = DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), prediction, DirectX::XMLoadFloat3(&m_pos));
  predictedPos = DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_hover), prediction, predictedPos);

  float size = 20.0f;

  glColor4f(1.0f, 1.0f, 1.0f, 1.0);
  DirectX::XMFLOAT3 spritePos;
  DirectX::XMStoreFloat3(&spritePos, predictedPos);
  Render3DSprite(spritePos, size, size, g_resource->GetTexture("Textures/Starburst.bmp"));
}
