#include "pch.h"
#include "GlVertex.h"
#include "SoundSources.h"

#include <math.h>

#include "DebugRender.h"
#include "MathUtils.h"
#include "OglExtensions.h"
#include "Profiler.h"
#include "Resource.h"
#include "Shape.h"

#include "EntityGrid.h"
#include "ProtocolLimits.h"
#include "Location.h"
#include "GameTime.h"
#include "GlobalWorld.h"
#include "Unit.h"

#include "SoundSystem.h"

#include "RadarDish.h"
#include "WorldPointers.h"
#include "AppState.h"


RadarDish::RadarDish()
  : Teleport(),
    m_signal(0.0f),
    m_range(0.0f),
    m_receiverId(-1),
    m_horizontallyAligned(true),
    m_verticallyAligned(true),
    m_movementSoundsPlaying(false),
    m_newlyCreated(true)
{
  m_type = Building::TypeRadarDish;
  m_target = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
  m_front = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
  m_sendPeriod = RADARDISH_TRANSPORTPERIOD;

  Shape* radarShape = g_resource->GetShapeCopy("RadarDish.shp", true);
  SetShape(radarShape);

  m_dish = m_shape->m_rootFragment->LookupFragment("Dish");
  m_upperMount = m_shape->m_rootFragment->LookupFragment("UpperMount");
  m_focusMarker = m_shape->m_rootFragment->LookupMarker("MarkerFocus");
}


RadarDish::~RadarDish()
{
  delete m_shape;
  m_shape = nullptr;
}


// Matrix34::RotateAround applied to a ShapeFragment transform, which is what
// the two mount fragments' per-frame spin used to be written as.
//
// It rotates the three BASIS rows and leaves the position row alone -- that is
// what Matrix34::FastRotateAround did to r, u and f while never touching pos --
// and the angle is the angular-velocity vector's magnitude. The 1e-5 guard is
// Matrix34::RotateAround's own, and is a hundredfold tighter than the 1e-8 that
// Vector3::RotateAround used; the two are easy to confuse and only one is right
// here. Reproducing it also means a zero angular velocity leaves the transform
// untouched rather than storing QNaN through a normalise of the zero vector.
static void SpinBasisRows(DirectX::XMFLOAT4X4& _transform, DirectX::XMFLOAT3 const& _angularVelocity)
{
  DirectX::XMVECTOR const rotation = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&_angularVelocity), SERVER_ADVANCE_PERIOD);
  float const magSquared = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(rotation));
  if (magSquared < 0.00001f * 0.00001f)
    return;

  float const angle = sqrtf(magSquared);
  DirectX::XMMATRIX const spin = DirectX::XMMatrixRotationAxis(DirectX::XMVectorScale(rotation, 1.0f / angle), angle);

  // TransformNormal, not Transform: the rows are directions, and Transform
  // would write w = 1 into each of them from the matrix's translation row.
  // The xyz are identical either way for a pure rotation, so this is invisible
  // until something downstream multiplies the transform and picks up three
  // spurious translations.
  DirectX::XMMATRIX transform = DirectX::XMLoadFloat4x4(&_transform);
  transform.r[0] = DirectX::XMVector3TransformNormal(transform.r[0], spin);
  transform.r[1] = DirectX::XMVector3TransformNormal(transform.r[1], spin);
  transform.r[2] = DirectX::XMVector3TransformNormal(transform.r[2], spin);
  DirectX::XMStoreFloat4x4(&_transform, transform);
}

void RadarDish::SetDetail(int _detail)
{
  Teleport::SetDetail(_detail);

  DirectX::XMFLOAT4X4 rootMat = GetWorldMatrix();

  // ShapeMarker::GetWorldMatrix still returns Matrix34 -- T10's seam.
  DirectX::XMFLOAT4X4 const worldMat = m_entrance->GetWorldMatrix(rootMat);
  m_entrancePos = DirectX::XMFLOAT3(worldMat._41, worldMat._42, worldMat._43);
  m_entranceFront = DirectX::XMFLOAT3(worldMat._31, worldMat._32, worldMat._33);
}


