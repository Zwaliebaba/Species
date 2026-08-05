#include "pch.h"
#include "GlVertex.h"


#include "MathUtils.h"
#include "Profiler.h"
#include "Resource.h"

#include "Explosion.h"
#include "Globals.h"
#include "GameTime.h"
#include "WorldPointers.h"


// #define EXPLOSION_LIFETIME		50.0f
// #define MAX_INITIAL_SPEED		0.01f
// #define MAX_ANG_VEL				0.01f
// #define ACCEL_DUE_TO_GRAV		0.0f
// #define INITIAL_VERTICAL_SPEED	0.0f
// #define MIN_FRAG_LIFE			0.9f			// As a fraction of EXPLOSION_LIFETIME
#define EXPLOSION_LIFETIME 5.0f
#define MAX_INITIAL_SPEED 3.0f
#define MAX_ANG_VEL 4.0f
#define ACCEL_DUE_TO_GRAV (-GRAVITY)
#define INITIAL_VERTICAL_SPEED 10.0f
#define MIN_FRAG_LIFE 0.0f     // As a fraction of EXPLOSION_LIFETIME
#define FRICTION_COEF 0.05f    // Bigger means more friction
#define ROT_FRICTION_COEF 0.2f // Bigger means more rotational friction
#define NUM_TUMBLERS 5


//*****************************************************************************
// Class Tumbler
//*****************************************************************************

Tumbler::Tumbler()
{
  // m_rotMat is identity from its declaration — see Explosion.h.
  m_angVel.x = sfrand(MAX_ANG_VEL);
  m_angVel.y = sfrand(MAX_ANG_VEL);
  m_angVel.z = sfrand(MAX_ANG_VEL);
}


void Tumbler::Advance()
{
  // BOTH of this conversion's reversals are in these two lines, and both are
  // pinned in NeuronMathTests because both look right the other way round.
  //
  // 1. Matrix33(yaw, dive, roll) called RotateAroundZ, X, then Y, but those
  //    rewrite the matrix's basis rows in place, so the equivalent composition
  //    is RotationY * RotationX * RotationZ — the reverse of the call order,
  //    and NOT XMMatrixRotationRollPitchYaw.
  // 2. `m_rotMat *= rotIncrement` becomes increment * rotMat, because the
  //    native matrix is the transpose of the legacy one and transposing
  //    reverses a product.
  //
  // Note also that the legacy call passed m_angVel.x as YAW, .y as DIVE and
  // .z as ROLL, which is preserved here.
  DirectX::XMMATRIX const rotIncrement = DirectX::XMMatrixRotationY(m_angVel.x * g_advanceTime) *
                                         DirectX::XMMatrixRotationX(m_angVel.y * g_advanceTime) *
                                         DirectX::XMMatrixRotationZ(m_angVel.z * g_advanceTime);
  DirectX::XMStoreFloat3x3(&m_rotMat, rotIncrement * DirectX::XMLoadFloat3x3(&m_rotMat));

  // Rotational Friction
  DirectX::XMStoreFloat3(&m_angVel, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_angVel), 1.0f - (g_advanceTime * ROT_FRICTION_COEF)));
}


//*****************************************************************************
// Class Explosion
//*****************************************************************************

