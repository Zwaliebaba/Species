#include "pch.h"
#include "GlVertex.h"
#include "SoundSources.h"

#include <math.h>

#include "Resource.h"
#include "Shape.h"
#include "MathUtils.h"
#include "DebugRender.h"
#include "TextRenderer.h"

#include "SporeGenerator.h"

#include "Location.h"
#include "GameTime.h"
#include "Explosion.h"

#include "SoundSystem.h"
#include "WorldPointers.h"
#include "AppState.h"


#define SPOREGENERATOR_HOVERHEIGHT 100.0f
#define SPOREGENERATOR_EGGLAYHEIGHT 20.0f
#define SPOREGENERATOR_SPIRITSEARCHRANGE 200.0f


SporeGenerator::SporeGenerator()
  : Entity(),
    m_eggTimer(0.0f),
    m_retargetTimer(0.0f),
    m_state(StateIdle)
{
  SetType(TypeSporeGenerator);

  m_shape = g_resource->GetShape("SporeGenerator.shp");
  m_eggMarker = m_shape->m_rootFragment->LookupMarker("MarkerEggs");

  for (int i = 0; i < SPOREGENERATOR_NUMTAILS; ++i)
  {
    char name[256];
    sprintf(name, "MarkerTail0%d", i + 1);
    m_tail[i] = m_shape->m_rootFragment->LookupMarker(name);
  }
}


void SporeGenerator::Begin()
{
  Entity::Begin();

  m_targetPos = m_pos;
  m_pos.y = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);
  m_pos.y += SPOREGENERATOR_HOVERHEIGHT;
}


void SporeGenerator::ChangeHealth(int _amount)
{
  if (!m_dead && _amount < -1)
  {
    Entity::ChangeHealth(_amount);

    float fractionDead = 1.0f - (float)m_stats[StatHealth] / (float)EntityBlueprint::GetStat(TypeSporeGenerator, StatHealth);
    fractionDead = std::max(fractionDead, 0.0f);
    fractionDead = std::min(fractionDead, 1.0f);
    if (m_dead)
      fractionDead = 1.0f;

    DirectX::XMFLOAT4X4 transform;
    DirectX::XMStoreFloat4x4(&transform,
                             BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));
    g_explosionManager.AddExplosion(m_shape, transform, fractionDead);

    m_state = StatePanic;
    m_retargetTimer = 10.0f;
    m_vel = DirectX::XMFLOAT3(0.0f, m_stats[StatSpeed] * 3.0f, 0.0f);
  }
}


bool SporeGenerator::SearchForRandomPos()
{
  float distToSpawnPoint =
    DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&m_spawnPoint))));
  float chanceOfReturn = (distToSpawnPoint / m_roamRange);
  if (chanceOfReturn >= 1.0f || syncfrand(1.0f) <= chanceOfReturn)
  {
    // We have strayed too far from our spawn point
    // So head back there now
    // SetLength, unreachable at zero length: this branch needs the generator to
    // have strayed from its spawn point.
    DirectX::XMVECTOR const returnVector = DirectX::XMVectorScale(
      DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_spawnPoint), DirectX::XMLoadFloat3(&m_pos))), 200.0f);
    DirectX::XMStoreFloat3(&m_targetPos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), returnVector));
  }
  else
  {
    // The two syncsfrand calls stay in this order: they advance the RNG.
    DirectX::XMFLOAT3 const wander(syncsfrand(200.0f), 0.0f, syncsfrand(200.0f));
    m_targetPos = DirectX::XMFLOAT3(m_pos.x + wander.x, m_pos.y, m_pos.z + wander.z);
  }

  m_targetPos.y = g_location->m_landscape.m_heightMap->GetValue(m_targetPos.x, m_targetPos.z);
  m_targetPos.y += SPOREGENERATOR_HOVERHEIGHT * (1.0f + syncsfrand(0.3f));

  m_state = StateIdle;


  //
  // Give us a random chance we will lay some eggs anyway

  if (syncfrand(100.0f) < 10.0f + 20.0f * g_difficultyLevel / 10.0f)
  {
    m_state = StateEggLaying;
    m_targetPos.y = g_location->m_landscape.m_heightMap->GetValue(m_targetPos.x, m_targetPos.z);
    m_targetPos.y += SPOREGENERATOR_EGGLAYHEIGHT;
  }

  return true;
}


