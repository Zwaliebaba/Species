#include "pch.h"
#include "GlVertex.h"
#include "SoundSources.h"
#include "Debug.h"
#include "FileWriter.h"
#include "HiResTime.h"
#include "MathUtils.h"
#include "Profiler.h"
#include "Resource.h"
#include "Shape.h"
#include "TextRenderer.h"
#include "TextStreamReaders.h"
#include "DebugRender.h"
#include "Preferences.h"
#include "LanguageTable.h"

#include "SpiritReceiver.h"
#include "Citizen.h"

#include "Location.h"
#include "GlobalWorld.h"

#include "SoundSystem.h"
#include "WorldPointers.h"
#include "AppState.h"


// ****************************************************************************
// Class ReceiverBuilding
// ****************************************************************************


namespace Species
{
  ReceiverBuilding::ReceiverBuilding()
    : Building(),
      m_spiritLink(-1),
      m_spiritLocation(nullptr)
  {
  }

  void ReceiverBuilding::Initialise(Building* _template)
  {
    Building::Initialise(_template);

    m_spiritLink = ((ReceiverBuilding*)_template)->m_spiritLink;
  }

  DirectX::XMFLOAT3 ReceiverBuilding::GetSpiritLocation()
  {
    if (!m_spiritLocation)
    {
      m_spiritLocation = m_shape->m_rootFragment->LookupMarker("MarkerSpiritLink");
      DEBUG_ASSERT(m_spiritLocation);
    }

    DirectX::XMFLOAT4X4 rootMat = GetWorldMatrix();

    return m_spiritLocation->GetWorldPosition(rootMat);
  }


