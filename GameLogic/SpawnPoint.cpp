#include "pch.h"
#include "GlVertex.h"
#include "FileWriter.h"
#include "Resource.h"
#include "Shape.h"
#include "DebugRender.h"
#include "TextRenderer.h"
#include "Profiler.h"
#include "MathUtils.h"
#include "TextStreamReaders.h"
#include "HiResTime.h"
#include "Preferences.h"
#include "LanguageTable.h"

#include "Location.h"
#include "EntityGrid.h"
#include "Team.h"
#include "GlobalWorld.h"
#include "GameTime.h"

#include "SpawnPoint.h"
#include "Citizen.h"
#include "WorldPointers.h"
#include "AppState.h"


SpawnBuilding::SpawnBuilding()
  : Building(),
    m_spiritLink(nullptr),
    m_visibilityRadius(0.0f)
{
}

SpawnBuilding::~SpawnBuilding()
{
  EmptyAndDelete(m_links);
  // SAFE_DELETE(m_spiritLink); probably not necessary
}

void SpawnBuilding::Initialise(Building* _template)
{
  Building::Initialise(_template);

  SpawnBuilding* spawn = (SpawnBuilding*)_template;
  for (int i = 0; i < static_cast<int>(spawn->m_links.size()); ++i)
  {
    SpawnBuildingLink* link = spawn->m_links[i];
    SetBuildingLink(link->m_targetBuildingId);
  }
}


bool SpawnBuilding::IsInView()
{
  if (m_visibilityRadius == 0.0f || g_editing)
  {
    if (static_cast<int>(m_links.size()) == 0)
    {
      m_visibilityRadius = m_radius;
      m_visibilityMidpoint = m_centrePos;
      return Building::IsInView();
    }
    else
    {
      m_visibilityMidpoint = m_centrePos;
      int numLinks = 1;
      m_visibilityRadius = 0.0f;

      // Find mid point

      for (int i = 0; i < static_cast<int>(m_links.size()); ++i)
      {
        SpawnBuildingLink* link = m_links[i];
        SpawnBuilding* building = (SpawnBuilding*)g_location->GetBuilding(link->m_targetBuildingId);
        if (building)
        {
          DirectX::XMStoreFloat3(&m_visibilityMidpoint,
                                 DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_visibilityMidpoint), DirectX::XMLoadFloat3(&building->m_centrePos)));
          ++numLinks;
        }
      }

      DirectX::XMStoreFloat3(&m_visibilityMidpoint, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_visibilityMidpoint), 1.0f / (float)numLinks));

      // Find radius

      for (int i = 0; i < static_cast<int>(m_links.size()); ++i)
      {
        SpawnBuildingLink* link = m_links[i];
        SpawnBuilding* building = (SpawnBuilding*)g_location->GetBuilding(link->m_targetBuildingId);
        if (building)
        {
          float distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(
            DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&building->m_centrePos), DirectX::XMLoadFloat3(&m_visibilityMidpoint))));
          distance += building->m_radius / 2.0f;
          m_visibilityRadius = std::max(m_visibilityRadius, distance);
        }
      }
    }
  }

  // Determine visibility

  // SphereInViewFrustum still takes a Vector3 -- Camera belongs to T22.
  return g_camera->SphereInViewFrustum(m_visibilityMidpoint, m_visibilityRadius);
}


