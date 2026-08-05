#include "pch.h"
#include "GlVertex.h"
#include "DebugRender.h"
#include "MathUtils.h"
#include "Profiler.h"
#include "Resource.h"
#include "Shape.h"
#include "TextRenderer.h"
#include "3dSprite.h"
#include "Preferences.h"

#include "ConstructionYard.h"
#include "ResearchItem.h"
#include "Armour.h"

#include "GameTime.h"
#include "GlobalWorld.h"
#include "Location.h"
#include "Team.h"
#include "WorldPointers.h"
#include "AppState.h"


namespace Species
{
  ConstructionYard::ConstructionYard()
    : Building(),
      m_numPrimitives(0),
      m_numSurges(0),
      m_timer(-1.0f),
      m_fractionPopulated(0.0f),
      m_numTanksProduced(0),
      m_alpha(0.0f)
  {
    m_type = TypeYard;
    SetShape(g_resource->GetShape("ConstructionYard.shp"));

    m_rung = g_resource->GetShape("ConstructionYardRung.shp");
    m_primitive = g_resource->GetShape("MinePrimitive1.shp");

    for (int i = 0; i < YARD_NUMPRIMITIVES; ++i)
    {
      const std::string name = std::format("MarkerPrimitive0{}", i + 1);
      m_primitives[i] = m_shape->m_rootFragment->LookupMarker(name.c_str());
      DEBUG_ASSERT(m_primitives[i]);
    }

    for (int i = 0; i < YARD_NUMRUNGSPIKES; ++i)
    {
      const std::string name = std::format("MarkerSpike0{}", i + 1);
      m_rungSpikes[i] = m_rung->m_rootFragment->LookupMarker(name.c_str());
      DEBUG_ASSERT(m_rungSpikes[i]);
    }
  }


