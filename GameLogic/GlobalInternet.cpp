#include "pch.h"
#include "GlVertex.h"

#include <float.h>

#include "HiResTime.h"
#include "MathUtils.h"
#include "Resource.h"
#include "Profiler.h"
#include "Debug.h"

#include "GlobalInternet.h"
#include "GlobalWorld.h"
#include "GameTime.h"
#include "WorldPointers.h"


#define DISPLAY_LIST_NAME_LINKS "GlobalInternetLinks"
#define DISPLAY_LIST_NAME_NODES "GlobalInternetNodes"


//*****************************************************************************
// Class GlobalInternetNode
//*****************************************************************************

GlobalInternetNode::GlobalInternetNode()
  : m_size(0),
    m_burst(0),
    m_numLinks(0)
{
}


void GlobalInternetNode::AddLink(int _id)
{
  DEBUG_ASSERT(m_numLinks < GLOBALINTERNET_MAXNODELINKS);
  m_links[m_numLinks] = _id;
  m_numLinks++;
}


// ****************************************************************************
// Class GlobalInternet
// ****************************************************************************

GlobalInternet::GlobalInternet()
  : m_links(0),
    m_numLinks(0),
    m_nodes(nullptr),
    m_numNodes(0),
    m_nearestNodeToCentre(-1),
    m_nearestDistance(FLT_MAX)
{
  speciesSeedRandom(1);
  GenerateInternet();
}


GlobalInternet::~GlobalInternet() { DeleteInternet(); }


unsigned short GlobalInternet::GenerateInternet(DirectX::XMFLOAT3 const& _pos, unsigned char _size)
{
  GetHighResTime();

  GlobalInternetNode* node = &m_nodes[m_numNodes];
  node->m_pos = _pos;
  node->m_size = _size;
  unsigned short nodeIndex = m_numNodes;
  m_numNodes++;
  DEBUG_ASSERT(m_numNodes < GLOBALINTERNET_MAXNODES);

  float distanceToCentre = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&_pos)));
  if (distanceToCentre < m_nearestDistance)
  {
    m_nearestDistance = distanceToCentre;
    m_nearestNodeToCentre = nodeIndex;
  }

  unsigned char numLinks = _size;
  float distance = powf(_size, 4.0f) * 2.0f;

  while (numLinks > 0)
  {
    float z = sfrand(distance);
    float y = sfrand(distance);
    float x = sfrand(distance);
    DirectX::XMFLOAT3 const newPos(_pos.x + x, _pos.y + y, _pos.z + z);
    unsigned short newIndex = GenerateInternet(newPos, _size - 1);
    m_links[m_numLinks].m_from = nodeIndex;
    m_links[m_numLinks].m_to = newIndex;
    m_links[m_numLinks].m_size = _size;

    m_nodes[newIndex].AddLink(m_numLinks);
    node->AddLink(m_numLinks);

    m_numLinks++;
    DEBUG_ASSERT(m_numLinks <= GLOBALINTERNET_MAXLINKS);

    --numLinks;
  }

  return nodeIndex;
}