bool SporeGenerator::SearchForSpirits()
{
  Spirit* found = nullptr;
  int foundIndex = -1;
  float nearest = 9999.9f;

  for (int i = 0; i < g_location->m_spirits.Size(); ++i)
  {
    if (g_location->m_spirits.ValidIndex(i))
    {
      Spirit* s = g_location->m_spirits.GetPointer(i);
      if (s->NumNearbyEggs() < 3 && s->m_pos.y > 0.0f)
      {
        float theDist =
          DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&s->m_pos), DirectX::XMLoadFloat3(&m_pos))));

        if (theDist <= SPOREGENERATOR_SPIRITSEARCHRANGE && theDist < nearest && s->m_state == Spirit::StateFloating)
        {
          found = s;
          foundIndex = i;
          nearest = theDist;
        }
      }
    }
  }

  if (found)
  {
    m_spiritId = foundIndex;
    // The two syncsfrand calls stay in this order: they advance the RNG.
    DirectX::XMFLOAT3 const scatter(syncsfrand(60.0f), 0.0f, syncsfrand(60.0f));
    m_targetPos = DirectX::XMFLOAT3(found->m_pos.x + scatter.x, found->m_pos.y, found->m_pos.z + scatter.z);
    m_targetPos.y = g_location->m_landscape.m_heightMap->GetValue(m_targetPos.x, m_targetPos.z);
    m_targetPos.y += SPOREGENERATOR_EGGLAYHEIGHT;
    m_state = StateEggLaying;
  }

  return found;
}


bool SporeGenerator::AdvancePanic()
{
  m_retargetTimer -= SERVER_ADVANCE_PERIOD;

  if (m_retargetTimer <= 0.0f)
  {
    m_state = StateIdle;
    return false;
  }

  // The three RNG calls stay in this order.
  DirectX::XMFLOAT3 const panicOffset(syncsfrand(100.0f), syncfrand(10.0f), syncsfrand(100.0f));
  m_targetPos = DirectX::XMFLOAT3(m_pos.x + panicOffset.x, m_pos.y + panicOffset.y, m_pos.z + panicOffset.z);

  AdvanceToTargetPosition();

  return false;
}