Explosion::Explosion(ShapeFragment* _frag, DirectX::XMFLOAT4X4 const& _transform, float _fraction)
  : m_numTumblers(NUM_TUMBLERS),
    m_numTris(0),
    m_tris(nullptr)
{
  m_tumblers = new Tumbler[m_numTumblers];

  m_timeToDie = g_gameTime + EXPLOSION_LIFETIME;

  std::vector<ExplodingTri> triangles;

  DirectX::XMMATRIX const totalTransform = DirectX::XMLoadFloat4x4(&_frag->m_transform) * DirectX::XMLoadFloat4x4(&_transform);
  DirectX::XMVECTOR const transformedFragCentre = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&_frag->m_centre), totalTransform);

  for (int j = 0; j < _frag->m_numTriangles; ++j)
  {
    if (_fraction < 1.0f && frand(1.0f) > _fraction)
    {
      continue;
    }

    ShapeTriangle const& shapeTri = _frag->m_triangles[j];
    VertexPosCol const& v1 = _frag->m_vertices[shapeTri.v1];
    VertexPosCol const& v2 = _frag->m_vertices[shapeTri.v2];
    VertexPosCol const& v3 = _frag->m_vertices[shapeTri.v3];

    ExplodingTri tri;
    tri.m_colour = _frag->m_colours[v1.m_colId];
    DirectX::XMVECTOR const p1 = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&_frag->m_positions[v1.m_posId]), totalTransform);
    DirectX::XMVECTOR const p2 = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&_frag->m_positions[v2.m_posId]), totalTransform);
    DirectX::XMVECTOR const p3 = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&_frag->m_positions[v3.m_posId]), totalTransform);
    DirectX::XMStoreFloat3(&tri.m_v1, p1);
    DirectX::XMStoreFloat3(&tri.m_v2, p2);
    DirectX::XMStoreFloat3(&tri.m_v3, p3);

    if (DirectX::XMVector3Equal(p1, p2) || DirectX::XMVector3Equal(p2, p3) || DirectX::XMVector3Equal(p1, p3))
    {
      continue;
    }

    float circum = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(p1, p2))) +
                   DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(p2, p3))) +
                   DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(p3, p1)));

    if (circum < 6.0f)
    {
      // Too small to care about
      continue;
    }

    // 0.3333f rather than a third, preserved exactly as it was.
    DirectX::XMVECTOR const centre = DirectX::XMVectorScale(DirectX::XMVectorAdd(DirectX::XMVectorAdd(p1, p2), p3), 0.3333f);

    DirectX::XMVECTOR const c1 = DirectX::XMVectorSubtract(p1, centre);
    DirectX::XMVECTOR const c2 = DirectX::XMVectorSubtract(p2, centre);
    DirectX::XMVECTOR const c3 = DirectX::XMVectorSubtract(p3, centre);
    DirectX::XMStoreFloat3(&tri.m_v1, c1);
    DirectX::XMStoreFloat3(&tri.m_v2, c2);
    DirectX::XMStoreFloat3(&tri.m_v3, c3);

    // Vector3::operator^ was the cross product.
    DirectX::XMVECTOR norm =
      DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::XMVectorSubtract(c1, c2), DirectX::XMVectorSubtract(c2, c3)));
    if (j & 1)
      norm = DirectX::XMVectorNegate(norm);
    DirectX::XMStoreFloat3(&tri.m_norm, norm);

    DirectX::XMStoreFloat3(&tri.m_vel, DirectX::XMVectorScale(DirectX::XMVectorSubtract(centre, transformedFragCentre), MAX_INITIAL_SPEED));
    tri.m_vel.y += INITIAL_VERTICAL_SPEED;
    DirectX::XMStoreFloat3(&tri.m_pos, centre);
    tri.m_tumbler = &m_tumblers[speciesRandom() % NUM_TUMBLERS];
    float const minFragLife = EXPLOSION_LIFETIME * MIN_FRAG_LIFE;
    // tri.m_timeToDie = frand(EXPLOSION_LIFETIME - minFragLife) + g_gameTime + minFragLife;
    tri.m_timeToDie = g_gameTime + EXPLOSION_LIFETIME;

    triangles.push_back(tri);
  }

  m_numTris = static_cast<int>(triangles.size());
  if (m_numTris > 0)
  {
    m_tris = new ExplodingTri[m_numTris];
    memcpy(m_tris, triangles.data(), sizeof(ExplodingTri) * m_numTris);
  }
}


Explosion::~Explosion()
{
  m_numTumblers = 0;
  delete[] m_tumblers;
  m_tumblers = nullptr;

  m_numTris = 0;
  delete[] m_tris;
  m_tris = nullptr;
}


bool Explosion::Advance()
{
  float deltaVelY = ACCEL_DUE_TO_GRAV * g_advanceTime;

  // Advance all tumblers
  for (int i = 0; i < m_numTumblers; ++i)
  {
    m_tumblers[i].Advance();
  }

  // Advance all ExplodingTriangles
  for (int i = 0; i < m_numTris; ++i)
  {
    if (g_gameTime > m_tris[i].m_timeToDie)
      continue;

    DirectX::XMVECTOR const vel = DirectX::XMLoadFloat3(&m_tris[i].m_vel);
    DirectX::XMStoreFloat3(&m_tris[i].m_pos,
                           DirectX::XMVectorMultiplyAdd(vel, DirectX::XMVectorReplicate(g_advanceTime), DirectX::XMLoadFloat3(&m_tris[i].m_pos)));

    // Friction
    float speed = DirectX::XMVectorGetX(DirectX::XMVector3Length(vel));
    float friction = speed * FRICTION_COEF * g_advanceTime;
    if (friction > 1.0f)
      friction = 1.0f;
    DirectX::XMStoreFloat3(&m_tris[i].m_vel, DirectX::XMVectorScale(vel, 1.0f - friction));

    // Gravity
    m_tris[i].m_vel.y += deltaVelY;
  }

  if (g_gameTime > m_timeToDie)
  {
    return true;
  }

  return false;
}


