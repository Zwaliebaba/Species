#include "pch.h"
#include "GlVertex.h"
#include "DebugRender.h"
#include "MathUtils.h"

#include "Location.h"
#include "Team.h"
#include "Unit.h"

#include "LaserTrooper.h"
#include "Resource.h"
#include "WorldPointers.h"


// *** Advance

/*
bool LaserTrooper::Advance(Unit *_unit)
{
    Vector3 oldPos = m_pos;
    bool aiControlled = (g_location->m_teams[ m_teamId ].m_teamType == Team::TeamTypeAI);

    if( aiControlled )
    {
        m_obeyUnit -= syncfrand(100.0f) * SERVER_ADVANCE_PERIOD;
        if( m_obeyUnit < -100.0f )
        {
            m_obeyUnit = 100.0f;
        }
    }

    if( !m_dead && m_onGround && m_inWater < 0.0f )
    {
        Vector3 targetPos = _unit->GetWayPoint();
        targetPos.y = 0.0f;
        Vector3 offset = _unit->GetOffset(Unit::FormationRectangle, m_formationIndex );
        targetPos += offset;
        targetPos.y = g_location->m_landscape.m_heightMap->GetValue( targetPos.x, targetPos.z );
        targetPos = PushFromObstructions( targetPos );
        //targetPos = PushFromEachOther( targetPos );

        m_targetPos = targetPos;

//		Vector3 targetPos = _unit->GetLeadVector(&m_wayPointId, 0.0f, m_pos);
        Vector3 targetVector = targetPos - m_pos;
        //targetVector = PushFromEachOther( targetVector );
        targetVector.y = 0.0f;

        if( !aiControlled || m_obeyUnit != 0.0f )
        {
      float distance = targetVector.Mag();

      // If moving towards way point...
      if (distance > 0.01f)
      {
        m_wobble += SERVER_ADVANCE_PERIOD * 15.0f;
        if (m_wobble > M_PI * 2.0f) m_wobble -= M_PI * 2.0f;

        m_vel = targetVector / distance;
        m_vel = PushFromEachOther( m_vel );
                m_vel.Normalise();
                m_vel *= m_stats[StatSpeed];

                //
                // Slow us down if we're going up hill
                // Speed up if going down hill

                Vector3 nextPos = m_pos + m_vel * SERVER_ADVANCE_PERIOD;
                nextPos.y = g_location->m_landscape.m_heightMap->GetValue(nextPos.x, nextPos.z);
                float currentHeight = g_location->m_landscape.m_heightMap->GetValue( m_pos.x, m_pos.z );
                float nextHeight = g_location->m_landscape.m_heightMap->GetValue( nextPos.x, nextPos.z );
                float factor = 1.0f - (currentHeight - nextHeight) / -3.0f;
                if( factor < 0.01f ) factor = 0.01f;
                if( factor > 2.0f ) factor = 2.0f;
                m_vel *= factor;

        if (distance < m_stats[StatSpeed] * SERVER_ADVANCE_PERIOD)
        {
          m_vel.SetLength(distance / SERVER_ADVANCE_PERIOD);
        }
        m_pos += m_vel * SERVER_ADVANCE_PERIOD;
        m_pos.y = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z) + 0.1f;

                if( targetVector.Mag() < 1.0f )
                {
                    targetVector = _unit->GetWayPoint() - m_pos;
                }

        targetVector.HorizontalAndNormalise();
        m_angVel = (targetVector ^ m_front) * 8.0f;
        float const maxTurnRate = 10.0f; // radians per second
        if (m_angVel.Mag() > maxTurnRate)
        {
          m_angVel.SetLength(maxTurnRate);
        }
        m_front.RotateAround(m_angVel * SERVER_ADVANCE_PERIOD);
      }
      else // otherwise we are standing on the spot
      {
        Team *team = &g_location->m_teams[m_teamId];

        // Does this entity belong to the currently selectedUnit
        if (m_unitId == team->m_currentUnitId)
        {
          Vector3 toMouse = g_userInput->GetMousePos3d() - m_pos;
          toMouse.HorizontalAndNormalise();
          m_angVel = (toMouse ^ m_front) * 4.0f;
        }
        else
        {
          m_angVel = (Vector3(1,0,0) ^ m_front) * 4.0f;
        }

        float const maxTurnRate = 10.0f; // radians per second
        if (m_angVel.Mag() > maxTurnRate)
        {
          m_angVel.SetLength(maxTurnRate);
        }
        m_front.RotateAround(m_angVel * SERVER_ADVANCE_PERIOD);
        m_front.Normalise();
                m_vel.Zero();
      }
        }
        else
        {
            m_vel.Zero();
        }

        if( m_pos.y <= 0.0f )
        {
            m_inWater = syncfrand(3.0f);
        }
    }

  if( !m_onGround ) AdvanceInAir(_unit);

    bool enteredTeleport = EnterTeleports();
    if( enteredTeleport )
    {
        return true;
    }

    return Entity::Advance( _unit );
}
*/