void SpawnBuilding::RenderSpirit(DirectX::XMFLOAT3 const& _pos)
{
  DirectX::XMVECTOR const pos = DirectX::XMLoadFloat3(&_pos);

  // Camera's accessors are still legacy -- Species belongs to T22. Hoisted out
  // of the eight vertices below, which each called them afresh.
  DirectX::XMFLOAT3 const camUpStore = g_camera->GetUp();
  DirectX::XMFLOAT3 const camRightStore = g_camera->GetRight();
  DirectX::XMVECTOR const camUp = DirectX::XMLoadFloat3(&camUpStore);
  DirectX::XMVECTOR const camRight = DirectX::XMLoadFloat3(&camRightStore);

  int innerAlpha = 255;
  int outerAlpha = 150;
  int spiritInnerSize = 2;
  int spiritOuterSize = 6;

  float size = spiritInnerSize;
  glColor4ub(150, 50, 25, innerAlpha);

  glBegin(GL_QUADS);
  EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camUp, DirectX::XMVectorReplicate(size), pos));
  EmitVertex(DirectX::XMVectorMultiplyAdd(camRight, DirectX::XMVectorReplicate(size), pos));
  EmitVertex(DirectX::XMVectorMultiplyAdd(camUp, DirectX::XMVectorReplicate(size), pos));
  EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camRight, DirectX::XMVectorReplicate(size), pos));
  glEnd();

  size = spiritOuterSize;
  glColor4ub(150, 50, 25, outerAlpha);

  glBegin(GL_QUADS);
  EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camUp, DirectX::XMVectorReplicate(size), pos));
  EmitVertex(DirectX::XMVectorMultiplyAdd(camRight, DirectX::XMVectorReplicate(size), pos));
  EmitVertex(DirectX::XMVectorMultiplyAdd(camUp, DirectX::XMVectorReplicate(size), pos));
  EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(camRight, DirectX::XMVectorReplicate(size), pos));
  glEnd();
}


void SpawnBuilding::RenderAlphas(float _predictionTime)
{
  DirectX::XMFLOAT3 const ourPosStore = GetSpiritLink();
  DirectX::XMVECTOR const ourPos = DirectX::XMLoadFloat3(&ourPosStore);

  int buildingDetail = g_prefsManager->GetInt("RenderBuildingDetail", 1);

  for (int i = 0; i < static_cast<int>(m_links.size()); ++i)
  {
    SpawnBuildingLink* link = m_links[i];
    SpawnBuilding* building = (SpawnBuilding*)g_location->GetBuilding(link->m_targetBuildingId);
    if (building)
    {
      DirectX::XMFLOAT3 const theirPosStore = building->GetSpiritLink();
      DirectX::XMVECTOR const theirPos = DirectX::XMLoadFloat3(&theirPosStore);
      DirectX::XMVECTOR const alongLink = DirectX::XMVectorSubtract(theirPos, ourPos);

      // Camera's accessors are still legacy -- Species belongs to T22.
      DirectX::XMFLOAT3 const cameraPosStore = g_camera->GetPos();
      DirectX::XMVECTOR const cameraPos = DirectX::XMLoadFloat3(&cameraPosStore);

      DirectX::XMVECTOR const ourPosCross = DirectX::XMVector3Cross(DirectX::XMVectorSubtract(cameraPos, ourPos), alongLink);
      DirectX::XMVECTOR const theirPosCross = DirectX::XMVector3Cross(DirectX::XMVectorSubtract(cameraPos, theirPos), alongLink);

      glDisable(GL_CULL_FACE);
      glDepthMask(false);
      glColor4f(0.9f, 0.9f, 0.5f, 1.0f);

      float size = 0.5f;

      if (buildingDetail == 1)
      {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);

        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Laser.bmp"));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

        size = 1.0f;
      }

      // SetLength; see the note on the same call in Generator.cpp. Rendering
      // only, and degenerate only with the camera exactly on the link, so this
      // takes the native normalise rather than reproducing the (size, 0, 0).
      DirectX::XMVECTOR const theirPosRight = DirectX::XMVectorScale(DirectX::XMVector3Normalize(theirPosCross), size);
      DirectX::XMVECTOR const ourPosRight = DirectX::XMVectorScale(DirectX::XMVector3Normalize(ourPosCross), size);

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
      // Render spirits in transit

      for (int j = 0; j < static_cast<int>(link->m_spirits.size()); ++j)
      {
        SpawnBuildingSpirit* spirit = link->m_spirits[j];
        float predictedProgress = spirit->m_currentProgress + _predictionTime;
        if (predictedProgress >= 0.0f && predictedProgress <= 1.0f)
        {
          DirectX::XMFLOAT3 position;
          DirectX::XMStoreFloat3(&position, DirectX::XMVectorMultiplyAdd(alongLink, DirectX::XMVectorReplicate(predictedProgress), ourPos));
          RenderSpirit(position);
        }
      }
    }
  }

  Building::RenderAlphas(_predictionTime);
}