void Explosion::Render()
{
  // someone tries to render explosion with 0 tris
  // happens at "kill all enemies"
  if (!m_numTris)
    return;

  float age = (EXPLOSION_LIFETIME + (g_gameTime - m_timeToDie)) / EXPLOSION_LIFETIME;

  glBegin(GL_TRIANGLES);
  for (int i = 0; i < m_numTris; ++i)
  {
    if (g_gameTime > m_tris[i].m_timeToDie)
      continue;

    DirectX::XMMATRIX const rot = DirectX::XMLoadFloat3x3(&m_tris[i].m_tumbler->m_rotMat);
    DirectX::XMVECTOR const pos = DirectX::XMLoadFloat3(&m_tris[i].m_pos);
    DirectX::XMFLOAT3 norm;
    DirectX::XMStoreFloat3(&norm, DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&m_tris[i].m_norm), rot));
    DirectX::XMVECTOR const v1 = DirectX::XMVectorAdd(DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&m_tris[i].m_v1), rot), pos);
    DirectX::XMVECTOR const v2 = DirectX::XMVectorAdd(DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&m_tris[i].m_v2), rot), pos);
    DirectX::XMVECTOR const v3 = DirectX::XMVectorAdd(DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&m_tris[i].m_v3), rot), pos);

    glColor4ub(m_tris[i].m_colour.r, m_tris[i].m_colour.g, m_tris[i].m_colour.b, (1.0f - age) * 255);

    glNormal3fv(&norm.x);

    glTexCoord2i(0, 0);
    EmitVertex(v1);
    glTexCoord2i(0, 1);
    EmitVertex(v2);
    glTexCoord2i(1, 1);
    EmitVertex(v3);
  }
  glEnd();
}

DirectX::XMFLOAT3 Explosion::GetCenter() const
{
  DirectX::XMVECTOR sum = DirectX::XMVectorZero();
  unsigned summed = 0;
  for (int i = 0; i < m_numTris; ++i)
  {
    if (g_gameTime > m_tris[i].m_timeToDie)
      continue;
    sum = DirectX::XMVectorAdd(sum, DirectX::XMLoadFloat3(&m_tris[i].m_pos));
    summed++;
  }
  DirectX::XMFLOAT3 result;
  if (summed)
  {
    // operator/ computed 1/b once and multiplied, which is what XMVectorScale does.
    DirectX::XMStoreFloat3(&result, DirectX::XMVectorScale(sum, 1.0f / static_cast<float>(summed)));
    return result;
  }
  DEBUG_ASSERT(0);
  DirectX::XMStoreFloat3(&result, sum);
  return result;
}


//*****************************************************************************
// Class ExplosionManager
//*****************************************************************************

ExplosionManager g_explosionManager;


ExplosionManager::ExplosionManager() {}


ExplosionManager::~ExplosionManager() { EmptyAndDelete(m_explosions); }


void ExplosionManager::AddExplosion(ShapeFragment* _frag, DirectX::XMFLOAT4X4 const& _transform, bool _recurse, float _fraction)
{
  if (_fraction <= 0.0f)
  {
    return;
  }

  if (_frag->m_numTriangles > 0)
  {
    Explosion* explosion = new Explosion(_frag, _transform, _fraction);
    m_explosions.push_back(explosion);
  }

  if (_recurse)
  {
    DirectX::XMFLOAT4X4 totalMatrix;
    DirectX::XMStoreFloat4x4(&totalMatrix, DirectX::XMLoadFloat4x4(&_frag->m_transform) * DirectX::XMLoadFloat4x4(&_transform));

    for (int i = 0; i < static_cast<int>(_frag->m_childFragments.size()); ++i)
    {
      ShapeFragment* child = _frag->m_childFragments[i].get();
      AddExplosion(child, totalMatrix, true, _fraction);
    }
  }
}


void ExplosionManager::AddExplosion(Shape* _shape, DirectX::XMFLOAT4X4 const& _transform, float _fraction)
{
  if (_fraction > 0.0f)
    AddExplosion(_shape->m_rootFragment.get(), _transform, true, _fraction);
}


void ExplosionManager::Reset() { EmptyAndDelete(m_explosions); }


void ExplosionManager::Advance()
{
  START_PROFILE(g_profiler, "Advance Explosions");

  for (unsigned int i = 0; i < static_cast<int>(m_explosions.size()); ++i)
  {
    if (m_explosions[i]->Advance())
    {
      Explosion* explosion = m_explosions[i];
      m_explosions.erase(m_explosions.begin() + i);
      delete explosion;
      --i;
    }
  }

  END_PROFILE(g_profiler, "Advance Explosions");
}


void ExplosionManager::Render()
{
  START_PROFILE(g_profiler, "Render Explosions");

  int numExplosions = static_cast<int>(m_explosions.size());

  if (numExplosions > 0)
  {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/ShapeWireframe.bmp"));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

    g_renderer->SetObjectLighting();

    glEnable(GL_COLOR_MATERIAL);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);

    for (unsigned int i = 0; i < numExplosions; ++i)
    {
      m_explosions[i]->Render();
    }

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glDisable(GL_COLOR_MATERIAL);
    g_renderer->UnsetObjectLighting();

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDisable(GL_TEXTURE_2D);
  }

  END_PROFILE(g_profiler, "Render Explosions");
}
