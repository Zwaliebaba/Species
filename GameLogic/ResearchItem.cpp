#include "pch.h"
#include "GlVertex.h"
#include "SoundSources.h"

#include "FileWriter.h"
#include "Resource.h"
#include "Shape.h"
#include "TextStreamReaders.h"
#include "TextRenderer.h"
#include "MathUtils.h"
#include "3dSprite.h"
#include "Preferences.h"

#include "ResearchItem.h"

#include "Explosion.h"
#include "GameTime.h"
#include "GlobalWorld.h"
#include "ProtocolLimits.h"
#include "Location.h"

#include "SoundSystem.h"
#include "WorldPointers.h"
#include "AppState.h"


namespace Species
{
  ResearchItem::ResearchItem()
    : Building(),
      m_researchType(-1),
      m_inLibrary(false),
      m_reprogrammed(100.0f),
      m_end1(nullptr),
      m_end2(nullptr),
      m_level(1)
  {
    m_type = TypeResearchItem;
    m_researchType = GlobalResearch::TypeEngineer;

    SetShape(g_resource->GetShape("ResearchItem.shp"));

    // SYNCHRONISED since determinism.yaml T5, for the same reason as Spam's
    // identical line. A level-file item has its facing overwritten by
    // Read/Initialise, but Library::Advance news one straight into
    // g_location->m_buildings without calling either, so the facing this draw
    // produces is what that world building keeps.
    DirectX::XMStoreFloat3(&m_front,
                           DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&m_front), DirectX::XMMatrixRotationY(syncfrand(2.0f * M_PI))));

    m_end1 = m_shape->m_rootFragment->LookupMarker("MarkerGrab1");
    m_end2 = m_shape->m_rootFragment->LookupMarker("MarkerGrab2");
  }


  void ResearchItem::Initialise(Building* _template)
  {
    Building::Initialise(_template);

    m_researchType = ((ResearchItem*)_template)->m_researchType;
    m_level = ((ResearchItem*)_template)->m_level;
  }


  void ResearchItem::SetDetail(int _detail)
  {
    m_pos.y = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);
    m_pos.y += 20.0f;

    DirectX::XMFLOAT4X4 mat = GetWorldMatrix();
    m_centrePos = m_shape->CalculateCentre(mat);
    m_radius = m_shape->CalculateRadius(mat, m_centrePos);
  }


  bool ResearchItem::Advance()
  {
    DirectX::XMVECTOR const vel = DirectX::XMLoadFloat3(&m_vel);
    if (DirectX::XMVectorGetX(DirectX::XMVector3Length(vel)) > 1.0f)
    {
      DirectX::XMStoreFloat3(&m_pos,
                             DirectX::XMVectorMultiplyAdd(vel, DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD), DirectX::XMLoadFloat3(&m_pos)));
      m_pos.y = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);
      DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(vel, 1.0f - SERVER_ADVANCE_PERIOD * 0.5f));

      // g_upVector, not m_up: this one path deliberately re-levels the item as it
      // settles, and BasisFromFrontAndUp is fed the world up rather than the
      // building's own. GetWorldMatrix would quietly change that.
      DirectX::XMFLOAT4X4 mat;
      DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));
      m_centrePos = m_shape->CalculateCentre(mat);
    }
    else
    {
      m_vel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
    }

    if (m_researchType > -1 && g_globalWorld->m_research->HasResearch(m_researchType) &&
        g_globalWorld->m_research->CurrentLevel(m_researchType) >= m_level)
    {
      return true;
    }


    if (m_reprogrammed <= 0.0f)
    {
      DirectX::XMFLOAT4X4 mat = GetWorldMatrix();
      g_explosionManager.AddExplosion(m_shape, mat, 1.0f);

      int existingLevel = g_globalWorld->m_research->CurrentLevel(m_researchType);

      g_globalWorld->m_research->AddResearch(m_researchType);
      g_globalWorld->m_research->m_researchLevel[m_researchType] = m_level;

      g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), "AquireResearch");

      if (existingLevel == 0)
      {
        g_taskManagerInterface->SetCurrentMessage(TaskManagerInterfaceAccess::MessageResearch, m_researchType, 4.0f);
      }
      else
      {
        g_taskManagerInterface->SetCurrentMessage(TaskManagerInterfaceAccess::MessageResearchUpgrade, m_researchType, 4.0f);
      }

      return true;
    }

    return false;
  }


  bool ResearchItem::NeedsReprogram() { return (m_reprogrammed > 0.0f); }


  bool ResearchItem::Reprogram()
  {
    m_reprogrammed -= SERVER_ADVANCE_PERIOD * 3.0f;

    return (m_reprogrammed <= 0.0f);
  }


  void ResearchItem::GetEndPositions(DirectX::XMFLOAT3& _end1, DirectX::XMFLOAT3& _end2)
  {
    DirectX::XMFLOAT4X4 mat = GetWorldMatrix();

    _end1 = m_end1->GetWorldPosition(mat);
    _end2 = m_end2->GetWorldPosition(mat);
  }


  void ResearchItem::Render(float _predictionTime)
  {
    if (m_reprogrammed <= 0.0f)
      return;

    DirectX::XMVECTOR rotateAround = DirectX::g_XMIdentityR1;
    rotateAround = DirectX::XMVector3Transform(rotateAround, DirectX::XMMatrixRotationX(g_gameTime * 1.0f));
    rotateAround = DirectX::XMVector3Transform(rotateAround, DirectX::XMMatrixRotationZ(g_gameTime * 0.7f));
    rotateAround = DirectX::XMVector3Normalize(rotateAround);

    // RotateAround's angle is the vector's magnitude, with the same 1e-8 guard.
    // rotateAround is unit length above, so the angle here is just g_advanceTime,
    // but the guard is kept rather than assumed away -- g_advanceTime is zero
    // while the game is paused, and the legacy code returned untouched then.
    DirectX::XMVECTOR const rotation = DirectX::XMVectorScale(rotateAround, g_advanceTime);
    float const rotationLengthSquared = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(rotation));
    if (rotationLengthSquared >= 1e-8f)
    {
      float const angle = sqrtf(rotationLengthSquared);
      DirectX::XMMATRIX const spin = DirectX::XMMatrixRotationAxis(DirectX::XMVectorScale(rotation, 1.0f / angle), angle);
      DirectX::XMStoreFloat3(&m_front, DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&m_front), spin));
      DirectX::XMStoreFloat3(&m_up, DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&m_up), spin));
    }

    DirectX::XMVECTOR const predicted =
      DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(_predictionTime), DirectX::XMLoadFloat3(&m_pos));
    DirectX::XMFLOAT3 predictedPos;
    DirectX::XMStoreFloat3(&predictedPos, predicted);

    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::XMLoadFloat3(&m_up), predicted));

    m_shape->Render(0.0f, mat);

    if (g_editing && m_researchType != -1)
    {
      g_gameFont.DrawText3DCentre(DirectX::XMFLOAT3(predictedPos.x, predictedPos.y + 25.0f, predictedPos.z), 5,
                                  GlobalResearch::GetTypeName(m_researchType));
      g_gameFont.DrawText3DCentre(DirectX::XMFLOAT3(predictedPos.x, predictedPos.y + 20.0f, predictedPos.z), 5, "{:2.2f}", m_reprogrammed);
    }
  }


  void ResearchItem::RenderAlphas(float _predictionTime)
  {
    Building::RenderAlphas(_predictionTime);

    // Hoisted out of the sixteen vertices below rather than fetched at each.
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

    float alpha = 1.0f;

    for (int i = 0; i < maxBlobs; ++i)
    {
      DirectX::XMVECTOR const pos = DirectX::XMVectorAdd(
        DirectX::XMLoadFloat3(&m_centrePos),
        DirectX::XMVectorSet(sinf(timeIndex + i) * i * 0.3f, cosf(timeIndex + i) * sinf(i * 10) * 5, cosf(timeIndex + i) * i * 0.3f, 0.0f));

      float size = 5.0f + sinf(timeIndex + i * 10) * 7.0f;
      size = std::max(size, 2.0f);

      DirectX::XMVECTOR const right = DirectX::XMVectorScale(camRight, size);
      DirectX::XMVECTOR const up = DirectX::XMVectorScale(camUp, size);

      // glColor4f( 0.6f, 0.2f, 0.1f, alpha);
      glColor4f(0.1f, 0.2f, 0.8f, alpha);

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
    // Starbursts

    alpha = 1.0f - m_reprogrammed / 100.0f;
    alpha *= 0.3f;

    glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Starburst.bmp"));

    if (alpha > 0.0f)
    {
      int numStars = 10;
      if (buildingDetail == 2)
        numStars = 5;
      if (buildingDetail == 3)
        numStars = 2;

      for (int i = 0; i < numStars; ++i)
      {
        DirectX::XMVECTOR const pos = DirectX::XMVectorAdd(
          DirectX::XMLoadFloat3(&m_centrePos),
          DirectX::XMVectorSet(sinf(timeIndex + i) * i * 0.3f, cosf(timeIndex + i) * cosf(i * 10) * 2, cosf(timeIndex + i) * i * 0.3f, 0.0f));

        float size = i * 10 * alpha;
        if (i > numStars - 2)
          size = i * 20 * alpha;

        DirectX::XMVECTOR const right = DirectX::XMVectorScale(camRight, size);
        DirectX::XMVECTOR const up = DirectX::XMVectorScale(camUp, size);

        // glColor4f( 1.0f, 0.4f, 0.2f, alpha );
        glColor4f(0.1f, 0.2f, 0.8f, alpha);

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


    //
    // Draw control line to heaven

    alpha = 1.0f - m_reprogrammed / 100.0f;
    alpha *= (0.5f + fabs(cosf(g_gameTime)) * 0.5f);

    if (alpha > 0.0f)
    {
      glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Laser.bmp"));
      glDisable(GL_CULL_FACE);
      glShadeModel(GL_SMOOTH);

      float w = 10.0f * alpha;

      glBegin(GL_QUADS);
      glColor4f(0.1f, 0.2f, 0.8f, alpha);
      {
        DirectX::XMVECTOR const bottom = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMVectorSet(0.0f, -50.0f, 0.0f, 0.0f));
        DirectX::XMVECTOR const top = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMVectorSet(0.0f, 1000.0f, 0.0f, 0.0f));
        DirectX::XMVECTOR const halfWidth = DirectX::XMVectorScale(camRight, w);

        glTexCoord2i(0, 0);
        EmitVertex(DirectX::XMVectorSubtract(bottom, halfWidth));
        glTexCoord2i(0, 1);
        EmitVertex(DirectX::XMVectorAdd(bottom, halfWidth));

        glColor4f(0.1f, 0.2f, 0.8f, 0.0f);
        glTexCoord2i(1, 1);
        EmitVertex(DirectX::XMVectorAdd(top, halfWidth));
        glTexCoord2i(1, 0);
        EmitVertex(DirectX::XMVectorSubtract(top, halfWidth));
      }
      glEnd();

      w *= 0.3f;

      glBegin(GL_QUADS);
      glColor4f(0.1f, 0.2f, 0.8f, alpha);
      {
        DirectX::XMVECTOR const bottom = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMVectorSet(0.0f, -50.0f, 0.0f, 0.0f));
        DirectX::XMVECTOR const top = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMVectorSet(0.0f, 1000.0f, 0.0f, 0.0f));
        DirectX::XMVECTOR const halfWidth = DirectX::XMVectorScale(camRight, w);

        glTexCoord2i(0, 0);
        EmitVertex(DirectX::XMVectorSubtract(bottom, halfWidth));
        glTexCoord2i(0, 1);
        EmitVertex(DirectX::XMVectorAdd(bottom, halfWidth));

        glColor4f(0.1f, 0.2f, 0.8f, 0.0f);
        glTexCoord2i(1, 1);
        EmitVertex(DirectX::XMVectorAdd(top, halfWidth));
        glTexCoord2i(1, 0);
        EmitVertex(DirectX::XMVectorSubtract(top, halfWidth));
      }
      glEnd();
    }


    glShadeModel(GL_FLAT);
    glDisable(GL_TEXTURE_2D);
    glDepthMask(true);
  }


  bool ResearchItem::RenderPixelEffect(float _predictionTime)
  {
    //	Matrix34 mat(m_front, m_up, m_pos);
    //	m_shape->Render(0.0f, mat);
    //	g_renderer->MarkUsedCells(m_shape, mat);
    return false;
  }


  void ResearchItem::Read(TextReader* _in, bool _dynamic)
  {
    Building::Read(_in, _dynamic);

    m_researchType = GlobalResearch::GetType(_in->GetNextToken());

    if (_in->TokenAvailable())
    {
      m_level = atoi(_in->GetNextToken());
    }
  }


  void ResearchItem::Write(FileWriter* _out)
  {
    Building::Write(_out);

    _out->printf("%s ", GlobalResearch::GetTypeName(m_researchType));
    _out->printf("%6d", m_level);
  }


  void ResearchItem::ListSoundEvents(std::vector<const char*>* _list)
  {
    Building::ListSoundEvents(_list);

    _list->push_back("AquireResearch");
  }


  bool ResearchItem::DoesSphereHit(DirectX::XMFLOAT3 const& _pos, float _radius) { return false; }


  bool ResearchItem::DoesShapeHit(Shape* _shape, DirectX::XMFLOAT4X4 _transform) { return false; }


  bool ResearchItem::DoesRayHit(DirectX::XMFLOAT3 const& _rayStart, DirectX::XMFLOAT3 const& _rayDir, float _rayLen, DirectX::XMFLOAT3* _pos,
                                DirectX::XMFLOAT3* norm)
  {
    return RaySphereIntersection(_rayStart, _rayDir, m_pos, m_radius, _rayLen);
  }


  bool ResearchItem::IsInView()
  {
    if (Building::IsInView())
      return true;

    DirectX::XMFLOAT3 const eyeLevelPos(m_pos.x, m_pos.y + g_camera->GetPos().y, m_pos.z);
    if (g_camera->PosInViewFrustum(eyeLevelPos))
    {
      return true;
    }

    return false;
  }
} // namespace Species