void GlobalInternet::GenerateInternet()
{
  double timeStart = GetHighResTime();

  m_links = new GlobalInternetLink[GLOBALINTERNET_MAXLINKS];
  m_nodes = new GlobalInternetNode[GLOBALINTERNET_MAXNODES];

  // XMFLOAT3 centre(200, 200, 200);
  // XMFLOAT3 centre(449,1787,-139);
  DirectX::XMFLOAT3 const centre(-797.0f, 1949.0f, -1135.0f);
  unsigned short firstNode = GenerateInternet(centre, GLOBALINTERNET_ITERATIONS);

  m_nodes[m_numNodes].m_pos = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
  m_nodes[m_numNodes].m_size = 0.0f;
  unsigned short nodeIndex = m_numNodes;
  m_numNodes++;
  DEBUG_ASSERT(m_numNodes <= GLOBALINTERNET_MAXNODES);

  m_links[m_numLinks].m_from = m_nearestNodeToCentre;
  m_links[m_numLinks].m_to = nodeIndex;
  m_links[m_numLinks].m_size = 1.0f;
  m_numLinks++;
  DEBUG_ASSERT(m_numLinks <= GLOBALINTERNET_MAXLINKS);

  for (int i = 0; i < m_numNodes; ++i)
  {
    GlobalInternetNode* node = &m_nodes[i];
    if (node->m_numLinks == 1)
    {
      m_leafs.push_back(i);
    }
  }

#ifdef DEBUG
  for (int i = 0; i < 5; ++i)
  {
    GlobalInternetNode* node = &m_nodes[i];
    DebugTrace("Node {} : {:.2f} {:.2f} {:.2f}\n", i, node->m_pos.x, node->m_pos.y, node->m_pos.z);

    /*
            Node 0 : -797.00 1949.00 -1135.00
            Node 1 : 675.75 1643.66 1259.99
            Node 2 : 727.92 1423.32 459.74
            Node 3 : 324.37 928.37 646.87
            Node 4 : 140.59 1095.22 520.61
    */


  double timeTaken = GetHighResTime() - timeStart;
  DebugTrace("Global Internet time to generate : {:.2f}ms\n", timeTaken * 1000.0);
  DebugTrace("Global Internet number of nodes  : {}\n", m_numNodes);
  DebugTrace("Global Internet number of links  : {}\n", m_numLinks);
  DebugTrace("Global Internet number of leafs  : {}\n", static_cast<int>(m_leafs.size()));
#endif
}


void GlobalInternet::DeleteInternet()
{
  delete[] m_nodes;
  m_numNodes = 0;
  delete[] m_links;
  m_numLinks = 0;
  m_leafs.clear();
  m_bursts.clear();

  g_resource->DeleteDisplayList(DISPLAY_LIST_NAME_LINKS);
  g_resource->DeleteDisplayList(DISPLAY_LIST_NAME_NODES);
}


void GlobalInternet::Render()
{
  START_PROFILE(g_profiler, "Internet");

  /*static*/ float scale = 1000.0f;

  glPushMatrix();
  glScalef(scale, scale, scale);


  float fog = 0.0f;
  float fogCol[] = {fog, fog, fog, fog};

  /*static*/ int fogVal = 5810000;
  //    if( g_keys[KEY_P] )
  //    {
  //        fogVal += 100000;
  //    }
  //    if( g_keys[KEY_O] )
  //    {
  //        fogVal -= 100000;
  //    }

  glFogf(GL_FOG_DENSITY, 1.0f);
  glFogf(GL_FOG_START, 0.0f);
  glFogf(GL_FOG_END, (float)fogVal);
  glFogfv(GL_FOG_COLOR, fogCol);
  glFogi(GL_FOG_MODE, GL_LINEAR);
  glEnable(GL_FOG);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glDepthMask(false);
  glDisable(GL_CULL_FACE);
  glEnable(GL_TEXTURE_2D);


  int linksId = g_resource->GetDisplayList(DISPLAY_LIST_NAME_LINKS);
  if (linksId >= 0)
  {
    glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/LaserFence2.bmp"));

    glCallList(linksId);
  }
  else
  {
    linksId = g_resource->CreateDisplayList(DISPLAY_LIST_NAME_LINKS);
    glNewList(linksId, GL_COMPILE);

    glColor4f(0.25f, 0.25f, 0.5f, 0.8f);

    //
    // Render Links

    glBegin(GL_QUADS);

    for (int i = 0; i < m_numLinks; ++i)
    {
      GlobalInternetLink* link = &m_links[i];
      GlobalInternetNode* from = &m_nodes[link->m_from];
      GlobalInternetNode* to = &m_nodes[link->m_to];

      DirectX::XMVECTOR const fromPos = DirectX::XMLoadFloat3(&from->m_pos);
      DirectX::XMVECTOR const toPos = DirectX::XMLoadFloat3(&to->m_pos);
      DirectX::XMVECTOR const midPoint = DirectX::XMVectorScale(DirectX::XMVectorAdd(toPos, fromPos), 0.5f);
      DirectX::XMVECTOR const camToMidPoint = DirectX::XMVectorNegate(midPoint);
      DirectX::XMVECTOR const rightAngle = DirectX::XMVectorScale(
        DirectX::XMVector3Normalize(DirectX::XMVector3Cross(camToMidPoint, DirectX::XMVectorSubtract(midPoint, toPos))), link->m_size * 0.5f);

      glTexCoord2i(0, 0);
      EmitVertex(DirectX::XMVectorSubtract(fromPos, rightAngle));
      glTexCoord2i(0, 1);
      EmitVertex(DirectX::XMVectorAdd(fromPos, rightAngle));
      glTexCoord2i(1, 1);
      EmitVertex(DirectX::XMVectorAdd(toPos, rightAngle));
      glTexCoord2i(1, 0);
      EmitVertex(DirectX::XMVectorSubtract(toPos, rightAngle));
    }
    glEnd();

    glEndList();
  }


  int nodesId = g_resource->GetDisplayList(DISPLAY_LIST_NAME_NODES);
  if (nodesId >= 0)
  {
    glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Glow.bmp"));

    glCallList(nodesId);
  }
  else
  {
    nodesId = g_resource->CreateDisplayList(DISPLAY_LIST_NAME_NODES);
    glNewList(nodesId, GL_COMPILE);

    glColor4f(0.8f, 0.8f, 1.0f, 0.6f);
    float nodeSize = 10.0f;

    glBegin(GL_QUADS);
    for (int i = 0; i < m_numNodes; ++i)
    {
      GlobalInternetNode* node = &m_nodes[i];

      DirectX::XMVECTOR const nodePos = DirectX::XMLoadFloat3(&node->m_pos);
      DirectX::XMVECTOR const camToMidPoint = DirectX::XMVector3Normalize(DirectX::XMVectorNegate(nodePos));
      // up is crossed from the UNSCALED right, then both are scaled -- taking
      // it from the scaled one would square nodeSize.
      DirectX::XMVECTOR const rightAxis = DirectX::XMVector3Cross(camToMidPoint, DirectX::g_XMIdentityR2);
      DirectX::XMVECTOR const right = DirectX::XMVectorScale(rightAxis, nodeSize);
      DirectX::XMVECTOR const up = DirectX::XMVectorScale(DirectX::XMVector3Cross(rightAxis, camToMidPoint), nodeSize);
      glTexCoord2i(0, 0);
      EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorSubtract(nodePos, up), right));
      glTexCoord2i(1, 0);
      EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorSubtract(nodePos, up), right));
      glTexCoord2i(1, 1);
      EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(nodePos, up), right));
      glTexCoord2i(0, 1);
      EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorAdd(nodePos, up), right));
    }
    glEnd();

    glEndList();
  }


  glDisable(GL_TEXTURE_2D);
  glEnable(GL_CULL_FACE);
  glDepthMask(true);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_BLEND);


  RenderPackets();

  g_globalWorld->SetupFog();
  glDisable(GL_FOG);

  glPopMatrix();

  END_PROFILE(g_profiler, "Internet");
}


