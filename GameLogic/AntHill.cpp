#include "pch.h"
#include "SoundSources.h"

#include "DebugRender.h"
#include "FileWriter.h"
#include "HiResTime.h"
#include "MathUtils.h"
#include "Profiler.h"
#include "Resource.h"
#include "TextStreamReaders.h"
#include "TextRenderer.h"

#include "Location.h"
#include "EntityGrid.h"
#include "Explosion.h"
#include "Team.h"
#include "GameTime.h"

#include "AntHill.h"
#include "ArmyAnt.h"
#include "Citizen.h"
#include "SpawnPoint.h"

#include "SoundSystem.h"
#include "WorldPointers.h"
#include "AppState.h"


namespace Species
{
  AntHill::AntHill()
    : Building(),
      m_objectiveTimer(0),
      m_spawnTimer(0),
      m_numAntsInside(0),
      m_numSpiritsInside(0),
      m_eggConvertTimer(0),
      m_health(100),
      m_unitId(-1),
      m_populationLock(-1),
      m_renderDamaged(false)
  {
    m_type = TypeAntHill;

    SetShape(g_resource->GetShape("AntHill.shp"));
  }


  void AntHill::Initialise(Building* _template)
  {
    Building::Initialise(_template);

    m_numAntsInside = ((AntHill*)_template)->m_numAntsInside;

    m_spawnTimer = GetHighResTime() + 5.0f;
  }