void LaserTrooper::Begin()
{
  Entity::Begin();

  m_victoryDance = -1.0f;
}


bool LaserTrooper::Advance(Unit* _unit)
{
  // Was `m_targetPos == g_zeroVector`, which is Vector3::operator== -- a
  // PER-COMPONENT NearlyEquals at 1e-6, not an exact comparison. XMVector3Equal
  // would be a different test; XMVector3NearEqual with 1e-6 in every lane is
  // the same one.
  if (DirectX::XMVector3NearEqual(DirectX::XMLoadFloat3(&m_targetPos), DirectX::XMVectorZero(), DirectX::XMVectorReplicate(1e-6f)))
    m_targetPos = m_pos;

  if (m_enabled && m_onGround && !m_dead)
  {
    DirectX::XMVECTOR const toTarget = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_targetPos), DirectX::XMLoadFloat3(&m_pos));
    DirectX::XMVECTOR const movementDir = DirectX::XMVector3Normalize(toTarget);
    float distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(toTarget));
    float speed = m_stats[StatSpeed];
    if (speed * SERVER_ADVANCE_PERIOD > distance)
      speed = distance / SERVER_ADVANCE_PERIOD;

    DirectX::XMVECTOR const velocity = DirectX::XMVectorScale(movementDir, speed);
    DirectX::XMStoreFloat3(&m_vel, velocity);
    DirectX::XMStoreFloat3(&m_pos,
                           DirectX::XMVectorMultiplyAdd(velocity, DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD), DirectX::XMLoadFloat3(&m_pos)));
    DirectX::XMStoreFloat3(&m_front, DirectX::XMVector3Normalize(velocity));

    if (EnterTeleports())
      return true;
  }

  if (!m_onGround)
    AdvanceInAir(_unit);
  if (m_inWater != -1)
    AdvanceInWater(_unit);

  if (m_reloading > 0.0f)
  {
    m_reloading -= SERVER_ADVANCE_PERIOD;
    if (m_reloading < 0.0f)
      m_reloading = 0.0f;
  }

  if (m_dead)
  {
    bool amIDead = AdvanceDead(_unit);
    if (amIDead)
      return true;
  }

  if (m_victoryDance != -1.0f)
  {
    AdvanceVictoryDance();
  }

  _unit->UpdateEntityPosition(m_pos, m_radius);

  return false;
}


void LaserTrooper::AdvanceVictoryDance()
{
  if (syncfrand(100.0f) < 1.0f)
  {
    m_vel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_vel.y += 10.0f + syncfrand(10.0f);
    m_onGround = false;
  }
}