bool RadarDish::GetEntrance(DirectX::XMFLOAT3& _pos, DirectX::XMFLOAT3& _front)
{
  _pos = m_entrancePos;
  _front = m_entranceFront;
  return true;
}


bool RadarDish::Advance()
{
  //
  // If this is our first advance, align to our dish
  // if we saved a target dish previously
  if (m_newlyCreated)
  {
    GlobalBuilding* gb = g_globalWorld->GetBuilding(m_id.GetUniqueId(), g_locationId);
    if (gb)
    {
      Building* targetBuilding = g_location->GetBuilding(gb->m_link);
      if (targetBuilding && targetBuilding->m_type == TypeRadarDish)
      {
        RadarDish* dish = (RadarDish*)targetBuilding;
        Aim(dish->GetStartPoint());
      }
    }
    m_newlyCreated = false;
  }

  if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(DirectX::XMLoadFloat3(&m_target))) < 0.001f)
    return Building::Advance();

  DirectX::XMFLOAT4X4 rootMat;
  DirectX::XMStoreFloat4x4(&rootMat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

  // ShapeMarker::GetWorldMatrix still returns Matrix34 -- T10's seam.
  DirectX::XMFLOAT4X4 const worldMat = m_focusMarker->GetWorldMatrix(rootMat);
  DirectX::XMFLOAT3 const dishPos = DirectX::XMFLOAT3(worldMat._41, worldMat._42, worldMat._43);
  DirectX::XMFLOAT3 const dishFront = DirectX::XMFLOAT3(worldMat._31, worldMat._32, worldMat._33);


  //
  // Rotate slowly to face our target (horiz)

  if (!m_horizontallyAligned)
  {
    // HorizontalAndNormalise: flatten to the XZ plane, then normalise.
    DirectX::XMVECTOR const targetFront = DirectX::XMVector3Normalize(
      DirectX::XMVectorSetY(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_target), DirectX::XMLoadFloat3(&dishPos)), 0.0f));
    DirectX::XMVECTOR const currentFront = DirectX::XMVector3Normalize(DirectX::XMVectorSetY(DirectX::XMLoadFloat3(&dishFront), 0.0f));

    DirectX::XMFLOAT3 rotationAxis;
    DirectX::XMStoreFloat3(&rotationAxis, DirectX::XMVector3Cross(currentFront, targetFront));
    rotationAxis.y /= 4.0f;
    if (fabsf(rotationAxis.y) > M_PI / 3000.0f)
    {
      m_upperMount->m_angVel.y = signf(rotationAxis.y) * sqrtf(fabsf(rotationAxis.y));
    }
    else
    {
      m_horizontallyAligned = true;
      m_upperMount->m_angVel.y = 0.0f;
    }
  }


  //
  // Rotate slowly to face our target (vert)

  m_dish->m_angVel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

  if (m_horizontallyAligned && !m_verticallyAligned)
  {
    DirectX::XMFLOAT3 targetFront;
    DirectX::XMStoreFloat3(&targetFront,
                           DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_target), DirectX::XMLoadFloat3(&dishPos))));
    float const maxAngle = 0.3f;
    float const minAngle = -0.2f;
    if (targetFront.y > maxAngle)
      targetFront.y = maxAngle;
    if (targetFront.y < minAngle)
      targetFront.y = minAngle;
    DirectX::XMVECTOR const clampedFront = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&targetFront));

    // worldMat is Matrix34 -- T10's seam -- so its rows are Vector3 and &row is
    // a Vector3*. Copy-initialising takes the seam's reference conversion.
    DirectX::XMFLOAT3 const dishUp = DirectX::XMFLOAT3(worldMat._21, worldMat._22, worldMat._23);
    float amount = DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMLoadFloat3(&dishUp), clampedFront));

    // Row 0 of the transform is what Matrix34 called r. ShapeFragment stores
    // native types as of T10, and this reads that row directly now rather than
    // rebuilding a Vector3 from its three components.
    DirectX::XMVECTOR const dishRight = DirectX::XMVectorSet(m_dish->m_transform._11, m_dish->m_transform._12, m_dish->m_transform._13, 0.0f);
    DirectX::XMStoreFloat3(&m_dish->m_angVel, DirectX::XMVectorScale(dishRight, amount));

    m_verticallyAligned = (DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&m_dish->m_angVel))) < 0.001f);
  }

  SpinBasisRows(m_upperMount->m_transform, m_upperMount->m_angVel);
  SpinBasisRows(m_dish->m_transform, m_dish->m_angVel);

  if (m_movementSoundsPlaying && m_horizontallyAligned &&
      DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&m_dish->m_angVel))) < 0.05f)
  {
    g_soundSystem->StopAllSounds(m_id, "RadarDish BeginRotation");
    g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), "EndRotation");
    m_movementSoundsPlaying = false;
  }


  //
  // Are there any receiving Radar dishes?

  m_range = 99999.9f;
  bool found = false;

  bool previouslyAligned = (m_receiverId != -1);

  for (int i = 0; i < g_location->m_buildings.Size(); ++i)
  {
    // Skip empty slots
    if (!g_location->m_buildings.ValidIndex(i))
      continue;

    // Filter out non radar dish buildings
    Building* building = g_location->m_buildings.GetData(i);
    if (building->m_type != TypeRadarDish)
      continue;

    // Don't compare against ourself
    if (building == this)
      continue;

    // Does our "ray" hit their dish
    RadarDish* otherDish = (RadarDish*)building;
    bool hit = RaySphereIntersection(dishPos, dishFront, otherDish->m_centrePos, otherDish->m_radius, 1e9);
    if (hit)
    {
      // Make sure we aren't hitting the back of the receiving dish
      DirectX::XMFLOAT3 const theirFront = otherDish->GetDishFront(0.0f);
      float dotProd = DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMLoadFloat3(&dishFront), DirectX::XMLoadFloat3(&theirFront)));
      if (dotProd < 0.0f)
      {
        if (g_location->IsVisible(dishPos, otherDish->GetDishPos(0.0f)))
        {
          DirectX::XMFLOAT3 const otherDishPos = otherDish->GetDishPos(0.0f);
          float newRange = DirectX::XMVectorGetX(
            DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&otherDishPos), DirectX::XMLoadFloat3(&dishPos))));
          if (newRange < m_range)
          {
            m_receiverId = otherDish->m_id.GetUniqueId();
            m_range = newRange;
            m_signal = 1.0f;
            found = true;
          }
        }
      }
    }
  }

  if (previouslyAligned && !found)
  {
    m_range = 0.0f;
    m_signal = 0.0f;
    m_receiverId = -1;
    g_soundSystem->StopAllSounds(m_id, "RadarDish ConnectionEstablished");
    g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), "ConnectionLost");

    GlobalBuilding* gb = g_globalWorld->GetBuilding(m_id.GetUniqueId(), g_locationId);
    if (gb)
      gb->m_link = -1;
  }

  if (!previouslyAligned && found)
  {
    g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), "ConnectionEstablished");

    GlobalBuilding* gb = g_globalWorld->GetBuilding(m_id.GetUniqueId(), g_locationId);
    if (gb)
      gb->m_link = m_receiverId;
  }

  return Teleport::Advance();
}