bool SporeGenerator::AdvanceToTargetPosition()
{
  float heightAboveGround = m_pos.y - g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);
  DirectX::XMVECTOR const ourPos = DirectX::XMLoadFloat3(&m_pos);
  DirectX::XMVECTOR const target = DirectX::XMLoadFloat3(&m_targetPos);
  float distanceToTarget = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(ourPos, target)));

  float speed = m_stats[StatSpeed];
  if (distanceToTarget < 50.0f)
  {
    speed *= distanceToTarget / 50.0f;
  }

  // SetLength on (target - pos). distanceToTarget can be zero, and the legacy
  // fallback then left (speed, 0, 0) -- but speed is scaled by
  // distanceToTarget/50 in that case, so it is zero too and both give a zero
  // velocity. Taking the native normalise would give QNaN instead, so the
  // fallback is kept.
  DirectX::XMVECTOR const toTarget = DirectX::XMVectorSubtract(target, ourPos);
  DirectX::XMVECTOR const targetVel =
    NearlyEquals(distanceToTarget, 0.0f) ? DirectX::XMVectorSet(speed, 0.0f, 0.0f, 0.0f) : DirectX::XMVectorScale(toTarget, speed / distanceToTarget);

  float factor1 = SERVER_ADVANCE_PERIOD * 0.5f;
  float factor2 = 1.0f - factor1;

  DirectX::XMVECTOR vel =
    DirectX::XMVectorMultiplyAdd(targetVel, DirectX::XMVectorReplicate(factor1), DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), factor2));

  // RotateAround's angle is the vector's magnitude, with its own 1e-8 guard.
  DirectX::XMVECTOR const right = DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1);
  DirectX::XMVECTOR const rotation = DirectX::XMVectorScale(right, sinf(g_gameTime * 3.0f) * SERVER_ADVANCE_PERIOD * 1.5f);
  float const rotationLengthSquared = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(rotation));
  if (rotationLengthSquared >= 1e-8f)
  {
    float const spin = sqrtf(rotationLengthSquared);
    vel = DirectX::XMVector3Transform(vel, DirectX::XMMatrixRotationAxis(DirectX::XMVectorScale(rotation, 1.0f / spin), spin));
  }
  DirectX::XMStoreFloat3(&m_vel, vel);

  DirectX::XMStoreFloat3(&m_front, DirectX::XMVector3Normalize(DirectX::XMVectorSetY(vel, 0.0f)));
  DirectX::XMStoreFloat3(&m_pos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD),
                                                              DirectX::XMLoadFloat3(&m_pos)));

  return (distanceToTarget < 10.0f);
}


bool SporeGenerator::Advance(Unit* _unit)
{
  bool amIDead = Entity::Advance(_unit);

  if (!amIDead && !m_dead)
  {
    switch (m_state)
    {
    case StateIdle:
      amIDead = AdvanceIdle();
      break;
    case StateEggLaying:
      amIDead = AdvanceEggLaying();
      break;
    case StatePanic:
      amIDead = AdvancePanic();
      break;
    }
  }

  return amIDead;
}


bool SporeGenerator::AdvanceIdle()
{
  m_retargetTimer -= SERVER_ADVANCE_PERIOD;
  if (m_retargetTimer <= 0.0f)
  {
    m_retargetTimer = 6.0f;

    bool targetFound = false;
    if (!targetFound)
      targetFound = SearchForSpirits();
    if (!targetFound)
      targetFound = SearchForRandomPos();

    if (m_state != StateIdle)
      return false;
  }


  bool arrived = AdvanceToTargetPosition();

  return false;
}


bool SporeGenerator::AdvanceEggLaying()
{
  //
  // Advance to where we think the spirits are

  bool arrived = AdvanceToTargetPosition();


  //
  // Lay eggs if we are in range

  float distance =
    DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_targetPos), DirectX::XMLoadFloat3(&m_pos))));
  if (distance < 50.0f)
  {
    m_eggTimer -= SERVER_ADVANCE_PERIOD;
    if (m_eggTimer <= 0.0f)
    {
      m_eggTimer = 2.0f + syncfrand(2.0f) - 1.0 * (float)g_difficultyLevel / 10.0f;
      DirectX::XMFLOAT4X4 mat;
      DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

      DirectX::XMFLOAT3 const eggLayPos = m_eggMarker->GetWorldPosition(mat);
      g_location->SpawnEntities(eggLayPos, m_id.GetTeamId(), -1, TypeEgg, 1, m_vel, 0.0f);
      g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "LayEgg");
    }
  }


  //
  // See if enough eggs are present

  if (arrived)
  {
    bool targetFound = false;
    if (!targetFound)
      targetFound = SearchForSpirits();
    if (!targetFound)
      targetFound = SearchForRandomPos();
  }

  return false;
}


