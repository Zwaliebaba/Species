#include "pch.h"
#include "GlVertex.h"
#include "Profiler.h"
#include "TextRenderer.h"

#include "Flag.h"

#include "GameTime.h"
#include "WorldPointers.h"


Flag::Flag() {}


void Flag::Initialise()
{
  for (int x = 0; x < FLAG_RESOLUTION; ++x)
  {
    for (int z = 0; z < FLAG_RESOLUTION; ++z)
    {
      DirectX::XMVECTOR targetPos = DirectX::XMLoadFloat3(&m_pos);
      targetPos =
        DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_front), DirectX::XMVectorReplicate((float)x / (float)FLAG_RESOLUTION), targetPos);
      targetPos =
        DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_up), DirectX::XMVectorReplicate((float)z / (float)FLAG_RESOLUTION), targetPos);

      DirectX::XMStoreFloat3(&m_flag[x][z], targetPos);
    }
  }
}


void Flag::SetTexture(int _textureId) { m_texId = _textureId; }


void Flag::SetPosition(DirectX::XMFLOAT3 const& _pos) { m_pos = _pos; }


void Flag::SetOrientation(DirectX::XMFLOAT3 const& _front, DirectX::XMFLOAT3 const& _up)
{
  m_front = _front;
  m_up = _up;
}


void Flag::SetSize(float _size) { m_size = _size; }


void Flag::Render()
{
  START_PROFILE(g_profiler, "RenderFlag");

  //
  // Advance the flag

  DirectX::XMVECTOR const front = DirectX::XMLoadFloat3(&m_front);
  DirectX::XMVECTOR const up = DirectX::XMLoadFloat3(&m_up);

  for (int x = 0; x < FLAG_RESOLUTION; ++x)
  {
    for (int z = 0; z < FLAG_RESOLUTION; ++z)
    {
      float factor1 = g_advanceTime * 10 * (FLAG_RESOLUTION - x) / FLAG_RESOLUTION;
      if (x == 0)
        factor1 = 1.0f;
      float factor2 = 1.0f - factor1;
      DirectX::XMVECTOR targetPos;

      if (x == 0)
      {
        targetPos = DirectX::XMVectorMultiplyAdd(up, DirectX::XMVectorReplicate(m_size / 2.0f), DirectX::XMLoadFloat3(&m_pos));
        targetPos = DirectX::XMVectorMultiplyAdd(front, DirectX::XMVectorReplicate((float)x / (float)(FLAG_RESOLUTION - 1) * m_size), targetPos);
        targetPos = DirectX::XMVectorMultiplyAdd(up, DirectX::XMVectorReplicate((float)z / (float)(FLAG_RESOLUTION - 1) * m_size), targetPos);
      }
      else
      {
        targetPos = DirectX::XMVectorMultiplyAdd(front, DirectX::XMVectorReplicate(1.0f / (float)(FLAG_RESOLUTION - 1) * m_size),
                                                 DirectX::XMLoadFloat3(&m_flag[x - 1][z]));
      }

      // Three separate component writes become one added vector: an XMVECTOR
      // has no .x/.y/.z to assign through. Same three expressions, same order.
      targetPos = DirectX::XMVectorAdd(targetPos, DirectX::XMVectorSet(1 * cosf(g_gameTime + x * 1.5f) * (float)x / (float)FLAG_RESOLUTION,
                                                                       3 * sinf(g_gameTime + x * 1.1f) * (float)x / (float)FLAG_RESOLUTION,
                                                                       1 * cosf(g_gameTime + x * 1.3f) * (float)x / (float)FLAG_RESOLUTION, 0.0f));

      DirectX::XMStoreFloat3(&m_flag[x][z], DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_flag[x][z]), DirectX::XMVectorReplicate(factor2),
                                                                         DirectX::XMVectorScale(targetPos, factor1)));

      if (x > 0)
      {
        DirectX::XMVECTOR const previous = DirectX::XMLoadFloat3(&m_flag[x - 1][z]);
        DirectX::XMVECTOR const theVector = DirectX::XMVectorSubtract(previous, DirectX::XMLoadFloat3(&m_flag[x][z]));
        float const distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(theVector));
        if (distance > 10.0f)
        {
          // distance > 10 here, so SetLength's zero-length fallback was
          // unreachable and XMVector3Normalize is an exact stand-in.
          DirectX::XMStoreFloat3(&m_flag[x][z],
                                 DirectX::XMVectorSubtract(previous, DirectX::XMVectorScale(DirectX::XMVector3Normalize(theVector), 10.0f)));
        }
      }
    }
  }

  //
  // Render the flag pole

  glColor4ub(255, 255, 100, 255);

  glDisable(GL_CULL_FACE);
  glDisable(GL_TEXTURE_2D);

  DirectX::XMFLOAT3 const camFrontStore = g_camera->GetFront();
  // operator^ was the cross product. SetLength(0.2) is normalise-then-scale,
  // and m_up crossed with the camera front is only degenerate when the camera
  // looks straight along the pole, which the legacy fallback handled no better.
  DirectX::XMVECTOR const right =
    DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMVector3Cross(up, DirectX::XMLoadFloat3(&camFrontStore))), 0.2f);
  DirectX::XMVECTOR const polePos = DirectX::XMLoadFloat3(&m_pos);
  DirectX::XMVECTOR const poleTop = DirectX::XMVectorMultiplyAdd(up, DirectX::XMVectorReplicate(m_size * 1.5f), polePos);

  glBegin(GL_QUADS);
  EmitVertex(DirectX::XMVectorSubtract(polePos, right));
  EmitVertex(DirectX::XMVectorAdd(polePos, right));
  EmitVertex(DirectX::XMVectorAdd(poleTop, right));
  EmitVertex(DirectX::XMVectorSubtract(poleTop, right));
  glEnd();

  //
  // Render the flag

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, m_texId);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

  glColor4f(1.0f, 1.0f, 1.0f, 0.8f);
  for (int x = 0; x < FLAG_RESOLUTION - 1; ++x)
  {
    for (int y = 0; y < FLAG_RESOLUTION - 1; ++y)
    {
      float texX = (float)x / (float)(FLAG_RESOLUTION - 1);
      float texY = (float)y / (float)(FLAG_RESOLUTION - 1);
      float texW = 1.0f / (FLAG_RESOLUTION - 1);

      glBegin(GL_QUADS);
      glTexCoord2f(texX, texY);
      EmitVertex(DirectX::XMLoadFloat3(&m_flag[x][y]));
      glTexCoord2f(texX + texW, texY);
      EmitVertex(DirectX::XMLoadFloat3(&m_flag[x + 1][y]));
      glTexCoord2f(texX + texW, texY + texW);
      EmitVertex(DirectX::XMLoadFloat3(&m_flag[x + 1][y + 1]));
      glTexCoord2f(texX, texY + texW);
      EmitVertex(DirectX::XMLoadFloat3(&m_flag[x][y + 1]));
      glEnd();
    }
  }

  glDepthMask(true);
  glDisable(GL_BLEND);
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_CULL_FACE);

  END_PROFILE(g_profiler, "RenderFlag");
}


