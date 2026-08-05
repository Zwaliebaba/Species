#include "pch.h"
#include "GlVertex.h"

#include "Resource.h"
#include "Shape.h"
#include "TextStreamReaders.h"
#include "DebugRender.h"
#include "FileWriter.h"
#include "TextRenderer.h"
#include "LanguageTable.h"

#include "BlueprintStore.h"
#include "Citizen.h"

#include "Location.h"
#include "GameTime.h"
#include "Team.h"
#include "GlobalWorld.h"
#include "WorldPointers.h"
#include "AppState.h"


namespace Species
{
  BlueprintBuilding::BlueprintBuilding()
    : Building(),
      m_buildingLink(-1),
      m_infected(0.0f),
      m_segment(0),
      m_marker(nullptr)
  {
    m_vel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
  }


  void BlueprintBuilding::Initialise(Building* _template)
  {
    Building::Initialise(_template);

    m_marker = m_shape->m_rootFragment->LookupMarker("MarkerBlueprint");
    DEBUG_ASSERT(m_marker);

    BlueprintBuilding* blueprintBuilding = (BlueprintBuilding*)_template;

    m_buildingLink = blueprintBuilding->m_buildingLink;

    if (m_id.GetTeamId() == 1)
      m_infected = 100.0f;
    else
      m_infected = 0.0f;
  }


  bool BlueprintBuilding::Advance()
  {
    BlueprintBuilding* blueprintBuilding = (BlueprintBuilding*)g_location->GetBuilding(m_buildingLink);
    if (blueprintBuilding)
    {
      if (m_infected > 80.0f)
        blueprintBuilding->SendBlueprint(m_segment, true);
      if (m_infected < 20.0f)
        blueprintBuilding->SendBlueprint(m_segment, false);
    }

    return Building::Advance();
  }