void GlobalInternet::TriggerPacket(unsigned short _nodeId, unsigned short _fromLinkId)
{
  GlobalInternetNode* newNode = &m_nodes[_nodeId];

  if (newNode->m_numLinks == 1 && newNode->m_links[0] == _fromLinkId)
  {
    return;
  }

  while (true)
  {
    int newLinkIndex = speciesRandom() % newNode->m_numLinks;
    if (newNode->m_links[newLinkIndex] != _fromLinkId)
    {
      GlobalInternetLink* newLink = &m_links[newNode->m_links[newLinkIndex]];
      if (newLink->m_from == _nodeId)
      {
        newLink->m_packets.push_back(1.0f);
      }
      else
      {
        newLink->m_packets.push_back(-1.0f);
      }
      break;
    }
  }
}


void GlobalInternet::RenderPackets()
{
  //
  // Create new packets

  if (frand(100.0f) < 11.0f)
  {
    int leafIndex = frand(static_cast<int>(m_leafs.size()));
    GlobalInternetNode* node = &m_nodes[m_leafs[leafIndex]];
    node->m_burst = 4.0f;
    m_bursts.push_back(m_leafs[leafIndex]);
  }


  //
  // Deal with data bursts

  for (int i = 0; i < static_cast<int>(m_bursts.size()); ++i)
  {
    GlobalInternetNode* node = &m_nodes[m_bursts[i]];
    node->m_burst -= g_advanceTime;
    if (node->m_burst > 0.0f)
    {
      TriggerPacket(m_bursts[i], -1);
    }
    else
    {
      m_bursts.erase(m_bursts.begin() + i);
      --i;
    }
  }


  //
  // Advance / render all packets

  float packetSize = 30.0f;
  DirectX::XMFLOAT3 const cameraRight = g_camera->GetRight();
  DirectX::XMFLOAT3 const cameraUp = g_camera->GetUp();
  DirectX::XMVECTOR const camRight = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&cameraRight), packetSize);
  DirectX::XMVECTOR const camUp = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&cameraUp), packetSize);
  float posChange = g_advanceTime;

  glColor4f(0.25f, 0.25f, 0.5f, 0.8f);

  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Starburst.bmp"));
  glDepthMask(false);

  for (int i = 0; i < m_numLinks; ++i)
  {
    GlobalInternetLink* link = &m_links[i];
    for (int j = 0; j < static_cast<int>(link->m_packets.size()); ++j)
    {
      float* thisPacket = &link->m_packets[j];
      float packetVal = *thisPacket;
      if (*thisPacket > 0.0f)
      {
        *thisPacket -= posChange;
        if (*thisPacket <= 0.01f)
        {
          *thisPacket = 0.01f;
          TriggerPacket(link->m_to, i);
          link->m_packets.erase(link->m_packets.begin() + j);
          --j;
        }
      }
      else if (*thisPacket < 0.0f)
      {
        *thisPacket += posChange;
        if (*thisPacket >= -0.01f)
        {
          *thisPacket = -0.01f;
          TriggerPacket(link->m_from, i);
          link->m_packets.erase(link->m_packets.begin() + j);
          --j;
        }
      }

      GlobalInternetNode* from = &m_nodes[link->m_from];
      GlobalInternetNode* to = &m_nodes[link->m_to];
      // Zeroed, and load-bearing: neither branch runs when packetVal is
      // exactly 0, and Vector3's default constructor zeroed where XMVECTOR
      // carries whatever was in the register.
      DirectX::XMVECTOR packetPos = DirectX::XMVectorZero();
      DirectX::XMVECTOR const fromPos = DirectX::XMLoadFloat3(&from->m_pos);
      DirectX::XMVECTOR const toPos = DirectX::XMLoadFloat3(&to->m_pos);
      if (packetVal > 0.0f)
      {
        packetPos = DirectX::XMVectorMultiplyAdd(DirectX::XMVectorSubtract(fromPos, toPos), DirectX::XMVectorReplicate(packetVal), toPos);
      }
      else if (packetVal < 0.0f)
      {
        packetPos = DirectX::XMVectorMultiplyAdd(DirectX::XMVectorSubtract(toPos, fromPos), DirectX::XMVectorReplicate(-packetVal), fromPos);
      }

      glBegin(GL_QUADS);
      glTexCoord2i(0, 0);
      EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorSubtract(packetPos, camUp), camRight));
      glTexCoord2i(1, 0);
      EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorSubtract(packetPos, camUp), camRight));
      glTexCoord2i(1, 1);
      EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(packetPos, camUp), camRight));
      glTexCoord2i(0, 1);
      EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorAdd(packetPos, camUp), camRight));
      glEnd();
    }
  }

  glDepthMask(true);
  glDisable(GL_TEXTURE_2D);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_BLEND);
  glEnable(GL_CULL_FACE);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
}
