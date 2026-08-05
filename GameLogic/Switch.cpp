#include "pch.h"
#include "GlVertex.h"

#include <math.h>

#include "DebugRender.h"
#include "Debug.h"
#include "FileWriter.h"
#include "Profiler.h"
#include "Resource.h"
#include "Shape.h"
#include "TextStreamReaders.h"

#include "ClientToServer.h"

#include "Location.h"
#include "GlobalWorld.h"

#include "Building.h"
#include "LaserFence.h"
#include "Switch.h"
#include "Generator.h"
#include "WorldPointers.h"
#include "AppState.h"


// ****************************************************************************
// Class FenceSwitch
// ****************************************************************************

// *** Constructor
FenceSwitch::FenceSwitch()
  : Building(),
    m_linkedBuildingId(-1),
    m_linkedBuildingId2(-1),
    m_switchValue(-1),
    m_switchable(false),
    m_timer(20.0f),
    m_locked(false),
    m_lockable(0),
    m_connectionLocation(nullptr)
{
  m_type = Building::TypeFenceSwitch;
  SetShape(g_resource->GetShape("FenceSwitch.shp"));
  m_script = "none";
}


// *** Initialise
void FenceSwitch::Initialise(Building* _template)
{
  Building::Initialise(_template);
  DEBUG_ASSERT(_template->m_type == Building::TypeFenceSwitch);
  m_linkedBuildingId = ((FenceSwitch*)_template)->m_linkedBuildingId;
  m_linkedBuildingId2 = ((FenceSwitch*)_template)->m_linkedBuildingId2;
  m_switchValue = ((FenceSwitch*)_template)->m_switchValue;
  m_lockable = ((FenceSwitch*)_template)->m_lockable;
  m_locked = ((FenceSwitch*)_template)->m_locked;
  m_script = ((FenceSwitch*)_template)->m_script;
}

void FenceSwitch::SetDetail(int _detail)
{
  if (m_timer < 10.0f)
  {
    m_timer = 10.0f;
  }
  Building::SetDetail(_detail);
}

