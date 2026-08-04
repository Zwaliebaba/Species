#include "pch.h"

#include <math.h>

#include "NeuronMath.h"

#include "SphereRenderer.h"


// ******************
// * Class Triangle *
// ******************
Triangle::Triangle(DirectX::XMFLOAT3 const& c1, DirectX::XMFLOAT3 const& c2, DirectX::XMFLOAT3 const& c3)
{
  m_corner[0] = c1;
  m_corner[1] = c2;
  m_corner[2] = c3;
}


// ****************
// * Class Sphere *
// ****************

Sphere::Sphere()
{
  float const x = 0.5257311121;
  float const z = 0.85065080835;

  DirectX::XMFLOAT3 c[12];
  c[0] = DirectX::XMFLOAT3(-x, 0, z);
  c[1] = DirectX::XMFLOAT3(x, 0, z);
  c[2] = DirectX::XMFLOAT3(-x, 0, -z);
  c[3] = DirectX::XMFLOAT3(x, 0, -z);
  c[4] = DirectX::XMFLOAT3(0, z, x);
  c[5] = DirectX::XMFLOAT3(0, z, -x);
  c[6] = DirectX::XMFLOAT3(0, -z, x);
  c[7] = DirectX::XMFLOAT3(0, -z, -x);
  c[8] = DirectX::XMFLOAT3(z, x, 0);
  c[9] = DirectX::XMFLOAT3(-z, x, 0);
  c[10] = DirectX::XMFLOAT3(z, -x, 0);
  c[11] = DirectX::XMFLOAT3(-z, -x, 0);

  m_topLevelTriangle[0] = Triangle(c[1], c[4], c[0]);
  m_topLevelTriangle[1] = Triangle(c[4], c[9], c[0]);
  m_topLevelTriangle[2] = Triangle(c[4], c[5], c[9]);
  m_topLevelTriangle[3] = Triangle(c[8], c[5], c[4]);
  m_topLevelTriangle[4] = Triangle(c[1], c[8], c[4]);
  m_topLevelTriangle[5] = Triangle(c[1], c[10], c[8]);
  m_topLevelTriangle[6] = Triangle(c[10], c[3], c[8]);
  m_topLevelTriangle[7] = Triangle(c[8], c[3], c[5]);
  m_topLevelTriangle[8] = Triangle(c[3], c[2], c[5]);
  m_topLevelTriangle[9] = Triangle(c[3], c[7], c[2]);
  m_topLevelTriangle[10] = Triangle(c[3], c[10], c[7]);
  m_topLevelTriangle[11] = Triangle(c[10], c[6], c[7]);
  m_topLevelTriangle[12] = Triangle(c[6], c[11], c[7]);
  m_topLevelTriangle[13] = Triangle(c[6], c[0], c[11]);
  m_topLevelTriangle[14] = Triangle(c[6], c[1], c[0]);
  m_topLevelTriangle[15] = Triangle(c[10], c[1], c[6]);
  m_topLevelTriangle[16] = Triangle(c[11], c[0], c[9]);
  m_topLevelTriangle[17] = Triangle(c[2], c[11], c[9]);
  m_topLevelTriangle[18] = Triangle(c[5], c[2], c[9]);
  m_topLevelTriangle[19] = Triangle(c[11], c[2], c[7]);

  m_displayListId = glGenLists(1);
  glNewList(m_displayListId, GL_COMPILE);
  RenderLong();
  glEndList();
}

void Sphere::ConsiderTriangle(int level, DirectX::XMFLOAT3 const& a, DirectX::XMFLOAT3 const& b, DirectX::XMFLOAT3 const& c)
{
  if (level > 0)
  {
    glBegin(GL_TRIANGLES);
    glVertex3f(a.x, a.y, a.z);
    glVertex3f(b.x, b.y, b.z);
    glVertex3f(c.x, c.y, c.z);
    glEnd();
  }
  else
  {
    level++;

    // Three new vertices, at the midpoints of each existing edge, pushed back
    // out to the unit sphere.
    DirectX::XMVECTOR const av = DirectX::XMLoadFloat3(&a);
    DirectX::XMVECTOR const bv = DirectX::XMLoadFloat3(&b);
    DirectX::XMVECTOR const cv = DirectX::XMLoadFloat3(&c);

    DirectX::XMFLOAT3 p, q, r;
    DirectX::XMStoreFloat3(&p, DirectX::XMVector3Normalize(DirectX::XMVectorAdd(av, bv)));
    DirectX::XMStoreFloat3(&q, DirectX::XMVector3Normalize(DirectX::XMVectorAdd(bv, cv)));
    DirectX::XMStoreFloat3(&r, DirectX::XMVector3Normalize(DirectX::XMVectorAdd(cv, av)));

    ConsiderTriangle(level, a, p, r);
    ConsiderTriangle(level, p, q, r);
    ConsiderTriangle(level, p, b, q);
    ConsiderTriangle(level, r, q, c);
  }
}

void Sphere::RenderLong()
{
  // Render each top level triangle
  for (int i = 0; i < 20; i++)
  {
    ConsiderTriangle(0, m_topLevelTriangle[i].m_corner[0], m_topLevelTriangle[i].m_corner[1], m_topLevelTriangle[i].m_corner[2]);
  }
}


void Sphere::Render(DirectX::XMFLOAT3 const& pos, float radius)
{
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glTranslatef(pos.x, pos.y, pos.z);
  glScalef(radius, radius, radius);
  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  glLineWidth(1.0f);
  glCallList(m_displayListId);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glPopMatrix();
}
