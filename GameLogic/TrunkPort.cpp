#include "pch.h"
#include "GlVertex.h"
#include "SoundSources.h"
#include "Resource.h"
#include "Debug.h"
#include "FileWriter.h"
#include "TextStreamReaders.h"
#include "Shape.h"
#include "HiResTime.h"
#include "TextRenderer.h"
#include "Profiler.h"
#include "LanguageTable.h"

#include "TrunkPort.h"

#include "SoundSystem.h"

#include "GlobalWorld.h"
#include "GameTime.h"
#include "WorldPointers.h"
#include "AppState.h"


TrunkPort::TrunkPort()
  : Building(),
    m_targetLocationId(-1),
    m_openTimer(0.0f),
    m_heightMap(nullptr),
    m_heightMapSize(TRUNKPORT_HEIGHTMAP_MAXSIZE)
{
  m_type = TypeTrunkPort;
  SetShape(g_resource->GetShape("TrunkPort.shp"));

  m_destination1 = m_shape->m_rootFragment->LookupMarker("MarkerDestination1");
  m_destination2 = m_shape->m_rootFragment->LookupMarker("MarkerDestination2");
}


void TrunkPort::SetDetail(int _detail)
{
  m_heightMapSize = int(TRUNKPORT_HEIGHTMAP_MAXSIZE / (float)_detail);

  //
  // Pre-Generate our height map

  // delete[], not delete. The array was allocated with new[] and freed with
  // scalar delete, which is undefined behaviour -- it happened to work because
  // the element type is trivially destructible. Fixed here rather than left,
  // since this line had to change for the type anyway.
  if (m_heightMap)
    delete[] m_heightMap;
  m_heightMap = new DirectX::XMFLOAT3[m_heightMapSize * m_heightMapSize];

  // Zeroing the array as raw bytes, so this depends on the element being three
  // tightly packed floats with no padding and nothing a memset would corrupt.
  // The assert is here so the day that stops holding is a build error rather
  // than a height map full of rubbish. It now says XMFLOAT3, which is what the
  // comment above used to promise the migration would arrive at.
  static_assert(sizeof(DirectX::XMFLOAT3) == 3 * sizeof(float), "this memset assumes XMFLOAT3 is three tightly packed floats");
  memset(m_heightMap, 0, m_heightMapSize * m_heightMapSize * sizeof(DirectX::XMFLOAT3));

  ShapeMarker* marker = m_shape->m_rootFragment->LookupMarker("MarkerSurface");
  DEBUG_ASSERT(marker);

  DirectX::XMFLOAT4X4 transform;
  DirectX::XMStoreFloat4x4(&transform, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

  DirectX::XMFLOAT3 const worldPosStore = marker->GetWorldPosition(transform);
  DirectX::XMVECTOR const worldPos = DirectX::XMLoadFloat3(&worldPosStore);

  float size = 90.0f;
  DirectX::XMVECTOR const up = DirectX::XMVectorScale(DirectX::g_XMIdentityR1, size);
  DirectX::XMVECTOR const right =
    DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1)), size);


  for (int x = 0; x < m_heightMapSize; ++x)
  {
    for (int z = 0; z < m_heightMapSize; ++z)
    {
      float fractionX = (float)x / (float)(m_heightMapSize - 1);
      float fractionZ = (float)z / (float)(m_heightMapSize - 1);

      fractionX -= 0.5f;
      fractionZ -= 0.5f;

      DirectX::XMVECTOR basePos = DirectX::XMVectorMultiplyAdd(right, DirectX::XMVectorReplicate(fractionX), worldPos);
      basePos = DirectX::XMVectorMultiplyAdd(up, DirectX::XMVectorReplicate(fractionZ), basePos);

      // basePos += right * 0.02f;
      // basePos += up * 0.02f;

      // basePos += right * 0.05f;
      // basePos += up * 0.05f;

      DirectX::XMStoreFloat3(&m_heightMap[z * m_heightMapSize + x], basePos);
    }
  }
}


bool TrunkPort::Advance()
{
  GlobalBuilding* gb = g_globalWorld->GetBuilding(m_id.GetUniqueId(), g_locationId);
  if (gb && gb->m_online && m_openTimer == 0.0f)
  {
    m_openTimer = GetHighResTime();
    g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), "PowerUp");
  }

  return Building::Advance();
}