// *** Advance
bool FenceSwitch::Advance()
{
  if (m_timer > 0.0f)
  {
    m_timer -= SERVER_ADVANCE_PERIOD;
    return Building::Advance();
  }

  bool friendly = true;
  for (int i = 0; i < GetNumPorts(); ++i)
  {
    if (m_ports[i]->m_occupant.GetTeamId() == 1)
    {
      friendly = false;
      break;
    }
  }

  if (friendly && GetNumPorts() == GetNumPortsOccupied())
  {
    SetTeamId(2);
  }
  else
  {
    SetTeamId(1);
  }

  Building* b = g_location->GetBuilding(m_linkedBuildingId);

  bool switched = false;

  if (!m_locked)
  {
    if (b->m_type == Building::TypeLaserFence)
    {
      LaserFence* fence = (LaserFence*)b;

      if (GetNumPorts() == GetNumPortsOccupied())
      {
        GlobalBuilding* gb = g_globalWorld->GetBuilding(m_id.GetUniqueId(), g_locationId);
        if (gb && !gb->m_online)
        {
          if (friendly)
          {
            m_switchable = true;
            if (m_switchValue == m_linkedBuildingId)
            {
              if (!fence->IsEnabled())
              {
                fence->Enable();
                switched = true;
              }

              Building* b2 = g_location->GetBuilding(m_linkedBuildingId2);
              if (b2 && b2->m_type == Building::TypeLaserFence)
              {
                LaserFence* fence2 = (LaserFence*)b2;
                if (fence2->IsEnabled())
                {
                  fence2->Disable();
                }
              }
            }
            else
            {
              if (fence->IsEnabled())
              {
                fence->Disable();
                switched = true;
              }

              Building* b2 = g_location->GetBuilding(m_linkedBuildingId2);
              if (b2 && b2->m_type == Building::TypeLaserFence)
              {
                LaserFence* fence2 = (LaserFence*)b2;
                if (!fence2->IsEnabled())
                {
                  fence2->Enable();
                }
              }
            }
          }
          else
          {
            m_switchable = false;
            m_switchValue = m_linkedBuildingId;

            if (!fence->IsEnabled())
            {
              fence->Enable();
              switched = true;
            }

            Building* b2 = g_location->GetBuilding(m_linkedBuildingId2);
            if (b2 && b2->m_type == Building::TypeLaserFence)
            {
              LaserFence* fence2 = (LaserFence*)b2;
              if (fence2->IsEnabled())
              {
                fence2->Disable();
              }
            }
          }
          gb->m_online = true;
          g_globalWorld->EvaluateEvents();
        }
      }
      else
      {
        GlobalBuilding* gb = g_globalWorld->GetBuilding(m_id.GetUniqueId(), g_locationId);
        if (gb && gb->m_online)
        {
          m_switchValue = m_linkedBuildingId;
          m_switchable = false;

          if (m_linkedBuildingId2 == -1)
          {
            if (fence->IsEnabled())
            {
              fence->Disable();
              switched = true;
            }
          }
          else
          {
            if (!fence->IsEnabled())
            {
              fence->Enable();
              switched = true;
            }

            Building* b2 = g_location->GetBuilding(m_linkedBuildingId2);
            if (b2 && b2->m_type == Building::TypeLaserFence)
            {
              LaserFence* fence2 = (LaserFence*)b2;
              if (fence2->IsEnabled())
              {
                fence2->Disable();
              }
            }
          }
          gb->m_online = false;
          g_globalWorld->EvaluateEvents();
        }
      }
    }
  }

  if (switched)
  {
    if (m_script.find(".txt") != std::string::npos)
    {
      g_script->RunScript(m_script.c_str());
    }
    if (m_lockable)
    {
      m_locked = true;
    }
  }

  return Building::Advance();
}


// *** Render
void FenceSwitch::Render(float predictionTime) { Building::Render(predictionTime); }

void FenceSwitch::RenderAlphas(float predictionTime)
{
  Building* building = g_location->GetBuilding(m_linkedBuildingId);
  if (building && building->m_type == Building::TypeLaserFence)
  {
    LaserFence* fence = (LaserFence*)building;
    RenderConnection(fence->GetTopPosition(), m_switchValue == m_linkedBuildingId);
  }

  building = g_location->GetBuilding(m_linkedBuildingId2);
  if (building && building->m_type == Building::TypeLaserFence)
  {
    LaserFence* fence = (LaserFence*)building;
    RenderConnection(fence->GetTopPosition(), m_switchValue == m_linkedBuildingId2);
  }
}

void FenceSwitch::RenderConnection(DirectX::XMFLOAT3 _targetPos, bool _active)
{
  DirectX::XMFLOAT3 const ourPosStore = GetConnectionLocation();
  DirectX::XMVECTOR const ourPos = DirectX::XMLoadFloat3(&ourPosStore);
  DirectX::XMVECTOR const theirPos = DirectX::XMLoadFloat3(&_targetPos);
  DirectX::XMVECTOR const alongLink = DirectX::XMVectorSubtract(theirPos, ourPos);

  DirectX::XMFLOAT3 const cameraPosStore = g_camera->GetPos();
  DirectX::XMVECTOR const cameraPos = DirectX::XMLoadFloat3(&cameraPosStore);

  // SetLength; see the note on the same call in Generator.cpp. Rendering only,
  // and degenerate only with the camera exactly on the link, so this takes the
  // native normalise rather than reproducing the (2, 0, 0) fallback.
  DirectX::XMVECTOR const ourPosRight =
    DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::XMVectorSubtract(cameraPos, ourPos), alongLink)), 2.0f);
  DirectX::XMVECTOR const theirPosRight =
    DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::XMVectorSubtract(cameraPos, theirPos), alongLink)), 2.0f);

  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glDepthMask(false);

  if (_active)
  {
    glColor4f(0.9f, 0.9f, 0.5f, 1.0f);
  }
  else
  {
    glColor4f(0.5f, 0.5f, 0.5f, 1.0f);
  }

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Laser.bmp"));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

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
}