DirectX::XMFLOAT3 RadarDish::GetDishPos(float _predictionTime)
{
  DirectX::XMFLOAT4X4 rootMat;
  DirectX::XMStoreFloat4x4(&rootMat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

  // ShapeMarker::GetWorldMatrix still returns Matrix34 -- T10's seam.
  return m_focusMarker->GetWorldPosition(rootMat);
}


DirectX::XMFLOAT3 RadarDish::GetDishFront(float _predictionTime)
{
  if (m_receiverId != -1)
  {
    RadarDish* receiver = (RadarDish*)g_location->GetBuilding(m_receiverId);
    if (receiver)
    {
      DirectX::XMFLOAT3 const ourDishPos = GetDishPos(_predictionTime);
      DirectX::XMFLOAT3 const receiverDishPos = receiver->GetDishPos(_predictionTime);

      DirectX::XMFLOAT3 result;
      DirectX::XMStoreFloat3(
        &result, DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&receiverDishPos), DirectX::XMLoadFloat3(&ourDishPos))));
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


void RadarDish::Aim(DirectX::XMFLOAT3 _worldPos)
{
  m_target = _worldPos;

  m_horizontallyAligned = false;
  m_verticallyAligned = false;

  if (m_movementSoundsPlaying)
  {
    g_soundSystem->StopAllSounds(m_id, "RadarDish BeginRotation");
  }

  g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), "BeginRotation");
  m_movementSoundsPlaying = true;
}


