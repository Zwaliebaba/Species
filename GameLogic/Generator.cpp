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
#include "LanguageTable.h"

#include "Generator.h"
#include "ConstructionYard.h"
#include "Citizen.h"
#include "ControlTower.h"
#include "Rocket.h"
#include "Switch.h"

#include "Location.h"
#include "GlobalWorld.h"
#include "GameTime.h"

#include "SoundSystem.h"
#include "WorldPointers.h"
#include "AppState.h"


// ****************************************************************************
// Class PowerBuilding
// ****************************************************************************

PowerBuilding::PowerBuilding()
  : Building(),
    m_powerLink(-1),
    m_powerLocation(nullptr)
{
}

void PowerBuilding::Initialise(Building* _template)
{
  Building::Initialise(_template);
  m_powerLink = ((PowerBuilding*)_template)->m_powerLink;
}

DirectX::XMFLOAT3 PowerBuilding::GetPowerLocation()
{
  if (!m_powerLocation)
  {
    m_powerLocation = m_shape->m_rootFragment->LookupMarker("MarkerPowerLocation");
    DEBUG_ASSERT(m_powerLocation);
  }

  // ShapeMarker::GetWorldMatrix still returns Matrix34 -- T10's seam.
  DirectX::XMFLOAT4X4 rootMat = GetWorldMatrix();
  return m_powerLocation->GetWorldMatrix(rootMat).pos;
}


bool PowerBuilding::IsInView()
{
  Building* powerLink = g_location->GetBuilding(m_powerLink);

  if (powerLink)
  {
    DirectX::XMVECTOR const theirCentre = DirectX::XMLoadFloat3(&powerLink->m_centrePos);
    DirectX::XMVECTOR const ourCentre = DirectX::XMLoadFloat3(&m_centrePos);

    DirectX::XMFLOAT3 midPoint;
    DirectX::XMStoreFloat3(&midPoint, DirectX::XMVectorScale(DirectX::XMVectorAdd(theirCentre, ourCentre), 0.5f));
    float radius = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(theirCentre, ourCentre))) / 2.0f;
    radius += m_radius;

    // SphereInViewFrustum still takes a Vector3 -- Camera belongs to T22.
    return (g_camera->SphereInViewFrustum(midPoint, radius));
  }
  else
  {
    return Building::IsInView();
  }
}


void PowerBuilding::Render(float _predictionTime)
{
  DirectX::XMFLOAT4X4 mat = GetWorldMatrix();
  m_shape->Render(_predictionTime, mat);
}


void PowerBuilding::RenderAlphas(float _predictionTime)
{
  Building::RenderAlphas(_predictionTime);

  Building* powerLink = g_location->GetBuilding(m_powerLink);
  if (powerLink)
  {
    //
    // Render the power line itself
    PowerBuilding* powerBuilding = (PowerBuilding*)powerLink;

    DirectX::XMFLOAT3 const ourPosStore = GetPowerLocation();
    DirectX::XMFLOAT3 const theirPosStore = powerBuilding->GetPowerLocation();
    DirectX::XMVECTOR const ourPos = DirectX::XMLoadFloat3(&ourPosStore);
    DirectX::XMVECTOR const theirPos = DirectX::XMLoadFloat3(&theirPosStore);
    DirectX::XMVECTOR const alongLine = DirectX::XMVectorSubtract(theirPos, ourPos);

    // Camera's accessors are still legacy -- Species belongs to T22.
    DirectX::XMFLOAT3 const cameraPosStore = g_camera->GetPos();
    DirectX::XMVECTOR const cameraPos = DirectX::XMLoadFloat3(&cameraPosStore);

    // SetLength, which on a zero-length vector left (2, 0, 0); XMVector3Normalize
    // yields QNaN there instead. That needs the camera exactly collinear with the
    // power line, and it only degenerates one rendered quad, so this takes the
    // native behaviour rather than reproducing the fallback -- the same call the
    // three simulation sites in Entity, Citizen and Building do guard, because
    // there the fallback is what terminates a push loop.
    DirectX::XMVECTOR const ourPosRight =
      DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::XMVectorSubtract(cameraPos, ourPos), alongLine)), 2.0f);
    DirectX::XMVECTOR const theirPosRight =
      DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::XMVectorSubtract(cameraPos, theirPos), alongLine)), 2.0f);

    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(false);
    glColor4f(0.9f, 0.9f, 0.5f, 1.0f);

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

    //
    // Render any surges

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Starburst.bmp"));

    float surgeSize = 25.0f;
    glColor4f(0.5f, 0.5f, 1.0f, 1.0f);
    DirectX::XMFLOAT3 const camUpStore = g_camera->GetUp();
    DirectX::XMFLOAT3 const camRightStore = g_camera->GetRight();
    DirectX::XMVECTOR const camUp = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&camUpStore), surgeSize);
    DirectX::XMVECTOR const camRight = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&camRightStore), surgeSize);
    glBegin(GL_QUADS);
    for (int i = 0; i < static_cast<int>(m_surges.size()); ++i)
    {
      float thisSurge = m_surges[i];
      thisSurge += _predictionTime * 2;
      if (thisSurge < 0.0f)
        thisSurge = 0.0f;
      if (thisSurge > 1.0f)
        thisSurge = 1.0f;
      DirectX::XMVECTOR const thisSurgePos = DirectX::XMVectorMultiplyAdd(alongLine, DirectX::XMVectorReplicate(thisSurge), ourPos);
      glTexCoord2i(0, 0);
      EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorSubtract(thisSurgePos, camUp), camRight));
      glTexCoord2i(1, 0);
      EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorSubtract(thisSurgePos, camUp), camRight));
      glTexCoord2i(1, 1);
      EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(thisSurgePos, camUp), camRight));
      glTexCoord2i(0, 1);
      EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorAdd(thisSurgePos, camUp), camRight));
    }
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glDepthMask(true);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
  }
}