// *** GetBuildingLink
int FenceSwitch::GetBuildingLink() { return m_linkedBuildingId; }


// *** SetBuildingLink
void FenceSwitch::SetBuildingLink(int _buildingId)
{
  if (m_linkedBuildingId == -1)
  {
    m_linkedBuildingId = _buildingId;
  }
  else if (m_linkedBuildingId2 == -1)
  {
    m_linkedBuildingId2 = _buildingId;
  }
  else
  {
    m_linkedBuildingId = _buildingId;
  }
}


// *** Read
void FenceSwitch::Read(TextReader* _in, bool _dynamic)
{
  Building::Read(_in, _dynamic);

  m_linkedBuildingId = atoi(_in->GetNextToken());
  m_linkedBuildingId2 = atoi(_in->GetNextToken());
  m_switchValue = atoi(_in->GetNextToken());
  m_lockable = atoi(_in->GetNextToken());
  m_script = _in->GetNextToken();
  if (_in->TokenAvailable())
  {
    int locked = atoi(_in->GetNextToken());
    if (locked == 1)
    {
      m_locked = true;
    }
  }

  if (m_switchValue == -1)
  {
    m_switchValue = m_linkedBuildingId;
  }
}


// *** Write
void FenceSwitch::Write(FileWriter* _out)
{
  Building::Write(_out);

  _out->printf("%-8d", m_linkedBuildingId);
  _out->printf("%-8d", m_linkedBuildingId2);
  _out->printf("%-8d", m_switchValue);

  _out->printf("%-3d", m_lockable);

  _out->printf("%s  ", m_script.c_str());

  int locked = m_locked ? 1 : 0;
  _out->printf("%-3d", locked);
}

void FenceSwitch::RenderLink()
{
#ifdef DEBUG_RENDER_ENABLED

  Building::RenderLink();

  int buildingId = m_linkedBuildingId2;
  if (buildingId != -1)
  {
    Building* linkBuilding = g_location->GetBuilding(buildingId);
    if (linkBuilding)
    {
      DirectX::XMFLOAT3 const start(m_pos.x, m_pos.y + 10.0f, m_pos.z);
      DirectX::XMFLOAT3 const end(linkBuilding->m_pos.x, linkBuilding->m_pos.y + 10.0f, linkBuilding->m_pos.z);
      RenderArrow(start, end, 6.0f, RGBAColour(255, 0, 0));
    }
  }
#endif
}

void FenceSwitch::Switch()
{
  if (!m_switchable)
    return;


  if ((m_lockable == 1 && !m_locked) || !m_lockable)
  {
    if (m_switchValue == m_linkedBuildingId)
    {
      m_switchValue = m_linkedBuildingId2;
    }
    else
    {
      m_switchValue = m_linkedBuildingId;
    }

    GlobalBuilding* gb = g_globalWorld->GetBuilding(m_id.GetUniqueId(), g_locationId);
    gb->m_online = false;
    g_globalWorld->EvaluateEvents();
    /*if( strstr( m_script, ".txt" ) )
    {
        g_script->RunScript( m_script );
    }*/
  }
}