void TrunkPort::Initialise(Building* _template)
{
  Building::Initialise(_template);

  m_targetLocationId = ((TrunkPort*)_template)->m_targetLocationId;
}


void TrunkPort::Render(float predictionTime)
{
  Building::Render(predictionTime);

  char caption[256];

  char const* locationName = g_globalWorld->GetLocationNameTranslated(m_targetLocationId);
  if (locationName)
  {
    strcpy(caption, locationName);
  }
  else
  {
    sprintf(caption, "[%s]", LANGUAGEPHRASE("location_unknown"));
  }

  START_PROFILE(g_profiler, "RenderDestination");

  float fontSize = 70.0f / strlen(caption);
  fontSize = std::min(fontSize, 10.0f);

  DirectX::XMFLOAT4X4 portMat;
  DirectX::XMStoreFloat4x4(&portMat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

  DirectX::XMFLOAT4X4 destMat = m_destination1->GetWorldMatrix(portMat);
  glColor4f(0.9f, 0.8f, 0.8f, 1.0f);
  g_gameFont.DrawText3D(DirectX::XMFLOAT3(destMat._41, destMat._42, destMat._43), DirectX::XMFLOAT3(destMat._31, destMat._32, destMat._33),
                        DirectX::XMFLOAT3(destMat._21, destMat._22, destMat._23), fontSize, "%s", caption);
  g_gameFont.SetRenderShadow(true);
  // The shadow offset: a tenth of a unit forward, a fifth right, a fifth up.
  // operator^ was the cross product, and front x up is the RIGHT vector -- the
  // opposite hand from GetRight()'s up x front, which is why it is spelled out
  // in this order rather than reached for from elsewhere.
  {
    DirectX::XMVECTOR const front = DirectX::XMVectorSet(destMat._31, destMat._32, destMat._33, 0.0f);
    DirectX::XMVECTOR const up = DirectX::XMVectorSet(destMat._21, destMat._22, destMat._23, 0.0f);
    DirectX::XMVECTOR pos = DirectX::XMVectorSet(destMat._41, destMat._42, destMat._43, 0.0f);
    pos = DirectX::XMVectorAdd(pos, DirectX::XMVectorScale(front, 0.1f));
    pos = DirectX::XMVectorAdd(pos, DirectX::XMVectorScale(DirectX::XMVector3Cross(front, up), 0.2f));
    pos = DirectX::XMVectorAdd(pos, DirectX::XMVectorScale(up, 0.2f));
    destMat._41 = DirectX::XMVectorGetX(pos);
    destMat._42 = DirectX::XMVectorGetY(pos);
    destMat._43 = DirectX::XMVectorGetZ(pos);
  }
  glColor4f(0.9f, 0.8f, 0.8f, 0.0f);
  g_gameFont.DrawText3D(DirectX::XMFLOAT3(destMat._41, destMat._42, destMat._43), DirectX::XMFLOAT3(destMat._31, destMat._32, destMat._33),
                        DirectX::XMFLOAT3(destMat._21, destMat._22, destMat._23), fontSize, "%s", caption);

  g_gameFont.SetRenderShadow(false);
  glColor4f(0.9f, 0.8f, 0.8f, 1.0f);
  destMat = m_destination2->GetWorldMatrix(portMat);
  g_gameFont.DrawText3D(DirectX::XMFLOAT3(destMat._41, destMat._42, destMat._43), DirectX::XMFLOAT3(destMat._31, destMat._32, destMat._33),
                        DirectX::XMFLOAT3(destMat._21, destMat._22, destMat._23), fontSize, "%s", caption);
  g_gameFont.SetRenderShadow(true);
  // The shadow offset: a tenth of a unit forward, a fifth right, a fifth up.
  // operator^ was the cross product, and front x up is the RIGHT vector -- the
  // opposite hand from GetRight()'s up x front, which is why it is spelled out
  // in this order rather than reached for from elsewhere.
  {
    DirectX::XMVECTOR const front = DirectX::XMVectorSet(destMat._31, destMat._32, destMat._33, 0.0f);
    DirectX::XMVECTOR const up = DirectX::XMVectorSet(destMat._21, destMat._22, destMat._23, 0.0f);
    DirectX::XMVECTOR pos = DirectX::XMVectorSet(destMat._41, destMat._42, destMat._43, 0.0f);
    pos = DirectX::XMVectorAdd(pos, DirectX::XMVectorScale(front, 0.1f));
    pos = DirectX::XMVectorAdd(pos, DirectX::XMVectorScale(DirectX::XMVector3Cross(front, up), 0.2f));
    pos = DirectX::XMVectorAdd(pos, DirectX::XMVectorScale(up, 0.2f));
    destMat._41 = DirectX::XMVectorGetX(pos);
    destMat._42 = DirectX::XMVectorGetY(pos);
    destMat._43 = DirectX::XMVectorGetZ(pos);
  }
  glColor4f(0.9f, 0.8f, 0.8f, 0.0f);
  g_gameFont.DrawText3D(DirectX::XMFLOAT3(destMat._41, destMat._42, destMat._43), DirectX::XMFLOAT3(destMat._31, destMat._32, destMat._33),
                        DirectX::XMFLOAT3(destMat._21, destMat._22, destMat._23), fontSize, "%s", caption);

  g_gameFont.SetRenderShadow(false);

  END_PROFILE(g_profiler, "RenderDestination");
}


bool TrunkPort::PerformDepthSort(DirectX::XMFLOAT3& _centrePos)
{
  _centrePos = m_centrePos;

  return true;
}


void TrunkPort::RenderAlphas(float predictionTime)
{
  Building::RenderAlphas(predictionTime);

  if (m_openTimer > 0.0f)
  {
    ShapeMarker* marker = m_shape->m_rootFragment->LookupMarker("MarkerSurface");
    DEBUG_ASSERT(marker);

    DirectX::XMFLOAT4X4 transform;
    DirectX::XMStoreFloat4x4(&transform,
                             BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

    DirectX::XMFLOAT3 const markerPosStore = marker->GetWorldPosition(transform);
    DirectX::XMVECTOR const markerPos = DirectX::XMLoadFloat3(&markerPosStore);
    float maxDistance = 40.0f;

    float timeOpen = GetHighResTime() - m_openTimer;
    float timeScale = (5 - timeOpen);
    if (timeScale < 1.0f)
      timeScale = 1.0f;

    //
    // Calculate our Dif map based on some nice sine curves

    START_PROFILE(g_profiler, "Advance Heightmap");

    DirectX::XMFLOAT3 difMap[TRUNKPORT_HEIGHTMAP_MAXSIZE][TRUNKPORT_HEIGHTMAP_MAXSIZE];

    for (int x = 0; x < m_heightMapSize; ++x)
    {
      for (int z = 0; z < m_heightMapSize; ++z)
      {
        float centreDif = DirectX::XMVectorGetX(
          DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_heightMap[z * m_heightMapSize + x]), markerPos)));
        float fractionOut = centreDif / maxDistance;
        if (fractionOut > 1.0f)
          fractionOut = 1.0f;

        float wave1 = cosf(centreDif * 0.15f);
        float wave2 = cosf(centreDif * 0.05f);

        DirectX::XMVECTOR const front = DirectX::XMLoadFloat3(&m_front);
        DirectX::XMVECTOR thisDif = DirectX::XMVectorScale(front, sinf(g_gameTime * 2) * wave1 * (1.0f - fractionOut) * 15 * timeScale);
        thisDif = DirectX::XMVectorMultiplyAdd(
          front, DirectX::XMVectorReplicate(sinf(g_gameTime * 2.5) * wave2 * (1.0f - fractionOut) * 15 * timeScale), thisDif);
        thisDif = DirectX::XMVectorMultiplyAdd(DirectX::g_XMIdentityR1,
                                               DirectX::XMVectorReplicate(cosf(g_gameTime) * wave1 * timeScale * 10 * (1.0f - fractionOut)), thisDif);
        DirectX::XMStoreFloat3(&difMap[x][z], thisDif);
      }
    }

    END_PROFILE(g_profiler, "Advance Heightmap");

    //
    // Render our height map with the dif map added on

    START_PROFILE(g_profiler, "Render Heightmap");

    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(false);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/LaserFence.bmp"));

    float alphaValue = timeOpen;
    if (alphaValue > 0.7f)
      alphaValue = 0.7f;
    glColor4f(0.8f, 0.8f, 1.0f, alphaValue);

    for (int x = 0; x < m_heightMapSize - 1; ++x)
    {
      for (int z = 0; z < m_heightMapSize - 1; ++z)
      {
        DirectX::XMVECTOR const thisPos1 =
          DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_heightMap[z * m_heightMapSize + x]), DirectX::XMLoadFloat3(&difMap[x][z]));
        DirectX::XMVECTOR const thisPos2 =
          DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_heightMap[z * m_heightMapSize + x + 1]), DirectX::XMLoadFloat3(&difMap[x + 1][z]));
        DirectX::XMVECTOR const thisPos3 =
          DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_heightMap[(z + 1) * m_heightMapSize + x + 1]), DirectX::XMLoadFloat3(&difMap[x + 1][z + 1]));
        DirectX::XMVECTOR const thisPos4 =
          DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_heightMap[(z + 1) * m_heightMapSize + x]), DirectX::XMLoadFloat3(&difMap[x][z + 1]));

        float fractionX = (float)x / (float)m_heightMapSize;
        float fractionZ = (float)z / (float)m_heightMapSize;
        float width = 1.0f / m_heightMapSize;

        glBegin(GL_QUADS);
        glTexCoord2f(fractionX, fractionZ);
        EmitVertex(thisPos1);
        glTexCoord2f(fractionX + width, fractionZ);
        EmitVertex(thisPos2);
        glTexCoord2f(fractionX + width, fractionZ + width);
        EmitVertex(thisPos3);
        glTexCoord2f(fractionX, fractionZ + width);
        EmitVertex(thisPos4);
        glEnd();
      }
    }

    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Glow.bmp"));
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    for (int x = 0; x < m_heightMapSize - 1; ++x)
    {
      for (int z = 0; z < m_heightMapSize - 1; ++z)
      {
        DirectX::XMVECTOR const thisPos1 =
          DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_heightMap[z * m_heightMapSize + x]), DirectX::XMLoadFloat3(&difMap[x][z]));
        DirectX::XMVECTOR const thisPos2 =
          DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_heightMap[z * m_heightMapSize + x + 1]), DirectX::XMLoadFloat3(&difMap[x + 1][z]));
        DirectX::XMVECTOR const thisPos3 =
          DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_heightMap[(z + 1) * m_heightMapSize + x + 1]), DirectX::XMLoadFloat3(&difMap[x + 1][z + 1]));
        DirectX::XMVECTOR const thisPos4 =
          DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_heightMap[(z + 1) * m_heightMapSize + x]), DirectX::XMLoadFloat3(&difMap[x][z + 1]));

        float fractionX = (float)x / (float)m_heightMapSize;
        float fractionZ = (float)z / (float)m_heightMapSize;
        float width = 1.0f / m_heightMapSize;

        glBegin(GL_QUADS);
        glTexCoord2f(fractionX, fractionZ);
        EmitVertex(thisPos1);
        glTexCoord2f(fractionX + width, fractionZ);
        EmitVertex(thisPos2);
        glTexCoord2f(fractionX + width, fractionZ + width);
        EmitVertex(thisPos3);
        glTexCoord2f(fractionX, fractionZ + width);
        EmitVertex(thisPos4);
        glEnd();
      }
    }

    glDisable(GL_TEXTURE_2D);

    glDepthMask(true);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);

    END_PROFILE(g_profiler, "Render Heightmap");
  }
}

void TrunkPort::ReprogramComplete()
{
  GlobalLocation* location = g_globalWorld->GetLocation(m_targetLocationId);
  if (location)
  {
    location->m_available = true;

    // Look for a "receiver" trunk port and set that to the same state
    for (auto const& building : g_globalWorld->m_buildings)
    {
      if (building->m_type == Building::TypeTrunkPort && building->m_locationId == m_targetLocationId && building->m_link == g_locationId)
      {
        building->m_online = true;
      }
    }
  }

  Building::ReprogramComplete();
}


void TrunkPort::ListSoundEvents(std::vector<const char*>* _list)
{
  Building::ListSoundEvents(_list);

  _list->push_back("PowerUp");
}


void TrunkPort::Read(TextReader* _in, bool _dynamic)
{
  Building::Read(_in, _dynamic);

  if (_in->TokenAvailable())
  {
    m_targetLocationId = atoi(_in->GetNextToken());
  }
}

void TrunkPort::Write(FileWriter* _out)
{
  Building::Write(_out);

  _out->printf("%-8d", m_targetLocationId);
}