bool PowerBuilding::Advance()
{
  for (int i = 0; i < static_cast<int>(m_surges.size()); ++i)
  {
    float* thisSurge = &m_surges[i];
    *thisSurge += SERVER_ADVANCE_PERIOD * 2;
    if (*thisSurge >= 1.0f)
    {
      m_surges.erase(m_surges.begin() + i);
      --i;

      Building* powerLink = g_location->GetBuilding(m_powerLink);
      if (powerLink)
      {
        PowerBuilding* powerBuilding = (PowerBuilding*)powerLink;
        powerBuilding->TriggerSurge(0.0f);
      }
    }
  }
  return Building::Advance();
}

void PowerBuilding::TriggerSurge(float _initValue)
{
  m_surges.insert(m_surges.begin(), _initValue);

  g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), "TriggerSurge");
}


void PowerBuilding::ListSoundEvents(std::vector<const char*>* _list)
{
  Building::ListSoundEvents(_list);

  _list->push_back("TriggerSurge");
}


void PowerBuilding::Read(TextReader* _in, bool _dynamic)
{
  Building::Read(_in, _dynamic);
  m_powerLink = atoi(_in->GetNextToken());
}

void PowerBuilding::Write(FileWriter* _out)
{
  Building::Write(_out);

  _out->printf("%-8d", m_powerLink);
}

int PowerBuilding::GetBuildingLink() { return m_powerLink; }

void PowerBuilding::SetBuildingLink(int _buildingId) { m_powerLink = _buildingId; }


// ****************************************************************************
// Class Generator
// ****************************************************************************

Generator::Generator()
  : PowerBuilding(),
    m_throughput(0.0f),
    m_timerSync(0.0f),
    m_numThisSecond(0),
    m_enabled(false)
{
  m_type = TypeGenerator;
  SetShape(g_resource->GetShape("Generator.shp"));

  m_counter = m_shape->m_rootFragment->LookupMarker("MarkerCounter");
}


void Generator::TriggerSurge(float _initValue)
{
  if (m_enabled)
  {
    PowerBuilding::TriggerSurge(_initValue);
    ++m_numThisSecond;
  }
}


char const* Generator::GetObjectiveCounter()
{
  static char result[256];
  sprintf(result, "%s : %d Gq/s", LANGUAGEPHRASE("objective_output"), int(m_throughput * 10));
  return result;
}


void Generator::ReprogramComplete()
{
  m_enabled = true;
  g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), "Enable");
}


void Generator::ListSoundEvents(std::vector<const char*>* _list)
{
  PowerBuilding::ListSoundEvents(_list);

  _list->push_back("Enable");
}


