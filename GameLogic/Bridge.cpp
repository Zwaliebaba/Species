#include "pch.h"

#include "Debug.h"
#include "FileWriter.h"
#include "MathUtils.h"
#include "Resource.h"
#include "Shape.h"
#include "TextStreamReaders.h"

#include "EntityGrid.h"
#include "Location.h"
#include "Team.h"

#include "Bridge.h"
#include "WorldPointers.h"


namespace Species
{
  Bridge::Bridge()
    : Teleport(),
      m_nextBridgeId(-1),
      m_status(0.0f),
      m_signal(nullptr),
      m_beingOperated(false)
  {
    m_type = Building::TypeBridge;
    m_sendPeriod = BRIDGE_TRANSPORTPERIOD;

    m_shapes[0] = g_resource->GetShape("BridgeEnd.shp");
    m_shapes[1] = g_resource->GetShape("BridgeTower.shp");

    DEBUG_ASSERT(m_shapes[0]);
    DEBUG_ASSERT(m_shapes[1]);

    SetBridgeType(BridgeTypeTower);
  }

  void Bridge::Initialise(Building* _template)
  {
    Teleport::Initialise(_template);

    m_nextBridgeId = ((Bridge*)_template)->m_nextBridgeId;
    m_status = ((Bridge*)_template)->m_status;
    SetBridgeType(((Bridge*)_template)->m_bridgeType);
  }

  void Bridge::SetBridgeType(int _type)
  {
    m_bridgeType = _type;
    SetShape(m_shapes[m_bridgeType]);

    m_signal = m_shape->m_rootFragment->LookupMarker("MarkerSignal");
    DEBUG_ASSERT(m_signal);
  }