void Flag::RenderText(int _posX, int _posY, char* _caption)
{
  if (_posX < 0 || _posY < 0 || _posX >= FLAG_RESOLUTION || _posY >= FLAG_RESOLUTION)
  {
    return;
  }

  DirectX::XMVECTOR pos = DirectX::XMLoadFloat3(&m_flag[_posX][_posY]);
  DirectX::XMVECTOR up = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_flag[_posX][_posY + 1]), pos);
  DirectX::XMVECTOR const right = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_flag[_posX + 1][_posY]), pos);

  pos = DirectX::XMVectorMultiplyAdd(right, DirectX::XMVectorReplicate(0.5f), pos);
  pos = DirectX::XMVectorMultiplyAdd(up, DirectX::XMVectorReplicate(0.5f), pos);

  DirectX::XMVECTOR const front = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(right, up));
  up = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(front, right));

  DirectX::XMFLOAT3 frontStore, upStore, posFront, posBack;
  DirectX::XMStoreFloat3(&frontStore, front);
  DirectX::XMStoreFloat3(&upStore, up);
  DirectX::XMStoreFloat3(&posFront, DirectX::XMVectorMultiplyAdd(front, DirectX::XMVectorReplicate(0.1f), pos));
  DirectX::XMStoreFloat3(&posBack, DirectX::XMVectorNegativeMultiplySubtract(front, DirectX::XMVectorReplicate(0.1f), pos));
  DirectX::XMFLOAT3 frontBack;
  DirectX::XMStoreFloat3(&frontBack, DirectX::XMVectorNegate(front));

  g_gameFont.DrawText3D(posFront, frontStore, upStore, 4.0f, _caption);
  g_gameFont.DrawText3D(posBack, frontBack, upStore, 4.0f, _caption);
}
