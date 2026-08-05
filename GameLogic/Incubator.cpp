#include "pch.h"
#include "GlVertex.h"
#include "SoundSources.h"
#include "FileWriter.h"
#include "MathUtils.h"
#include "Profiler.h"
#include "Resource.h"
#include "Shape.h"
#include "TextStreamReaders.h"
#include "ProtocolLimits.h"
#include "Location.h"
#include "ParticleSystem.h"
#include "SoundSystem.h"
#include "Incubator.h"
#include "WorldPointers.h"


namespace Species
{
  Incubator::Incubator()
    : Building(),
      m_spiritCentre(nullptr),
      m_troopType(Entity::TypeCitizen),
      m_timer(INCUBATOR_PROCESSTIME),
      m_numStartingSpirits(0)
  {
    m_type = TypeIncubator;

    SetShape(g_resource->GetShape("Incubator.shp"));

    m_spiritCentre = m_shape->m_rootFragment->LookupMarker("MarkerSpirits");
    m_exit = m_shape->m_rootFragment->LookupMarker("MarkerExit");
    m_dock = m_shape->m_rootFragment->LookupMarker("MarkerDock");

    m_spiritEntrance[0] = m_shape->m_rootFragment->LookupMarker("MarkerSpiritIncoming0");
    m_spiritEntrance[1] = m_shape->m_rootFragment->LookupMarker("MarkerSpiritIncoming1");
    m_spiritEntrance[2] = m_shape->m_rootFragment->LookupMarker("MarkerSpiritIncoming2");

    DEBUG_ASSERT(m_spiritCentre);
    DEBUG_ASSERT(m_exit);
    DEBUG_ASSERT(m_dock);
    DEBUG_ASSERT(m_spiritEntrance[0]);
    DEBUG_ASSERT(m_spiritEntrance[1]);
    DEBUG_ASSERT(m_spiritEntrance[2]);

    m_spirits.SetStepSize(20);
  }

  Incubator::~Incubator() {}