  void AntHill::Damage(float _damage)
  {
    Building::Damage(_damage);

    if (m_health > 0)
    {
      int healthBandBefore = int(m_health / 20.0f);
      m_health += _damage;
      int healthBandAfter = int(m_health / 20.0f);

      if (healthBandAfter != healthBandBefore)
      {
        DirectX::XMFLOAT4X4 mat;
        DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));
        g_explosionManager.AddExplosion(m_shape, mat, 1.0f - (float)m_health / 100.0f);
        g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), "Damage");
      }

      if (m_health <= 0)
      {
        DirectX::XMFLOAT4X4 mat;
        DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));
        g_explosionManager.AddExplosion(m_shape, mat);

        int numSpirits = m_numAntsInside + m_numSpiritsInside;
        for (int i = 0; i < numSpirits; ++i)
        {
          // The three syncfrand calls stay in this order: they advance the
          // synchronised RNG, and the migration may not change the sequence.
          float radius = syncfrand(20.0f);
          float theta = syncfrand(M_PI * 2);
          DirectX::XMFLOAT3 const pos(m_pos.x + radius * sinf(theta), m_pos.y, m_pos.z + radius * cosf(theta));

          // SetLength on (pos - m_pos), which is (radius*sin, 0, radius*cos).
          // radius can be zero -- syncfrand's range is inclusive of it -- and the
          // legacy fallback then left (len, 0, 0), a real direction. The native
          // normalise would give QNaN and spawn a spirit that never moves, so
          // this reproduces the fallback rather than taking the divergence.
          DirectX::XMVECTOR const offset = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&pos), DirectX::XMLoadFloat3(&m_pos));
          float const offsetLength = DirectX::XMVectorGetX(DirectX::XMVector3Length(offset));
          float const speed = syncfrand(50.0f);
          DirectX::XMFLOAT3 vel;
          DirectX::XMStoreFloat3(&vel, NearlyEquals(offsetLength, 0.0f) ? DirectX::XMVectorSet(speed, 0.0f, 0.0f, 0.0f)
                                                                        : DirectX::XMVectorScale(offset, speed / offsetLength));

          g_location->SpawnSpirit(pos, vel, m_id.GetTeamId(), WorldObjectId());
        }

        g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), "Explode");
        m_health = 0;
      }
    }
  }

  void AntHill::Destroy(float _intensity)
  {
    Building::Destroy(_intensity);
    m_health = 0;
  }

  bool AntHill::SearchingArea(DirectX::XMFLOAT3 _pos)
  {
    for (int i = 0; i < static_cast<int>(m_objectives.size()); ++i)
    {
      AntObjective* obj = m_objectives[i];
      float distance =
        DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&obj->m_pos), DirectX::XMLoadFloat3(&_pos))));
      if (distance < 20.0f)
        return true;
    }

    return false;
  }


  bool AntHill::TargettedEntity(WorldObjectId _id)
  {
    for (int i = 0; i < static_cast<int>(m_objectives.size()); ++i)
    {
      AntObjective* obj = m_objectives[i];
      if (obj->m_targetId == _id)
        return true;
    }

    return false;
  }


  bool AntHill::SearchForSpirits(DirectX::XMFLOAT3& _pos)
  {
    for (int i = 0; i < g_location->m_spirits.Size(); ++i)
    {
      if (g_location->m_spirits.ValidIndex(i))
      {
        Spirit* s = g_location->m_spirits.GetPointer(i);
        float theDist =
          DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&s->m_pos), DirectX::XMLoadFloat3(&m_pos))));

        if (theDist <= ANTHILL_SEARCHRANGE && !SearchingArea(s->m_pos) && (s->m_state == Spirit::StateBirth || s->m_state == Spirit::StateFloating))
        {
          _pos = s->m_pos;
          return true;
        }
      }
    }

    return false;
  }


  bool AntHill::SearchForCitizens(DirectX::XMFLOAT3& _pos, WorldObjectId& _id)
  {
    int numFound;
    WorldObjectId* ids = g_location->m_entityGrid->GetEnemies(m_pos.x, m_pos.z, ANTHILL_SEARCHRANGE, &numFound, m_id.GetTeamId());

    for (int i = 0; i < numFound; ++i)
    {
      WorldObjectId id = ids[i];
      Entity* entity = g_location->GetEntity(id);
      if (entity && entity->m_type == Entity::TypeCitizen)
      {
        Citizen* citizen = (Citizen*)entity;
        float theDist = DirectX::XMVectorGetX(
          DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&entity->m_pos), DirectX::XMLoadFloat3(&m_pos))));

        if (theDist <= ANTHILL_SEARCHRANGE && citizen->m_state != Citizen::StateCapturedByAnt && !TargettedEntity(entity->m_id))
        {
          _pos = entity->m_pos;
          _id = entity->m_id;
          return true;
        }
      }
    }

    return false;
  }


  bool AntHill::SearchForEnemies(DirectX::XMFLOAT3& _pos, WorldObjectId& _id)
  {
    int numFound;
    WorldObjectId* ids = g_location->m_entityGrid->GetEnemies(m_pos.x, m_pos.z, ANTHILL_SEARCHRANGE, &numFound, m_id.GetTeamId());

    for (int i = 0; i < numFound; ++i)
    {
      WorldObjectId id = ids[i];
      Entity* entity = g_location->GetEntity(id);

      float theDist = DirectX::XMVectorGetX(
        DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&entity->m_pos), DirectX::XMLoadFloat3(&m_pos))));

      if (theDist <= ANTHILL_SEARCHRANGE && !TargettedEntity(entity->m_id))
      {
        _pos = entity->m_pos;
        _id = entity->m_id;
        return true;
      }
    }

    return false;
  }


  bool AntHill::SearchForScoutArea(DirectX::XMFLOAT3& _pos)
  {
    DirectX::XMFLOAT3 scoutPos = m_pos;
    float radius = ANTHILL_SEARCHRANGE / 2.0f + syncfrand(ANTHILL_SEARCHRANGE / 2.0f);
    float theta = syncfrand(M_PI * 2);
    scoutPos.x += radius * sinf(theta);
    scoutPos.z += radius * cosf(theta);
    scoutPos.y = g_location->m_landscape.m_heightMap->GetValue(scoutPos.x, scoutPos.z);

    if (scoutPos.y > 0)
    {
      _pos = scoutPos;
      return true;
    }

    return false;
  }


  bool AntHill::PopulationLocked()
  {
    //
    // If we haven't yet looked up a nearby Pop lock,
    // do so now

    if (m_populationLock == -1)
    {
      m_populationLock = -2;

      for (int i = 0; i < g_location->m_buildings.Size(); ++i)
      {
        if (g_location->m_buildings.ValidIndex(i))
        {
          Building* building = g_location->m_buildings[i];
          if (building && building->m_type == TypeSpawnPopulationLock)
          {
            SpawnPopulationLock* lock = (SpawnPopulationLock*)building;
            float distance = DirectX::XMVectorGetX(
              DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&building->m_pos), DirectX::XMLoadFloat3(&m_pos))));
            if (distance < lock->m_searchRadius)
            {
              m_populationLock = lock->m_id.GetUniqueId();
              break;
            }
          }
        }
      }
    }


    //
    // If we are inside a Population Lock, query it now

    if (m_populationLock > 0)
    {
      SpawnPopulationLock* lock = (SpawnPopulationLock*)g_location->GetBuilding(m_populationLock);
      if (lock && m_id.GetTeamId() != 255 && lock->m_teamCount[m_id.GetTeamId()] >= lock->m_maxPopulation)
      {
        return true;
      }
    }


    return false;
  }


  bool AntHill::Advance()
  {
    Building::Advance();

    //
    // Is the world awake yet ?

    if (!g_location)
      return false;
    if (!g_location->m_teams)
      return false;
    if (g_location->m_teams[m_id.GetTeamId()].m_teamType != Team::TeamTypeCPU)
      return false;


    bool popLocked = PopulationLocked();

    //
    // Is it time to look for a new objective?

    if (!popLocked && GetHighResTime() > m_objectiveTimer && static_cast<int>(m_objectives.size()) < 3)
    {
      DirectX::XMFLOAT3 targetPos{0.0f, 0.0f, 0.0f};
      WorldObjectId targetId;
      bool targetFound = false;

      if (!targetFound)
        targetFound = SearchForCitizens(targetPos, targetId);
      if (!targetFound)
        targetFound = SearchForEnemies(targetPos, targetId);
      if (!targetFound)
        targetFound = SearchForSpirits(targetPos);
      if (!targetFound)
        targetFound = SearchForScoutArea(targetPos);

      if (targetFound)
      {
        AntObjective* objective = new AntObjective();
        objective->m_pos = targetPos;
        objective->m_targetId = targetId;

        objective->m_numToSend = 5 + 5 * (g_difficultyLevel / 10.0);
        m_objectives.push_back(objective);
      }

      m_objectiveTimer = GetHighResTime() + syncrand() % 5;
    }


    //
    // Send out ants to our existing objectives

    if (!popLocked && static_cast<int>(m_objectives.size()) > 0 && GetHighResTime() > m_spawnTimer && m_numAntsInside > 0)
    {
      Unit* unit = g_location->GetUnit(WorldObjectId(m_id.GetTeamId(), m_unitId, -1, -1));
      if (!unit)
      {
        unit = g_location->m_teams[m_id.GetTeamId()].NewUnit(Entity::TypeArmyAnt, m_numAntsInside, &m_unitId, m_pos);
      }

      int chosenIndex = syncrand() % static_cast<int>(m_objectives.size());
      AntObjective* objective = m_objectives[chosenIndex];
      DirectX::XMFLOAT3 spawnPos = m_pos;
      spawnPos.y = g_location->m_landscape.m_heightMap->GetValue(spawnPos.x, spawnPos.z);

      WorldObjectId spawnedId = g_location->SpawnEntities(spawnPos, m_id.GetTeamId(), m_unitId, Entity::TypeArmyAnt, 1, g_zeroVector, 10.0f);
      ArmyAnt* ant = (ArmyAnt*)g_location->GetEntity(spawnedId);

      ant->m_buildingId = m_id.GetUniqueId();
      DirectX::XMStoreFloat3(
        &ant->m_front, DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&ant->m_pos), DirectX::XMLoadFloat3(&m_pos))));
      ant->m_orders = ArmyAnt::ScoutArea;
      ant->m_wayPoint = objective->m_pos;
      ant->m_targetId = objective->m_targetId;
      float radius = syncfrand(30.0f);
      float theta = syncfrand(M_PI * 2);
      ant->m_wayPoint.x += radius * sinf(theta);
      ant->m_wayPoint.z += radius * cosf(theta);
      ant->m_wayPoint = ant->PushFromObstructions(ant->m_wayPoint);
      ant->m_wayPoint.y = g_location->m_landscape.m_heightMap->GetValue(ant->m_wayPoint.x, ant->m_wayPoint.z);

      m_numAntsInside--;

      objective->m_numToSend--;
      if (objective->m_numToSend <= 0)
      {
        m_objectives.erase(m_objectives.begin() + chosenIndex);
      }

      m_spawnTimer = GetHighResTime() + 0.2f - (0.2 * g_difficultyLevel / 10.0);
    }


    //
    // Convert spirits into ants

    if (m_numSpiritsInside > 0 && GetHighResTime() > m_eggConvertTimer)
    {
      m_numSpiritsInside--;
      m_numAntsInside += 3;

      m_eggConvertTimer = GetHighResTime() + 5.0f;
    }


    //
    // Flicker if we are damaged

    float healthFraction = (float)m_health / 100.0f;
    float timeIndex = g_gameTime + m_id.GetUniqueId() * 10;
    m_renderDamaged = (frand(0.75f) * (1.0f - fabs(sinf(timeIndex)) * 1.2f) > healthFraction);


    return (m_health <= 0);
  }


  void AntHill::Render(float _predictionTime)
  {
    DirectX::XMFLOAT4X4 const worldMat = GetWorldMatrix();
    DirectX::XMMATRIX basis = DirectX::XMLoadFloat4x4(&worldMat);

    //
    // If we are damaged, flicked in and out based on our health

    if (m_renderDamaged)
    {
      float timeIndex = g_gameTime + m_id.GetUniqueId() * 10;
      float thefrand = frand();

      // r is row 0, u row 1, f row 2, in the row-vector convention.
      if (thefrand > 0.7f)
        basis.r[2] = DirectX::XMVectorScale(basis.r[2], 1.0f - sinf(timeIndex) * 0.7f);
      else if (thefrand > 0.4f)
        basis.r[1] = DirectX::XMVectorScale(basis.r[1], 1.0f - sinf(timeIndex) * 0.5f);
      else
        basis.r[0] = DirectX::XMVectorScale(basis.r[0], 1.0f - sinf(timeIndex) * 0.7f);
      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ONE);
    }

    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(&mat, basis);

    m_shape->Render(_predictionTime, mat);

    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