void SporeGenerator::RenderTail(DirectX::XMFLOAT3 const& _from, DirectX::XMFLOAT3 const& _to, float _size)
{
  // RenderArrow( _from, _to, _size, RGBAColour(255,50,50,255) );

  DirectX::XMVECTOR const from = DirectX::XMLoadFloat3(&_from);
  DirectX::XMVECTOR const to = DirectX::XMLoadFloat3(&_to);

  DirectX::XMFLOAT3 const cameraPos = g_camera->GetPos();

  // SetLength; rendering only, so this takes the native normalise.
  DirectX::XMVECTOR const lineOurPos =
    DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&cameraPos), from),
                                                                               DirectX::XMVectorSubtract(from, to))),
                           _size);

  DirectX::XMVECTOR const lineVector = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(to, from));
  DirectX::XMVECTOR const right = DirectX::XMVector3Cross(lineVector, DirectX::g_XMIdentityR1);

  DirectX::XMFLOAT3 normal;
  DirectX::XMStoreFloat3(&normal, DirectX::XMVector3Normalize(DirectX::XMVector3Cross(right, lineVector)));
  glNormal3fv(&normal.x);

  EmitVertex(DirectX::XMVectorSubtract(from, lineOurPos));
  EmitVertex(DirectX::XMVectorAdd(from, lineOurPos));
  EmitVertex(DirectX::XMVectorSubtract(to, lineOurPos));
  EmitVertex(DirectX::XMVectorAdd(to, lineOurPos));
}


