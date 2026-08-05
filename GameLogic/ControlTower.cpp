#include "pch.h"
#include "GlVertex.h"
#include "SoundSources.h"

#include <math.h>

#include "Debug.h"
#include "FileWriter.h"
#include "Profiler.h"
#include "Resource.h"
#include "Shape.h"
#include "TextStreamReaders.h"
#include "MathUtils.h"
#include "HiResTime.h"

#include "ClientToServer.h"

#include "SoundSystem.h"

#include "Location.h"
#include "Team.h"
#include "GameTime.h"
#include "ParticleSystem.h"
#include "GlobalWorld.h"

#include "ControlTower.h"
#include "TrunkPort.h"
#include "WorldPointers.h"
#include "AppState.h"


namespace Species
{
  Shape* ControlTower::s_dishShape = nullptr;


  ControlTower::ControlTower()
    : Building(),
      m_ownership(100.0f),
      m_controlBuildingId(-1),
      m_checkTargetTimer(0.0f)
  {
    m_radius = 4.0f;
    m_type = TypeControlTower;

    SetShape(g_resource->GetShape("ControlTower.shp"));
    m_lightPos = m_shape->m_rootFragment->LookupMarker("MarkerLightPos");
    m_dishPos = m_shape->m_rootFragment->LookupMarker("MarkerDishPos");

    for (int i = 0; i < 3; ++i)
    {
      m_beingReprogrammed[i] = false;
      const std::string reprogrammerName = std::format("MarkerReprogrammer{}", i);
      m_reprogrammer[i] = m_shape->m_rootFragment->LookupMarker(reprogrammerName.c_str());

      const std::string consoleName = std::format("MarkerConsole{}", i);
      m_console[i] = m_shape->m_rootFragment->LookupMarker(consoleName.c_str());
    }

    if (!s_dishShape)
    {
      s_dishShape = g_resource->GetShape("ControlTowerDish.shp");
    }
  }

  void ControlTower::Initialise(Building* _template)
  {
    Building::Initialise(_template);

    m_controlBuildingId = ((ControlTower*)_template)->m_controlBuildingId;
  }


  // See the call in Advance: the legacy `m_dishMatrix == Matrix34()` was an
  // indeterminate read, and this is the question it was reaching for. Exact
  // comparison against zero rather than NearlyEquals, because the header braces
  // the member to exactly zero and only this function's caller ever writes it.
  static bool IsUncomputed(DirectX::XMFLOAT4X4 const& _matrix)
  {
    for (int row = 0; row < 4; ++row)
    {
      for (int column = 0; column < 4; ++column)
      {
        if (_matrix.m[row][column] != 0.0f)
          return false;
      }
    }
    return true;
  }

  bool ControlTower::Advance()
  {
    //
    // Calculate our Dish matrix (once only, during first Advance)
    // Also look to see if our building is already captured

    // Legacy read: `m_dishMatrix == Matrix34()`, where Matrix34's default
    // constructor leaves the matrix UNINITIALISED. That comparison was reading
    // indeterminate memory on both sides and only worked because a fresh
    // allocation happens to be zero. m_dishMatrix is braced to zero in the header
    // now and this asks the question the old code meant to ask -- have we
    // computed the dish basis yet -- without the indeterminate read.
    if (IsUncomputed(m_dishMatrix))
    {
      Building* targetBuilding = g_location->GetBuilding(m_controlBuildingId);
      if (targetBuilding)
      {
        DirectX::XMFLOAT4X4 mat;
        DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

        DirectX::XMFLOAT3 const dishPosStore = m_dishPos->GetWorldPosition(mat);
        DirectX::XMVECTOR const dishPos = DirectX::XMLoadFloat3(&dishPosStore);

        DirectX::XMVECTOR const dishFront =
          DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(dishPos, DirectX::XMLoadFloat3(&targetBuilding->m_centrePos)));
        DirectX::XMVECTOR const dishRight = DirectX::XMVector3Cross(dishFront, DirectX::g_XMIdentityR1);
        DirectX::XMVECTOR const dishUp = DirectX::XMVector3Cross(dishRight, dishFront);
        DirectX::XMStoreFloat4x4(&m_dishMatrix, BasisFromFrontAndUp(dishFront, dishUp, dishPos));

        if (targetBuilding->m_id.GetTeamId() == 255)
        {
          m_ownership = 0.0f;
        }
        else
        {
          m_ownership = 100.0f;
        }
      }
    }