void SpawnBuilding::SetBuildingLink(int _buildingId)
{
  SpawnBuildingLink* link = new SpawnBuildingLink();
  link->m_targetBuildingId = _buildingId;
  m_links.push_back(link);
}


void SpawnBuilding::ClearLinks() { EmptyAndDelete(m_links); }


std::vector<int>* SpawnBuilding::ExploreLinks()
{
  auto result = new std::vector<int>();

  if (m_type == TypeSpawnPoint)
  {
    result->push_back(m_id.GetUniqueId());
  }

  for (int i = 0; i < static_cast<int>(m_links.size()); ++i)
  {
    SpawnBuildingLink* link = m_links[i];
    link->m_targets.clear();

    SpawnBuilding* target = (SpawnBuilding*)g_location->GetBuilding(link->m_targetBuildingId);
    if (target)
    {
      std::vector<int>* availableLinks = target->ExploreLinks();
      for (int thisLink : *availableLinks)
      {
        result->push_back(thisLink);
        link->m_targets.push_back(thisLink);
      }
      delete availableLinks;
    }
  }

  return result;
}


DirectX::XMFLOAT3 SpawnBuilding::GetSpiritLink()
{
  if (!m_spiritLink)
  {
    m_spiritLink = m_shape->m_rootFragment->LookupMarker("MarkerSpiritLink");
    DEBUG_ASSERT(m_spiritLink);
  }

  DirectX::XMFLOAT4X4 mat;
  DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

  // ShapeMarker::GetWorldMatrix still returns Matrix34 -- T10's seam.
  return m_spiritLink->GetWorldMatrix(mat).pos;
}


void SpawnBuilding::TriggerSpirit(SpawnBuildingSpirit* _spirit)
{
  int numAdds = 0;

  for (int i = 0; i < static_cast<int>(m_links.size()); ++i)
  {
    SpawnBuildingLink* link = m_links[i];
    for (int j = 0; j < static_cast<int>(link->m_targets.size()); ++j)
    {
      int thisLink = link->m_targets[j];
      if (link->m_targets[j] == _spirit->m_targetBuildingId)
      {
        link->m_spirits.push_back(_spirit);
        _spirit->m_currentProgress = 0.0f;
        ++numAdds;
      }
    }
  }

  if (numAdds == 0)
  {
    // We have nowhere else to go
    // Or maybe we have arrived
    delete _spirit;
  }
}


bool SpawnBuilding::Advance()
{
  //
  // Advance all spirits in transit

  for (int i = 0; i < static_cast<int>(m_links.size()); ++i)
  {
    SpawnBuildingLink* link = m_links[i];
    for (int j = 0; j < static_cast<int>(link->m_spirits.size()); ++j)
    {
      SpawnBuildingSpirit* spirit = link->m_spirits[j];
      spirit->m_currentProgress += SERVER_ADVANCE_PERIOD;
      if (spirit->m_currentProgress >= 1.0f)
      {
        link->m_spirits.erase(link->m_spirits.begin() + j);
        --j;
        SpawnBuilding* building = (SpawnBuilding*)g_location->GetBuilding(link->m_targetBuildingId);
        if (building)
          building->TriggerSpirit(spirit);
      }
    }
  }

  return Building::Advance();
}


void SpawnBuilding::Render(float _predictionTime) { Building::Render(_predictionTime); }


void SpawnBuilding::Read(TextReader* _in, bool _dynamic)
{
  Building::Read(_in, _dynamic);

  while (_in->TokenAvailable())
  {
    int nextLink = atoi(_in->GetNextToken());
    SetBuildingLink(nextLink);
  }
}


void SpawnBuilding::Write(FileWriter* _out)
{
  Building::Write(_out);

  for (int i = 0; i < static_cast<int>(m_links.size()); ++i)
  {
    SpawnBuildingLink* link = m_links[i];
    _out->printf("%-6d", link->m_targetBuildingId);
  }
}


// ============================================================================


SpawnLink::SpawnLink()
  : SpawnBuilding()
{
  m_type = TypeSpawnLink;

  SetShape(g_resource->GetShape("SpawnLink.shp"));
}


// ============================================================================

int MasterSpawnPoint::s_masterSpawnPointId = -1;