void SporeGenerator::Render(float _predictionTime)
{
  if (m_dead)
    return;

  //    RenderArrow( m_pos, m_pos+m_front*20.0f, 1.0f );
  //    RenderArrow( m_pos, m_targetPos, 2.0f );
  //    g_editorFont.DrawText3DCentre( m_pos+Vector3(0,50,0), 10.0f, "%d", (int) m_stats[StatHealth] );

  DirectX::XMFLOAT3 predictedPos;
  DirectX::XMStoreFloat3(&predictedPos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(_predictionTime),
                                                                     DirectX::XMLoadFloat3(&m_pos)));

  // The generator is drawn axis-aligned: world front and world up, never its
  // own m_front.
  DirectX::XMMATRIX const basis = BasisFromFrontAndUp(DirectX::g_XMIdentityR2, DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&predictedPos));

  //
  // 3d Shape

  g_renderer->SetObjectLighting();
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);

  DirectX::XMMATRIX damaged = basis;

  if (m_renderDamaged)
  {
    float timeIndex = g_gameTime + m_id.GetUniqueId() * 10;
    float thefrand = frand();

    // r is row 0, u row 1, f row 2, in the row-vector convention.
    if (thefrand > 0.7f)
      damaged.r[2] = DirectX::XMVectorScale(damaged.r[2], 1.0f - sinf(timeIndex) * 0.5f);
    else if (thefrand > 0.4f)
      damaged.r[1] = DirectX::XMVectorScale(damaged.r[1], 1.0f - sinf(timeIndex) * 0.5f);
    else
      damaged.r[0] = DirectX::XMVectorScale(damaged.r[0], 1.0f - sinf(timeIndex) * 0.5f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
  }

  DirectX::XMFLOAT4X4 mat;
  DirectX::XMStoreFloat4x4(&mat, damaged);

  m_shape->Render(_predictionTime, mat);


  //
  // Tails

  glColor4f(0.2f, 0.0f, 0.0f, 1.0f);
  glEnable(GL_COLOR_MATERIAL);

  int numTailParts = 3;
  // Deliberately static: the tail lags the body, and that lag persists between
  // frames. Braced to zero because Vector3's default constructor did it.
  static DirectX::XMFLOAT3 s_vel{0.0f, 0.0f, 0.0f};
  DirectX::XMStoreFloat3(&s_vel, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(0.05f),
                                                              DirectX::XMVectorScale(DirectX::XMLoadFloat3(&s_vel), 0.95f)));

  for (int i = 0; i < SPOREGENERATOR_NUMTAILS; ++i)
  {
    DirectX::XMFLOAT3 prevTailPos = m_tail[i]->GetWorldPosition(mat);
    // HorizontalAndNormalise: flatten to the XZ plane, then normalise.
    DirectX::XMVECTOR prevTailDir = DirectX::XMVector3Normalize(
      DirectX::XMVectorSetY(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&prevTailPos), DirectX::XMLoadFloat3(&predictedPos)), 0.0f));

    glBegin(GL_QUAD_STRIP);

    for (int j = 0; j < numTailParts; ++j)
    {
      DirectX::XMVECTOR const prevTail = DirectX::XMLoadFloat3(&prevTailPos);
      DirectX::XMVECTOR thisTail = DirectX::XMVectorMultiplyAdd(prevTailDir, DirectX::XMVectorReplicate(15.0f), prevTail);

      // HorizontalAndNormalise: flatten to the XZ plane, then normalise.
      prevTailDir = DirectX::XMVector3Normalize(DirectX::XMVectorSetY(DirectX::XMVectorSubtract(thisTail, prevTail), 0.0f));

      float timeIndex = g_gameTime + i + j + m_id.GetUniqueId() * 10;
      thisTail = DirectX::XMVectorAdd(thisTail, DirectX::XMVectorReplicate(sinf(timeIndex) * 10.0f));

      // SetLength on the lagged body velocity, which can be exactly zero on the
      // first frame -- s_vel starts at zero -- so the legacy (len, 0, 0)
      // fallback is kept rather than taking a QNaN into the tail geometry.
      float const trail = 10.0f * (float)j / (float)numTailParts;
      float const velLength = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&s_vel)));
      DirectX::XMVECTOR const vel = NearlyEquals(velLength, 0.0f) ? DirectX::XMVectorSet(trail, 0.0f, 0.0f, 0.0f)
                                                                  : DirectX::XMVectorScale(DirectX::XMLoadFloat3(&s_vel), trail / velLength);
      thisTail = DirectX::XMVectorAdd(thisTail, vel);
      thisTail = DirectX::XMVectorSubtract(thisTail, DirectX::XMVectorSet(0.0f, trail, 0.0f, 0.0f));

      if (m_state == StatePanic)
      {
        float panicFraction = 5.0f;
        thisTail = DirectX::XMVectorAdd(thisTail, DirectX::XMVectorReplicate(cosf(g_gameTime * panicFraction + i + j) * 5.0f));
      }

      DirectX::XMFLOAT3 thisTailPos;
      DirectX::XMStoreFloat3(&thisTailPos, thisTail);

      float size = 1.0f - (float)j / (float)numTailParts;
      size *= 2.0f;
      size += sinf(g_gameTime + i + j) * 0.3f;

      RenderTail(prevTailPos, thisTailPos, size);
      prevTailPos = thisTailPos;
    }

    glEnd();
  }

  glDisable(GL_COLOR_MATERIAL);
  glEnable(GL_CULL_FACE);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  g_renderer->UnsetObjectLighting();

  //
  // Shadow

  BeginRenderShadow();
  RenderShadow(predictedPos, 20.0f);
  EndRenderShadow();
}


bool SporeGenerator::IsInView()
{
  DirectX::XMFLOAT3 centre;
  DirectX::XMStoreFloat3(&centre, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&m_centrePos)));

  return g_camera->SphereInViewFrustum(centre, m_radius);
}