bool Generator::Advance()
{
  if (!m_enabled)
  {
    m_surges.clear();
    m_throughput = 0.0f;
    m_numThisSecond = 0;

    //
    // Check to see if our control tower has been captured.
    // This can happen if a user captures the control tower, exits the level and saves,
    // then returns to the level.  The tower is captured and cannot be changed, but
    // the m_enabled state of this building has been lost.

    for (int i = 0; i < g_location->m_buildings.Size(); ++i)
    {
      if (g_location->m_buildings.ValidIndex(i))
      {
        Building* building = g_location->m_buildings[i];
        if (building && building->m_type == TypeControlTower)
        {
          ControlTower* tower = (ControlTower*)building;
          if (tower->GetBuildingLink() == m_id.GetUniqueId() && tower->m_id.GetTeamId() == m_id.GetTeamId())
          {
            m_enabled = true;
            break;
          }
        }
      }
    }
  }
  else
  {
    if (GetHighResTime() >= m_timerSync + 1.0f)
    {
      float newAverage = m_numThisSecond;
      m_numThisSecond = 0;
      m_timerSync = GetHighResTime();
      m_throughput = m_throughput * 0.8f + newAverage * 0.2f;
    }

    if (m_throughput > 6.5f)
    {
      GlobalBuilding* gb = g_globalWorld->GetBuilding(m_id.GetUniqueId(), g_locationId);
      gb->m_online = true;
    }
  }

  return PowerBuilding::Advance();
}