MasterSpawnPoint::MasterSpawnPoint()
  : SpawnBuilding(),
    m_exploreLinks(false)
{
  m_type = TypeSpawnPointMaster;

  SetShape(g_resource->GetShape("MasterSpawnPoint.shp"));
}


MasterSpawnPoint* MasterSpawnPoint::GetMasterSpawnPoint()
{
  Building* building = g_location->GetBuilding(s_masterSpawnPointId);
  if (building && building->m_type == TypeSpawnPointMaster)
  {
    return (MasterSpawnPoint*)building;
  }

  return nullptr;
}


void MasterSpawnPoint::RequestSpirit(int _targetBuildingId)
{
  SpawnBuildingSpirit* spirit = new SpawnBuildingSpirit();
  spirit->m_targetBuildingId = _targetBuildingId;
  TriggerSpirit(spirit);
}


bool MasterSpawnPoint::Advance()
{
  s_masterSpawnPointId = m_id.GetUniqueId();

  //
  // Explore our links first time through

  if (!m_exploreLinks)
  {
    m_exploreLinks = true;
    float startTime = GetHighResTime();
    ExploreLinks();
    float timeTaken = GetHighResTime() - startTime;
    DebugTrace("Time to Explore all Spawn Point links : %d ms\n", int(timeTaken * 1000.0f));
  }


  //
  // Accelerate spirits to heaven

  if (m_isGlobal)
  {
    for (int i = 0; i < g_location->m_spirits.Size(); ++i)
    {
      if (g_location->m_spirits.ValidIndex(i))
      {
        Spirit* s = g_location->m_spirits.GetPointer(i);
        if (s->m_state == Spirit::StateBirth || s->m_state == Spirit::StateFloating)
        {
          s->SkipStage();
        }
      }
    }
  }

  //
  // Have the red guys been wiped out?

  if (g_location->m_teams[1].m_teamType != Team::TeamTypeUnused)
  {
    int numRed = g_location->m_teams[1].m_others.NumUsed();
    if (numRed < 10)
    {
      GlobalBuilding* gb = g_globalWorld->GetBuilding(m_id.GetUniqueId(), g_locationId);
      if (gb)
        gb->m_online = true;
    }
  }

  return SpawnBuilding::Advance();
}


void MasterSpawnPoint::Render(float _predictionTime)
{
  if (m_isGlobal || g_editing)
  {
    SpawnBuilding::Render(_predictionTime);
  }
}


void MasterSpawnPoint::RenderAlphas(float _predictionTime)
{
  if (m_isGlobal || g_editing)
  {
    SpawnBuilding::RenderAlphas(_predictionTime);
  }
}


char const* MasterSpawnPoint::GetObjectiveCounter()
{
  static char result[256];

  if (g_location->m_teams[1].m_teamType != Team::TeamTypeUnused)
  {
    int numRed = g_location->m_teams[1].m_others.NumUsed();
    sprintf(result, "%s : %d", LANGUAGEPHRASE("objective_redpopulation"), numRed);
  }
  else
  {
    sprintf(result, "%s", LANGUAGEPHRASE("objective_redpopulation"));
  }

  return result;
}


// ============================================================================

SpawnPoint::SpawnPoint()
  : SpawnBuilding(),
    m_spawnTimer(0.0f),
    m_evaluateTimer(0.0f),
    m_numFriendsNearby(0),
    m_populationLock(-1)
{
  m_type = Building::TypeSpawnPoint;

  SetShape(g_resource->GetShape("SpawnPoint.shp"));
  m_doorMarker = m_shape->m_rootFragment->LookupMarker("MarkerDoor");

  m_evaluateTimer = syncfrand(2.0f);
  m_spawnTimer = 2.0f + syncfrand(2.0f);
}


bool SpawnPoint::PopulationLocked()
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


void SpawnPoint::RecalculateOwnership()
{
  int teamCount[NUM_TEAMS];
  memset(teamCount, 0, NUM_TEAMS * sizeof(int));

  for (int i = 0; i < GetNumPorts(); ++i)
  {
    WorldObjectId id = GetPortOccupant(i);
    if (id.IsValid())
    {
      teamCount[id.GetTeamId()]++;
    }
  }

  int winningTeam = -1;
  for (int i = 0; i < NUM_TEAMS; ++i)
  {
    if (teamCount[i] > 2 && winningTeam == -1)
    {
      winningTeam = i;
    }
    else if (winningTeam != -1 && teamCount[i] > 2 && teamCount[i] > teamCount[winningTeam])
    {
      winningTeam = i;
    }
  }

  if (winningTeam == -1)
  {
    SetTeamId(255);
  }
  else
  {
    SetTeamId(winningTeam);
  }
}