bool SporeGenerator::RenderPixelEffect(float _predictionTime)
{
  Render(_predictionTime);

  DirectX::XMFLOAT3 predictedPos;
  DirectX::XMStoreFloat3(&predictedPos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(_predictionTime),
                                                                     DirectX::XMLoadFloat3(&m_pos)));

  // The generator is drawn axis-aligned: world front and world up, never its
  // own m_front.
  DirectX::XMMATRIX const basis = BasisFromFrontAndUp(DirectX::g_XMIdentityR2, DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&predictedPos));


  //
  // Shape

  DirectX::XMFLOAT4X4 mat;
  DirectX::XMStoreFloat4x4(&mat, basis);
  g_renderer->MarkUsedCells(m_shape, mat);


  //
  // Tails

  int numTailParts = 3;
  // Deliberately static: the tail lags the body, and that lag persists between
  // frames. Braced to zero because Vector3's default constructor did it.
  static DirectX::XMFLOAT3 s_vel{0.0f, 0.0f, 0.0f};
  DirectX::XMStoreFloat3(&s_vel, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(0.05f),
                                                              DirectX::XMVectorScale(DirectX::XMLoadFloat3(&s_vel), 0.95f)));

  for (int i = 0; i < SPOREGENERATOR_NUMTAILS; ++i)
  {
    DirectX::XMFLOAT3 prevTailPos = m_tail[i]->GetWorldPosition(mat);
    // HorizontalAndNormalise: flatten to the XZ plane, then normalise.
    DirectX::XMVECTOR prevTailDir = DirectX::XMVector3Normalize(
      DirectX::XMVectorSetY(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&prevTailPos), DirectX::XMLoadFloat3(&predictedPos)), 0.0f));

    for (int j = 0; j < numTailParts; ++j)
    {
      DirectX::XMVECTOR const prevTail = DirectX::XMLoadFloat3(&prevTailPos);
      DirectX::XMVECTOR thisTail = DirectX::XMVectorMultiplyAdd(prevTailDir, DirectX::XMVectorReplicate(15.0f), prevTail);

      // HorizontalAndNormalise: flatten to the XZ plane, then normalise.
      prevTailDir = DirectX::XMVector3Normalize(DirectX::XMVectorSetY(DirectX::XMVectorSubtract(thisTail, prevTail), 0.0f));

      float timeIndex = g_gameTime + i + j + m_id.GetUniqueId() * 10;
      thisTail = DirectX::XMVectorAdd(thisTail, DirectX::XMVectorReplicate(sinf(timeIndex) * 10.0f));

      // SetLength on the lagged body velocity, which can be exactly zero on the
      // first frame -- s_vel starts at zero -- so the legacy (len, 0, 0)
      // fallback is kept rather than taking a QNaN into the tail geometry.
      float const trail = 10.0f * (float)j / (float)numTailParts;
      float const velLength = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&s_vel)));
      DirectX::XMVECTOR const vel = NearlyEquals(velLength, 0.0f) ? DirectX::XMVectorSet(trail, 0.0f, 0.0f, 0.0f)
                                                                  : DirectX::XMVectorScale(DirectX::XMLoadFloat3(&s_vel), trail / velLength);
      thisTail = DirectX::XMVectorAdd(thisTail, vel);
      thisTail = DirectX::XMVectorSubtract(thisTail, DirectX::XMVectorSet(0.0f, trail, 0.0f, 0.0f));

      if (m_state == StatePanic)
      {
        float panicFraction = 5.0f;
        thisTail = DirectX::XMVectorAdd(thisTail, DirectX::XMVectorReplicate(cosf(g_gameTime * panicFraction + i + j) * 5.0f));
      }

      DirectX::XMFLOAT3 thisTailPos;
      DirectX::XMStoreFloat3(&thisTailPos, thisTail);

      float size = 1.0f - (float)j / (float)numTailParts;
      size *= 2.0f;
      size += sinf(g_gameTime + i + j) * 0.3f;

      DirectX::XMFLOAT3 pos;
      DirectX::XMStoreFloat3(&pos, DirectX::XMVectorScale(DirectX::XMVectorAdd(prevTail, thisTail), 0.5f));
      float const diameter = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(prevTail, thisTail)));
      g_renderer->RasteriseSphere(pos, diameter * 0.5f);
      prevTailPos = thisTailPos;
    }
  }

  return true;
}


void SporeGenerator::ListSoundEvents(std::vector<const char*>* _list)
{
  Entity::ListSoundEvents(_list);

  _list->push_back("LayEgg");
}