#ifdef DEBUG_RENDER_ENABLED
  // g_editorFont.DrawText3DCentre( m_pos + Vector3(0,140,0), 20, "Health %d", m_health );
  // g_editorFont.DrawText3DCentre( m_pos + Vector3(0,120,0), 20, "%d Ants Inside", m_numAntsInside );
  // g_editorFont.DrawText3DCentre( m_pos + Vector3(0,100,0), 20, "%d Spirits Inside", m_numSpiritsInside );
//    for( int i = 0; i < static_cast<int>(m_objectives.size()); ++i )
//    {
//        AntObjective *objective = m_objectives[i];
//        RenderSphere( objective->m_pos, 5.0f );
//    }
#endif
  }


void AntHill::ListSoundEvents(std::vector<const char*>* _list)
{
  Building::ListSoundEvents(_list);

  _list->push_back("Explode");
}


void AntHill::Read(TextReader* _in, bool _dynamic)
{
  Building::Read(_in, _dynamic);

  m_numAntsInside = atoi(_in->GetNextToken());

  if (m_numAntsInside < 0 || m_numAntsInside > 10000)
  {
    DebugTrace("Error loading Anthill : Bogus population of {}\n", m_numAntsInside);
    m_numAntsInside = 0;
  }
}


void AntHill::Write(FileWriter* _out)
{
  Building::Write(_out);

  _out->printf("%d", m_numAntsInside);
}
} // namespace Species
