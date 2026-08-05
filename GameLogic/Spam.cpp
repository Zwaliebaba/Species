#include "pch.h"
#include "GlVertex.h"
#include "SoundSources.h"
#include "DebugRender.h"
#include "TextRenderer.h"
#include "Resource.h"
#include "MathUtils.h"
#include "Preferences.h"
#include "HiResTime.h"

#include "Spam.h"

#include "Explosion.h"
#include "Location.h"
#include "ProtocolLimits.h"
#include "EntityGrid.h"
#include "Team.h"
#include "ParticleSystem.h"
#include "GlobalWorld.h"
#include "GameTime.h"

#include "SoundSystem.h"
#include "WorldPointers.h"
#include "AppState.h"


namespace Species
{
  static inline float SpamReloadTime() { return SPAM_RELOADTIME - (SPAM_RELOADTIME / 2.0) * g_difficultyLevel / 10.0; }

  Spam::Spam()
    : Building(),
      m_timer(0.0f),
      m_damage(SPAM_DAMAGE),
      m_onGround(true),
      m_research(false),
      m_activated(true)
  {
    m_type = TypeSpam;
    m_timer = syncfrand(SpamReloadTime());

    // RotateAroundY was FastRotateAround(g_upVector, angle); the pinned
    // NativeRotationYMatchesLegacyRotateAroundY test covers this exact pair.
    //
    // SYNCHRONISED since determinism.yaml T5. A level-file Spam has its facing
    // overwritten by Read/Initialise, so this draw is discarded there -- but
    // GodDish::SpawnSpam stack-constructs a template that is never read from a
    // file, and Building::Initialise computes m_centrePos from that facing.
    // SpawnInfection then hands m_centrePos to twenty SpamInfections, which live
    // in m_effects, which GenerateSyncValue sums.
    DirectX::XMStoreFloat3(&m_front,
                           DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&m_front), DirectX::XMMatrixRotationY(syncfrand(2.0f * M_PI))));

    SetShape(g_resource->GetShape("ResearchItem.shp"));
  }


  void Spam::Initialise(Building* _template) { Building::Initialise(_template); }


  void Spam::SetDetail(int _detail)
  {
    if (m_onGround)
    {
      m_pos.y = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);
      m_pos.y += 20.0f;
    }

    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::XMLoadFloat3(&m_up), DirectX::XMLoadFloat3(&m_pos)));
    m_centrePos = m_shape->CalculateCentre(mat);
    m_radius = m_shape->CalculateRadius(mat, m_centrePos);
  }


  void Spam::Damage(float _damage)
  {
    bool dead = m_damage <= 0.0f;

    int oldHealthBand = int(m_damage / 10.0f);
    Building::Damage(_damage);
    m_damage += _damage;
    int newHealthBand = int(m_damage / 10.0f);

    if (newHealthBand <= 0 || newHealthBand < oldHealthBand)
    {
      float percentDead = 1.0f - m_damage / SPAM_DAMAGE;
      percentDead = std::min(percentDead, 1.0f);
      percentDead = std::max(percentDead, 0.0f);
      DirectX::XMFLOAT4X4 mat;
      DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));
      g_explosionManager.AddExplosion(m_shape, mat, percentDead);
    }

    if (!dead && m_damage <= 0.0f)
    {
      // We just died
      GlobalBuilding* gb = g_globalWorld->GetBuilding(m_id.GetUniqueId(), g_locationId);
      if (gb)
        gb->m_online = true;
      g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), "Explode");
    }
  }

  void Spam::Destroy(float _intensity)
  {
    Building::Destroy(_intensity);
    m_damage = 0.0f;
  }

  void Spam::ListSoundEvents(std::vector<const char*>* _list)
  {
    _list->push_back("Attack");
    _list->push_back("Explode");
    _list->push_back("CreateResearch");
  }


  void Spam::Render(float _predictionTime)
  {
    //    Vector3 predictedPos = m_pos + m_vel * _predictionTime;
    //    Vector3 predictedFront = m_front;
    //    predictedFront.RotateAroundY( _predictionTime );
    //    Matrix34 mat( predictedFront, g_upVector, predictedPos );
    //    m_shape->Render( _predictionTime, mat );

    // RotateAroundX/Y/Z on a Vector3 were FastRotateAround about that axis, which
    // is XMVector3Rotate with an axis quaternion — or equivalently a transform by
    // XMMatrixRotationX/Z, which is what the pinned NativeRotationYMatchesLegacy
    // test asserts for the Y case.
    DirectX::XMVECTOR rotateAround = DirectX::g_XMIdentityR1;
    rotateAround = DirectX::XMVector3Transform(rotateAround, DirectX::XMMatrixRotationX(g_gameTime * 1.0f));
    rotateAround = DirectX::XMVector3Transform(rotateAround, DirectX::XMMatrixRotationZ(g_gameTime * 0.7f));
    rotateAround = DirectX::XMVector3Normalize(rotateAround);

    // RotateAround(axis) took the ANGLE FROM THE AXIS'S MAGNITUDE and returned
    // early below 1e-8 squared. rotateAround is normalised immediately above, so
    // the magnitude here is exactly g_advanceTime and the guard is what it always
    // was: no rotation when the frame time is negligible.
    if (g_advanceTime * g_advanceTime >= 1e-8f)
    {
      DirectX::XMMATRIX const spin = DirectX::XMMatrixRotationAxis(rotateAround, g_advanceTime);
      DirectX::XMStoreFloat3(&m_front, DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&m_front), spin));
      DirectX::XMStoreFloat3(&m_up, DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&m_up), spin));
    }

    DirectX::XMFLOAT3 predictedPos;
    DirectX::XMStoreFloat3(&predictedPos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(_predictionTime),
                                                                       DirectX::XMLoadFloat3(&m_pos)));
    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(
      &mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::XMLoadFloat3(&m_up), DirectX::XMLoadFloat3(&predictedPos)));

    m_shape->Render(0.0f, mat);
  }


  void Spam::RenderAlphas(float _predictionTime)
  {
    // g_editorFont.DrawText3DCentre( m_pos+Vector3(0,100,0), 10.0f, "timer {}", (int) m_timer );
    // g_editorFont.DrawText3DCentre( m_pos+Vector3(0,90,0), 10.0f, "Damage {}", (int) m_damage );

    DirectX::XMFLOAT3 const camUpStore = g_camera->GetUp();
    DirectX::XMFLOAT3 const camRightStore = g_camera->GetRight();
    DirectX::XMVECTOR const camUp = DirectX::XMLoadFloat3(&camUpStore);
    DirectX::XMVECTOR const camRight = DirectX::XMLoadFloat3(&camRightStore);

    glDepthMask(false);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/CloudyGlow.bmp"));

    float timeIndex = g_gameTime + m_id.GetUniqueId() * 10.0f;

    int buildingDetail = g_prefsManager->GetInt("RenderBuildingDetail", 1);
    int maxBlobs = 20;
    if (buildingDetail == 2)
      maxBlobs = 10;
    if (buildingDetail == 3)
      maxBlobs = 0;

    float alpha = 1.0f;

    DirectX::XMVECTOR const predictedPos =
      DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(_predictionTime), DirectX::XMLoadFloat3(&m_pos));
    DirectX::XMVECTOR const centreToMpos = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&m_centrePos));

    for (int i = 0; i < maxBlobs; ++i)
    {
      DirectX::XMFLOAT3 pos;
      DirectX::XMStoreFloat3(&pos, DirectX::XMVectorAdd(predictedPos, centreToMpos));
      pos.x += sinf(timeIndex + i) * i * 0.3f;
      pos.y += cosf(timeIndex + i) * sinf(i * 10) * 5;
      pos.z += cosf(timeIndex + i) * i * 0.3f;

      float size = 5.0f + sinf(timeIndex + i * 10) * 7.0f;
      size = std::max(size, 2.0f);

      // glColor4f( 0.6f, 0.2f, 0.1f, alpha);
      glColor4f(0.9f, 0.2f, 0.2f, alpha);

      if (m_research)
        glColor4f(0.1f, 0.2f, 0.8f, alpha);

      glBegin(GL_QUADS);
      glTexCoord2i(0, 0);
      DirectX::XMVECTOR const posVec = DirectX::XMLoadFloat3(&pos);
      DirectX::XMVECTOR const rightSized = DirectX::XMVectorScale(camRight, size);
      DirectX::XMVECTOR const upSized = DirectX::XMVectorScale(camUp, size);
      EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorSubtract(posVec, rightSized), upSized));
      glTexCoord2i(1, 0);
      EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(posVec, rightSized), upSized));
      glTexCoord2i(1, 1);
      EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorAdd(posVec, rightSized), upSized));
      glTexCoord2i(0, 1);
      EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorSubtract(posVec, rightSized), upSized));
      glEnd();
    }


    //
    // Starbursts

    alpha = 1.0f - m_timer / SpamReloadTime();
    alpha *= 0.3f;

    glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Starburst.bmp"));

    int numStars = 10;
    if (buildingDetail == 2)
      numStars = 5;
    if (buildingDetail == 3)
      numStars = 2;

    for (int i = 0; i < numStars; ++i)
    {
      DirectX::XMFLOAT3 pos;
      DirectX::XMStoreFloat3(&pos, DirectX::XMVectorAdd(predictedPos, centreToMpos));
      pos.x += sinf(timeIndex + i) * i * 0.3f;
      pos.y += (cosf(timeIndex + i) * cosf(i * 10) * 2);
      pos.z += cosf(timeIndex + i) * i * 0.3f;

      float size = i * 10 * alpha;
      if (i > numStars - 2)
        size = i * 20 * alpha;

      // glColor4f( 1.0f, 0.4f, 0.2f, alpha );
      glColor4f(0.8f, 0.2f, 0.2f, alpha);

      if (m_research)
        glColor4f(0.1f, 0.2f, 0.8f, alpha);

      glBegin(GL_QUADS);
      glTexCoord2i(0, 0);
      DirectX::XMVECTOR const posVec = DirectX::XMLoadFloat3(&pos);
      DirectX::XMVECTOR const rightSized = DirectX::XMVectorScale(camRight, size);
      DirectX::XMVECTOR const upSized = DirectX::XMVectorScale(camUp, size);
      EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorSubtract(posVec, rightSized), upSized));
      glTexCoord2i(1, 0);
      EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(posVec, rightSized), upSized));
      glTexCoord2i(1, 1);
      EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorAdd(posVec, rightSized), upSized));
      glTexCoord2i(0, 1);
      EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorSubtract(posVec, rightSized), upSized));
      glEnd();
    }

    glDisable(GL_TEXTURE_2D);
  }


  void Spam::SpawnInfection()
  {
    if (m_research)
      return;

    for (int i = 0; i < 20; ++i)
    {
      // THREE SYNCHRONISED DRAWS IN ONE CONSTRUCTOR CALL, and C++ leaves the
      // argument evaluation order unspecified. That is a pre-existing determinism
      // hazard, not one this conversion introduces: the shape is preserved
      // exactly — same three calls, same source order, same compiler — so
      // whatever MSVC did before, it still does.
      DirectX::XMFLOAT3 const wobble(syncsfrand(1.0f), syncfrand(2.0f), syncsfrand(1.0f));
      DirectX::XMFLOAT3 vel;
      DirectX::XMStoreFloat3(
        &vel,
        DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMVectorAdd(DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&wobble))), 100.0f));

      SpamInfection* infection = new SpamInfection();
      infection->m_pos = m_centrePos;
      infection->m_vel = vel;
      infection->m_parentId = m_id.GetUniqueId();
      int index = g_location->m_effects.PutData(infection);
      infection->m_id.Set(-1, UNIT_EFFECTS, index, -1);
      infection->m_id.GenerateUniqueId();
    }

    g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), "Attack");
  }


  bool Spam::Advance()
  {
    if (m_damage <= 0.0f)
      return true;

    if (!m_onGround)
    {
      DirectX::XMStoreFloat3(&m_pos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD),
                                                                  DirectX::XMLoadFloat3(&m_pos)));

      float landHeight = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);
      if (m_pos.y <= landHeight + 20.0f)
      {
        m_onGround = true;
        m_pos.y = landHeight + 20.0f;
        m_vel.x += syncsfrand(20.0f);
        m_vel.z += syncsfrand(30.0f);
        float speed = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&m_vel)));
        m_vel.y = 0.0f;
        DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&m_vel)), speed));
      }

      DirectX::XMFLOAT4X4 mat;
      DirectX::XMStoreFloat4x4(&mat,
                               BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::XMLoadFloat3(&m_up), DirectX::XMLoadFloat3(&m_pos)));
      m_centrePos = m_shape->CalculateCentre(mat);
    }
    else if (m_onGround)
    {
      if (DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&m_vel))) > 1.0f)
      {
        DirectX::XMVECTOR const damped = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), 1.0f - SERVER_ADVANCE_PERIOD * 0.3f);
        DirectX::XMStoreFloat3(&m_vel, damped);
        DirectX::XMStoreFloat3(
          &m_pos, DirectX::XMVectorMultiplyAdd(damped, DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD), DirectX::XMLoadFloat3(&m_pos)));
        float landHeight = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);
        m_pos.y = landHeight + 20.0f;

        DirectX::XMFLOAT4X4 mat;
        DirectX::XMStoreFloat4x4(&mat,
                                 BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::XMLoadFloat3(&m_up), DirectX::XMLoadFloat3(&m_pos)));
        m_centrePos = m_shape->CalculateCentre(mat);
      }
      else
      {
        m_vel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
      }

      //
      // Push from nearby SPAM

      for (int i = 0; i < g_location->m_buildings.Size(); ++i)
      {
        if (g_location->m_buildings.ValidIndex(i))
        {
          Building* b = g_location->m_buildings[i];
          if (b && b->m_type == TypeSpam)
          {
            bool intersect = SphereSphereIntersection(m_centrePos, m_radius, b->m_centrePos, b->m_radius);
            if (intersect)
            {
              DirectX::XMVECTOR const dir = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&b->m_pos));
              DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorMultiplyAdd(dir, DirectX::XMVectorReplicate(0.25f), DirectX::XMLoadFloat3(&m_vel)));
            }
          }
        }
      }
    }

    bool readyToSpawn = m_activated;
    if (readyToSpawn)
    {
      m_timer -= SERVER_ADVANCE_PERIOD;
      if (m_timer <= 0.0f)
      {
        if (readyToSpawn)
        {
          m_timer = SpamReloadTime();
          SpawnInfection();
        }
      }
    }

    if (m_research)
    {
      g_soundSystem->StopAllSounds(m_id, "Spam Create");
    }

    return Building::Advance();
  }


  void Spam::SendFromHeaven()
  {
    m_onGround = false;
    m_activated = false;
  }


  void Spam::SetAsResearch()
  {
    m_research = true;
    m_activated = false;

    g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), "CreateResearch");
  }


  // ============================================================================


  SpamInfection::SpamInfection()
    : WorldObject(),
      m_state(StateIdle),
      m_retargetTimer(0.0f),
      m_life(SPAMINFECTION_LIFE),
      m_parentId(-1)
  {
    m_retargetTimer = syncfrand(1.0f);

    m_type = EffectSpamInfection;
  }


  bool SpamInfection::SearchForEntities()
  {
    m_targetId = g_location->m_entityGrid->GetBestEnemy(m_pos.x, m_pos.z, SPAMINFECTION_MINSEARCHRANGE, SPAMINFECTION_MAXSEARCHRANGE, 1);

    if (m_targetId.IsValid())
    {
      m_state = StateAttackingEntity;
      return true;
    }

    return false;
  }


  bool SpamInfection::SearchForRandomPosition()
  {
    Building* building = g_location->GetBuilding(m_parentId);
    if (building)
    {
      float distance = DirectX::XMVectorGetX(
        DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&building->m_pos), DirectX::XMLoadFloat3(&m_pos))));
      if (distance > 400.0f)
      {
        m_targetPos = building->m_pos;
        return true;
      }
    }

    m_targetPos = m_pos;
    m_targetPos.x += syncsfrand(200.0f);
    m_targetPos.y += syncsfrand(200.0f);
    m_targetPos.z += syncsfrand(200.0f);

    float landHeight = g_location->m_landscape.m_heightMap->GetValue(m_targetPos.x, m_targetPos.z);
    m_targetPos.y = std::max(m_targetPos.y, landHeight);

    return true;
  }


  bool SpamInfection::SearchForSpirits()
  {
    // We don't always want this thing to convert the spirits.
    // It's better if sometimes the spirits are used in eggs to create more virii
    // as it shows the reproduction system better
    if (syncfrand(2.0f) < 1.0f)
      return false;

    int index = -1;
    float nearest = 9999.9f;

    for (int i = 0; i < g_location->m_spirits.Size(); ++i)
    {
      if (g_location->m_spirits.ValidIndex(i))
      {
        Spirit* s = g_location->m_spirits.GetPointer(i);
        float theDist =
          DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&s->m_pos), DirectX::XMLoadFloat3(&m_pos))));

        if (theDist <= SPAMINFECTION_MAXSEARCHRANGE && theDist >= SPAMINFECTION_MINSEARCHRANGE && theDist < nearest &&
            s->m_state == Spirit::StateFloating && s->m_pos.y > 10.0f)
        {
          index = i;
          nearest = theDist;
        }
      }
    }

    if (index != -1)
    {
      m_spiritId = index;
      m_state = StateAttackingSpirit;
      return true;
    }

    return false;
  }


  void SpamInfection::AdvanceIdle()
  {
    m_retargetTimer -= SERVER_ADVANCE_PERIOD;
    if (m_retargetTimer <= 0.0)
    {
      m_retargetTimer = 1.0f;
      bool targetFound = false;
      if (syncfrand(2.0f) < 1.0f)
      {
        if (!targetFound)
          targetFound = SearchForEntities();
        if (!targetFound)
          targetFound = SearchForSpirits();
      }
      else
      {
        if (!targetFound)
          targetFound = SearchForSpirits();
        if (!targetFound)
          targetFound = SearchForEntities();
      }
      if (!targetFound)
        targetFound = SearchForRandomPosition();
    }

    AdvanceToTargetPosition();
  }


  void SpamInfection::AdvanceAttackingEntity()
  {
    //
    // Is our target alive and well?

    Entity* target = g_location->GetEntity(m_targetId);
    if (!target || target->m_dead)
    {
      m_state = StateIdle;
      return;
    }

    m_targetPos = target->m_pos;
    bool arrived = AdvanceToTargetPosition();

    if (arrived)
    {
      if (m_targetId.GetTeamId() == 0)
      {
        // Green citizen
        int citizenResearch = g_globalWorld->m_research->CurrentLevel(GlobalResearch::TypeCitizen);
        if (citizenResearch > 2 && syncfrand(10.0f) < 5.0f)
        {
          g_location->SpawnEntities(target->m_pos, 1, -1, Entity::TypeCitizen, 1, target->m_vel, 0.0f);
          g_location->m_entityGrid->RemoveObject(m_targetId, target->m_pos.x, target->m_pos.z, target->m_radius);
          g_location->m_teams[0].m_others.MarkNotUsed(m_targetId.GetIndex());
          delete target;
        }
        else
        {
          target->ChangeHealth(-999);
        }
      }
      else if (m_targetId.GetTeamId() == 2)
      {
        // Player
        target->ChangeHealth(-999);
      }

      int numFlashes = 5 + speciesRandom() % 5;
      for (int i = 0; i < numFlashes; ++i)
      {
        DirectX::XMFLOAT3 vel(sfrand(15.0f), frand(15.0f), sfrand(15.0f));
        float size = i * 30;
        DirectX::XMFLOAT3 pos(m_pos.x, m_pos.y + 50.0f, m_pos.z);
        g_particleSystem->CreateParticle(m_pos, vel, Particle::TypeFire, size);
      }

      m_life += 1.0f;
      m_state = StateIdle;
    }
  }


  void SpamInfection::AdvanceAttackingSpirit()
  {
    //
    // Is our spirit still alive and well?

    if (!g_location->m_spirits.ValidIndex(m_spiritId))
    {
      m_state = StateIdle;
      return;
    }


    Spirit* spirit = g_location->m_spirits.GetPointer(m_spiritId);

    if (spirit->m_state != Spirit::StateFloating)
    {
      m_state = StateIdle;
      return;
    }

    m_targetPos = spirit->m_pos;

    bool arrived = AdvanceToTargetPosition();
    if (arrived)
    {
      int entityType = Entity::TypeVirii;
      if (syncfrand(20.0f) < 1.0f)
        entityType = Entity::TypeSpider;
      g_location->SpawnEntities(spirit->m_pos, 1, -1, entityType, 1, g_zeroVector, 0.0f, 200.0f);
      g_location->m_spirits.MarkNotUsed(m_spiritId);

      int numFlashes = 5 + speciesRandom() % 5;
      for (int i = 0; i < numFlashes; ++i)
      {
        DirectX::XMFLOAT3 vel(sfrand(15.0f), frand(15.0f), sfrand(15.0f));
        float size = i * 30;
        DirectX::XMFLOAT3 pos(m_pos.x, m_pos.y + 50.0f, m_pos.z);
        g_particleSystem->CreateParticle(m_pos, vel, Particle::TypeFire, size);
      }

      m_life += 1.0f;
      m_state = StateIdle;
    }
  }


  bool SpamInfection::AdvanceToTargetPosition()
  {
    m_positionHistory.insert(m_positionHistory.begin(), m_pos);
    int maxLength = SPAMINFECTION_TAILLENGTH;
    for (int i = maxLength; i < static_cast<int>(m_positionHistory.size()); ++i)
    {
      m_positionHistory.erase(m_positionHistory.begin() + i);
    }

    DirectX::XMVECTOR const targetVel = DirectX::XMVectorScale(
      DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_targetPos), DirectX::XMLoadFloat3(&m_pos))), 200.0f);
    float factor = SERVER_ADVANCE_PERIOD * 0.5f;
    DirectX::XMVECTOR const newVel =
      DirectX::XMVectorAdd(DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), 1.0f - factor), DirectX::XMVectorScale(targetVel, factor));
    DirectX::XMStoreFloat3(&m_vel, newVel);
    DirectX::XMStoreFloat3(&m_pos,
                           DirectX::XMVectorMultiplyAdd(newVel, DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD), DirectX::XMLoadFloat3(&m_pos)));

    float distance =
      DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_targetPos), DirectX::XMLoadFloat3(&m_pos))));
    return (distance < 20.0f);
  }


  bool SpamInfection::Advance()
  {
    switch (m_state)
    {
    case StateIdle:
      AdvanceIdle();
    case StateAttackingEntity:
      AdvanceAttackingEntity();
    case StateAttackingSpirit:
      AdvanceAttackingSpirit();
    }

    m_life -= SERVER_ADVANCE_PERIOD;
    if (m_life <= 0.0f)
      return true;

    return false;
  }


  void SpamInfection::Render(float _time)
  {
    DirectX::XMVECTOR const predictedPos =
      DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(_time), DirectX::XMLoadFloat3(&m_pos));

    // RenderSphere( predictedPos, 200.0f, RGBAColour(255,0,0,255) );
    // RenderArrow( predictedPos, m_targetPos, 1.0f, RGBAColour(255,255,255,255) );

    glShadeModel(GL_SMOOTH);
    glEnable(GL_BLEND);
    // glDepthMask( false );
    int maxLength = SPAMINFECTION_TAILLENGTH * (m_life / SPAMINFECTION_LIFE);
    maxLength = std::max(maxLength, 2);
    maxLength = std::min(maxLength, static_cast<int>(m_positionHistory.size()));

    DirectX::XMFLOAT3 const camPosStore = g_camera->GetPos();
    DirectX::XMVECTOR const camPos = DirectX::XMLoadFloat3(&camPosStore);
    int numRepeats = 4;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Laser.bmp"));
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    for (int j = 0; j < numRepeats; ++j)
    {
      float size = 0.3f * numRepeats;
      for (int i = 1; i < maxLength; ++i)
      {
        float alpha = 1.0f - i / (float)maxLength;
        alpha *= 0.75f;
        glColor4f(1.0f, 0.1f, 0.1f, alpha);
        DirectX::XMVECTOR const thisPos = DirectX::XMLoadFloat3(&m_positionHistory[i]);
        DirectX::XMVECTOR const lastPos = DirectX::XMLoadFloat3(&m_positionHistory[i - 1]);
        // operator^ was the cross product; SetLength is normalise-then-scale.
        DirectX::XMVECTOR const rightAngle =
          DirectX::XMVectorScale(DirectX::XMVector3Normalize(
                                   DirectX::XMVector3Cross(DirectX::XMVectorSubtract(thisPos, lastPos), DirectX::XMVectorSubtract(camPos, thisPos))),
                                 size);
        glBegin(GL_QUADS);
        glTexCoord2f(0.2f, 0.0f);
        EmitVertex(DirectX::XMVectorSubtract(thisPos, rightAngle));
        glTexCoord2f(0.2f, 1.0f);
        EmitVertex(DirectX::XMVectorAdd(thisPos, rightAngle));
        glTexCoord2f(0.8f, 1.0f);
        EmitVertex(DirectX::XMVectorAdd(lastPos, rightAngle));
        glTexCoord2f(0.8f, 0.0f);
        EmitVertex(DirectX::XMVectorSubtract(lastPos, rightAngle));
        glEnd();
      }
      if (static_cast<int>(m_positionHistory.size()) > 0)
      {
        glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
        DirectX::XMVECTOR const lastPos = DirectX::XMLoadFloat3(&m_positionHistory[0]);
        DirectX::XMVECTOR const thisPos = predictedPos;
        DirectX::XMVECTOR const rightAngle =
          DirectX::XMVectorScale(DirectX::XMVector3Normalize(
                                   DirectX::XMVector3Cross(DirectX::XMVectorSubtract(thisPos, lastPos), DirectX::XMVectorSubtract(camPos, thisPos))),
                                 size);
        glBegin(GL_QUADS);
        glTexCoord2f(0.2f, 0.0f);
        EmitVertex(DirectX::XMVectorSubtract(thisPos, rightAngle));
        glTexCoord2f(0.2f, 1.0f);
        EmitVertex(DirectX::XMVectorAdd(thisPos, rightAngle));
        glTexCoord2f(0.8f, 1.0f);
        EmitVertex(DirectX::XMVectorAdd(lastPos, rightAngle));
        glTexCoord2f(0.8f, 0.0f);
        EmitVertex(DirectX::XMVectorSubtract(lastPos, rightAngle));
        glEnd();
      }
    }

    // glDepthMask( true );
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_FLAT);
  }
} // namespace Species