void RadarDish::Render(float _predictionTime) { Building::Render(_predictionTime); }

void RadarDish::RenderAlphas(float _predictionTime)
{
  if (m_signal > 0.0f)
  {
    RenderSignal(_predictionTime, 10.0f, 0.4f);
    RenderSignal(_predictionTime, 9.0f, 0.2f);
    RenderSignal(_predictionTime, 8.0f, 0.2f);
    RenderSignal(_predictionTime, 4.0f, 0.5f);
  }

  Teleport::RenderAlphas(_predictionTime);
}


void RadarDish::RenderSignal(float _predictionTime, float _radius, float _alpha)
{
  START_PROFILE(g_profiler, "Signal");

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
  DirectX::XMFLOAT3 const dishFront = GetDishFront(_predictionTime);
  double eqn1[4] = {dishFront.x, dishFront.y, dishFront.z, -1.0f};
  glClipPlane(GL_CLIP_PLANE0, eqn1);

  RadarDish* receiver = (RadarDish*)g_location->GetBuilding(m_receiverId);
  DirectX::XMFLOAT3 const receiverPos = receiver->GetDishPos(_predictionTime);
  DirectX::XMFLOAT3 const receiverFront = receiver->GetDishFront(_predictionTime);
  glTranslatef(-startPos.x, -startPos.y, -startPos.z);
  glTranslatef(receiverPos.x, receiverPos.y, receiverPos.z);

  DirectX::XMVECTOR const diff = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&receiverPos), DirectX::XMLoadFloat3(&startPos));
  float thisDistance = -DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMLoadFloat3(&receiverFront), diff));

  thisDistance = -1.0f;

  double eqn2[4] = {receiverFront.x, receiverFront.y, receiverFront.z, thisDistance};
  glClipPlane(GL_CLIP_PLANE1, eqn2);
  glTranslatef(-receiverPos.x, -receiverPos.y, -receiverPos.z);
  glTranslatef(startPos.x, startPos.y, startPos.z);

  glEnable(GL_CLIP_PLANE0);
  glEnable(GL_CLIP_PLANE1);

  float texXInner = -g_gameTime / _radius;
  float texXOuter = -g_gameTime;

  // float texXInner = -1.0f/_radius;
  // float texXOuter = -1.0f;

  glBegin(GL_QUAD_STRIP);

  for (int s = 0; s < numSteps; ++s)
  {
    DirectX::XMVECTOR const deltaFrom = DirectX::XMVectorScale(delta, 1.2f * (float)s / (float)numSteps);
    DirectX::XMVECTOR const deltaTo = DirectX::XMVectorScale(delta, 1.2f * (float)(s + 1) / (float)numSteps);

    DirectX::XMVECTOR currentPos = DirectX::XMVectorAdd(DirectX::XMVectorScale(delta, -0.1f), DirectX::XMVectorSet(0.0f, _radius, 0.0f, 0.0f));

    // Vector3::RotateAround's 1e-8 guard, not the 1e-5 one Matrix34 used. The
    // step angle here is a fixed 2*pi/numRadii and never trips either, but the
    // two thresholds sit within a few lines of each other in this file.
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


bool RadarDish::Connected() { return (m_receiverId != -1 && m_signal > 0.0f); }


int RadarDish::GetConnectedDishId() { return m_receiverId; }


bool RadarDish::ReadyToSend() { return (Connected() && Teleport::ReadyToSend()); }


DirectX::XMFLOAT3 RadarDish::GetStartPoint() { return GetDishPos(0.0f); }


DirectX::XMFLOAT3 RadarDish::GetEndPoint()
{
  DirectX::XMFLOAT3 const dishPos = GetDishPos(0.0f);
  DirectX::XMFLOAT3 const dishFront = GetDishFront(0.0f);

  DirectX::XMFLOAT3 endPoint;
  DirectX::XMStoreFloat3(
    &endPoint, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&dishFront), DirectX::XMVectorReplicate(m_range), DirectX::XMLoadFloat3(&dishPos)));
  return endPoint;
}