void LaserTrooper::Render(float predictionTime, int teamId)
{
  if (!m_enabled)
    return;

  //
  // Work out our predicted position and orientation

  DirectX::XMFLOAT3 predictedPos;
  DirectX::XMStoreFloat3(&predictedPos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(predictionTime),
                                                                     DirectX::XMLoadFloat3(&m_pos)));
  if (m_onGround && m_inWater == -1)
  {
    predictedPos.y = g_location->m_landscape.m_heightMap->GetValue(predictedPos.x, predictedPos.z);
  }

  float size = 2.0f;

  DirectX::XMFLOAT3 const landNormal = g_location->m_landscape.m_normalMap->GetValue(predictedPos.x, predictedPos.z);
  DirectX::XMVECTOR entityUp = DirectX::XMLoadFloat3(&landNormal);
  //	float wobble = m_wobble;
  //    if( !m_onGround ) wobble = 0.0f;
  //	if ( m_vel.Mag() > 0.01f )
  //	{
  //		wobble += predictionTime * 15.0f;
  //	}
  //	entityUp.FastRotateAround(m_front, sinf(wobble) * 0.1f);
  entityUp = DirectX::XMVector3Normalize(entityUp);

  // RotateAround's angle is the vector's magnitude, and it did nothing below
  // 1e-8 squared -- reproduced, or a stationary trooper rotates by a NaN.
  DirectX::XMVECTOR entityFront = DirectX::XMLoadFloat3(&m_front);
  DirectX::XMVECTOR const rotation = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_angVel), predictionTime);
  float const rotationLengthSquared = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(rotation));
  if (rotationLengthSquared >= 1e-8f)
  {
    float const angle = sqrtf(rotationLengthSquared);
    entityFront = DirectX::XMVector3Transform(entityFront, DirectX::XMMatrixRotationAxis(DirectX::XMVectorScale(rotation, 1.0f / angle), angle));
  }

  //	Matrix34 transform(entityFront, entityUp, predictedPos);
  //	Shape *aShape = g_resource->GetShape("LaserTroop.shp");
  //	aShape->Render(predictionTime, transform);

  DirectX::XMVECTOR entityRight = DirectX::XMVector3Cross(entityFront, entityUp);
  entityFront = DirectX::XMVectorScale(entityFront, 0.2f);
  entityUp = DirectX::XMVectorScale(entityUp, size * 2.0f);
  entityRight = DirectX::XMVectorScale(DirectX::XMVector3Normalize(entityRight), size);

  if (m_justFired)
  {
    m_justFired = false;
  }

  // glDisable( GL_TEXTURE_2D );
  // RenderSphere( m_targetPos, 1.0f, RGBAColour(255,255,255,100) );
  // RenderSphere( m_unitTargetPos, 1.1f, RGBAColour(255,155,155,200) );
  // glEnable( GL_TEXTURE_2D );

  if (!m_dead)
  {
    RGBAColour colour = g_location->m_teams[teamId].m_colour;

    if (m_reloading > 0.0f)
    {
      colour.r *= 0.7f;
      colour.g *= 0.7f;
      colour.b *= 0.7f;
    }

    //
    // Draw our texture

    float maxHealth = EntityBlueprint::GetStat(m_type, StatHealth);
    float health = (float)m_stats[StatHealth] / maxHealth;
    if (health > 1.0f)
      health = 1.0f;

    colour *= 0.5f + 0.5f * health;
    glColor3ubv(colour.GetData());
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f);
    EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&predictedPos), entityRight), entityUp));
    glTexCoord2f(1.0f, 1.0f);
    EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&predictedPos), entityRight), entityUp));
    glTexCoord2f(1.0f, 0.0f);
    EmitVertex(DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&predictedPos), entityRight));
    glTexCoord2f(0.0f, 0.0f);
    EmitVertex(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&predictedPos), entityRight));
    glEnd();


    //
    // Draw our shadow on the landscape

    if (m_onGround)
    {
      glColor4ub(0, 0, 0, 90);
      DirectX::XMVECTOR const centre = DirectX::XMLoadFloat3(&predictedPos);
      DirectX::XMVECTOR const forward = DirectX::XMVectorSet(0.0f, 0.0f, size * 2.0f, 0.0f);

      DirectX::XMFLOAT3 pos1;
      DirectX::XMFLOAT3 pos2;
      DirectX::XMFLOAT3 pos3;
      DirectX::XMFLOAT3 pos4;
      DirectX::XMStoreFloat3(&pos1, DirectX::XMVectorSubtract(centre, entityRight));
      DirectX::XMStoreFloat3(&pos2, DirectX::XMVectorAdd(centre, entityRight));
      DirectX::XMStoreFloat3(&pos4, DirectX::XMVectorAdd(DirectX::XMVectorSubtract(centre, entityRight), forward));
      DirectX::XMStoreFloat3(&pos3, DirectX::XMVectorAdd(DirectX::XMVectorAdd(centre, entityRight), forward));

      pos1.y = 0.2f + g_location->m_landscape.m_heightMap->GetValue(pos1.x, pos1.z);
      pos2.y = 0.2f + g_location->m_landscape.m_heightMap->GetValue(pos2.x, pos2.z);
      pos3.y = 0.2f + g_location->m_landscape.m_heightMap->GetValue(pos3.x, pos3.z);
      pos4.y = 0.2f + g_location->m_landscape.m_heightMap->GetValue(pos4.x, pos4.z);

      glLineWidth(1.0f);
      glBegin(GL_QUADS);
      glTexCoord2f(0.0f, 0.0f);
      glVertex3fv(&pos1.x);
      glTexCoord2f(1.0f, 0.0f);
      glVertex3fv(&pos2.x);
      glTexCoord2f(1.0f, 1.0f);
      glVertex3fv(&pos3.x);
      glTexCoord2f(0.0f, 1.0f);
      glVertex3fv(&pos4.x);
      glEnd();
    }


    //
    // Draw a line through us if we are side-on with the camera

    // operator* between two Vector3s was the DOT product, not a scale.
    DirectX::XMFLOAT3 const camFrontStore = g_camera->GetFront();
    float alpha = 1.0f - fabsf(DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMLoadFloat3(&camFrontStore), DirectX::XMLoadFloat3(&m_front))));
    if (alpha > 0.5f)
    {
      // colour.a = 255;
      colour.a = 255 * alpha;
      glBlendFunc(GL_SRC_ALPHA, GL_ONE);
      glColor4ubv(colour.GetData());
      glBegin(GL_QUADS);
      glTexCoord2f(0.0f, 1.0f);
      EmitVertex(DirectX::XMVectorAdd(
        DirectX::XMVectorNegativeMultiplySubtract(entityFront, DirectX::XMVectorReplicate(1.5f), DirectX::XMLoadFloat3(&predictedPos)), entityUp));
      glTexCoord2f(1.0f, 1.0f);
      EmitVertex(DirectX::XMVectorAdd(
        DirectX::XMVectorMultiplyAdd(entityFront, DirectX::XMVectorReplicate(1.5f), DirectX::XMLoadFloat3(&predictedPos)), entityUp));
      glTexCoord2f(1.0f, 0.0f);
      EmitVertex(DirectX::XMVectorMultiplyAdd(entityFront, DirectX::XMVectorReplicate(1.5f), DirectX::XMLoadFloat3(&predictedPos)));
      glTexCoord2f(0.0f, 0.0f);
      EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(entityFront, DirectX::XMVectorReplicate(1.5f), DirectX::XMLoadFloat3(&predictedPos)));
      glEnd();
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
  }
  else
  {
    entityFront = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&m_front));
    entityUp = DirectX::g_XMIdentityR1;
    entityRight = DirectX::XMVector3Cross(entityFront, entityUp);
    entityUp = DirectX::XMVectorScale(entityUp, size * 2.0f);
    entityRight = DirectX::XMVectorScale(DirectX::XMVector3Normalize(entityRight), size);
    unsigned char alpha = (float)m_stats[StatHealth] * 2.55f;

    glColor4ub(0, 0, 0, alpha);

    entityRight = DirectX::XMVectorScale(entityRight, 0.5f);
    entityUp = DirectX::XMVectorScale(entityUp, 0.5f);
    float predictedHealth = m_stats[StatHealth];
    if (m_onGround)
      predictedHealth -= 40 * predictionTime;
    else
      predictedHealth -= 20 * predictionTime;
    float landHeight = g_location->m_landscape.m_heightMap->GetValue(predictedPos.x, predictedPos.z);

    for (int i = 0; i < 3; ++i)
    {
      DirectX::XMFLOAT3 fragmentPos = predictedPos;
      if (i == 0)
        fragmentPos.x += 10.0f - predictedHealth / 10.0f;
      if (i == 1)
        fragmentPos.z += 10.0f - predictedHealth / 10.0f;
      if (i == 2)
        fragmentPos.x -= 10.0f - predictedHealth / 10.0f;
      fragmentPos.y += (fragmentPos.y - landHeight) * i * 0.5f;


      float left = 0.0f;
      float right = 1.0f;
      float top = 1.0f;
      float bottom = 0.0f;

      if (i == 0)
      {
        right -= (right - left) / 2;
        bottom -= (bottom - top) / 2;
      }
      if (i == 1)
      {
        left += (right - left) / 2;
        bottom -= (bottom - top) / 2;
      }
      if (i == 2)
      {
        top += (bottom - top) / 2;
        left += (right - left) / 2;
      }

      glBegin(GL_QUADS);
      glTexCoord2f(left, bottom);
      EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&fragmentPos), entityRight), entityUp));
      glTexCoord2f(right, bottom);
      EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&fragmentPos), entityRight), entityUp));
      glTexCoord2f(right, top);
      EmitVertex(DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&fragmentPos), entityRight));
      glTexCoord2f(left, top);
      EmitVertex(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&fragmentPos), entityRight));
      glEnd();
    }
  }
}