  void Bridge::Render(float predictionTime)
  {
    //
    // Render our shape

    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));
    m_shape->Render(predictionTime, mat);
  }


  void Bridge::RenderAlphas(float predictionTime)
  {
    //
    // Render our signal

    if (m_nextBridgeId != -1)
    {
      Building* building = g_location->GetBuilding(m_nextBridgeId);
      if (building && building->m_type == Building::TypeBridge)
      {
        Bridge* bridge = (Bridge*)building;
        if (m_status > 0.0f && bridge->m_status > 0.0f)
        {
          DirectX::XMFLOAT3 const ourPos = GetStartPoint();
          DirectX::XMFLOAT3 const theirPos = bridge->GetStartPoint();

          if (m_id.GetTeamId() == 255)
          {
            glColor4f(0.5f, 0.5f, 0.5f, m_status / 100.0f);
          }
          else
          {
            RGBAColour col = g_location->m_teams[m_id.GetTeamId()].m_colour;
            glColor4ub(col.r, col.g, col.b, (unsigned char)(255.0f * m_status / 100.0f));
          }

          glLineWidth(3.0f);
          glEnable(GL_BLEND);
          glEnable(GL_LINE_SMOOTH);
          glBegin(GL_LINES);
          glVertex3fv(&ourPos.x);
          glVertex3fv(&theirPos.x);
          glEnd();
          glDisable(GL_LINE_SMOOTH);
          glDisable(GL_BLEND);
        }
      }
    }


    Teleport::RenderAlphas(predictionTime);
  }

  bool Bridge::Advance()
  {
    if (m_status < 0.0f)
      return true;

    if (m_beingOperated)
    {
      m_status = 100.0f;
    }
    else
    {
      m_status = 0.0f;
    }

    return Teleport::Advance();
  }

  bool Bridge::GetAvailablePosition(DirectX::XMFLOAT3& _pos, DirectX::XMFLOAT3& _front)
  {
    DirectX::XMFLOAT4X4 ourMat;
    DirectX::XMStoreFloat4x4(&ourMat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

    DirectX::XMFLOAT4X4 const ourEngineer = m_signal->GetWorldMatrix(ourMat);
    _pos = DirectX::XMFLOAT3(ourEngineer._41, ourEngineer._42, ourEngineer._43);
    _front = DirectX::XMFLOAT3(ourEngineer._31, ourEngineer._32, ourEngineer._33);

    return (!m_beingOperated);
  }


  void Bridge::BeginOperation() { m_beingOperated = true; }


  void Bridge::EndOperation() { m_beingOperated = false; }


  int Bridge::GetBuildingLink() { return m_nextBridgeId; }

  void Bridge::SetBuildingLink(int _buildingId) { m_nextBridgeId = _buildingId; }

  void Bridge::Read(TextReader* _in, bool _dynamic)
  {
    Building::Read(_in, _dynamic);

    char* word = _in->GetNextToken();
    m_nextBridgeId = atoi(word);

    word = _in->GetNextToken();
    m_bridgeType = atoi(word);
    SetBridgeType(m_bridgeType);
  }

  void Bridge::Write(FileWriter* _out)
  {
    Building::Write(_out);

    _out->printf("{:<8d}", m_nextBridgeId);
    _out->printf("{:<8d}", m_bridgeType);
  }

  bool Bridge::ReadyToSend()
  {
    Building* nextBridge = g_location->GetBuilding(m_nextBridgeId);
    return (m_status > 0.0f && nextBridge && m_bridgeType == Bridge::BridgeTypeEnd && nextBridge->m_type == Building::TypeBridge &&
            Teleport::ReadyToSend());
  }

  DirectX::XMFLOAT3 Bridge::GetStartPoint()
  {
    DirectX::XMFLOAT4X4 ourMat;
    DirectX::XMStoreFloat4x4(&ourMat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

    return m_signal->GetWorldPosition(ourMat);
  }

  DirectX::XMFLOAT3 Bridge::GetEndPoint()
  {
    Bridge* nextBridge = (Bridge*)this;
    while (nextBridge->m_nextBridgeId != -1)
    {
      nextBridge = (Bridge*)g_location->GetBuilding(nextBridge->m_nextBridgeId);
    }

    if (!nextBridge)
      return DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

    return nextBridge->GetStartPoint();
  }

  bool Bridge::GetExit(DirectX::XMFLOAT3& _pos, DirectX::XMFLOAT3& _front)
  {
    Bridge* nextBridge = (Bridge*)this;
    while (nextBridge->m_nextBridgeId != -1)
    {
      nextBridge = (Bridge*)g_location->GetBuilding(nextBridge->m_nextBridgeId);
    }

    if (!nextBridge)
      return false;

    DirectX::XMFLOAT4X4 theirMat;
    DirectX::XMStoreFloat4x4(&theirMat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&nextBridge->m_front), DirectX::g_XMIdentityR1,
                                                            DirectX::XMLoadFloat3(&nextBridge->m_pos)));

    DirectX::XMFLOAT4X4 const theirEntrance = nextBridge->m_entrance->GetWorldMatrix(theirMat);

    _pos = DirectX::XMFLOAT3(theirEntrance._41, theirEntrance._42, theirEntrance._43);
    _front = DirectX::XMFLOAT3(theirEntrance._31, theirEntrance._32, theirEntrance._33);

    return true;
  }

  bool Bridge::UpdateEntityInTransit(Entity* _entity)
  {
    Building* building = g_location->GetBuilding(m_nextBridgeId);
    Bridge* nextBridge = (Bridge*)building;

    WorldObjectId id(_entity->m_id);

    if (m_status > 0.0f && nextBridge && nextBridge->m_type == Building::TypeBridge && nextBridge->m_status > 0.0f)
    {
      DirectX::XMFLOAT4X4 theirMat;
      DirectX::XMStoreFloat4x4(&theirMat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&nextBridge->m_front), DirectX::g_XMIdentityR1,
                                                              DirectX::XMLoadFloat3(&nextBridge->m_pos)));

      DirectX::XMFLOAT3 const theirSignalPos = nextBridge->m_signal->GetWorldPosition(theirMat);

      DirectX::XMVECTOR const signalPos = DirectX::XMLoadFloat3(&theirSignalPos);
      DirectX::XMVECTOR const entityPos = DirectX::XMLoadFloat3(&_entity->m_pos);
      DirectX::XMVECTOR const offset = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(signalPos, entityPos));
      float dist = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(entityPos, signalPos)));
      bool arrived = false;

      DirectX::XMStoreFloat3(&_entity->m_vel, DirectX::XMVectorScale(offset, BRIDGE_TRANSPORTSPEED));
      if (DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&_entity->m_vel))) * SERVER_ADVANCE_PERIOD > dist)
      {
        // PRESERVED AS WRITTEN, INCLUDING THE SIGN. Every other velocity here
        // points from the entity TOWARDS the signal; this one is entityPos minus
        // signalPos, which points away, so the arrival frame steps the entity to
        // 2*pos - target before the branch below overwrites m_pos. That is what
        // the legacy code did and the migration is not the place to change it.
        DirectX::XMStoreFloat3(&_entity->m_vel,
                               DirectX::XMVectorScale(DirectX::XMVectorSubtract(entityPos, signalPos), 1.0f / SERVER_ADVANCE_PERIOD));
        arrived = true;
      }

      DirectX::XMStoreFloat3(&_entity->m_pos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&_entity->m_vel),
                                                                           DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD), entityPos));
      _entity->m_onGround = false;
      _entity->m_enabled = false;


      if (arrived)
      {
        // We are there
        if (nextBridge->m_bridgeType == Bridge::BridgeTypeEnd)
        {
          DirectX::XMFLOAT3 exitPos, exitFront;
          nextBridge->GetExit(exitPos, exitFront);
          _entity->m_pos = exitPos;
          _entity->m_front = exitFront;
          _entity->m_enabled = true;
          _entity->m_onGround = true;
          _entity->m_vel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

          g_location->m_entityGrid->AddObject(id, _entity->m_pos.x, _entity->m_pos.z, _entity->m_radius);
          return true;
        }
        else if (nextBridge->m_bridgeType == Bridge::BridgeTypeTower)
        {
          nextBridge->EnterTeleport(id, true);
          return true;
        }
      }

      return false;
    }
    else
    {
      // Shit - we lost the carrier signal, so we die
      _entity->ChangeHealth(-500);
      _entity->m_enabled = true;
      // The three RNG calls stay in this order.
      _entity->m_vel = DirectX::XMFLOAT3(syncsfrand(10.0f), syncfrand(10.0f), syncsfrand(10.0f));

      g_location->m_entityGrid->AddObject(id, _entity->m_pos.x, _entity->m_pos.z, _entity->m_radius);
      return true;
    }
  }
} // namespace Species