  bool ConstructionYard::Advance()
  {
    m_fractionPopulated = (float)GetNumPortsOccupied() / (float)GetNumPorts();

    if (m_fractionPopulated > 0.0f && m_numSurges > 0 && m_numPrimitives > 0)
    {
      GlobalBuilding* gb = g_globalWorld->GetBuilding(m_id.GetUniqueId(), g_locationId);
      if (gb && !gb->m_online)
      {
        gb->m_online = true;
        g_globalWorld->EvaluateEvents();
      }
    }


    if (m_timer < 0.0f)
    {
      // Not currently building anything
      if (m_numPrimitives == YARD_NUMPRIMITIVES && m_numSurges > 50)
      {
        m_timer = 30.0f;
      }
    }
    else
    {
      // Building a tank
      m_timer -= m_fractionPopulated * SERVER_ADVANCE_PERIOD;
      if (m_timer <= 0.0f)
      {
        if (IsPopulationLocked())
        {
          m_timer = 30.0f;
        }
        else
        {
          m_numPrimitives = 0;
          m_numSurges = 1;

          DirectX::XMFLOAT4X4 mat;
          DirectX::XMStoreFloat4x4(&mat,
                                   BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

          DirectX::XMFLOAT3 const primPos = m_primitives[5]->GetWorldPosition(mat);
          WorldObjectId objId = g_location->SpawnEntities(primPos, 2, -1, Entity::TypeArmour, 1, g_zeroVector, 0.0f);
          Entity* entity = g_location->GetEntity(objId);
          Armour* armour = (Armour*)entity;
          armour->m_front = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
          armour->m_vel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

          armour->SetWayPoint(DirectX::XMFLOAT3(m_pos.x, m_pos.y, m_pos.z + 500.0f));

          ++m_numTanksProduced;
          m_timer = -1.0f;
        }
      }
    }

    return Building::Advance();
  }


  DirectX::XMFLOAT4X4 ConstructionYard::GetRungMatrix1()
  {
    DirectX::XMFLOAT4X4 mat;

    if (m_numSurges > 0)
    {
      float rungHeight = 55.0f + sinf(g_gameTime) * 10.0f * m_fractionPopulated;
      DirectX::XMVECTOR const rungPos = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMVectorSet(0.0f, rungHeight, 0.0f, 0.0f));
      DirectX::XMVECTOR const front = DirectX::XMVector3TransformNormal(
        DirectX::XMLoadFloat3(&m_front), DirectX::XMMatrixRotationY(cosf(g_gameTime * 0.5f) * 0.5f * m_fractionPopulated));

      DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(front, DirectX::g_XMIdentityR1, rungPos));
    }
    else
    {
      DirectX::XMVECTOR const rungPos = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMVectorSet(0.0f, 45.0f, 0.0f, 0.0f));
      DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, rungPos));
    }

    return mat;
  }


  DirectX::XMFLOAT4X4 ConstructionYard::GetRungMatrix2()
  {
    DirectX::XMFLOAT4X4 mat;

    if (m_numSurges > 0)
    {
      float rungHeight = 110.0f + sinf(g_gameTime * 0.8) * 15.0f * m_fractionPopulated;
      DirectX::XMVECTOR const rungPos = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMVectorSet(0.0f, rungHeight, 0.0f, 0.0f));
      DirectX::XMVECTOR const front = DirectX::XMVector3TransformNormal(
        DirectX::XMLoadFloat3(&m_front), DirectX::XMMatrixRotationY(cosf(g_gameTime * 0.4f) * 0.6f * m_fractionPopulated));

      DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(front, DirectX::g_XMIdentityR1, rungPos));
    }
    else
    {
      DirectX::XMVECTOR const rungPos = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMVectorSet(0.0f, 75.0f, 0.0f, 0.0f));
      DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, rungPos));
    }

    return mat;
  }


  void ConstructionYard::Render(float _predictionTime)
  {
    Building::Render(_predictionTime);

    // Rung 1
    DirectX::XMFLOAT4X4 mat = GetRungMatrix1();
    m_rung->Render(_predictionTime, mat);


    // Rung 2
    mat = GetRungMatrix2();
    m_rung->Render(_predictionTime, mat);


    //
    // Primitives

    for (int i = 0; i < m_numPrimitives; ++i)
    {
      DirectX::XMFLOAT4X4 mat;
      DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

      DirectX::XMFLOAT4X4 prim = m_primitives[i]->GetWorldMatrix(mat);
      prim._42 += sinf(g_gameTime + i) * 5.0f;

      m_primitive->Render(_predictionTime, prim);
    }
  }


  void ConstructionYard::RenderAlphas(float _predictionTime)
  {
    Building::RenderAlphas(_predictionTime);

    DirectX::XMFLOAT3 const camUpStore = g_camera->GetUp();
    DirectX::XMFLOAT3 const camRightStore = g_camera->GetRight();
    DirectX::XMVECTOR const camUp = DirectX::XMLoadFloat3(&camUpStore);
    DirectX::XMVECTOR const camRight = DirectX::XMLoadFloat3(&camRightStore);

    glDepthMask(false);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/CloudyGlow.bmp"));

    float timeIndex = g_gameTime * 2;

    int buildingDetail = g_prefsManager->GetInt("RenderBuildingDetail", 1);
    int maxBlobs = 50;
    if (buildingDetail == 2)
      maxBlobs = 25;
    if (buildingDetail == 3)
      maxBlobs = 10;


    //
    // Calculate alpha value

    float targetAlpha = 0.0f;
    if (m_timer > 0.0f)
    {
      targetAlpha = 1.0f - (m_timer / 60.0f);
    }
    else if (m_numPrimitives > 0)
    {
      targetAlpha = (m_numPrimitives / 9.0f) * 0.5f;
    }
    if (m_numSurges > 0)
    {
      targetAlpha = std::max(targetAlpha, 0.2f);
    }

    float factor1 = g_advanceTime;
    float factor2 = 1.0f - factor1;
    m_alpha = m_alpha * factor2 + targetAlpha * factor1;


    //
    // Central glow effect

    for (int i = 0; i < maxBlobs; ++i)
    {
      DirectX::XMVECTOR const pos = DirectX::XMVectorAdd(
        DirectX::XMLoadFloat3(&m_centrePos),
        DirectX::XMVectorSet(sinf(timeIndex + i) * i * 1.7f, fabs(cosf(timeIndex + i) * cosf(i * 20) * 64), cosf(timeIndex + i) * i * 1.7f, 0.0f));

      float size = 30.0f * sinf(timeIndex + i * 13);
      size = std::max(size, 5.0f);

      DirectX::XMVECTOR const right = DirectX::XMVectorScale(camRight, size);
      DirectX::XMVECTOR const up = DirectX::XMVectorScale(camUp, size);

      glColor4f(0.6f, 0.2f, 0.1f, m_alpha);
      // glColor4f( 0.5f, 0.6f, 0.8f, m_alpha );


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


    //
    // Central starbursts

    glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Starburst.bmp"));

    int numStars = 10;
    if (buildingDetail == 2)
      numStars = 5;
    if (buildingDetail == 3)
      numStars = 2;

    for (int i = 0; i < numStars; ++i)
    {
      DirectX::XMVECTOR const pos = DirectX::XMVectorAdd(
        DirectX::XMLoadFloat3(&m_centrePos),
        DirectX::XMVectorSet(sinf(timeIndex + i) * i * 1.7f, fabs(cosf(timeIndex + i) * cosf(i * 20) * 64), cosf(timeIndex + i) * i * 1.7f, 0.0f));

      float size = i * 30.0f;

      DirectX::XMVECTOR const right = DirectX::XMVectorScale(camRight, size);
      DirectX::XMVECTOR const up = DirectX::XMVectorScale(camUp, size);

      glColor4f(1.0f, 0.4f, 0.2f, m_alpha);
      // glColor4f( 0.4f, 0.5f, 1.0f, m_alpha );

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


    //
    // Starbursts on rungs

    if (m_timer > 0.0f)
    {
      glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Starburst.bmp"));

      for (int r = 0; r < 2; ++r)
      {
        DirectX::XMFLOAT4X4 rungMat;
        if (r == 0)
          rungMat = GetRungMatrix1();
        else
          rungMat = GetRungMatrix2();

        for (int i = 0; i < YARD_NUMRUNGSPIKES; ++i)
        {
          DirectX::XMFLOAT3 const spikePos = m_rungSpikes[i]->GetWorldPosition(rungMat);
          DirectX::XMVECTOR const pos = DirectX::XMLoadFloat3(&spikePos);

          for (int j = 0; j < numStars; ++j)
          {
            float size = sinf(timeIndex + r + i) * j * 5.0f;

            DirectX::XMVECTOR const right = DirectX::XMVectorScale(camRight, size);
            DirectX::XMVECTOR const up = DirectX::XMVectorScale(camUp, size);

            glColor4f(0.6f, 0.2f, 0.1f, m_alpha);
            // glColor4f( 0.4f, 0.5f, 0.9f, m_alpha );

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
        }
      }
    }

    glDisable(GL_TEXTURE_2D);
    glDepthMask(true);


    //    glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
    //    g_gameFont.DrawText3DCentre( m_pos+Vector3(0,300,0), 20.0f, "Surges : {}", m_numSurges );
    //    g_gameFont.DrawText3DCentre( m_pos+Vector3(0,270,0), 20.0f, "Primitives : {}", m_numPrimitives );
    //    g_gameFont.DrawText3DCentre( m_pos+Vector3(0,240,0), 20.0f, "Timer : {:2.2f}", m_timer );
    //    g_gameFont.DrawText3DCentre( m_pos+Vector3(0,210,0), 20.0f, "Armour : {}", m_numTanksProduced );
  }


  bool ConstructionYard::IsPopulationLocked()
  {
    Team* team = g_location->GetMyTeam();

    int numArmour = 0;
    for (int i = 0; i < static_cast<int>(team->m_specials.size()); ++i)
    {
      WorldObjectId id = team->m_specials[i];
      Entity* entity = g_location->GetEntity(id);
      if (entity && entity->m_type == Entity::TypeArmour)
      {
        ++numArmour;
      }
    }

    return (numArmour >= 5);
  }


  bool ConstructionYard::AddPrimitive()
  {
    if (m_numPrimitives < YARD_NUMPRIMITIVES)
    {
      ++m_numPrimitives;
      return true;
    }

    return false;
  }


  void ConstructionYard::AddPowerSurge() { ++m_numSurges; }


  // ============================================================================


  DisplayScreen::DisplayScreen()
    : Building()
  {
    m_type = TypeDisplayScreen;
    SetShape(g_resource->GetShape("DisplayScreen.shp"));

    m_armour = g_resource->GetShape("Armour.shp");

    for (int i = 0; i < DISPLAYSCREEN_NUMRAYS; ++i)
    {
      const std::string name = std::format("MarkerRay0{}", i + 1);
      m_rays[i] = m_shape->m_rootFragment->LookupMarker(name.c_str());
    }
  }


  void DisplayScreen::RenderAlphas(float _predictionTime)
  {
    Building::RenderAlphas(_predictionTime);

    DirectX::XMVECTOR const armourPos = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_centrePos), DirectX::XMVectorSet(0.0f, 75.0f, 0.0f, 0.0f));
    DirectX::XMVECTOR const armourFront = DirectX::XMVector3TransformNormal(DirectX::g_XMIdentityR2, DirectX::XMMatrixRotationY(g_gameTime * -0.75f));

    // Only the three basis rows are scaled; the position row is left alone,
    // exactly as the legacy f/u/r assignments did.
    DirectX::XMMATRIX armour = BasisFromFrontAndUp(armourFront, DirectX::g_XMIdentityR1, armourPos);
    DirectX::XMVECTOR const scale = DirectX::XMVectorReplicate(3.0f);
    armour.r[0] = DirectX::XMVectorMultiply(armour.r[0], scale);
    armour.r[1] = DirectX::XMVectorMultiply(armour.r[1], scale);
    armour.r[2] = DirectX::XMVectorMultiply(armour.r[2], scale);

    DirectX::XMFLOAT4X4 armourMat;
    DirectX::XMStoreFloat4x4(&armourMat, armour);

    DirectX::XMVECTOR const targetPos = DirectX::XMVectorAdd(armour.r[3], DirectX::XMVectorSet(0.0f, 50.0f, 0.0f, 0.0f));

    glEnable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDepthMask(false);

    //
    // Render black blob

    DirectX::XMFLOAT3 const camRightStore = g_camera->GetRight();
    DirectX::XMFLOAT3 const camUpStore = g_camera->GetUp();
    DirectX::XMVECTOR const camRight = DirectX::XMLoadFloat3(&camRightStore);
    DirectX::XMVECTOR const camUp = DirectX::XMLoadFloat3(&camUpStore);
    float size = 70.0f;
    glColor4f(0.4f, 0.3f, 0.4f, 0.0f);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Glow.bmp"));

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_COLOR);

    // glBegin( GL_QUADS );
    DirectX::XMVECTOR const blobRight = DirectX::XMVectorScale(camRight, size);
    DirectX::XMVECTOR const blobUp = DirectX::XMVectorScale(camUp, size);

    glTexCoord2i(0, 0);
    EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorSubtract(targetPos, blobRight), blobUp));
    glTexCoord2i(1, 0);
    EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorAdd(targetPos, blobRight), blobUp));
    glTexCoord2i(1, 1);
    EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(targetPos, blobRight), blobUp));
    glTexCoord2i(0, 1);
    EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorSubtract(targetPos, blobRight), blobUp));
    glEnd();

    glDisable(GL_TEXTURE_2D);


    //
    // Render rays

    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glShadeModel(GL_SMOOTH);

    for (int i = 0; i < DISPLAYSCREEN_NUMRAYS; ++i)
    {
      DirectX::XMFLOAT4X4 buildingMat = GetWorldMatrix();

      DirectX::XMFLOAT3 const rayPosStore = m_rays[i]->GetWorldPosition(buildingMat);
      DirectX::XMVECTOR const rayPos = DirectX::XMLoadFloat3(&rayPosStore);

      DirectX::XMFLOAT3 const cameraPos = g_camera->GetPos();
      DirectX::XMVECTOR const rayToArmour = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(rayPos, targetPos));
      DirectX::XMVECTOR const right =
        DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&cameraPos), rayPos), rayToArmour));

      glBegin(GL_QUADS);
      glColor4f(0.9f, 0.9f, 0.9f, 0.5f);
      EmitVertex(DirectX::XMVectorSubtract(rayPos, right));
      EmitVertex(DirectX::XMVectorAdd(rayPos, right));

      glColor4f(0.9f, 0.9f, 0.9f, 0.0f);
      EmitVertex(DirectX::XMVectorMultiplyAdd(right, DirectX::XMVectorReplicate(30.0f), targetPos));
      EmitVertex(DirectX::XMVectorNegativeMultiplySubtract(right, DirectX::XMVectorReplicate(30.0f), targetPos));
      glEnd();
    }

    glShadeModel(GL_FLAT);


    //
    // Render armour

    glEnable(GL_NORMALIZE);

    glBlendFunc(GL_ZERO, GL_SRC_COLOR);
    m_armour->Render(_predictionTime, armourMat);

    // g_renderer->SetObjectLighting();
    // glBlendFunc( GL_SRC_ALPHA, GL_ONE );
    // m_armour->Render( _predictionTime, armourMat );

    g_renderer->UnsetObjectLighting();

    glDisable(GL_NORMALIZE);
  }
} // namespace Species
