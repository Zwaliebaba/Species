#include "pch.h"
#include "Resource.h"
#include "Debug.h"
#include "DebugRender.h"
#include "MathUtils.h"
#include "Shape.h"

#include "Location.h"
#include "ParticleSystem.h"
#include "Team.h"
#include "Unit.h"

#include "Lander.h"
#include "WorldPointers.h"


namespace Species
{
  Lander::Lander()
    : Entity(),
      m_state(StateSailing),
      m_spawnTimer(0.0f)
  {
    m_type = Entity::TypeLander;

    m_shape = g_resource->GetShape("Lander.shp");
    DEBUG_ASSERT(m_shape);
  }

  bool Lander::Advance(Unit* _unit)
  {
    m_front = DirectX::XMFLOAT3(-1.0f, 0.0f, 0.0f);

    if (m_dead)
    {
      bool amIDead = AdvanceDead(_unit);
      if (amIDead)
      {
        g_location->Bang(m_pos, 30.0f, 50.0f);
        return true;
      }
    }

    switch (m_state)
    {
    case StateSailing:
      return AdvanceSailing();
      break;
    case StateLanded:
      return AdvanceLanded();
      break;
    }

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

  void Lander::ChangeHealth(int amount) { g_particleSystem->CreateParticle(m_pos, g_zeroVector, Particle::TypeMuzzleFlash); }

  bool Lander::AdvanceSailing()
  {
    DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_front), m_stats[StatSpeed]));
    DirectX::XMStoreFloat3(&m_pos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD),
                                                                DirectX::XMLoadFloat3(&m_pos)));

    float groundLevel = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);
    m_pos.y = groundLevel;
    if (m_pos.y < 0.0f)
      m_pos.y = 0.0f;

    if (groundLevel > 0.0f)
    {
      m_vel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
      m_state = StateLanded;
    }

    return false;
  }

  bool Lander::AdvanceLanded()
  {
    m_vel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

    m_spawnTimer -= SERVER_ADVANCE_PERIOD;
    if (m_spawnTimer <= 0.0f)
    {
      int unitId;
      int numToSpawn = syncfrand(2.0f) + 2.0f;
      Unit* unit = g_location->m_teams[0].NewUnit(Entity::TypeLaserTroop, numToSpawn, &unitId, m_pos);
      g_location->SpawnEntities(m_pos, m_id.GetTeamId(), unitId, Entity::TypeLaserTroop, numToSpawn, g_zeroVector, 0);

      DirectX::XMFLOAT3 const offset(0.0f, 0.0f, syncsfrand(200.0f));
      DirectX::XMFLOAT3 wayPoint;
      DirectX::XMStoreFloat3(&wayPoint,
                             DirectX::XMVectorAdd(DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_front), DirectX::XMVectorReplicate(750.0f),
                                                                               DirectX::XMLoadFloat3(&m_pos)),
                                                  DirectX::XMLoadFloat3(&offset)));
      unit->SetWayPoint(wayPoint);

      m_spawnTimer = m_stats[StatRate];
    }

    return false;
  }

  void Lander::Render(float predictionTime, int teamId)
  {
    //
    // Work out our predicted position and orientation

    DirectX::XMFLOAT3 predictedPos;
    DirectX::XMStoreFloat3(&predictedPos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(predictionTime),
                                                                       DirectX::XMLoadFloat3(&m_pos)));

    // SurfaceMap2D<Vector3> still returns a Vector3 -- Landscape belongs to T18.
    DirectX::XMFLOAT3 const landUp = g_location->m_landscape.m_normalMap->GetValue(predictedPos.x, predictedPos.z);

    DirectX::XMVECTOR const entityFront = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&m_front));
    DirectX::XMVECTOR const entityRight = DirectX::XMVector3Cross(entityFront, DirectX::XMLoadFloat3(&landUp));
    DirectX::XMVECTOR const entityUp = DirectX::XMVector3Cross(entityRight, entityFront);

    if (!m_dead)
    {
      RGBAColour colour = g_location->m_teams[teamId].m_colour;

      if (m_reloading > 0.0f)
      {
        colour.r = (colour.r == 255 ? 255 : 100);
        colour.g = (colour.g == 255 ? 255 : 100);
        colour.b = (colour.b == 255 ? 255 : 100);
      }
      glColor3ubv(colour.GetData());

      //
      // 3d Shape

      g_renderer->SetObjectLighting();

      glEnable(GL_CULL_FACE);
      glDisable(GL_TEXTURE_2D);
      glEnable(GL_COLOR_MATERIAL);
      glDisable(GL_BLEND);

      DirectX::XMFLOAT4X4 mat;
      DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(entityFront, entityUp, DirectX::XMLoadFloat3(&predictedPos)));
      m_shape->Render(predictionTime, mat);

      glEnable(GL_BLEND);
      glDisable(GL_COLOR_MATERIAL);
      glEnable(GL_TEXTURE_2D);
      g_renderer->UnsetObjectLighting();
      glEnable(GL_CULL_FACE);
    }
  }
} // namespace Species