void SpawnPoint::TriggerSpirit(SpawnBuildingSpirit* _spirit)
{
  if (m_id.GetTeamId() == 0)
  {
    int b = 10;
  }

  if (m_id.GetUniqueId() == _spirit->m_targetBuildingId)
  {
    // This spirit has arrived at its destination
    if (m_id.GetTeamId() != 255)
    {
      DirectX::XMFLOAT4X4 mat = GetWorldMatrix();

      // ShapeMarker::GetWorldMatrix still returns Matrix34 -- T10's seam.
      DirectX::XMFLOAT3 const doorPos = m_doorMarker->GetWorldMatrix(mat).pos;
      g_location->SpawnEntities(doorPos, m_id.GetTeamId(), -1, Entity::TypeCitizen, 1, g_zeroVector, 0.0f);
    }

    delete _spirit;
  }
  else
  {
    SpawnBuilding::TriggerSpirit(_spirit);
  }
}


bool SpawnPoint::PerformDepthSort(DirectX::XMFLOAT3& _centrePos)
{
  _centrePos = m_centrePos;
  return true;
}


bool SpawnPoint::Advance()
{
  //
  // Re-evalulate our situation

  m_evaluateTimer -= SERVER_ADVANCE_PERIOD;
  if (m_evaluateTimer <= 0.0f)
  {
    START_PROFILE(g_profiler, "Evaluate");

    RecalculateOwnership();
    m_evaluateTimer = 2.0f;

    END_PROFILE(g_profiler, "Evaluate");
  }


  //
  // Time to request more spirits for our Citizens?

  START_PROFILE(g_profiler, "SpawnCitizens");
  if (m_id.GetTeamId() != 255 && !PopulationLocked())
  {
    m_spawnTimer -= SERVER_ADVANCE_PERIOD;
    if (m_spawnTimer <= 0.0f)
    {
      MasterSpawnPoint* masterSpawn = MasterSpawnPoint::GetMasterSpawnPoint();
      if (masterSpawn)
        masterSpawn->RequestSpirit(m_id.GetUniqueId());

      float fractionOccupied = (float)GetNumPortsOccupied() / (float)GetNumPorts();

      m_spawnTimer = 2.0f + 5.0f * (1.0f - fractionOccupied);

      // Reds spawn a bit quicker
      if (m_id.GetTeamId() == 1)
        m_spawnTimer -= 1.0 * (g_difficultyLevel / 10.0);
      else
        m_spawnTimer -= 0.5 * (g_difficultyLevel / 10.0);
    }
  }
  END_PROFILE(g_profiler, "SpawnCitizens");

  return SpawnBuilding::Advance();
}


void SpawnPoint::Render(float _predictionTime) { SpawnBuilding::Render(_predictionTime); }


void SpawnPoint::RenderAlphas(float _predictionTime)
{
  SpawnBuilding::RenderAlphas(_predictionTime);

  // Camera's accessors are still legacy -- Species belongs to T22.
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

  float alpha = GetNumPortsOccupied() / (float)GetNumPorts();

  for (int i = 0; i < maxBlobs; ++i)
  {
    DirectX::XMVECTOR const pos = DirectX::XMVectorAdd(
      DirectX::XMLoadFloat3(&m_centrePos),
      DirectX::XMVectorSet(sinf(timeIndex + i) * i * 0.7f, 25.0f + cosf(timeIndex + i) * sinf(i * 10) * 12, cosf(timeIndex + i) * i * 0.7f, 0.0f));

    float size = 10.0f + sinf(timeIndex + i * 10) * 10.0f;
    size = std::max(size, 5.0f);

    DirectX::XMVECTOR const right = DirectX::XMVectorScale(camRight, size);
    DirectX::XMVECTOR const up = DirectX::XMVectorScale(camUp, size);

    glColor4f(0.6f, 0.2f, 0.1f, alpha);

    glBegin(GL_QUADS);
    glTexCoord2i(0, 0);
    EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorSubtract(pos, right), up));
    glTexCoord2i(1, 0);
    EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(pos, right), up));
    glTexCoord2i(1, 1);
    EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorAdd(pos, right), up));
    glTexCoord2i(0, 1);
    EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorSubtract(pos, right), up));
    glEnd();
  }

  glDisable(GL_TEXTURE_2D);
  glDepthMask(true);
}