void Generator::Render(float _predictionTime)
{
  PowerBuilding::Render(_predictionTime);

  // g_gameFont.DrawText3DCentre( m_pos + Vector3(0,215,0), 10.0f, "NumThisSecond : %d", m_numThisSecond );

  // if( m_enabled ) g_gameFont.DrawText3DCentre( m_pos + Vector3(0,180,0), 10.0f, "Enabled" );
  // g_gameFont.DrawText3DCentre( m_pos + Vector3(0,170,0), 10.0f, "Output : %d Gq/s", int(m_throughput*10.0f) );

  DirectX::XMFLOAT4X4 generatorMat;
  DirectX::XMStoreFloat4x4(&generatorMat,
                           BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

  // ShapeMarker::GetWorldMatrix still returns Matrix34 -- T10's seam -- so the
  // counter's own basis stays legacy here and converts when that seam closes.
  Matrix34 counterMat = m_counter->GetWorldMatrix(generatorMat);

  glColor4f(0.6f, 0.8f, 0.9f, 1.0f);
  g_gameFont.DrawText3D(counterMat.pos, counterMat.f, counterMat.u, 7.0f, "%d", int(m_throughput * 10.0f));
  counterMat.pos += counterMat.f * 0.1f;
  counterMat.pos += (counterMat.f ^ counterMat.u) * 0.2f;
  counterMat.pos += counterMat.u * 0.2f;
  g_gameFont.SetRenderShadow(true);
  glColor4f(0.6f, 0.8f, 0.9f, 0.0f);
  g_gameFont.DrawText3D(counterMat.pos, counterMat.f, counterMat.u, 7.0f, "%d", int(m_throughput * 10.0f));
  g_gameFont.SetRenderShadow(false);
}


// ****************************************************************************
// Class Pylon
// ****************************************************************************

Pylon::Pylon()
  : PowerBuilding()
{
  m_type = TypePylon;
  SetShape(g_resource->GetShape("Pylon.shp"));
}


bool Pylon::Advance() { return PowerBuilding::Advance(); }


// ****************************************************************************
// Class PylonStart
// ****************************************************************************

PylonStart::PylonStart()
  : PowerBuilding(),
    m_reqBuildingId(-1)
{
  m_type = TypePylonStart;
  SetShape(g_resource->GetShape("Pylon.shp"));
};


void PylonStart::Initialise(Building* _template)
{
  PowerBuilding::Initialise(_template);

  m_reqBuildingId = ((PylonStart*)_template)->m_reqBuildingId;
}


bool PylonStart::Advance()
{
  //
  // Is the Generator online?

  bool generatorOnline = false;

  int generatorLocationId = g_globalWorld->GetLocationId("generator");
  GlobalBuilding* globalRefinery = nullptr;
  for (GlobalBuilding* gb : g_globalWorld->m_buildings)
  {
    if (gb && gb->m_locationId == generatorLocationId && gb->m_type == TypeGenerator && gb->m_online)
    {
      generatorOnline = true;
      break;
    }
  }

  if (generatorOnline)
  {
    //
    // Is our required building online yet?
    GlobalBuilding* globalBuilding = g_globalWorld->GetBuilding(m_reqBuildingId, g_locationId);
    if (globalBuilding && globalBuilding->m_online)
    {
      if (syncfrand() > 0.7f)
      {
        TriggerSurge(0.0f);
      }
    }
  }

  return PowerBuilding::Advance();
}


void PylonStart::RenderAlphas(float _predictionTime)
{
  PowerBuilding::RenderAlphas(_predictionTime);

#ifdef DEBUG_RENDER_ENABLED
  if (g_editing)
  {
    Building* req = g_location->GetBuilding(m_reqBuildingId);
    if (req)
    {
      RenderArrow(DirectX::XMFLOAT3(m_pos.x, m_pos.y + 50.0f, m_pos.z), DirectX::XMFLOAT3(req->m_pos.x, req->m_pos.y + 50.0f, req->m_pos.z), 2.0f,
                  RGBAColour(255, 0, 0));
    }
  }
#endif
}


void PylonStart::Read(TextReader* _in, bool _dynamic)
{
  PowerBuilding::Read(_in, _dynamic);

  m_reqBuildingId = atoi(_in->GetNextToken());
}


void PylonStart::Write(FileWriter* _out)
{
  PowerBuilding::Write(_out);

  _out->printf("%-8d", m_reqBuildingId);
}


// ****************************************************************************
// Class PylonEnd
// ****************************************************************************

PylonEnd::PylonEnd()
  : PowerBuilding()
{
  m_type = TypePylonEnd;
  SetShape(g_resource->GetShape("Pylon.shp"));
};


void PylonEnd::TriggerSurge(float _initValue)
{
  Building* building = g_location->GetBuilding(m_powerLink);

  if (building && building->m_type == Building::TypeYard)
  {
    ConstructionYard* yard = (ConstructionYard*)building;
    yard->AddPowerSurge();
  }

  if (building && building->m_type == Building::TypeFuelGenerator)
  {
    FuelGenerator* fuel = (FuelGenerator*)building;
    fuel->ProvideSurge();
  }
}


void PylonEnd::RenderAlphas(float _predictionTime)
{
  // Do nothing
}


// ****************************************************************************
// Class SolarPanel
// ****************************************************************************

SolarPanel::SolarPanel()
  : PowerBuilding(),
    m_operating(false)
{
  m_type = TypeSolarPanel;
  SetShape(g_resource->GetShape("SolarPanel.shp"));

  memset(m_glowMarker, 0, SOLARPANEL_NUMGLOWS * sizeof(ShapeMarker*));

  for (int i = 0; i < SOLARPANEL_NUMGLOWS; ++i)
  {
    char name[64];
    sprintf(name, "MarkerGlow0%d", i + 1);
    m_glowMarker[i] = m_shape->m_rootFragment->LookupMarker(name);
    DEBUG_ASSERT(m_glowMarker[i]);
  }

  for (int i = 0; i < SOLARPANEL_NUMSTATUSMARKERS; ++i)
  {
    char name[64];
    sprintf(name, "MarkerStatus0%d", i + 1);
    m_statusMarkers[i] = m_shape->m_rootFragment->LookupMarker(name);
  }
}


void SolarPanel::Initialise(Building* _template)
{
  _template->m_up = g_location->m_landscape.m_normalMap->GetValue(_template->m_pos.x, _template->m_pos.z);
  DirectX::XMStoreFloat3(&_template->m_front, DirectX::XMVector3Cross(DirectX::g_XMIdentityR0, DirectX::XMLoadFloat3(&_template->m_up)));

  PowerBuilding::Initialise(_template);
}


bool SolarPanel::Advance()
{
  float fractionOccupied = (float)GetNumPortsOccupied() / (float)GetNumPorts();

  if (syncfrand(20.0f) <= fractionOccupied)
  {
    TriggerSurge(0.0f);
  }

  if (fractionOccupied > 0.6f)
  {
    if (!m_operating)
      g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), "Operate");
    m_operating = true;
  }

  if (fractionOccupied < 0.3f)
  {
    if (m_operating)
      g_soundSystem->StopAllSounds(m_id, "SolarPanel Operate");
    m_operating = false;
  }

  return PowerBuilding::Advance();
}