void FenceSwitch::RenderLights()
{
  if (m_id.GetTeamId() != 255 && static_cast<int>(m_lights.size()) > 0)
  {
    if ((g_clientToServer->m_lastValidSequenceIdFromServer % 10) / 2 == m_id.GetTeamId() || g_editing)
    {
      for (int i = 0; i < static_cast<int>(m_lights.size()); ++i)
      {
        ShapeMarker* marker = m_lights[i];
        DirectX::XMFLOAT4X4 rootMat = GetWorldMatrix();

        // ShapeMarker::GetWorldMatrix still returns Matrix34 -- T10's seam.
        DirectX::XMFLOAT3 const lightPosStore = marker->GetWorldMatrix(rootMat).pos;
        DirectX::XMVECTOR const lightPos = DirectX::XMLoadFloat3(&lightPosStore);

        float signalSize = 6.0f;

        DirectX::XMFLOAT3 const camRightStore = g_camera->GetRight();
        DirectX::XMFLOAT3 const camUpStore = g_camera->GetUp();
        DirectX::XMVECTOR const camR = DirectX::XMLoadFloat3(&camRightStore);
        DirectX::XMVECTOR const camU = DirectX::XMLoadFloat3(&camUpStore);

        if (m_switchValue == m_linkedBuildingId)
        {
          glColor3f(0.0f, 1.0f, 0.0f);
        }
        else
        {
          glColor3f(1.0f, 0.0f, 0.0f);
        }

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

          DirectX::XMVECTOR const right = DirectX::XMVectorScale(camR, size);
          DirectX::XMVECTOR const up = DirectX::XMVectorScale(camU, size);

          glBegin(GL_QUADS);
          glTexCoord2f(0.0f, 0.0f);
          EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorSubtract(lightPos, right), up));
          glTexCoord2f(1.0f, 0.0f);
          EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorAdd(lightPos, right), up));
          glTexCoord2f(1.0f, 1.0f);
          EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(lightPos, right), up));
          glTexCoord2f(0.0f, 1.0f);
          EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorSubtract(lightPos, right), up));
          glEnd();
        }

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_BLEND);

        glDepthMask(true);
        glEnable(GL_CULL_FACE);
        glDisable(GL_TEXTURE_2D);
      }
    }
  }
}

DirectX::XMFLOAT3 FenceSwitch::GetConnectionLocation()
{
  if (!m_connectionLocation)
  {
    m_connectionLocation = m_shape->m_rootFragment->LookupMarker("MarkerConnectionLocation");
    DEBUG_ASSERT(m_connectionLocation);
  }

  DirectX::XMFLOAT4X4 rootMat = GetWorldMatrix();

  // ShapeMarker::GetWorldMatrix still returns Matrix34 -- T10's seam.
  return m_connectionLocation->GetWorldMatrix(rootMat).pos;
}

bool FenceSwitch::IsInView()
{
  if (m_linkedBuildingId != -1)
  {
    DirectX::XMVECTOR const startPoint = DirectX::XMLoadFloat3(&m_pos);
    Building* b = g_location->GetBuilding(m_linkedBuildingId);
    if (b)
    {
      DirectX::XMVECTOR const endPoint = DirectX::XMLoadFloat3(&b->m_pos);

      DirectX::XMFLOAT3 midPoint;
      DirectX::XMStoreFloat3(&midPoint, DirectX::XMVectorScale(DirectX::XMVectorAdd(startPoint, endPoint), 0.5f));

      float radius = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(startPoint, endPoint))) / 2.0f;
      radius += m_radius;

      if (g_camera->SphereInViewFrustum(midPoint, radius))
      {
        return true;
      }
    }
  }

  if (m_linkedBuildingId2 != -1)
  {
    DirectX::XMVECTOR const startPoint = DirectX::XMLoadFloat3(&m_pos);
    Building* b = g_location->GetBuilding(m_linkedBuildingId2);
    if (b)
    {
      DirectX::XMVECTOR const endPoint = DirectX::XMLoadFloat3(&b->m_pos);

      DirectX::XMFLOAT3 midPoint;
      DirectX::XMStoreFloat3(&midPoint, DirectX::XMVectorScale(DirectX::XMVectorAdd(startPoint, endPoint), 0.5f));

      float radius = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(startPoint, endPoint))) / 2.0f;
      radius += m_radius;

      if (g_camera->SphereInViewFrustum(midPoint, radius))
      {
        return true;
      }
    }
  }

  return Building::IsInView();
}