void SpawnPoint::RenderPorts()
{
  glDisable(GL_CULL_FACE);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Starburst.bmp"));
  glDepthMask(false);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glBegin(GL_QUADS);

  for (int i = 0; i < GetNumPorts(); ++i)
  {
    DirectX::XMFLOAT3 portPos;
    DirectX::XMFLOAT3 portFront;
    GetPortPosition(i, portPos, portFront);

    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&portFront), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&portPos)));

    //
    // Render the status light

    float size = 6.0f;

    // Camera's accessors are still legacy -- Species belongs to T22.
    DirectX::XMFLOAT3 const camRightStore = g_camera->GetRight();
    DirectX::XMFLOAT3 const camUpStore = g_camera->GetUp();
    DirectX::XMVECTOR const camR = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&camRightStore), size);
    DirectX::XMVECTOR const camU = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&camUpStore), size);

    // ShapeMarker::GetWorldMatrix still returns Matrix34 -- T10's seam.
    DirectX::XMFLOAT3 statusPosStore = s_controlPadStatus->GetWorldMatrix(mat).pos;
    statusPosStore.y = g_location->m_landscape.m_heightMap->GetValue(statusPosStore.x, statusPosStore.z);
    statusPosStore.y += 5.0f;
    DirectX::XMVECTOR const statusPos = DirectX::XMLoadFloat3(&statusPosStore);

    WorldObjectId occupantId = GetPortOccupant(i);
    if (!occupantId.IsValid())
    {
      glColor4ub(150, 150, 150, 255);
    }
    else
    {
      RGBAColour teamColour = g_location->m_teams[occupantId.GetTeamId()].m_colour;
      glColor4ubv(teamColour.GetData());
    }

    glTexCoord2i(0, 0);
    EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorSubtract(statusPos, camR), camU));
    glTexCoord2i(1, 0);
    EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorAdd(statusPos, camR), camU));
    glTexCoord2i(1, 1);
    EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(statusPos, camR), camU));
    glTexCoord2i(0, 1);
    EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorSubtract(statusPos, camR), camU));
  }

  glEnd();
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_BLEND);
  glDepthMask(true);
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_CULL_FACE);
}


// ============================================================================

float SpawnPopulationLock::s_overpopulationTimer = 0.0f;
int SpawnPopulationLock::s_overpopulation = 0;


SpawnPopulationLock::SpawnPopulationLock()
  : Building(),
    m_searchRadius(500.0f),
    m_maxPopulation(100),
    m_originalMaxPopulation(100),
    m_recountTeamId(-1)
{
  m_type = Building::TypeSpawnPopulationLock;

  memset(m_teamCount, 0, NUM_TEAMS * sizeof(int));

  m_recountTimer = syncfrand(10.0f);
  m_recountTeamId = 10;
}


void SpawnPopulationLock::Initialise(Building* _template)
{
  Building::Initialise(_template);

  SpawnPopulationLock* lock = (SpawnPopulationLock*)_template;
  m_searchRadius = lock->m_searchRadius;
  m_maxPopulation = lock->m_maxPopulation;

  m_originalMaxPopulation = m_maxPopulation;
}