  DirectX::XMFLOAT4X4 BlueprintBuilding::GetMarker(float _predictionTime)
  {
    DirectX::XMVECTOR const pos =
      DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(_predictionTime), DirectX::XMLoadFloat3(&m_pos));

    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, pos));

    if (m_marker)
    {
      // The marker's own matrix, rows and all -- rebuilding the basis from front
      // and up here would discard its right row.
      return m_marker->GetWorldMatrix(mat);
    }
    else
    {
      return mat;
    }
  }


  bool BlueprintBuilding::IsInView()
  {
    Building* link = g_location->GetBuilding(m_buildingLink);

    if (link)
    {
      DirectX::XMVECTOR const linkCentre = DirectX::XMLoadFloat3(&link->m_centrePos);
      DirectX::XMVECTOR const ourCentre = DirectX::XMLoadFloat3(&m_centrePos);
      DirectX::XMFLOAT3 midPoint;
      DirectX::XMStoreFloat3(&midPoint, DirectX::XMVectorScale(DirectX::XMVectorAdd(linkCentre, ourCentre), 0.5f));
      float radius = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(linkCentre, ourCentre)));
      radius += m_radius;
      return (g_camera->SphereInViewFrustum(midPoint, radius));
    }
    else
    {
      return Building::IsInView();
    }
  }


  void BlueprintBuilding::Render(float _predictionTime)
  {
    DirectX::XMVECTOR const pos =
      DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(_predictionTime), DirectX::XMLoadFloat3(&m_pos));

    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, pos));
    m_shape->Render(_predictionTime, mat);
  }


  void BlueprintBuilding::RenderAlphas(float _predictionTime)
  {
    Building::RenderAlphas(_predictionTime);

    BlueprintBuilding* link = (BlueprintBuilding*)g_location->GetBuilding(m_buildingLink);
    if (link)
    {
      float infected = m_infected / 100.0f;
      float linkInfected = link->m_infected / 100.0f;
      float ourTime = g_gameTime + m_id.GetUniqueId() + m_id.GetIndex();
      if (fabs(infected - linkInfected) < 0.01f)
      {
        glColor4f(infected, 0.7f - infected * 0.7f, 0.0f, 0.1f + fabs(sinf(ourTime)) * 0.2f);
      }
      else
      {
        glColor4f(infected, 0.7f - infected * 0.7f, 0.0f, 0.5f + fabs(sinf(ourTime)) * 0.5f);
      }

      // Row 3 of each marker matrix is its position.
      DirectX::XMFLOAT4X4 const ourMarker = GetMarker(_predictionTime);
      DirectX::XMFLOAT4X4 const theirMarker = link->GetMarker(_predictionTime);
      DirectX::XMVECTOR const ourPos = DirectX::XMLoadFloat4x4(&ourMarker).r[3];
      DirectX::XMVECTOR const theirPos = DirectX::XMLoadFloat4x4(&theirMarker).r[3];

      DirectX::XMFLOAT3 const cameraPos = g_camera->GetPos();
      // SetLength, taken natively: the cross is only zero when the camera is
      // exactly on the line joining the two markers, and this is rendering.
      DirectX::XMVECTOR const rightAngle =
        DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMVector3Cross(
                                 DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&cameraPos), ourPos), DirectX::XMVectorSubtract(theirPos, ourPos))),
                               20.0f);

      glDisable(GL_CULL_FACE);
      glEnable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Laser.bmp"));

      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE);
      glDepthMask(false);

      glBegin(GL_QUADS);
      glTexCoord2i(0, 0);
      EmitVertex(DirectX::XMVectorSubtract(ourPos, rightAngle));
      glTexCoord2i(0, 1);
      EmitVertex(DirectX::XMVectorAdd(ourPos, rightAngle));
      glTexCoord2i(1, 1);
      EmitVertex(DirectX::XMVectorAdd(theirPos, rightAngle));
      glTexCoord2i(1, 0);
      EmitVertex(DirectX::XMVectorSubtract(theirPos, rightAngle));
      glEnd();

      glDepthMask(true);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDisable(GL_TEXTURE_2D);
    }

    // g_editorFont.DrawText3DCentre( m_pos+XMFLOAT3(0,50,0), 10.0f, "{} Infected {:2.2f}", m_segment, m_infected );
  }


  void BlueprintBuilding::SendBlueprint(int _segment, bool _infected)
  {
    m_segment = _segment;

    if (_infected)
      m_infected += SERVER_ADVANCE_PERIOD * 10.0f;
    else
      m_infected -= SERVER_ADVANCE_PERIOD * 10.0f;

    m_infected = std::max(m_infected, 0.0f);
    m_infected = std::min(m_infected, 100.0f);
  }


  void BlueprintBuilding::Read(TextReader* _in, bool _dynamic)
  {
    Building::Read(_in, _dynamic);

    m_buildingLink = atoi(_in->GetNextToken());
  }


  void BlueprintBuilding::Write(FileWriter* _out)
  {
    Building::Write(_out);

    _out->printf("{:<8d}", m_buildingLink);
  }


  int BlueprintBuilding::GetBuildingLink() { return m_buildingLink; }


  void BlueprintBuilding::SetBuildingLink(int _buildingId) { m_buildingLink = _buildingId; }


  // ============================================================================


  BlueprintStore::BlueprintStore()
    : BlueprintBuilding()
  {
    m_type = Building::TypeBlueprintStore;

    SetShape(g_resource->GetShape("BlueprintStore.shp"));
  }


  char const* BlueprintStore::GetObjectiveCounter()
  {
    static char result[256];

    float totalInfection = 0;
    for (int i = 0; i < BLUEPRINTSTORE_NUMSEGMENTS; ++i)
      totalInfection += m_segments[i];

    sprintf(result, "%s %d%%", LANGUAGEPHRASE("objective_totalinfection"), int(100.0f * totalInfection / float(BLUEPRINTSTORE_NUMSEGMENTS * 100.0f)));

    return result;
  }


  void BlueprintStore::Initialise(Building* _template)
  {
    BlueprintBuilding::Initialise(_template);

    if (m_id.GetTeamId() == 1)
    {
      for (int i = 0; i < BLUEPRINTSTORE_NUMSEGMENTS; ++i)
      {
        m_segments[i] = 100.0f;
      }
    }
    else
    {
      for (int i = 0; i < BLUEPRINTSTORE_NUMSEGMENTS; ++i)
      {
        m_segments[i] = 0.0f;
      }
    }
  }


  void BlueprintStore::SendBlueprint(int _segment, bool _infected)
  {
    float oldValue = m_segments[_segment];

    if (_infected)
      oldValue += SERVER_ADVANCE_PERIOD * 1.0f;
    else
      oldValue -= SERVER_ADVANCE_PERIOD * 1.0f;

    oldValue = std::max(oldValue, 0.0f);
    oldValue = std::min(oldValue, 100.0f);

    m_segments[_segment] = oldValue;
  }


  bool BlueprintStore::Advance()
  {
    int fullyInfected = 0;
    for (int i = 0; i < BLUEPRINTSTORE_NUMSEGMENTS; ++i)
    {
      if (m_segments[i] == 100.0f)
      {
        fullyInfected++;
      }
    }
    int clean = BLUEPRINTSTORE_NUMSEGMENTS - fullyInfected;


    //
    // Spread our existing infection

    if (clean > 0)
    {
      float infectionChange = (fullyInfected * 0.9f) / (float)clean;

      for (int i = 0; i < BLUEPRINTSTORE_NUMSEGMENTS; ++i)
      {
        if (m_segments[i] < 100.0f)
        {
          float oldValue = m_segments[i];
          oldValue += SERVER_ADVANCE_PERIOD * infectionChange;
          oldValue = std::max(oldValue, 0.0f);
          oldValue = std::min(oldValue, 100.0f);
          m_segments[i] = oldValue;
        }
      }
    }


    //
    // Are we clean?

    bool totallyClean = true;
    for (int i = 0; i < BLUEPRINTSTORE_NUMSEGMENTS; ++i)
    {
      if (m_segments[i] > 0.0f)
      {
        totallyClean = false;
        break;
      }
    }

    if (totallyClean)
    {
      GlobalBuilding* gb = g_globalWorld->GetBuilding(m_id.GetUniqueId(), g_locationId);
      if (gb)
        gb->m_online = true;
    }

    return BlueprintBuilding::Advance();
  }


  int BlueprintStore::GetNumInfected()
  {
    int result = 0;
    for (int i = 0; i < BLUEPRINTSTORE_NUMSEGMENTS; ++i)
    {
      if (!(m_segments[i] < 100.0f))
      {
        ++result;
      }
    }
    return result;
  }


  int BlueprintStore::GetNumClean()
  {
    int result = 0;
    for (int i = 0; i < BLUEPRINTSTORE_NUMSEGMENTS; ++i)
    {
      if (!(m_segments[i] > 0.0f))
      {
        ++result;
      }
    }
    return result;
  }


  void BlueprintStore::GetDisplay(DirectX::XMFLOAT3& _pos, DirectX::XMFLOAT3& _right, DirectX::XMFLOAT3& _up, float& _size)
  {
    _size = 50.0f;

    DirectX::XMVECTOR const front =
      DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&m_front), DirectX::XMMatrixRotationY(sinf(g_gameTime) * 0.3f));

    // RotateAround's angle is the axis vector's magnitude, with its own 1e-8 guard.
    DirectX::XMVECTOR up = DirectX::g_XMIdentityR1;
    DirectX::XMVECTOR const spinAxis = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_front), cosf(g_gameTime) * 0.1f);
    float const spinLengthSquared = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(spinAxis));
    if (spinLengthSquared >= 1e-8f)
    {
      float const spin = sqrtf(spinLengthSquared);
      up = DirectX::XMVector3Transform(up, DirectX::XMMatrixRotationAxis(DirectX::XMVectorScale(spinAxis, 1.0f / spin), spin));
    }

    DirectX::XMVECTOR position = DirectX::XMVectorMultiplyAdd(up, DirectX::XMVectorReplicate(50.0f), DirectX::XMLoadFloat3(&m_pos));
    position = DirectX::XMVectorNegativeMultiplySubtract(front, DirectX::XMVectorReplicate(_size), position);

    DirectX::XMStoreFloat3(&_pos, position);
    DirectX::XMStoreFloat3(&_right, front);
    DirectX::XMStoreFloat3(&_up, up);
  }


  void BlueprintStore::Render(float _predictionTime)
  {
    BlueprintBuilding::Render(_predictionTime);

    //    for( int i = 0; i < BLUEPRINTSTORE_NUMSEGMENTS; ++i )
    //    {
    //        g_editorFont.DrawText3DCentre( m_pos+XMFLOAT3(0,170+i*20,0), 20, "Segment {} : {:2.2f}", i, m_segments[i] );
    //    }
  }


  /*
  void BlueprintStore::RenderAlphas( float _predictionTime )
  {
      BlueprintBuilding::RenderAlphas( _predictionTime );

      XMFLOAT3 screenPos, screenRight, screenUp;
      float screenSize;
      GetDisplay( screenPos, screenRight, screenUp, screenSize );

      glColor4f( 1.0f, 1.0f, 1.0f, 0.75f );
      glDisable( GL_CULL_FACE );
      glEnable( GL_BLEND );
      glEnable( GL_TEXTURE_2D );
      glBindTexture( GL_TEXTURE_2D, g_resource->GetTexture( "Sprites/Citizen.bmp" ) );
      glDepthMask( false );

      int numSteps = sqrt(BLUEPRINTSTORE_NUMSEGMENTS);

      for( int x = 0; x < numSteps; ++x )
      {
          for( int y = 0; y < numSteps; ++y )
          {
              float tx = (float) x / (float) numSteps;
              float ty = (float) y / (float) numSteps;
              float size = 1.0f / (float) numSteps;

              XMFLOAT3 pos = screenPos + x * screenRight * screenSize
                                       + y * screenUp * screenSize;
              XMFLOAT3 width = screenRight * screenSize;
              XMFLOAT3 height = screenUp * screenSize;

              float infected = m_segments[y*numSteps+x] / 100.0f;
              glColor4f( infected*0.8f, 0.8f-infected*0.8f, 0.0f, 1.0f );

              glBegin( GL_QUADS );
                  glTexCoord2f(tx,ty);            glVertex3fv( pos.GetData() );
                  glTexCoord2f(tx+size,ty);       glVertex3fv( (pos+width).GetData() );
                  glTexCoord2f(tx+size,ty+size);  glVertex3fv( (pos+width+height).GetData() );
                  glTexCoord2f(tx,ty+size);       glVertex3fv( (pos+height).GetData() );
              glEnd();
          }
      }

      glDepthMask( true );
      glDisable( GL_TEXTURE_2D );
  }*/


  void BlueprintStore::RenderAlphas(float _predictionTime)
  {
    BlueprintBuilding::RenderAlphas(_predictionTime);

    DirectX::XMFLOAT3 screenPosStore, screenRightStore, screenUpStore;
    float screenSize;
    GetDisplay(screenPosStore, screenRightStore, screenUpStore, screenSize);

    // The four corners below are all screenPos plus these two edges, and both
    // edges carry the * 2 the legacy expressions applied at each corner.
    DirectX::XMVECTOR const screenPos = DirectX::XMLoadFloat3(&screenPosStore);
    DirectX::XMVECTOR const acrossQuad = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&screenRightStore), screenSize * 2);
    DirectX::XMVECTOR const upQuad = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&screenUpStore), screenSize * 2);

    //
    // Render main citizen

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Sprites/Citizen.bmp"));
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glDepthMask(false);

    float texX = 0.0f;
    float texY = 0.0f;
    float texH = 1.0f;
    float texW = 1.0f;

    glShadeModel(GL_SMOOTH);

    glBegin(GL_QUADS);
    float infected = m_segments[0] / 100.0f;
    glColor4f(infected * 0.8f, 0.8f - infected * 0.8f, 0.0f, 1.0f);
    glTexCoord2f(texX, texY);
    EmitVertex(screenPos);

    infected = m_segments[1] / 100.0f;
    glColor4f(infected * 0.8f, 0.8f - infected * 0.8f, 0.0f, 1.0f);
    glTexCoord2f(texX + texW, texY);
    EmitVertex(DirectX::XMVectorAdd(screenPos, acrossQuad));

    infected = m_segments[2] / 100.0f;
    glColor4f(infected * 0.8f, 0.8f - infected * 0.8f, 0.0f, 1.0f);
    glTexCoord2f(texX + texW, texY + texH);
    EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(screenPos, acrossQuad), upQuad));

    infected = m_segments[3] / 100.0f;
    glColor4f(infected * 0.8f, 0.8f - infected * 0.8f, 0.0f, 1.0f);
    glTexCoord2f(texX, texY + texH);
    EmitVertex(DirectX::XMVectorAdd(screenPos, upQuad));
    glEnd();

    //
    // Render lines for over effect

    glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/InterfaceGrey.bmp"));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    texX = 0.0f;
    texW = 3.0f;
    texY = g_gameTime * 0.01f;
    texH = 0.3f;

    for (int i = 0; i < 2; ++i)
    {
      glBegin(GL_QUADS);
      float infected = m_segments[0] / 100.0f;
      glColor4f(infected * 0.8f, 0.8f - infected * 0.8f, 0.0f, 1.0f);
      glTexCoord2f(texX, texY);
      EmitVertex(screenPos);

      infected = m_segments[1] / 100.0f;
      glColor4f(infected * 0.8f, 0.8f - infected * 0.8f, 0.0f, 1.0f);
      glTexCoord2f(texX + texW, texY);
      EmitVertex(DirectX::XMVectorAdd(screenPos, acrossQuad));

      infected = m_segments[2] / 100.0f;
      glColor4f(infected * 0.8f, 0.8f - infected * 0.8f, 0.0f, 1.0f);
      glTexCoord2f(texX + texW, texY + texH);
      EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(screenPos, acrossQuad), upQuad));

      infected = m_segments[3] / 100.0f;
      glColor4f(infected * 0.8f, 0.8f - infected * 0.8f, 0.0f, 1.0f);
      glTexCoord2f(texX, texY + texH);
      EmitVertex(DirectX::XMVectorAdd(screenPos, upQuad));
      glEnd();

      texY *= 1.5f;
      texH = 0.1f;
    }

    glShadeModel(GL_FLAT);

    glDepthMask(true);
    glDisable(GL_TEXTURE_2D);
  }


  // ============================================================================


  BlueprintConsole::BlueprintConsole()
    : BlueprintBuilding()
  {
    m_type = Building::TypeBlueprintConsole;

    SetShape(g_resource->GetShape("BlueprintConsole.shp"));
  }


  void BlueprintConsole::Initialise(Building* _template)
  {
    BlueprintBuilding::Initialise(_template);

    BlueprintConsole* console = (BlueprintConsole*)_template;
    m_segment = console->m_segment;
  }


  void BlueprintConsole::RecalculateOwnership()
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


  void BlueprintConsole::Read(TextReader* _in, bool _dynamic)
  {
    BlueprintBuilding::Read(_in, _dynamic);

    m_segment = atoi(_in->GetNextToken());
  }


  void BlueprintConsole::Write(FileWriter* _out)
  {
    BlueprintBuilding::Write(_out);

    _out->printf("{:<8d}", m_segment);
  }


  bool BlueprintConsole::Advance()
  {
    RecalculateOwnership();

    bool infected = (m_id.GetTeamId() == 1);
    bool clean = (m_id.GetTeamId() == 0);

    if (infected)
      SendBlueprint(m_segment, true);
    if (clean)
      SendBlueprint(m_segment, false);

    return BlueprintBuilding::Advance();
  }


  void BlueprintConsole::Render(float _predictionTime)
  {
    BlueprintBuilding::Render(_predictionTime);

    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));
    m_shape->Render(0.0f, mat);
  }


  void BlueprintConsole::RenderPorts()
  {
    for (int i = 0; i < GetNumPorts(); ++i)
    {
      DirectX::XMFLOAT3 portPos;
      DirectX::XMFLOAT3 portFront;
      GetPortPosition(i, portPos, portFront);

      DirectX::XMFLOAT4X4 mat;
      DirectX::XMStoreFloat4x4(&mat,
                               BasisFromFrontAndUp(DirectX::XMLoadFloat3(&portFront), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&portPos)));

      //
      // Render the status light

      float size = 6.0f;
      DirectX::XMFLOAT3 const cameraRight = g_camera->GetRight();
      DirectX::XMFLOAT3 const cameraUp = g_camera->GetUp();
      DirectX::XMVECTOR const camR = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&cameraRight), size);
      DirectX::XMVECTOR const camU = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&cameraUp), size);

      DirectX::XMFLOAT3 statusPos = s_controlPadStatus->GetWorldPosition(mat);
      statusPos.y = g_location->m_landscape.m_heightMap->GetValue(statusPos.x, statusPos.z);
      statusPos.y += 5.0f;
      DirectX::XMVECTOR const status = DirectX::XMLoadFloat3(&statusPos);

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

      glDisable(GL_CULL_FACE);
      glEnable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Starburst.bmp"));
      glDepthMask(false);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE);
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
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDisable(GL_BLEND);
      glDepthMask(true);
      glDisable(GL_TEXTURE_2D);
      glEnable(GL_CULL_FACE);
    }
  }


  // ============================================================================


  BlueprintRelay::BlueprintRelay()
    : BlueprintBuilding(),
      m_altitude(400.0f)
  {
    m_type = Building::TypeBlueprintRelay;

    SetShape(g_resource->GetShape("BlueprintRelay.shp"));
  }


  void BlueprintRelay::Initialise(Building* _template)
  {
    BlueprintBuilding::Initialise(_template);

    BlueprintRelay* blueprintRelay = (BlueprintRelay*)_template;
    m_altitude = blueprintRelay->m_altitude;

    m_pos.y = m_altitude;
    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::XMLoadFloat3(&m_up), DirectX::XMLoadFloat3(&m_pos)));
    m_centrePos = m_shape->CalculateCentre(mat);
  }


  void BlueprintRelay::SetDetail(int _detail)
  {
    m_pos.y = m_altitude;

    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::XMLoadFloat3(&m_up), DirectX::XMLoadFloat3(&m_pos)));
    m_centrePos = m_shape->CalculateCentre(mat);
    m_radius = m_shape->CalculateRadius(mat, m_centrePos);
  }


  bool BlueprintRelay::Advance()
  {
    float ourTime = g_gameTime + m_id.GetUniqueId() + m_id.GetIndex();

    DirectX::XMVECTOR const oldPos = DirectX::XMLoadFloat3(&m_pos);

    m_pos.x += sinf(ourTime / 1.5f) * 1.0f;
    m_pos.y += sinf(ourTime / 1.3f) * 1.0f;
    m_pos.z += cosf(ourTime / 1.7f) * 1.0f;

    DirectX::XMStoreFloat3(&m_vel,
                           DirectX::XMVectorScale(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_pos), oldPos), 1.0f / SERVER_ADVANCE_PERIOD));

    m_centrePos = m_pos;

    return BlueprintBuilding::Advance();
  }


  void BlueprintRelay::Render(float _predictionTime)
  {
    BlueprintBuilding::Render(_predictionTime);

    if (g_editing)
    {
      m_pos.y = m_altitude;
    }
  }


  void BlueprintRelay::Read(TextReader* _in, bool _dynamic)
  {
    BlueprintBuilding::Read(_in, _dynamic);

    m_altitude = atof(_in->GetNextToken());
  }


  void BlueprintRelay::Write(FileWriter* _out)
  {
    BlueprintBuilding::Write(_out);

    _out->printf("{:<2.2f}", m_altitude);
  }
} // namespace Species