bool RadarDish::GetExit(DirectX::XMFLOAT3& _pos, DirectX::XMFLOAT3& _front)
{
  RadarDish* receiver = (RadarDish*)g_location->GetBuilding(m_receiverId);
  if (receiver)
  {
    DirectX::XMFLOAT4X4 rootMat;
    DirectX::XMStoreFloat4x4(
      &rootMat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&receiver->m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&receiver->m_pos)));

    // ShapeMarker::GetWorldMatrix still returns Matrix34 -- T10's seam.
    DirectX::XMFLOAT4X4 const worldMat = receiver->m_entrance->GetWorldMatrix(rootMat);
    _pos = DirectX::XMFLOAT3(worldMat._41, worldMat._42, worldMat._43);
    _front = DirectX::XMFLOAT3(worldMat._31, worldMat._32, worldMat._33);
    return true;
  }
  return false;
}


bool RadarDish::UpdateEntityInTransit(Entity* _entity)
{
  DirectX::XMFLOAT3 const dishPos = GetDishPos(0.0f);
  DirectX::XMFLOAT3 const dishFront = GetDishFront(0.0f);

  WorldObjectId id(_entity->m_id);

  DirectX::XMStoreFloat3(&_entity->m_vel, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&dishFront), RADARDISH_TRANSPORTSPEED));
  DirectX::XMStoreFloat3(&_entity->m_pos,
                         DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&_entity->m_vel), DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD),
                                                      DirectX::XMLoadFloat3(&_entity->m_pos)));
  _entity->m_onGround = false;
  _entity->m_enabled = false;

  if (_entity->m_id.GetUnitId() != -1)
  {
    Unit* unit = g_location->GetUnit(_entity->m_id);
    unit->UpdateEntityPosition(_entity->m_pos, _entity->m_radius);
  }

  float distTravelled = DirectX::XMVectorGetX(
    DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&_entity->m_pos), DirectX::XMLoadFloat3(&dishPos))));

  if (m_signal == 0.0f)
  {
    // Shit - we lost the carrier signal, so we die
    _entity->ChangeHealth(-500);
    _entity->m_enabled = true;
    // The three RNG calls stay in this order.
    DirectX::XMFLOAT3 const scatter(syncsfrand(10.0f), syncfrand(10.0f), syncsfrand(10.0f));
    DirectX::XMStoreFloat3(&_entity->m_vel, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&_entity->m_vel), DirectX::XMLoadFloat3(&scatter)));

    g_location->m_entityGrid->AddObject(id, _entity->m_pos.x, _entity->m_pos.z, _entity->m_radius);
    return true;
  }
  else if (distTravelled >= m_range)
  {
    // We are there
    DirectX::XMFLOAT3 exitPos, exitFront;
    GetExit(exitPos, exitFront);
    _entity->m_pos = exitPos;
    _entity->m_front = exitFront;
    _entity->m_enabled = true;
    _entity->m_onGround = true;
    _entity->m_vel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

    g_location->m_entityGrid->AddObject(id, _entity->m_pos.x, _entity->m_pos.z, _entity->m_radius);

    g_soundSystem->TriggerEntityEvent(SoundSourceOf(_entity), "ExitTeleport");
    return true;
  }

  return false;
}


void RadarDish::ListSoundEvents(std::vector<const char*>* _list)
{
  Building::ListSoundEvents(_list);

  _list->push_back("BeginRotation");
  _list->push_back("EndRotation");
  _list->push_back("ConnectionEstablished");
  _list->push_back("ConnectionLost");
}


bool RadarDish::DoesSphereHit(DirectX::XMFLOAT3 const& _pos, float _radius)
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