bool SpawnPopulationLock::Advance()
{
  //
  // Every once in a while, have a look at the global over-population rate
  // And reduce all local maxPopulations accordingly
  // This only really applies to Green citizens, as the player can arrange
  // it so an unlimited army gathers on a single island

  if (GetHighResTime() > s_overpopulationTimer)
  {
    s_overpopulationTimer = GetHighResTime() + 1.0f;

    int totalOverpopulation = 0;

    for (int i = 0; i < g_location->m_buildings.Size(); ++i)
    {
      if (g_location->m_buildings.ValidIndex(i))
      {
        Building* building = g_location->m_buildings[i];
        if (building && building->m_type == TypeSpawnPopulationLock)
        {
          SpawnPopulationLock* lock = (SpawnPopulationLock*)building;
          if (lock->m_teamCount[0] > lock->m_originalMaxPopulation)
          {
            totalOverpopulation += (lock->m_teamCount[0] - lock->m_originalMaxPopulation);
          }
        }
      }
    }

    s_overpopulation = totalOverpopulation / 2;
  }


  m_maxPopulation = m_originalMaxPopulation - s_overpopulation;
  m_maxPopulation = std::max(m_maxPopulation, 0);


  //
  // Recount the number of entities nearby

  m_recountTimer -= SERVER_ADVANCE_PERIOD;
  if (m_recountTimer <= 0.0f)
  {
    m_recountTimer = 10.0f;
    m_recountTeamId = 10;
  }


  int potentialTeamId = (int)m_recountTimer;
  if (potentialTeamId < m_recountTeamId && potentialTeamId < NUM_TEAMS)
  {
    m_recountTeamId = potentialTeamId;
    m_teamCount[potentialTeamId] = g_location->m_entityGrid->GetNumFriends(m_pos.x, m_pos.z, m_searchRadius, m_recountTeamId);
  }

  return Building::Advance();
}


void SpawnPopulationLock::Render(float _predictionTime) {}


void SpawnPopulationLock::RenderAlphas(float _predictionTime)
{
  if (g_editing)
  {
    RenderSphere(m_pos, 30.0f, RGBAColour(255, 255, 255, 255));

    DirectX::XMFLOAT3 const pos(m_pos.x, m_pos.y + 250.0f, m_pos.z);

    // Seven lines of debug text stacked above pos, each of which was written as
    // pos + Vector3(0, height, 0). One lambda rather than seven constructions.
    auto const above = [&pos](float _height) { return DirectX::XMFLOAT3(pos.x, pos.y + _height, pos.z); };

    g_editorFont.DrawText3DCentre(above(80.0f), 10, "SpawnPopulationLock");
    g_editorFont.DrawText3DCentre(above(70.0f), 10, "OriginalMaxPopulation = %d", m_originalMaxPopulation);
    g_editorFont.DrawText3DCentre(above(60.0f), 10, "CurrentMaxPopulation = %d", m_maxPopulation);
    g_editorFont.DrawText3DCentre(above(50.0f), 10, "Red = %d", m_teamCount[1]);
    g_editorFont.DrawText3DCentre(above(40.0f), 10, "Green = %d", m_teamCount[0]);

    if (m_teamCount[0] > m_originalMaxPopulation)
    {
      g_editorFont.DrawText3DCentre(above(30.0f), 10, "Green Overpopulated by %d", m_teamCount[0] - m_originalMaxPopulation);
    }

    if (m_teamCount[1] > m_originalMaxPopulation)
    {
      g_editorFont.DrawText3DCentre(above(20.0f), 10, "Red Overpopulated by %d", m_teamCount[1] - m_originalMaxPopulation);
    }

#ifdef LOCATION_EDITOR
    if (g_editing && g_locationEditor->GetMode() == LocationEditorAccess::ModeBuilding && g_locationEditor->GetSelectionId() == m_id.GetUniqueId())
    {
      RenderSphere(m_pos, m_searchRadius, RGBAColour(255, 255, 255, 100));
    }
#endif
  }
}

void SpawnPopulationLock::Read(TextReader* _in, bool _dynamic)
{
  Building::Read(_in, _dynamic);

  m_searchRadius = atof(_in->GetNextToken());
  m_maxPopulation = atoi(_in->GetNextToken());
}


void SpawnPopulationLock::Write(FileWriter* _out)
{
  Building::Write(_out);

  _out->printf("%-8.2f %-6d", m_searchRadius, m_maxPopulation);
}


bool SpawnPopulationLock::DoesSphereHit(DirectX::XMFLOAT3 const& _pos, float _radius) { return false; }


bool SpawnPopulationLock::DoesShapeHit(Shape* _shape, DirectX::XMFLOAT4X4 _transform) { return false; }