void SolarPanel::RenderPorts()
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

    // ShapeMarker::GetWorldMatrix still returns Matrix34 -- T10's seam.
    DirectX::XMFLOAT3 const statusPos = m_statusMarkers[i]->GetWorldMatrix(rootMat).pos;


    //
    // Render the status light

    float size = 6.0f;

    // Camera's accessors are still legacy -- Species belongs to T22.
    DirectX::XMFLOAT3 const camRightStore = g_camera->GetRight();
    DirectX::XMFLOAT3 const camUpStore = g_camera->GetUp();
    DirectX::XMVECTOR const camR = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&camRightStore), size);
    DirectX::XMVECTOR const camU = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&camUpStore), size);

    DirectX::XMVECTOR const status = DirectX::XMLoadFloat3(&statusPos);

    if (GetPortOccupant(i).IsValid())
      glColor4f(0.3f, 1.0f, 0.3f, 1.0f);
    else
      glColor4f(1.0f, 0.3f, 0.3f, 1.0f);

    glBegin(GL_QUADS);
    glTexCoord2i(0, 0);
    EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorSubtract(status, camR), camU));
    glTexCoord2i(1, 0);
    EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorAdd(status, camR), camU));
    glTexCoord2i(1, 1);
    EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(status, camR), camU));
    glTexCoord2i(0, 1);
    EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorSubtract(status, camR), camU));
    glEnd();
  }

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_BLEND);
  glDepthMask(true);
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_CULL_FACE);
}


void SolarPanel::Render(float _predictionTime)
{
  if (g_editing)
  {
    m_up = g_location->m_landscape.m_normalMap->GetValue(m_pos.x, m_pos.z);
    DirectX::XMStoreFloat3(&m_front, DirectX::XMVector3Cross(DirectX::g_XMIdentityR0, DirectX::XMLoadFloat3(&m_up)));
  }

  glShadeModel(GL_SMOOTH);
  PowerBuilding::Render(_predictionTime);
  glShadeModel(GL_FLAT);
}


void SolarPanel::RenderAlphas(float _predictionTime)
{
  PowerBuilding::RenderAlphas(_predictionTime);

  float fractionOccupied = (float)GetNumPortsOccupied() / (float)GetNumPorts();

  if (fractionOccupied > 0.0f)
  {
    DirectX::XMFLOAT4X4 mat = GetWorldMatrix();
    float glowWidth = 60.0f;
    float glowHeight = 40.0f;
    float alphaValue = fabs(sinf(g_gameTime)) * fractionOccupied;

    glColor4f(0.2f, 0.4f, 0.9f, alphaValue);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Glow.bmp"));
    glDepthMask(false);
    glDisable(GL_CULL_FACE);

    for (int i = 0; i < SOLARPANEL_NUMGLOWS; ++i)
    {
      // ShapeMarker::GetWorldMatrix still returns Matrix34 -- T10's seam -- and
      // this block wants the marker's whole basis, not just its position, so it
      // stays legacy until that seam closes.
      Matrix34 thisGlow = m_glowMarker[i]->GetWorldMatrix(mat);

      glBegin(GL_QUADS);
      glTexCoord2i(0, 0);
      glVertex3fv((thisGlow.pos - thisGlow.r * glowHeight + thisGlow.f * glowWidth).GetData());
      glTexCoord2i(0, 1);
      glVertex3fv((thisGlow.pos + thisGlow.r * glowHeight + thisGlow.f * glowWidth).GetData());
      glTexCoord2i(1, 1);
      glVertex3fv((thisGlow.pos + thisGlow.r * glowHeight - thisGlow.f * glowWidth).GetData());
      glTexCoord2i(1, 0);
      glVertex3fv((thisGlow.pos - thisGlow.r * glowHeight - thisGlow.f * glowWidth).GetData());
      glEnd();
    }

    glEnable(GL_CULL_FACE);
    glDepthMask(true);
    glDisable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
  }
}


void SolarPanel::ListSoundEvents(std::vector<const char*>* _list)
{
  PowerBuilding::ListSoundEvents(_list);

  _list->push_back("Operate");
}