  void Incubator::Initialise(Building* _template)
  {
    Building::Initialise(_template);

    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

    DirectX::XMFLOAT3 const spiritCentre = m_spiritCentre->GetWorldPosition(mat);

    m_numStartingSpirits = static_cast<Incubator*>(_template)->m_numStartingSpirits;

    for (int i = 0; i < m_numStartingSpirits; ++i)
    {
      Spirit* s = m_spirits.GetPointer(m_spirits.GetNextFree());
      // The three calls stay in this order: they advance the RNG. SYNCHRONISED
      // since determinism.yaml T5 -- a spirit's position is simulation state,
      // and these were drawn from the unsynchronised LCG.
      DirectX::XMFLOAT3 const scatter(syncsfrand(20.0f), syncsfrand(20.0f), syncsfrand(20.0f));
      DirectX::XMStoreFloat3(&s->m_pos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&spiritCentre), DirectX::XMLoadFloat3(&scatter)));
      s->m_teamId = m_id.GetTeamId();
      s->Begin();
      s->m_state = Spirit::StateInStore;
    }
  }

  bool Incubator::Advance()
  {
    //
    // If there are spirits in the building
    // Spawn entities at the door

    if (m_id.GetTeamId() != 255)
    {
      if (m_spirits.NumUsed() > 0)
      {
        m_timer -= SERVER_ADVANCE_PERIOD;
        if (m_timer <= 0.0f)
        {
          SpawnEntity();
          m_timer = INCUBATOR_PROCESSTIME;

          float overCrowded = static_cast<float>(m_spirits.NumUsed()) / 10.0f;
          m_timer -= overCrowded;

          if (m_timer < 0.5f)
            m_timer = 0.5f;
        }
      }
    }

    //
    // Advance all the spirits in our store

    for (int i = 0; i < m_spirits.Size(); ++i)
    {
      if (m_spirits.ValidIndex(i))
      {
        Spirit* spirit = m_spirits.GetPointer(i);
        spirit->Advance();
      }
    }

    //
    // Reduce incoming and outgoing logs

    for (int i = 0; i < static_cast<int>(m_incoming.size()); ++i)
    {
      IncubatorIncoming* ii = m_incoming[i];
      ii->m_alpha -= SERVER_ADVANCE_PERIOD;
      if (ii->m_alpha <= 0.0f)
      {
        m_incoming.erase(m_incoming.begin() + i);
        delete ii;
        --i;
      }
    }

    return Building::Advance();
  }

  void Incubator::SpawnEntity()
  {
    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

    // one wants both the exit position and its facing.
    DirectX::XMFLOAT4X4 const exit = m_exit->GetWorldMatrix(mat);

    //
    // Spawn the entity
    int teamId = m_id.GetTeamId();
    if (teamId == 2)
      teamId = 0; // Green rather than yellow

    g_location->SpawnEntities(DirectX::XMFLOAT3(exit._41, exit._42, exit._43), teamId, -1, m_troopType, 1,
                              DirectX::XMFLOAT3(exit._31, exit._32, exit._33), 0.0f);

    //
    // Remove a spirit
    DirectX::XMFLOAT3 spiritPos{0.0f, 0.0f, 0.0f};
    for (int i = 0; i < m_spirits.Size(); ++i)
    {
      if (m_spirits.ValidIndex(i))
      {
        spiritPos = m_spirits[i].m_pos;
        m_spirits.MarkNotUsed(i);
        break;
      }
    }

    //
    // Create Particle + outgoing affects

    auto ii = new IncubatorIncoming();
    ii->m_pos = spiritPos;
    ii->m_entrance = 2;
    ii->m_alpha = 1.0f;
    m_incoming.push_back(ii);

    int numFlashes = 4 + speciesRandom() % 4;
    for (int i = 0; i < numFlashes; ++i)
    {
      // The three RNG calls stay in this order.
      DirectX::XMFLOAT3 const vel(sfrand(15.0f), frand(35.0f), sfrand(15.0f));
      g_particleSystem->CreateParticle(DirectX::XMFLOAT3(exit._41, exit._42, exit._43), vel, Particle::TypeControlFlash);
      // g_particleSystem->CreateParticle( spiritPos, vel, Particle::TypeControlFlash );
    }

    //
    // Sound effect

    g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), "SpawnEntity");
  }

  void Incubator::AddSpirit(Spirit* _spirit)
  {
    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

    DirectX::XMFLOAT3 const spiritCentre = m_spiritCentre->GetWorldPosition(mat);

    Spirit* s = m_spirits.GetPointer(m_spirits.GetNextFree());

    // The three calls stay in this order: they advance the RNG. SYNCHRONISED
    // since determinism.yaml T5, as in Initialise above -- and note the
    // syncrand() draw fifteen lines below, which this function already made.
    DirectX::XMFLOAT3 const scatter(syncsfrand(20.0f), syncsfrand(20.0f), syncsfrand(20.0f));
    DirectX::XMStoreFloat3(&s->m_pos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&spiritCentre), DirectX::XMLoadFloat3(&scatter)));
    s->m_teamId = _spirit->m_teamId;
    s->m_state = Spirit::StateInStore;

    auto ii = new IncubatorIncoming();
    ii->m_pos = s->m_pos;
    ii->m_entrance = syncrand() % 2;
    ii->m_alpha = 1.0f;
    m_incoming.push_back(ii);

    g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), "AddSpirit");
  }

  void Incubator::GetDockPoint(DirectX::XMFLOAT3& _pos, DirectX::XMFLOAT3& _front)
  {
    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

    DirectX::XMFLOAT4X4 const dock = m_dock->GetWorldMatrix(mat);
    _pos = DirectX::XMFLOAT3(dock._41, dock._42, dock._43);
    _pos = PushFromBuilding(_pos, 5.0f);
    _front = DirectX::XMFLOAT3(dock._31, dock._32, dock._33);
  }

  int Incubator::NumSpiritsInside() { return m_spirits.NumUsed(); }

  void Incubator::Read(TextReader* _in, bool _dynamic)
  {
    Building::Read(_in, _dynamic);

    if (_in->TokenAvailable())
      m_numStartingSpirits = atoi(_in->GetNextToken());
  }

  void Incubator::Write(FileWriter* _out)
  {
    Building::Write(_out);

    _out->printf("%6d", m_numStartingSpirits);
  }

  void Incubator::Render(float _predictionTime)
  {
    Building::Render(_predictionTime);

    //    Vector3 dockPos, dockFront;
    //    GetDockPoint( dockPos, dockFront );
    //    RenderSphere( dockPos, 5.0f );
  }

  void Incubator::RenderAlphas(float _predictionTime)
  {
    Building::RenderAlphas(_predictionTime);

    glDisable(GL_CULL_FACE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glEnable(GL_BLEND);
    glDepthMask(false);

    //
    // Render all spirits

    for (int i = 0; i < m_spirits.Size(); ++i)
    {
      if (m_spirits.ValidIndex(i))
      {
        Spirit* spirit = m_spirits.GetPointer(i);
        spirit->Render(_predictionTime);
      }
    }

    //
    // Render incoming and outgoing effects

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Laser.bmp"));

    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

    DirectX::XMFLOAT3 entrances[3];
    entrances[0] = m_spiritEntrance[0]->GetWorldPosition(mat);
    entrances[1] = m_spiritEntrance[1]->GetWorldPosition(mat);
    entrances[2] = m_spiritEntrance[2]->GetWorldPosition(mat);

    for (int i = 0; i < static_cast<int>(m_incoming.size()); ++i)
    {
      IncubatorIncoming* ii = m_incoming[i];
      DirectX::XMVECTOR const fromPos = DirectX::XMLoadFloat3(&ii->m_pos);
      DirectX::XMVECTOR const toPos = DirectX::XMLoadFloat3(&entrances[ii->m_entrance]);

      DirectX::XMVECTOR const midPoint = DirectX::XMVectorScale(DirectX::XMVectorAdd(fromPos, toPos), 0.5f);

      DirectX::XMFLOAT3 const cameraPos = g_camera->GetPos();
      DirectX::XMVECTOR const camToMidPoint = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&cameraPos), midPoint);
      DirectX::XMVECTOR const rightAngle =
        DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMVector3Cross(camToMidPoint, DirectX::XMVectorSubtract(midPoint, toPos))), 1.5f);

      glColor4f(1.0f, 1.0f, 0.2f, ii->m_alpha);

      glBegin(GL_QUADS);
      glTexCoord2i(0, 0);
      EmitVertex(DirectX::XMVectorSubtract(fromPos, rightAngle));
      glTexCoord2i(0, 1);
      EmitVertex(DirectX::XMVectorAdd(fromPos, rightAngle));
      glTexCoord2i(1, 1);
      EmitVertex(DirectX::XMVectorAdd(toPos, rightAngle));
      glTexCoord2i(1, 0);
      EmitVertex(DirectX::XMVectorSubtract(toPos, rightAngle));
      glEnd();
    }

    glDisable(GL_TEXTURE_2D);
    glDepthMask(true);
    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);
  }

  void Incubator::ListSoundEvents(std::vector<const char*>* _list)
  {
    Building::ListSoundEvents(_list);

    _list->push_back("AddSpirit");
    _list->push_back("SpawnEntity");
  }
} // namespace Species