    //
    // If we are connected to a TrunkPort, somebody else may have opened that port from another map.
    // Every once in a while, check and turn on if it has happened

    m_checkTargetTimer -= SERVER_ADVANCE_PERIOD;

    if (m_checkTargetTimer <= 0.0f)
    {
      Building* building = g_location->GetBuilding(m_controlBuildingId);
      if (building && building->m_type == TypeTrunkPort && m_id.GetTeamId() != 2)
      {
        TrunkPort* tp = (TrunkPort*)building;
        if (tp->m_openTimer > 0.0f)
        {
          m_id.SetTeamId(2);
          m_ownership = 100.0f;
        }
      }
      m_checkTargetTimer = 2.0f;
    }


    //
    // Spawn particles if we are being reprogrammed

    for (int i = 0; i < 3; ++i)
    {
      if (m_beingReprogrammed[i])
      {
        DirectX::XMFLOAT4X4 rootMat;
        DirectX::XMStoreFloat4x4(&rootMat,
                                 BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

        DirectX::XMFLOAT3 const consolePos = m_console[i]->GetWorldPosition(rootMat);

        // The three sfrand calls stay in this order: they advance the RNG.
        DirectX::XMFLOAT3 const jitter(sfrand() * 10.0f, sfrand() * 5.0f, sfrand() * 10.0f);
        DirectX::XMFLOAT3 particleVel;
        DirectX::XMStoreFloat3(&particleVel,
                               DirectX::XMVectorAdd(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&consolePos), DirectX::XMLoadFloat3(&m_pos)),
                                                    DirectX::XMLoadFloat3(&jitter)));
        g_particleSystem->CreateParticle(consolePos, particleVel, Particle::TypeBlueSpark);
      }
    }

    return Building::Advance();
  }

  int ControlTower::GetAvailablePosition(DirectX::XMFLOAT3& _pos, DirectX::XMFLOAT3& _front)
  {
    for (int i = 0; i < 3; ++i)
    {
      if (!m_beingReprogrammed[i])
      {
        DirectX::XMFLOAT4X4 rootMat;
        DirectX::XMStoreFloat4x4(&rootMat,
                                 BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

        DirectX::XMFLOAT4X4 const worldMat = m_reprogrammer[i]->GetWorldMatrix(rootMat);

        _pos = DirectX::XMFLOAT3(worldMat._41, worldMat._42, worldMat._43);
        _front = DirectX::XMFLOAT3(worldMat._31, worldMat._32, worldMat._33);

        return i;
      }
    }

    return -1;
  }


  void ControlTower::GetConsolePosition(int _position, DirectX::XMFLOAT3& _pos)
  {
    DEBUG_ASSERT(_position >= 0 && _position < 3);

    DirectX::XMFLOAT4X4 rootMat;
    DirectX::XMStoreFloat4x4(&rootMat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

    _pos = m_console[_position]->GetWorldPosition(rootMat);
  }


  void ControlTower::BeginReprogram(int _position)
  {
    DEBUG_ASSERT(!m_beingReprogrammed[_position]);
    m_beingReprogrammed[_position] = true;
  }


  bool ControlTower::Reprogram(int _teamId)
  {
    if (_teamId != m_id.GetTeamId())
    {
      // Removing someone elses control
      m_ownership -= 0.1f;
      if (m_ownership <= 0.0f)
      {
        m_id.SetTeamId(_teamId);
        Building* targetBuilding = g_location->GetBuilding(m_controlBuildingId);
        if (targetBuilding && targetBuilding->m_id.GetTeamId() != m_id.GetTeamId())
        {
          targetBuilding->SetTeamId(m_id.GetTeamId());
        }
      }
    }
    else
    {
      // Increasing our own control
      if (m_ownership < 100.0f)
      {
        m_ownership += 0.1f;
        if (m_ownership > 100.0f)
          m_ownership = 100.0f;

        Building* targetBuilding = g_location->GetBuilding(m_controlBuildingId);
        if (targetBuilding)
        {
          targetBuilding->Reprogram(m_ownership);

          if (m_ownership == 100.0f)
          {
            g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), "ReprogramComplete");
            // g_app->m_sepulveda->Say("building_captured");
            targetBuilding->ReprogramComplete();
            SetTeamId(_teamId);
            g_globalWorld->m_research->GiveResearchPoints(GLOBALRESEARCH_POINTS_CONTROLTOWER);

            GlobalBuilding* gb = g_globalWorld->GetBuilding(m_id.GetUniqueId(), g_locationId);
            if (gb)
            {
              gb->m_online = true;
              g_globalWorld->EvaluateEvents();
            }


            return true;
          }
        }
      }
      else
      {
        return true;
      }
    }

    return false;
  }


  void ControlTower::EndReprogram(int _position)
  {
    DEBUG_ASSERT(m_beingReprogrammed[_position]);
    m_beingReprogrammed[_position] = false;
  }


  void ControlTower::Render(float _predictionTime)
  {
    Building::Render(_predictionTime);

    //
    // Render our dish

    s_dishShape->Render(_predictionTime, m_dishMatrix);
  }


  bool ControlTower::IsInView()
  {
    if (Building::IsInView())
      return true;

    //
    // Check against the tall thin control line to heaven

    DirectX::XMFLOAT3 const towerPos(m_pos.x, g_camera->GetPos().y, m_pos.z);
    return g_camera->PosInViewFrustum(towerPos);
  }


  void ControlTower::RenderAlphas(float _predictionTime)
  {
    Building::RenderAlphas(_predictionTime);


    //
    // Control lines will be bright when we are near a control tower
    // And dim when we are not
    // Recalculate our distance to the nearest control tower once per second

    static int s_lastRecalculation = 0.0f;
    static float s_distanceScale = 0.0f;
    static float s_desiredDistanceScale = 0.0f;

    if ((int)GetHighResTime() > s_lastRecalculation)
    {
      s_lastRecalculation = (int)GetHighResTime();

      float nearest = 99999.9f;
      for (int i = 0; i < g_location->m_buildings.Size(); ++i)
      {
        if (g_location->m_buildings.ValidIndex(i))
        {
          Building* building = g_location->m_buildings[i];
          if (building && building->m_type == TypeControlTower)
          {
            DirectX::XMFLOAT3 const cameraPos = g_camera->GetPos();
            float camDist = DirectX::XMVectorGetX(
              DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&building->m_pos), DirectX::XMLoadFloat3(&cameraPos))));
            if (camDist < nearest)
              nearest = camDist;
          }
        }
      }

      if (nearest < 200.0f)
        s_desiredDistanceScale = 1.0f;
      else
        s_desiredDistanceScale = 0.1f;
    }

    if (s_desiredDistanceScale > s_distanceScale)
    {
      s_distanceScale = (s_desiredDistanceScale * SERVER_ADVANCE_PERIOD * 0.1f) + (s_distanceScale * (1.0f - SERVER_ADVANCE_PERIOD * 0.1f));
    }
    else
    {
      s_distanceScale = (s_desiredDistanceScale * SERVER_ADVANCE_PERIOD * 0.03f) + (s_distanceScale * (1.0f - SERVER_ADVANCE_PERIOD * 0.03f));
    }


    //
    // Pre-compute some positions and shit

    DirectX::XMFLOAT4X4 rootMat;
    DirectX::XMStoreFloat4x4(&rootMat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

    DirectX::XMFLOAT3 const lightPosStore = m_lightPos->GetWorldPosition(rootMat);
    DirectX::XMVECTOR const lightPos = DirectX::XMLoadFloat3(&lightPosStore);

    DirectX::XMFLOAT3 const camRightStore = g_camera->GetRight();
    DirectX::XMFLOAT3 const camUpStore = g_camera->GetUp();
    DirectX::XMVECTOR const camR = DirectX::XMLoadFloat3(&camRightStore);
    DirectX::XMVECTOR const camU = DirectX::XMLoadFloat3(&camUpStore);

    RGBAColour colour;
    if (m_id.GetTeamId() == 255)
    {
      colour.Set(128, 128, 128, 255);
    }
    else
    {
      colour = g_location->m_teams[m_id.GetTeamId()].m_colour;
    }


    //
    // Draw control line to heaven

    if (!g_editing)
    {
      DirectX::XMVECTOR const controlUp = DirectX::XMVectorSet(0.0f, 50.0f + (m_id.GetUniqueId() % 50), 0.0f, 0.0f);

      glDisable(GL_CULL_FACE);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE);
      glShadeModel(GL_SMOOTH);
      glDepthMask(false);

      glEnable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Laser.bmp"));
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

      DirectX::XMFLOAT3 const cameraPos = g_camera->GetPos();
      float w = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(lightPos, DirectX::XMLoadFloat3(&cameraPos)))) * 0.002f;
      w = std::max(0.5f, w);

      for (int i = 0; i < 10; ++i)
      {
        DirectX::XMVECTOR const baseUp = DirectX::XMVectorSet(0.0f, -20.0f, 0.0f, 0.0f);
        DirectX::XMVECTOR const thisUp1 = DirectX::XMVectorMultiplyAdd(controlUp, DirectX::XMVectorReplicate((float)i), baseUp);
        DirectX::XMVECTOR const thisUp2 = DirectX::XMVectorMultiplyAdd(controlUp, DirectX::XMVectorReplicate((float)(i + 1)), baseUp);

        int alpha = 255 - 255 * (float)i / 10.0f;
        int alpha2 = 255 - 255 * (float)(i + 1) / 10.0f;

        alpha *= fabs(sinf(g_gameTime * 2 + (float)i / 5.0f));
        alpha2 *= fabs(sinf(g_gameTime * 2 + (float)(i + 1) / 5.0f));

        alpha *= s_distanceScale;
        alpha2 *= s_distanceScale;

        float y = (float)i / 10.0f;
        float h = 1.0f / 10.0f;

        glBegin(GL_QUADS);
        glColor4ub(colour.r, colour.g, colour.b, alpha);

        DirectX::XMVECTOR const halfWidth = DirectX::XMVectorScale(camR, w);

        glTexCoord2f(y, 0);
        EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorSubtract(lightPos, halfWidth), thisUp1));
        glTexCoord2f(y, 1);
        EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(lightPos, halfWidth), thisUp1));

        glColor4ub(colour.r, colour.g, colour.b, alpha2);

        glTexCoord2f(y + h, 1);
        EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(lightPos, halfWidth), thisUp2));
        glTexCoord2f(y + h, 0);
        EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorSubtract(lightPos, halfWidth), thisUp2));
        glEnd();
      }

      glDisable(GL_TEXTURE_2D);

      glDepthMask(true);
      glShadeModel(GL_FLAT);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDisable(GL_BLEND);
      glEnable(GL_CULL_FACE);
    }


    //
    // Draw our signal flash

    int lastSeqId = g_clientToServer->m_lastValidSequenceIdFromServer;

    if ((m_id.GetTeamId() != 255 && (lastSeqId % 10) / 2 == m_id.GetTeamId()) || m_beingReprogrammed[lastSeqId % 3] || g_editing)
    {
      // Shadows the outer lightPos, exactly as the legacy local did.
      DirectX::XMFLOAT4X4 signalRootMat;
      DirectX::XMStoreFloat4x4(&signalRootMat,
                               BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

      DirectX::XMFLOAT3 const signalPosStore = m_lightPos->GetWorldPosition(signalRootMat);
      DirectX::XMVECTOR const signalPos = DirectX::XMLoadFloat3(&signalPosStore);

      float signalSize = m_ownership / 5.0f;

      glColor4ub(colour.r, colour.g, colour.b, 255);

      glEnable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Starburst.bmp"));
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
      glDisable(GL_CULL_FACE);
      glDepthMask(false);

      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE);

      for (int i = 0; i < 10; ++i)
      {
        float size = signalSize * (float)i / 10.0f;
        glBegin(GL_QUADS);
        DirectX::XMVECTOR const right = DirectX::XMVectorScale(camR, size);
        DirectX::XMVECTOR const up = DirectX::XMVectorScale(camU, size);

        glTexCoord2f(0.0f, 0.0f);
        EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorSubtract(signalPos, right), up));
        glTexCoord2f(1.0f, 0.0f);
        EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorAdd(signalPos, right), up));
        glTexCoord2f(1.0f, 1.0f);
        EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(signalPos, right), up));
        glTexCoord2f(0.0f, 1.0f);
        EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorSubtract(signalPos, right), up));
        glEnd();
      }

      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDisable(GL_BLEND);

      glDepthMask(true);
      glEnable(GL_CULL_FACE);
      glDisable(GL_TEXTURE_2D);
    }
  }


  void ControlTower::ListSoundEvents(std::vector<const char*>* _list) { Building::ListSoundEvents(_list); }


  void ControlTower::Read(TextReader* _in, bool _dynamic)
  {
    Building::Read(_in, _dynamic);

    char* word = _in->GetNextToken();
    m_controlBuildingId = atoi(word);
  }

  void ControlTower::Write(FileWriter* _out)
  {
    Building::Write(_out);

    _out->printf("{:6d}", m_controlBuildingId);
  }

  int ControlTower::GetBuildingLink() { return m_controlBuildingId; }

  void ControlTower::SetBuildingLink(int _buildingId) { m_controlBuildingId = _buildingId; }
} // namespace Species
