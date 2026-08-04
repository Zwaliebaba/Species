#include "pch.h"

#include "3dSprite.h"

#include "WorldPointers.h"


void Render3DSprite(DirectX::XMFLOAT3 const& _pos, float _width, float _height, int _textureId)
{
  DirectX::XMFLOAT3 const cameraUp = g_camera->GetUp();
  DirectX::XMFLOAT3 const cameraFront = g_camera->GetFront();

  DirectX::XMVECTOR const up = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&cameraUp), _height);
  DirectX::XMVECTOR const right =
    DirectX::XMVectorScale(DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&cameraUp), DirectX::XMLoadFloat3(&cameraFront)), _width * 0.5f);
  DirectX::XMVECTOR const centre = DirectX::XMLoadFloat3(&_pos);

  DirectX::XMFLOAT3 bottomLeft, bottomRight, topLeft, topRight;
  DirectX::XMStoreFloat3(&bottomLeft, DirectX::XMVectorSubtract(centre, right));
  DirectX::XMStoreFloat3(&bottomRight, DirectX::XMVectorAdd(centre, right));
  DirectX::XMStoreFloat3(&topLeft, DirectX::XMVectorAdd(DirectX::XMVectorSubtract(centre, right), up));
  DirectX::XMStoreFloat3(&topRight, DirectX::XMVectorAdd(DirectX::XMVectorAdd(centre, right), up));

  glEnable(GL_TEXTURE_2D);
  // glColor3ub(255, 255, 255);
  glDisable(GL_CULL_FACE);
  glBindTexture(GL_TEXTURE_2D, _textureId);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glBegin(GL_QUADS);
  glTexCoord2f(1, 1);
  glVertex3fv(&topLeft.x);
  glTexCoord2f(0, 1);
  glVertex3fv(&topRight.x);
  glTexCoord2f(0, 0);
  glVertex3fv(&bottomRight.x);
  glTexCoord2f(1, 0);
  glVertex3fv(&bottomLeft.x);
  glEnd();
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_CULL_FACE);
}