  bool ReceiverBuilding::IsInView()
  {
    Building* spiritLink = g_location->GetBuilding(m_spiritLink);

    if (spiritLink)
    {
      DirectX::XMVECTOR const theirCentre = DirectX::XMLoadFloat3(&spiritLink->m_centrePos);
      DirectX::XMVECTOR const ourCentre = DirectX::XMLoadFloat3(&m_centrePos);

      DirectX::XMFLOAT3 midPoint;
      DirectX::XMStoreFloat3(&midPoint, DirectX::XMVectorScale(DirectX::XMVectorAdd(theirCentre, ourCentre), 0.5f));
      float radius = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(theirCentre, ourCentre))) / 2.0f;
      radius += m_radius;
      return (g_camera->SphereInViewFrustum(midPoint, radius));
    }
    else
    {
      return Building::IsInView();
    }
  }


  void ReceiverBuilding::ListSoundEvents(std::vector<const char*>* _list)
  {
    Building::ListSoundEvents(_list);

    _list->push_back("TriggerSpirit");
  }


  void ReceiverBuilding::Render(float _predictionTime)
  {
    DirectX::XMFLOAT4X4 mat = GetWorldMatrix();
    m_shape->Render(_predictionTime, mat);
  }


  void ReceiverBuilding::RenderAlphas(float _predictionTime)
  {
    Building::RenderAlphas(_predictionTime);

    _predictionTime -= 0.1f;

    Building* spiritLink = g_location->GetBuilding(m_spiritLink);

    int buildingDetail = g_prefsManager->GetInt("RenderBuildingDetail", 1);

    if (spiritLink)
    {
      //
      // Render the spirit line itself

      ReceiverBuilding* receiverBuilding = (ReceiverBuilding*)spiritLink;

      DirectX::XMFLOAT3 const ourPosStore = GetSpiritLocation();
      DirectX::XMFLOAT3 const theirPosStore = receiverBuilding->GetSpiritLocation();
      DirectX::XMVECTOR const ourPos = DirectX::XMLoadFloat3(&ourPosStore);
      DirectX::XMVECTOR const theirPos = DirectX::XMLoadFloat3(&theirPosStore);
      DirectX::XMVECTOR const alongLink = DirectX::XMVectorSubtract(theirPos, ourPos);

      DirectX::XMFLOAT3 const cameraPosStore = g_camera->GetPos();
      DirectX::XMVECTOR const cameraPos = DirectX::XMLoadFloat3(&cameraPosStore);

      DirectX::XMVECTOR ourPosRight = DirectX::XMVector3Cross(DirectX::XMVectorSubtract(cameraPos, ourPos), alongLink);
      DirectX::XMVECTOR theirPosRight = DirectX::XMVector3Cross(DirectX::XMVectorSubtract(cameraPos, theirPos), alongLink);

      glDisable(GL_CULL_FACE);
      glDepthMask(false);
      glColor4f(0.9f, 0.9f, 0.5f, 1.0f);

      float size = 0.5f;

      if (buildingDetail == 1)
      {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Laser.bmp"));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);

        size = 1.0f;
      }

      // SetLength; see the note on the same call in Generator.cpp. Rendering
      // only, and degenerate only with the camera exactly on the link, so this
      // takes the native normalise rather than reproducing the fallback. size is
      // set above and the two are scaled here rather than at declaration.
      ourPosRight = DirectX::XMVectorScale(DirectX::XMVector3Normalize(ourPosRight), size);
      theirPosRight = DirectX::XMVectorScale(DirectX::XMVector3Normalize(theirPosRight), size);

      glBegin(GL_QUADS);
      glTexCoord2f(0.1f, 0);
      EmitVertex(DirectX::XMVectorSubtract(ourPos, ourPosRight));
      glTexCoord2f(0.1f, 1);
      EmitVertex(DirectX::XMVectorAdd(ourPos, ourPosRight));
      glTexCoord2f(0.9f, 1);
      EmitVertex(DirectX::XMVectorAdd(theirPos, theirPosRight));
      glTexCoord2f(0.9f, 0);
      EmitVertex(DirectX::XMVectorSubtract(theirPos, theirPosRight));
      glEnd();

      glDisable(GL_TEXTURE_2D);


      //
      // Render any surges

      BeginRenderUnprocessedSpirits();
      for (int i = 0; i < static_cast<int>(m_spirits.size()); ++i)
      {
        float thisSpirit = m_spirits[i];
        thisSpirit += _predictionTime * 0.8f;
        if (thisSpirit < 0.0f)
          thisSpirit = 0.0f;
        if (thisSpirit > 1.0f)
          thisSpirit = 1.0f;
        DirectX::XMFLOAT3 thisSpiritPos;
        DirectX::XMStoreFloat3(&thisSpiritPos, DirectX::XMVectorMultiplyAdd(alongLink, DirectX::XMVectorReplicate(thisSpirit), ourPos));
        RenderUnprocessedSpirit(thisSpiritPos, 1.0f);
      }
      EndRenderUnprocessedSpirits();
    }
  }

  bool ReceiverBuilding::Advance()
  {
    for (int i = 0; i < static_cast<int>(m_spirits.size()); ++i)
    {
      float* thisSpirit = &m_spirits[i];
      *thisSpirit += SERVER_ADVANCE_PERIOD * 0.8f;
      if (*thisSpirit >= 1.0f)
      {
        m_spirits.erase(m_spirits.begin() + i);
        --i;

        Building* spiritLink = g_location->GetBuilding(m_spiritLink);
        if (spiritLink)
        {
          ReceiverBuilding* receiverBuilding = (ReceiverBuilding*)spiritLink;
          receiverBuilding->TriggerSpirit(0.0f);
        }
      }
    }
    return Building::Advance();
  }

  void ReceiverBuilding::TriggerSpirit(float _initValue)
  {
    m_spirits.insert(m_spirits.begin(), _initValue);
    g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), "TriggerSpirit");
  }

  void ReceiverBuilding::Read(TextReader* _in, bool _dynamic)
  {
    Building::Read(_in, _dynamic);
    m_spiritLink = atoi(_in->GetNextToken());
  }

  void ReceiverBuilding::Write(FileWriter* _out)
  {
    Building::Write(_out);

    _out->printf("%-8d", m_spiritLink);
  }

  int ReceiverBuilding::GetBuildingLink() { return m_spiritLink; }

  void ReceiverBuilding::SetBuildingLink(int _buildingId) { m_spiritLink = _buildingId; }


  SpiritProcessor* ReceiverBuilding::GetSpiritProcessor()
  {
    static int processorId = -1;

    SpiritProcessor* processor = (SpiritProcessor*)g_location->GetBuilding(processorId);

    if (!processor || processor->m_type != Building::TypeSpiritProcessor)
    {
      for (int i = 0; i < g_location->m_buildings.Size(); ++i)
      {
        if (g_location->m_buildings.ValidIndex(i))
        {
          Building* building = g_location->m_buildings[i];
          if (building->m_type == TypeSpiritProcessor)
          {
            processor = (SpiritProcessor*)building;
            processorId = processor->m_id.GetUniqueId();
            break;
          }
        }
      }
    }

    return processor;
  }


  static float s_nearPlaneStart;

  void ReceiverBuilding::BeginRenderUnprocessedSpirits()
  {
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(false);

    int buildingDetail = g_prefsManager->GetInt("RenderBuildingDetail", 1);
    if (buildingDetail == 1)
    {
      glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Glow.bmp"));
    }

    s_nearPlaneStart = g_renderer->GetNearPlane();
    g_camera->SetupProjectionMatrix(s_nearPlaneStart * 1.1f, g_renderer->GetFarPlane());
  }


  void ReceiverBuilding::RenderUnprocessedSpirit(DirectX::XMFLOAT3 const& _pos, float _life)
  {
    DirectX::XMVECTOR const position = DirectX::XMLoadFloat3(&_pos);

    // Hoisted out of the quads below, which each called them afresh.
    DirectX::XMFLOAT3 const camUpStore = g_camera->GetUp();
    DirectX::XMFLOAT3 const camRightStore = g_camera->GetRight();
    DirectX::XMVECTOR const camUp = DirectX::XMLoadFloat3(&camUpStore);
    DirectX::XMVECTOR const camRight = DirectX::XMLoadFloat3(&camRightStore);
    float scale = 2.0f * _life;
    float alphaValue = _life;

    glColor4f(0.6f, 0.2f, 0.1f, alphaValue);
    glBegin(GL_QUADS);
    glTexCoord2i(0, 0);
    EmitVertex(DirectX::XMVectorMultiplyAdd(camUp, DirectX::XMVectorReplicate(3 * scale), position));
    glTexCoord2i(1, 0);
    EmitVertex(DirectX::XMVectorMultiplyAdd(camRight, DirectX::XMVectorReplicate(3 * scale), position));
    glTexCoord2i(1, 1);
    EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camUp, DirectX::XMVectorReplicate(3 * scale), position));
    glTexCoord2i(0, 1);
    EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camRight, DirectX::XMVectorReplicate(3 * scale), position));
    glTexCoord2i(0, 0);
    EmitVertex(DirectX::XMVectorMultiplyAdd(camUp, DirectX::XMVectorReplicate(3 * scale), position));
    glTexCoord2i(1, 0);
    EmitVertex(DirectX::XMVectorMultiplyAdd(camRight, DirectX::XMVectorReplicate(3 * scale), position));
    glTexCoord2i(1, 1);
    EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camUp, DirectX::XMVectorReplicate(3 * scale), position));
    glTexCoord2i(0, 1);
    EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camRight, DirectX::XMVectorReplicate(3 * scale), position));
    glEnd();

    glColor4f(0.6f, 0.2f, 0.1f, alphaValue);
    glBegin(GL_QUADS);
    glTexCoord2i(0, 0);
    EmitVertex(DirectX::XMVectorMultiplyAdd(camUp, DirectX::XMVectorReplicate(1 * scale), position));
    glTexCoord2i(1, 0);
    EmitVertex(DirectX::XMVectorMultiplyAdd(camRight, DirectX::XMVectorReplicate(1 * scale), position));
    glTexCoord2i(1, 1);
    EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camUp, DirectX::XMVectorReplicate(1 * scale), position));
    glTexCoord2i(0, 1);
    EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camRight, DirectX::XMVectorReplicate(1 * scale), position));
    glTexCoord2i(0, 0);
    EmitVertex(DirectX::XMVectorMultiplyAdd(camUp, DirectX::XMVectorReplicate(1 * scale), position));
    glTexCoord2i(1, 0);
    EmitVertex(DirectX::XMVectorMultiplyAdd(camRight, DirectX::XMVectorReplicate(1 * scale), position));
    glTexCoord2i(1, 1);
    EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camUp, DirectX::XMVectorReplicate(1 * scale), position));
    glTexCoord2i(0, 1);
    EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camRight, DirectX::XMVectorReplicate(1 * scale), position));
    glEnd();

    int buildingDetail = g_prefsManager->GetInt("RenderBuildingDetail", 1);
    if (buildingDetail == 1)
    {
      glEnable(GL_TEXTURE_2D);
      glColor4f(0.6f, 0.2f, 0.1f, alphaValue / 1.5f);
      glBegin(GL_QUADS);
      glTexCoord2i(0, 0);
      EmitVertex(DirectX::XMVectorMultiplyAdd(camUp, DirectX::XMVectorReplicate(30 * scale), position));
      glTexCoord2i(1, 0);
      EmitVertex(DirectX::XMVectorMultiplyAdd(camRight, DirectX::XMVectorReplicate(30 * scale), position));
      glTexCoord2i(1, 1);
      EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camUp, DirectX::XMVectorReplicate(30 * scale), position));
      glTexCoord2i(0, 1);
      EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camRight, DirectX::XMVectorReplicate(30 * scale), position));
      glEnd();
      glDisable(GL_TEXTURE_2D);
    }
  }

  void ReceiverBuilding::RenderUnprocessedSpirit_basic(DirectX::XMFLOAT3 const& _pos, float _life)
  {
    DirectX::XMVECTOR const position = DirectX::XMLoadFloat3(&_pos);

    // Hoisted out of the quads below, which each called them afresh.
    DirectX::XMFLOAT3 const camUpStore = g_camera->GetUp();
    DirectX::XMFLOAT3 const camRightStore = g_camera->GetRight();
    DirectX::XMVECTOR const camUp = DirectX::XMLoadFloat3(&camUpStore);
    DirectX::XMVECTOR const camRight = DirectX::XMLoadFloat3(&camRightStore);
    float scale = 2.0f * _life;
    float alphaValue = _life;

    glColor4f(0.6f, 0.2f, 0.1f, alphaValue);
    glTexCoord2i(0, 0);
    EmitVertex(DirectX::XMVectorMultiplyAdd(camUp, DirectX::XMVectorReplicate(3 * scale), position));
    glTexCoord2i(1, 0);
    EmitVertex(DirectX::XMVectorMultiplyAdd(camRight, DirectX::XMVectorReplicate(3 * scale), position));
    glTexCoord2i(1, 1);
    EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camUp, DirectX::XMVectorReplicate(3 * scale), position));
    glTexCoord2i(0, 1);
    EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camRight, DirectX::XMVectorReplicate(3 * scale), position));
    glTexCoord2i(0, 0);
    EmitVertex(DirectX::XMVectorMultiplyAdd(camUp, DirectX::XMVectorReplicate(3 * scale), position));
    glTexCoord2i(1, 0);
    EmitVertex(DirectX::XMVectorMultiplyAdd(camRight, DirectX::XMVectorReplicate(3 * scale), position));
    glTexCoord2i(1, 1);
    EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camUp, DirectX::XMVectorReplicate(3 * scale), position));
    glTexCoord2i(0, 1);
    EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camRight, DirectX::XMVectorReplicate(3 * scale), position));

    glColor4f(0.6f, 0.2f, 0.1f, alphaValue);
    glTexCoord2i(0, 0);
    EmitVertex(DirectX::XMVectorMultiplyAdd(camUp, DirectX::XMVectorReplicate(1 * scale), position));
    glTexCoord2i(1, 0);
    EmitVertex(DirectX::XMVectorMultiplyAdd(camRight, DirectX::XMVectorReplicate(1 * scale), position));
    glTexCoord2i(1, 1);
    EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camUp, DirectX::XMVectorReplicate(1 * scale), position));
    glTexCoord2i(0, 1);
    EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camRight, DirectX::XMVectorReplicate(1 * scale), position));
    glTexCoord2i(0, 0);
    EmitVertex(DirectX::XMVectorMultiplyAdd(camUp, DirectX::XMVectorReplicate(1 * scale), position));
    glTexCoord2i(1, 0);
    EmitVertex(DirectX::XMVectorMultiplyAdd(camRight, DirectX::XMVectorReplicate(1 * scale), position));
    glTexCoord2i(1, 1);
    EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camUp, DirectX::XMVectorReplicate(1 * scale), position));
    glTexCoord2i(0, 1);
    EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camRight, DirectX::XMVectorReplicate(1 * scale), position));
  }

  void ReceiverBuilding::RenderUnprocessedSpirit_detail(DirectX::XMFLOAT3 const& _pos, float _life)
  {
    DirectX::XMVECTOR const position = DirectX::XMLoadFloat3(&_pos);

    // Hoisted out of the quads below, which each called them afresh.
    DirectX::XMFLOAT3 const camUpStore = g_camera->GetUp();
    DirectX::XMFLOAT3 const camRightStore = g_camera->GetRight();
    DirectX::XMVECTOR const camUp = DirectX::XMLoadFloat3(&camUpStore);
    DirectX::XMVECTOR const camRight = DirectX::XMLoadFloat3(&camRightStore);
    float scale = 2.0f * _life;
    float alphaValue = _life;

    glColor4f(0.6f, 0.2f, 0.1f, alphaValue / 1.5f);
    glTexCoord2i(0, 0);
    EmitVertex(DirectX::XMVectorMultiplyAdd(camUp, DirectX::XMVectorReplicate(30 * scale), position));
    glTexCoord2i(1, 0);
    EmitVertex(DirectX::XMVectorMultiplyAdd(camRight, DirectX::XMVectorReplicate(30 * scale), position));
    glTexCoord2i(1, 1);
    EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camUp, DirectX::XMVectorReplicate(30 * scale), position));
    glTexCoord2i(0, 1);
    EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camRight, DirectX::XMVectorReplicate(30 * scale), position));
  }


  void ReceiverBuilding::EndRenderUnprocessedSpirits()
  {
    g_camera->SetupProjectionMatrix(s_nearPlaneStart, g_renderer->GetFarPlane());

    glDisable(GL_TEXTURE_2D);
    glDepthMask(true);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
  }


  // ****************************************************************************
  // Class SpiritProcessor
  // ****************************************************************************

  SpiritProcessor::SpiritProcessor()
    : ReceiverBuilding(),
      m_throughput(0.0f),
      m_timerSync(0.0f),
      m_numThisSecond(0),
      m_spawnSync(0.0f)
  {
    m_type = TypeSpiritProcessor;
    SetShape(g_resource->GetShape("SpiritProcessor.shp"));
  }


  void SpiritProcessor::Initialise(Building* _building)
  {
    ReceiverBuilding::Initialise(_building);

    //
    // Spawn some unprocessed spirits

    for (int i = 0; i < 150; ++i)
    {
      float sizeX = g_location->m_landscape.GetWorldSizeX();
      float sizeZ = g_location->m_landscape.GetWorldSizeZ();
      float posY = syncfrand(1000.0f);
      // The two syncfrand calls stay in this order: they advance the RNG.
      DirectX::XMFLOAT3 const spawnPos(syncfrand(sizeX), posY, syncfrand(sizeZ));

      UnprocessedSpirit* spirit = new UnprocessedSpirit();
      spirit->m_pos = spawnPos;
      m_floatingSpirits.push_back(spirit);
    }
  }


  char const* SpiritProcessor::GetObjectiveCounter()
  {
    static char result[256];
    sprintf(result, "%s : %2.2f", LANGUAGEPHRASE("objective_throughput"), m_throughput);
    return result;
  }


  void SpiritProcessor::TriggerSpirit(float _initValue)
  {
    ReceiverBuilding::TriggerSpirit(_initValue);

    ++m_numThisSecond;
  }


  bool SpiritProcessor::IsInView() { return true; }


  bool SpiritProcessor::Advance()
  {
    //
    // Calculate our throughput

    m_timerSync -= SERVER_ADVANCE_PERIOD;

    if (m_timerSync <= 0.0f)
    {
      float newAverage = m_numThisSecond;
      newAverage *= 7.0f;
      m_numThisSecond = 0;
      m_timerSync = 10.0f;
      if (newAverage > m_throughput)
      {
        m_throughput = newAverage;
      }
      else
      {
        m_throughput = m_throughput * 0.8f + newAverage * 0.2f;
      }
    }

    if (m_throughput > 50.0f)
    {
      GlobalBuilding* gb = g_globalWorld->GetBuilding(m_id.GetUniqueId(), g_locationId);
      gb->m_online = true;
    }


    //
    // Advance all unprocessed spirits

    for (int i = 0; i < static_cast<int>(m_floatingSpirits.size()); ++i)
    {
      UnprocessedSpirit* spirit = m_floatingSpirits[i];
      bool finished = spirit->Advance();
      if (finished)
      {
        m_floatingSpirits.erase(m_floatingSpirits.begin() + i);
        delete spirit;
        --i;
      }
    }


    //
    // Spawn more unprocessed spirits

    m_spawnSync -= SERVER_ADVANCE_PERIOD;
    if (m_spawnSync <= 0.0f)
    {
      m_spawnSync = 0.2f;

      float sizeX = g_location->m_landscape.GetWorldSizeX();
      float sizeZ = g_location->m_landscape.GetWorldSizeZ();
      float posY = 700.0f + syncfrand(300.0f);
      // The two syncfrand calls stay in this order: they advance the RNG.
      DirectX::XMFLOAT3 const spawnPos(syncfrand(sizeX), posY, syncfrand(sizeZ));
      UnprocessedSpirit* spirit = new UnprocessedSpirit();
      spirit->m_pos = spawnPos;
      m_floatingSpirits.push_back(spirit);
    }

    return ReceiverBuilding::Advance();
  }


  void SpiritProcessor::Render(float _predictionTime)
  {
    ReceiverBuilding::Render(_predictionTime);

    // g_gameFont.DrawText3DCentre( m_pos + Vector3(0,215,0), 10.0f, "NumThisSecond : %d", m_numThisSecond );
    // g_gameFont.DrawText3DCentre( m_pos + Vector3(0,200,0), 10.0f, "Throughput    : %2.2f", m_throughput );
  }


  void SpiritProcessor::RenderAlphas(float _predictionTime)
  {
    ReceiverBuilding::RenderAlphas(_predictionTime);

    //
    // Render all floating spirits

    BeginRenderUnprocessedSpirits();

    _predictionTime -= SERVER_ADVANCE_PERIOD;

    for (int i = 0; i < static_cast<int>(m_floatingSpirits.size()); ++i)
    {
      UnprocessedSpirit* spirit = m_floatingSpirits[i];
      DirectX::XMVECTOR drift = DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&spirit->m_vel), DirectX::XMVectorReplicate(_predictionTime),
                                                             DirectX::XMLoadFloat3(&spirit->m_pos));
      drift = DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&spirit->m_hover), DirectX::XMVectorReplicate(_predictionTime), drift);
      DirectX::XMFLOAT3 pos;
      DirectX::XMStoreFloat3(&pos, drift);
      float life = spirit->GetLife();
      RenderUnprocessedSpirit(pos, life);
    }
    EndRenderUnprocessedSpirits();
  }


  // ****************************************************************************
  // Class ReceiverLink
  // ****************************************************************************

  ReceiverLink::ReceiverLink()
    : ReceiverBuilding()
  {
    m_type = TypeReceiverLink;
    SetShape(g_resource->GetShape("ReceiverLink.shp"));
  }


  bool ReceiverLink::Advance() { return ReceiverBuilding::Advance(); }


  // ****************************************************************************
  // Class ReceiverSpiritSpawner
  // ****************************************************************************

  ReceiverSpiritSpawner::ReceiverSpiritSpawner()
    : ReceiverBuilding()
  {
    m_type = TypeReceiverSpiritSpawner;
    SetShape(g_resource->GetShape("ReceiverLink.shp"));
  }


  bool ReceiverSpiritSpawner::Advance()
  {
    if (syncfrand(10.0f) < 1.0f)
    {
      TriggerSpirit(0.0f);
    }

    return ReceiverBuilding::Advance();
  }


  // ****************************************************************************
  // Class SpiritReceiver
  // ****************************************************************************

  SpiritReceiver::SpiritReceiver()
    : ReceiverBuilding(),
      m_headMarker(nullptr),
      m_headShape(nullptr),
      m_spiritLink(nullptr)
  {
    m_type = TypeSpiritReceiver;
    SetShape(g_resource->GetShape("SpiritReceiver.shp"));
    m_headMarker = m_shape->m_rootFragment->LookupMarker("MarkerHead");

    for (int i = 0; i < SPIRITRECEIVER_NUMSTATUSMARKERS; ++i)
    {
      char name[64];
      sprintf(name, "MarkerStatus0%d", i + 1);
      m_statusMarkers[i] = m_shape->m_rootFragment->LookupMarker(name);
    }

    m_headShape = g_resource->GetShape("SpiritReceiverHead.shp");
    m_spiritLink = m_headShape->m_rootFragment->LookupMarker("MarkerSpiritLink");
  }


  void SpiritReceiver::Initialise(Building* _template)
  {
    _template->m_up = g_location->m_landscape.m_normalMap->GetValue(_template->m_pos.x, _template->m_pos.z);
    DirectX::XMStoreFloat3(&_template->m_front, DirectX::XMVector3Cross(DirectX::g_XMIdentityR0, DirectX::XMLoadFloat3(&_template->m_up)));

    ReceiverBuilding::Initialise(_template);
  }


  bool SpiritReceiver::Advance()
  {
    float fractionOccupied = (float)GetNumPortsOccupied() / (float)GetNumPorts();

    //
    // Search for spirits nearby

    SpiritProcessor* processor = GetSpiritProcessor();
    if (processor && fractionOccupied > 0.0f)
    {
      for (int i = 0; i < static_cast<int>(processor->m_floatingSpirits.size()); ++i)
      {
        UnprocessedSpirit* spirit = processor->m_floatingSpirits[i];
        if (spirit->m_state == UnprocessedSpirit::StateUnprocessedFloating)
        {
          DirectX::XMVECTOR const spiritPos = DirectX::XMLoadFloat3(&spirit->m_pos);
          DirectX::XMVECTOR themToUs = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), spiritPos);
          float distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(themToUs));
          if (distance < 100.0f)
          {
            float fraction = 1.0f - distance / 100.0f;
            DirectX::XMVECTOR const targetPos =
              DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMVectorSet(0.0f, 100.0f * fraction, 0.0f, 0.0f));
            themToUs = DirectX::XMVectorSubtract(targetPos, spiritPos);
            distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(themToUs));
          }

          if (distance < 10.0f)
          {
            processor->m_floatingSpirits.erase(processor->m_floatingSpirits.begin() + i);
            delete spirit;
            --i;
            TriggerSpirit(0.0f);
          }
          else if (distance < 200.0f)
          {
            float fraction = 1.0f - distance / 200.0f;
            fraction *= fractionOccupied;
            // SetLength. distance is at least 10 in this branch, so the
            // zero-length fallback is unreachable and this takes the native
            // normalise.
            DirectX::XMVECTOR const pull = DirectX::XMVectorScale(DirectX::XMVector3Normalize(themToUs), 20.0f * fraction);
            DirectX::XMStoreFloat3(&spirit->m_vel, DirectX::XMVectorMultiplyAdd(pull, DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD),
                                                                                DirectX::XMLoadFloat3(&spirit->m_vel)));
          }
        }
      }
    }

    return ReceiverBuilding::Advance();
  }


  void SpiritReceiver::Render(float _predictionTime)
  {
    if (g_editing)
    {
      m_up = g_location->m_landscape.m_normalMap->GetValue(m_pos.x, m_pos.z);
      DirectX::XMStoreFloat3(&m_front, DirectX::XMVector3Cross(DirectX::g_XMIdentityR0, DirectX::XMLoadFloat3(&m_up)));
    }

    ReceiverBuilding::Render(_predictionTime);

    DirectX::XMFLOAT4X4 mat = GetWorldMatrix();

    DirectX::XMFLOAT3 const headPos = m_headMarker->GetWorldPosition(mat);

    // The head is deliberately levelled: world up, world right, and a front that
    // falls out of the two. Not the building's own basis.
    DirectX::XMVECTOR const front = DirectX::XMVector3Cross(DirectX::g_XMIdentityR1, DirectX::g_XMIdentityR0);

    DirectX::XMFLOAT4X4 headMat;
    DirectX::XMStoreFloat4x4(&headMat, BasisFromFrontAndUp(front, DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&headPos)));
    m_headShape->Render(_predictionTime, headMat);
  }


  DirectX::XMFLOAT3 SpiritReceiver::GetSpiritLocation()
  {
    DirectX::XMFLOAT4X4 mat = GetWorldMatrix();

    DirectX::XMFLOAT3 const headPos = m_headMarker->GetWorldPosition(mat);

    // The head is deliberately levelled: world up, world right, and a front that
    // falls out of the two. Not the building's own basis.
    DirectX::XMVECTOR const front = DirectX::XMVector3Cross(DirectX::g_XMIdentityR1, DirectX::g_XMIdentityR0);

    DirectX::XMFLOAT4X4 headMat;
    DirectX::XMStoreFloat4x4(&headMat, BasisFromFrontAndUp(front, DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&headPos)));

    return m_spiritLink->GetWorldPosition(headMat);
  }


  void SpiritReceiver::RenderPorts()
  {
    glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Starburst.bmp"));
    glDepthMask(false);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    for (int i = 0; i < GetNumPorts(); ++i)
    {
      DirectX::XMFLOAT4X4 rootMat = GetWorldMatrix();

      DirectX::XMFLOAT3 const statusPosStore = m_statusMarkers[i]->GetWorldPosition(rootMat);

      //
      // Render the status light

      float size = 6.0f;
      DirectX::XMFLOAT3 const camRightStore = g_camera->GetRight();
      DirectX::XMFLOAT3 const camUpStore = g_camera->GetUp();
      DirectX::XMVECTOR const camR = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&camRightStore), size);
      DirectX::XMVECTOR const camU = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&camUpStore), size);

      DirectX::XMVECTOR const statusPos = DirectX::XMLoadFloat3(&statusPosStore);

      if (GetPortOccupant(i).IsValid())
        glColor4f(0.3f, 1.0f, 0.3f, 1.0f);
      else
        glColor4f(1.0f, 0.3f, 0.3f, 1.0f);

      glBegin(GL_QUADS);
      glTexCoord2i(0, 0);
      EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorSubtract(statusPos, camR), camU));
      glTexCoord2i(1, 0);
      EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorAdd(statusPos, camR), camU));
      glTexCoord2i(1, 1);
      EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(statusPos, camR), camU));
      glTexCoord2i(0, 1);
      EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorSubtract(statusPos, camR), camU));
      glEnd();
    }

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
    glDepthMask(true);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_CULL_FACE);
  }


  void SpiritReceiver::RenderAlphas(float _predictionTime)
  {
    ReceiverBuilding::RenderAlphas(_predictionTime);

    // RenderHitCheck();

    float fractionOccupied = (float)GetNumPortsOccupied() / (float)GetNumPorts();
  }


  // ****************************************************************************
  // Class UnprocessedSpirit
  // ****************************************************************************

  UnprocessedSpirit::UnprocessedSpirit()
    : WorldObject()
  {
    m_state = StateUnprocessedFalling;

    m_positionOffset = syncfrand(10.0f);
    m_xaxisRate = syncfrand(2.0f);
    m_yaxisRate = syncfrand(2.0f);
    m_zaxisRate = syncfrand(2.0f);

    m_timeSync = GetHighResTime();
  }


  bool UnprocessedSpirit::Advance()
  {
    DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), 0.9f));

    //
    // Make me float around slowly

    m_positionOffset += SERVER_ADVANCE_PERIOD;
    m_xaxisRate += syncsfrand(1.0f);
    m_yaxisRate += syncsfrand(1.0f);
    m_zaxisRate += syncsfrand(1.0f);
    if (m_xaxisRate > 2.0f)
      m_xaxisRate = 2.0f;
    if (m_xaxisRate < 0.0f)
      m_xaxisRate = 0.0f;
    if (m_yaxisRate > 2.0f)
      m_yaxisRate = 2.0f;
    if (m_yaxisRate < 0.0f)
      m_yaxisRate = 0.0f;
    if (m_zaxisRate > 2.0f)
      m_zaxisRate = 2.0f;
    if (m_zaxisRate < 0.0f)
      m_zaxisRate = 0.0f;
    m_hover.x = sinf(m_positionOffset) * m_xaxisRate;
    m_hover.y = sinf(m_positionOffset) * m_yaxisRate;
    m_hover.z = sinf(m_positionOffset) * m_zaxisRate;

    switch (m_state)
    {
    case StateUnprocessedFalling:
    {
      float heightAboveGround = m_pos.y - g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);
      if (heightAboveGround > 15.0f)
      {
        float fractionAboveGround = heightAboveGround / 100.0f;
        fractionAboveGround = std::min(fractionAboveGround, 1.0f);
        m_hover.y = (-10.0f - syncfrand(10.0f)) * fractionAboveGround;
      }
      else
      {
        m_state = StateUnprocessedFloating;
        m_timeSync = 30.0f + syncsfrand(30.0f);
      }
      break;
    }

    case StateUnprocessedFloating:
      m_timeSync -= SERVER_ADVANCE_PERIOD;
      if (m_timeSync <= 0.0f)
      {
        m_state = StateUnprocessedDeath;
        m_timeSync = 10.0f;
      }
      break;

    case StateUnprocessedDeath:
      m_timeSync -= SERVER_ADVANCE_PERIOD;
      if (m_timeSync <= 0.0f)
        return true;
      break;
    }

    DirectX::XMFLOAT3 oldPos = m_pos;

    DirectX::XMVECTOR drift =
      DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD), DirectX::XMLoadFloat3(&m_pos));
    drift = DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_hover), DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD), drift);
    DirectX::XMStoreFloat3(&m_pos, drift);
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


  float UnprocessedSpirit::GetLife()
  {
    switch (m_state)
    {
    case StateUnprocessedFalling:
    {
      float timePassed = GetHighResTime() - m_timeSync;
      float life = timePassed / 10.0f;
      life = std::min(life, 1.0f);
      return life;
    }

    case StateUnprocessedFloating:
      return 1.0f;

    case StateUnprocessedDeath:
    {
      float timeLeft = m_timeSync;
      float life = timeLeft / 10.0f;
      life = std::min(life, 1.0f);
      life = std::max(life, 0.0f);
      return life;
    }
    }

    return 0.0f;
  }
} // namespace Species
