#include "pch.h"
#include "GlVertex.h"

#include <math.h>

#include "DebugRender.h"
#include "MathUtils.h"
#include "OglExtensions.h"
#include "Profiler.h"
#include "Resource.h"
#include "Shape.h"
#include "FileWriter.h"
#include "TextStreamReaders.h"

#include "Location.h"
#include "GameTime.h"

#include "FeedingTube.h"
#include "WorldPointers.h"
#include "AppState.h"


namespace Species
{
  FeedingTube::FeedingTube()
    : m_receiverId(-1),
      m_range(0.0f),
      m_signal(0.0f)
  {
    m_type = Building::TypeFeedingTube;
    // m_front.Set(0,0,1);

    SetShape(g_resource->GetShape("FeedingTube.shp"));
    m_focusMarker = m_shape->m_rootFragment->LookupMarker("MarkerFocus");
  }

  // *** Initialise
  void FeedingTube::Initialise(Building* _template)
  {
    Building::Initialise(_template);
    DEBUG_ASSERT(_template->m_type == Building::TypeFeedingTube);
    m_receiverId = ((FeedingTube*)_template)->m_receiverId;
  }

  bool FeedingTube::Advance()
  {
    DirectX::XMFLOAT4X4 rootMat;
    DirectX::XMStoreFloat4x4(&rootMat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

    DirectX::XMFLOAT3 const dishPos = m_focusMarker->GetWorldPosition(rootMat);

    FeedingTube* ft = (FeedingTube*)g_location->GetBuilding(m_receiverId);
    if (ft && ft->m_type == Building::TypeFeedingTube)
    {
      DirectX::XMFLOAT3 const theirDishPos = ft->GetDishPos(0.0f);
      m_range = DirectX::XMVectorGetX(
        DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&theirDishPos), DirectX::XMLoadFloat3(&dishPos))));
    }
    else
    {
      m_range = 0.0f;
    }

    return Building::Advance();
  }


  DirectX::XMFLOAT3 FeedingTube::GetDishPos(float _predictionTime)
  {
    DirectX::XMFLOAT4X4 rootMat;
    DirectX::XMStoreFloat4x4(&rootMat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

    DirectX::XMFLOAT4X4 const worldMat = m_focusMarker->GetWorldMatrix(rootMat);
    return DirectX::XMFLOAT3(worldMat._41, worldMat._42, worldMat._43);
  }


  DirectX::XMFLOAT3 FeedingTube::GetDishFront(float _predictionTime)
  {
    if (m_receiverId != -1)
    {
      FeedingTube* receiver = (FeedingTube*)g_location->GetBuilding(m_receiverId);
      if (receiver)
      {
        DirectX::XMFLOAT3 const ourDishPos = GetDishPos(_predictionTime);
        DirectX::XMFLOAT3 const receiverDishPos = receiver->GetDishPos(_predictionTime);

        DirectX::XMFLOAT3 result;
        DirectX::XMStoreFloat3(&result, DirectX::XMVector3Normalize(
                                          DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&receiverDishPos), DirectX::XMLoadFloat3(&ourDishPos))));
        return result;
      }
    }

    DirectX::XMFLOAT4X4 rootMat;
    DirectX::XMStoreFloat4x4(&rootMat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

    // Matrix34's `.f` was the FRONT basis vector. XMFLOAT4X4 numbers its rows
    // rather than naming them, and front is row 2 -- _31.._33, not _21 or _31.
    DirectX::XMFLOAT4X4 const worldMat = m_focusMarker->GetWorldMatrix(rootMat);
    return DirectX::XMFLOAT3(worldMat._31, worldMat._32, worldMat._33);
  }

  DirectX::XMFLOAT3 FeedingTube::GetForwardsClippingDir(float _predictionTime, FeedingTube* _sender)
  {
    if (_sender == nullptr)
    {
      return GetDishFront(_predictionTime);
    }

    DirectX::XMFLOAT3 const senderDishFrontStore = _sender->GetDishFront(_predictionTime);
    DirectX::XMFLOAT3 const dishFrontStore = GetDishFront(_predictionTime);
    DirectX::XMVECTOR senderDishFront = DirectX::XMLoadFloat3(&senderDishFrontStore);
    DirectX::XMVECTOR dishFront = DirectX::XMLoadFloat3(&dishFrontStore);

    // Make the two dishFronts point at each other.

    DirectX::XMFLOAT3 const ourPos = GetDishPos(_predictionTime);
    DirectX::XMFLOAT3 const theirPos = _sender->GetDishPos(_predictionTime);
    DirectX::XMVECTOR const SR =
      DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&ourPos), DirectX::XMLoadFloat3(&theirPos)));

    if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(SR, senderDishFront)) < 0)
      senderDishFront = DirectX::XMVectorNegate(senderDishFront);

    if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(SR, dishFront)) > 0)
      dishFront = DirectX::XMVectorNegate(dishFront);

    DirectX::XMVECTOR const combinedDirection = DirectX::XMVectorSubtract(dishFront, senderDishFront);

    DirectX::XMFLOAT3 result;
    if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(combinedDirection)) < 0.5f)
    {
      DirectX::XMStoreFloat3(&result, DirectX::XMVectorNegate(dishFront));
      return result;
    }

    DirectX::XMStoreFloat3(&result, DirectX::XMVector3Normalize(combinedDirection));
    return result;
  }

  void FeedingTube::Render(float _predictionTime) { Building::Render(_predictionTime); }


  void FeedingTube::RenderAlphas(float _predictionTime)
  {
    if (g_editing)
      return;

    if (m_receiverId != -1)
    {
      RenderSignal(_predictionTime, 10.0f, 0.4f);
      RenderSignal(_predictionTime, 9.0f, 0.2f);
      RenderSignal(_predictionTime, 8.0f, 0.2f);
      RenderSignal(_predictionTime, 4.0f, 0.5f);
    }
  }


  void FeedingTube::RenderSignal(float _predictionTime, float _radius, float _alpha)
  {
    START_PROFILE(g_profiler, "Signal");

    FeedingTube* receiver = (FeedingTube*)g_location->GetBuilding(m_receiverId);
    if (!receiver)
      return;

    DirectX::XMFLOAT3 const startPos = GetStartPoint();
    DirectX::XMFLOAT3 const endPos = GetEndPoint();
    DirectX::XMVECTOR const delta = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&endPos), DirectX::XMLoadFloat3(&startPos));
    DirectX::XMVECTOR const deltaNorm = DirectX::XMVector3Normalize(delta);

    float distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(delta));
    float numRadii = 20.0f;
    int numSteps = int(distance / 200.0f);
    numSteps = std::max(1, numSteps);

    glEnable(GL_TEXTURE_2D);

    gglActiveTextureARB(GL_TEXTURE0_ARB);
    glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/LaserFence.bmp", true, true));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
    glTexEnvf(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_REPLACE);
    glEnable(GL_TEXTURE_2D);

    gglActiveTextureARB(GL_TEXTURE1_ARB);
    glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/RadarSignal.bmp", true, true));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
    glTexEnvf(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE);
    glEnable(GL_TEXTURE_2D);

    glDisable(GL_CULL_FACE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glEnable(GL_BLEND);
    glDepthMask(false);
    glColor4f(1.0f, 1.0f, 1.0f, _alpha);

    glMatrixMode(GL_MODELVIEW);
    glTranslatef(startPos.x, startPos.y, startPos.z);
    DirectX::XMFLOAT3 const dishFront = GetForwardsClippingDir(_predictionTime, receiver);
    double eqn1[4] = {dishFront.x, dishFront.y, dishFront.z, -1.0f};
    glClipPlane(GL_CLIP_PLANE0, eqn1);


    DirectX::XMFLOAT3 const receiverPos = receiver->GetDishPos(_predictionTime);
    DirectX::XMFLOAT3 const receiverFront = receiver->GetForwardsClippingDir(_predictionTime, this);
    glTranslatef(-startPos.x, -startPos.y, -startPos.z);
    glTranslatef(receiverPos.x, receiverPos.y, receiverPos.z);

    DirectX::XMVECTOR const diff = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&receiverPos), DirectX::XMLoadFloat3(&startPos));
    float thisDistance = -DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMLoadFloat3(&receiverFront), diff));

    thisDistance = -1.0f;

    double eqn2[4] = {receiverFront.x, receiverFront.y, receiverFront.z, thisDistance};
    glClipPlane(GL_CLIP_PLANE1, eqn2);
    glTranslatef(-receiverPos.x, -receiverPos.y, -receiverPos.z);


    // RenderArrow(startPos, startPos + dishFront * 100, 2.0f, RGBAColour( 0, 255, 0, 255 ) );
    // RenderArrow(endPos, endPos + receiverFront * 100, 2.0f, RGBAColour( 255, 0, 0, 255 ) );
    // RenderArrow(startPos, endPos, 2.0f );

    glTranslatef(startPos.x, startPos.y, startPos.z);

    glEnable(GL_CLIP_PLANE0);
    glEnable(GL_CLIP_PLANE1);

    float texXInner = -g_gameTime / _radius;
    float texXOuter = -g_gameTime;

    // float texXInner = -1.0f/_radius;
    // float texXOuter = -1.0f;
    if (true)
    {
      glBegin(GL_QUAD_STRIP);

      for (int s = 0; s < numSteps; ++s)
      {
        DirectX::XMVECTOR const deltaFrom = DirectX::XMVectorScale(delta, 1.2f * (float)s / (float)numSteps);
        DirectX::XMVECTOR const deltaTo = DirectX::XMVectorScale(delta, 1.2f * (float)(s + 1) / (float)numSteps);

        DirectX::XMVECTOR currentPos = DirectX::XMVectorAdd(DirectX::XMVectorScale(delta, -0.1f), DirectX::XMVectorSet(0.0f, _radius, 0.0f, 0.0f));

        // Vector3::RotateAround's 1e-8 guard, not the 1e-5 one Matrix34 used.
        DirectX::XMVECTOR const spinStep = DirectX::XMVectorScale(deltaNorm, 2.0f * M_PI / (float)numRadii);
        float const spinLengthSquared = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(spinStep));
        DirectX::XMMATRIX const spin =
          spinLengthSquared >= 1e-8f
            ? DirectX::XMMatrixRotationAxis(DirectX::XMVectorScale(spinStep, 1.0f / sqrtf(spinLengthSquared)), sqrtf(spinLengthSquared))
            : DirectX::XMMatrixIdentity();

        for (int r = 0; r <= numRadii; ++r)
        {
          gglMultiTexCoord2fARB(GL_TEXTURE0_ARB, texXInner, r / numRadii);
          gglMultiTexCoord2fARB(GL_TEXTURE1_ARB, texXOuter, r / numRadii);
          EmitVertex(DirectX::XMVectorAdd(currentPos, deltaFrom));

          gglMultiTexCoord2fARB(GL_TEXTURE0_ARB, texXInner + 10.0f / (float)numSteps, (r) / numRadii);
          gglMultiTexCoord2fARB(GL_TEXTURE1_ARB, texXOuter + distance / (200.0f * (float)numSteps), (r) / numRadii);
          EmitVertex(DirectX::XMVectorAdd(currentPos, deltaTo));

          currentPos = DirectX::XMVector3TransformNormal(currentPos, spin);
        }

        texXInner += 10.0f / (float)numSteps;
        texXOuter += distance / (200.0f * (float)numSteps);
      }

      glEnd();
    }
    glTranslatef(-startPos.x, -startPos.y, -startPos.z);

    glDisable(GL_CLIP_PLANE0);
    glDisable(GL_CLIP_PLANE1);
    glDepthMask(true);
    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);

    gglActiveTextureARB(GL_TEXTURE1_ARB);
    glDisable(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    gglActiveTextureARB(GL_TEXTURE0_ARB);
    glDisable(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    END_PROFILE(g_profiler, "Signal");
  }

  DirectX::XMFLOAT3 FeedingTube::GetStartPoint() { return GetDishPos(0.0f); }


  DirectX::XMFLOAT3 FeedingTube::GetEndPoint()
  {
    DirectX::XMFLOAT3 const dishPos = GetDishPos(0.0f);
    DirectX::XMFLOAT3 const dishFront = GetDishFront(0.0f);

    DirectX::XMFLOAT3 endPoint;
    DirectX::XMStoreFloat3(&endPoint, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&dishFront), DirectX::XMVectorReplicate(m_range),
                                                                   DirectX::XMLoadFloat3(&dishPos)));
    return endPoint;
  }


  void FeedingTube::ListSoundEvents(std::vector<const char*>* _list)
  {
    Building::ListSoundEvents(_list);

    _list->push_back("BeginRotation");
    _list->push_back("EndRotation");
    _list->push_back("ConnectionEstablished");
    _list->push_back("ConnectionLost");
  }


  bool FeedingTube::DoesSphereHit(DirectX::XMFLOAT3 const& _pos, float _radius)
  {
    // This method is overridden for Radar Dish in order to expand the number
    // of cells the Radar Dish is placed into.  We were having problems where
    // entities couldn't get into the radar dish because its door was right on
    // the edge of an obstruction grid cell, so the entity didn't see the
    // building at all

    SpherePackage sphere(_pos, _radius * 1.5f);
    DirectX::XMFLOAT4X4 transform = GetWorldMatrix();
    return m_shape->SphereHit(&sphere, transform);
  }

  int FeedingTube::GetBuildingLink() { return m_receiverId; }

  void FeedingTube::SetBuildingLink(int _buildingId)
  {
    Building* b = g_location->GetBuilding(_buildingId);
    if (b && b->m_type == Building::TypeFeedingTube)
    {
      m_receiverId = _buildingId;

      FeedingTube* p = (FeedingTube*)b;
    }
  }

  // *** Read
  void FeedingTube::Read(TextReader* _in, bool _dynamic)
  {
    Building::Read(_in, _dynamic);

    m_receiverId = atoi(_in->GetNextToken());
  }


  // *** Write
  void FeedingTube::Write(FileWriter* _out)
  {
    Building::Write(_out);

    _out->printf("{:<8d}", m_receiverId);
  }

  bool FeedingTube::IsInView()
  {
    DirectX::XMFLOAT3 const startPointStore = GetStartPoint();
    DirectX::XMFLOAT3 const endPointStore = GetEndPoint();
    DirectX::XMVECTOR const startPoint = DirectX::XMLoadFloat3(&startPointStore);
    DirectX::XMVECTOR const endPoint = DirectX::XMLoadFloat3(&endPointStore);

    DirectX::XMFLOAT3 midPoint;
    DirectX::XMStoreFloat3(&midPoint, DirectX::XMVectorScale(DirectX::XMVectorAdd(startPoint, endPoint), 0.5f));
    float radius = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(startPoint, endPoint))) / 2.0f;
    radius += m_radius;

    if (g_camera->SphereInViewFrustum(midPoint, radius))
    {
      return true;
    }

    return Building::IsInView();
  }
} // namespace Species
