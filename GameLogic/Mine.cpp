#include "pch.h"
#include "GlVertex.h"
#include "SoundSources.h"

#include "TextStreamReaders.h"
#include "Shape.h"
#include "Debug.h"
#include "FileWriter.h"
#include "Resource.h"
#include "MathUtils.h"
#include "Profiler.h"
#include "DebugRender.h"
#include "TextRenderer.h"
#include "3dSprite.h"
#include "HiResTime.h"
#include "Preferences.h"
#include "LanguageTable.h"

#ifdef CHEATMENU_ENABLED
#include "Input.h"
#include "InputTypes.h"
#endif

#include "GlobalWorld.h"
#include "Location.h"
#include "GameTime.h"

#include "SoundSystem.h"

#include "Mine.h"
#include "ConstructionYard.h"
#include "TrunkPort.h"
#include "WorldPointers.h"
#include "AppState.h"

// ****************************************************************************
// Class MineBuilding
// ****************************************************************************


namespace Species
{
  Shape* MineBuilding::s_wheelShape = nullptr;

  Shape* MineBuilding::s_cartShape = nullptr;
  ShapeMarker* MineBuilding::s_cartMarker1 = nullptr;
  ShapeMarker* MineBuilding::s_cartMarker2 = nullptr;
  ShapeMarker* MineBuilding::s_cartContents[] = {nullptr, nullptr, nullptr};

  Shape* MineBuilding::s_polygon1 = nullptr;
  Shape* MineBuilding::s_primitive1 = nullptr;

  float MineBuilding::s_refineryPopulation = 0.0f;
  float MineBuilding::s_refineryRecalculateTimer = 0.0f;


  MineBuilding::MineBuilding()
    : Building(),
      m_trackLink(-1),
      m_trackMarker1(nullptr),
      m_trackMarker2(nullptr),
      m_previousMineSpeed(0.0f),
      m_wheelRotate(0.0f)
  {
    if (!s_cartShape)
    {
      s_wheelShape = g_resource->GetShape("Wheel.shp");
      s_cartShape = g_resource->GetShape("MineCart.shp");
      s_cartMarker1 = s_cartShape->m_rootFragment->LookupMarker("MarkerTrack1");
      s_cartMarker2 = s_cartShape->m_rootFragment->LookupMarker("MarkerTrack2");
      s_cartContents[0] = s_cartShape->m_rootFragment->LookupMarker("MarkerContents1");
      s_cartContents[1] = s_cartShape->m_rootFragment->LookupMarker("MarkerContents2");
      s_cartContents[2] = s_cartShape->m_rootFragment->LookupMarker("MarkerContents3");

      s_polygon1 = g_resource->GetShape("MinePolygon1.shp");
      s_primitive1 = g_resource->GetShape("MinePrimitive1.shp");
    }
  }


  void MineBuilding::Initialise(Building* _template)
  {
    Building::Initialise(_template);
    m_trackLink = ((MineBuilding*)_template)->m_trackLink;

    float cart = 0.0f;

    while (true)
    {
      cart += 0.2f;
      cart += syncfrand(1.5f);
      if (cart >= 1.0f)
        break;

      MineCart* mineCart = new MineCart();
      mineCart->m_progress = cart;
      m_carts.push_back(mineCart);
    }
  }


  bool MineBuilding::IsInView()
  {
    Building* trackLink = g_location->GetBuilding(m_trackLink);
    if (trackLink)
    {
      DirectX::XMVECTOR const theirCentre = DirectX::XMLoadFloat3(&trackLink->m_centrePos);
      DirectX::XMVECTOR const ourCentre = DirectX::XMLoadFloat3(&m_centrePos);

      DirectX::XMFLOAT3 midPoint;
      DirectX::XMStoreFloat3(&midPoint, DirectX::XMVectorScale(DirectX::XMVectorAdd(theirCentre, ourCentre), 0.5f));
      float radius = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(theirCentre, ourCentre))) / 2.0f;
      radius += m_radius;

      if (g_camera->SphereInViewFrustum(midPoint, radius))
      {
        return true;
      }
    }

    return Building::IsInView();
  }


  // Matrix34::RotateAroundF(a) rewrote the matrix's r and u rows in place --
  // r' = c*r + s*u, u' = -s*r + c*u -- leaving f and pos alone. Matrix34 reads
  // r/u/f as the basis vectors that XMFLOAT4X4 stores as rows 0, 1 and 2, so the
  // same thing natively is a rotation about the LOCAL z axis applied BEFORE the
  // matrix: XMMatrixRotationZ(a) * M, in that operand order.
  //
  // The order is the whole content of this. `M * RotationZ(a)` compiles, is a
  // rotation, and spins the mine wheels about the world axis instead of their
  // own -- which on a wheel is a wobble rather than a spin, and nothing in the
  // build or the checks would say so.
  static DirectX::XMFLOAT4X4 RotatedAroundLocalFront(DirectX::XMFLOAT4X4 const& _mat, float _angle)
  {
    DirectX::XMFLOAT4X4 result;
    DirectX::XMStoreFloat4x4(&result, DirectX::XMMatrixMultiply(DirectX::XMMatrixRotationZ(_angle), DirectX::XMLoadFloat4x4(&_mat)));
    return result;
  }


  void MineBuilding::RenderAlphas(float _predictionTime)
  {
    Building::RenderAlphas(_predictionTime);

    if (m_trackLink != -1)
    {
      Building* trackLink = g_location->GetBuilding(m_trackLink);
      if (trackLink)
      {
        int buildingDetail = g_prefsManager->GetInt("RenderBuildingDetail", 1);

        MineBuilding* mineBuilding = (MineBuilding*)trackLink;

        DirectX::XMFLOAT3 const ourPos1Store = GetTrackMarker1();
        DirectX::XMFLOAT3 const theirPos1Store = mineBuilding->GetTrackMarker1();
        DirectX::XMFLOAT3 const ourPos2Store = GetTrackMarker2();
        DirectX::XMFLOAT3 const theirPos2Store = mineBuilding->GetTrackMarker2();

        DirectX::XMVECTOR const ourPos1 = DirectX::XMLoadFloat3(&ourPos1Store);
        DirectX::XMVECTOR const theirPos1 = DirectX::XMLoadFloat3(&theirPos1Store);
        DirectX::XMVECTOR const ourPos2 = DirectX::XMLoadFloat3(&ourPos2Store);
        DirectX::XMVECTOR const theirPos2 = DirectX::XMLoadFloat3(&theirPos2Store);

        glColor4f(0.85f, 0.4f, 0.4f, 1.0f);

        float size = 2.0f;
        if (buildingDetail > 1)
          size = 1.0f;

        DirectX::XMFLOAT3 const cameraPosStore = g_camera->GetPos();
        DirectX::XMVECTOR const cameraPos = DirectX::XMLoadFloat3(&cameraPosStore);
        DirectX::XMVECTOR const alongTrack = DirectX::XMVectorSubtract(ourPos1, theirPos1);

        // SetLength; see the note on the same call in Generator.cpp. Rendering
        // only, so this takes the native normalise rather than the fallback.
        DirectX::XMVECTOR const lineOurPos1 = DirectX::XMVectorScale(
          DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::XMVectorSubtract(cameraPos, ourPos1), alongTrack)), size);
        DirectX::XMVECTOR const lineTheirPos1 = DirectX::XMVectorScale(
          DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::XMVectorSubtract(cameraPos, theirPos1), alongTrack)), size);

        glDisable(GL_CULL_FACE);

        if (buildingDetail == 1)
        {
          glEnable(GL_TEXTURE_2D);
          glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Laser.bmp"));
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

          glBlendFunc(GL_SRC_ALPHA, GL_ONE);
          glEnable(GL_BLEND);
        }

        glDepthMask(false);

        glBegin(GL_QUADS);
        glTexCoord2f(0.1, 0);
        EmitVertex(DirectX::XMVectorSubtract(ourPos1, lineOurPos1));
        glTexCoord2f(0.1, 1);
        EmitVertex(DirectX::XMVectorAdd(ourPos1, lineOurPos1));
        glTexCoord2f(0.9, 1);
        EmitVertex(DirectX::XMVectorAdd(theirPos1, lineTheirPos1));
        glTexCoord2f(0.9, 0);
        EmitVertex(DirectX::XMVectorSubtract(theirPos1, lineTheirPos1));
        glEnd();

        glBegin(GL_QUADS);
        glTexCoord2f(0.1, 0);
        EmitVertex(DirectX::XMVectorSubtract(ourPos2, lineOurPos1));
        glTexCoord2f(0.1, 1);
        EmitVertex(DirectX::XMVectorAdd(ourPos2, lineOurPos1));
        glTexCoord2f(0.9, 1);
        EmitVertex(DirectX::XMVectorAdd(theirPos2, lineTheirPos1));
        glTexCoord2f(0.9, 0);
        EmitVertex(DirectX::XMVectorSubtract(theirPos2, lineTheirPos1));
        glEnd();

        glDepthMask(true);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glDisable(GL_TEXTURE_2D);
        glEnable(GL_CULL_FACE);
      }
    }
  }


  void MineBuilding::Render(float _predictionTime)
  {
    _predictionTime -= 0.1f;

    for (int i = 0; i < static_cast<int>(m_carts.size()); ++i)
    {
      MineCart* thisCart = m_carts[i];
      RenderCart(thisCart, _predictionTime);
    }

    Building::Render(_predictionTime);
  }


  void MineBuilding::RenderCart(MineCart* _cart, float _predictionTime)
  {
    // START_PROFILE(g_profiler, "Mine Cart");

    if (m_trackLink != -1)
    {
      Building* trackLink = g_location->GetBuilding(m_trackLink);
      if (!trackLink)
        return;
      MineBuilding* mineBuilding = (MineBuilding*)trackLink;

      int buildingDetail = g_prefsManager->GetInt("RenderBuildingDetail", 1);

      DirectX::XMFLOAT3 const ourMarker1Store = GetTrackMarker1();
      DirectX::XMFLOAT3 const ourMarker2Store = GetTrackMarker2();
      DirectX::XMFLOAT3 const theirMarker1Store = mineBuilding->GetTrackMarker1();
      DirectX::XMFLOAT3 const theirMarker2Store = mineBuilding->GetTrackMarker2();

      DirectX::XMVECTOR const ourMarker1 = DirectX::XMLoadFloat3(&ourMarker1Store);
      DirectX::XMVECTOR const ourMarker2 = DirectX::XMLoadFloat3(&ourMarker2Store);
      DirectX::XMVECTOR const theirMarker1 = DirectX::XMLoadFloat3(&theirMarker1Store);
      DirectX::XMVECTOR const theirMarker2 = DirectX::XMLoadFloat3(&theirMarker2Store);

      float mineSpeed = RefinerySpeed();

      float predictedProgress = _cart->m_progress;
      predictedProgress += _predictionTime * mineSpeed;
      if (predictedProgress < 0.0f)
        predictedProgress = 0.0f;
      if (predictedProgress > 1.0f)
        predictedProgress = 1.0f;

      DirectX::XMVECTOR const progress = DirectX::XMVectorReplicate(predictedProgress);
      DirectX::XMVECTOR const trackLeft = DirectX::XMVectorMultiplyAdd(DirectX::XMVectorSubtract(theirMarker1, ourMarker1), progress, ourMarker1);
      DirectX::XMVECTOR const trackRight = DirectX::XMVectorMultiplyAdd(DirectX::XMVectorSubtract(theirMarker2, ourMarker2), progress, ourMarker2);

      DirectX::XMVECTOR const cartPosVec = DirectX::XMVectorAdd(DirectX::XMVectorScale(DirectX::XMVectorAdd(trackLeft, trackRight), 0.5f),
                                                                DirectX::XMVectorSet(0.0f, -40.0f, 0.0f, 0.0f));
      DirectX::XMFLOAT3 cartPos;
      DirectX::XMStoreFloat3(&cartPos, cartPosVec);

      if (g_camera->PosInViewFrustum(cartPos))
      {
        DirectX::XMVECTOR const cartFront = DirectX::XMVector3Normalize(
          DirectX::XMVectorSetY(DirectX::XMVector3Cross(DirectX::XMVectorSubtract(trackLeft, trackRight), DirectX::g_XMIdentityR1), 0.0f));

        // START_PROFILE(g_profiler, "RenderCartShape" );
        DirectX::XMFLOAT4X4 transform;
        DirectX::XMStoreFloat4x4(&transform, BasisFromFrontAndUp(cartFront, DirectX::g_XMIdentityR1, cartPosVec));
        s_cartShape->Render(0.0f, transform);
        // END_PROFILE(g_profiler, "RenderCartShape" );

        DirectX::XMFLOAT3 const cartLinkLeftStore = s_cartMarker1->GetWorldPosition(transform);
        DirectX::XMFLOAT3 const cartLinkRightStore = s_cartMarker2->GetWorldPosition(transform);
        DirectX::XMVECTOR const cartLinkLeft = DirectX::XMLoadFloat3(&cartLinkLeftStore);
        DirectX::XMVECTOR const cartLinkRight = DirectX::XMLoadFloat3(&cartLinkRightStore);

        // START_PROFILE(g_profiler, "RenderLines" );
        DirectX::XMFLOAT3 const camRightStore = g_camera->GetRight();
        DirectX::XMVECTOR const camRight = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&camRightStore), 0.5f);

        glBegin(GL_QUADS);
        EmitVertex(DirectX::XMVectorSubtract(trackLeft, camRight));
        EmitVertex(DirectX::XMVectorAdd(trackLeft, camRight));
        EmitVertex(DirectX::XMVectorAdd(cartLinkLeft, camRight));
        EmitVertex(DirectX::XMVectorSubtract(cartLinkLeft, camRight));

        EmitVertex(DirectX::XMVectorSubtract(trackRight, camRight));
        EmitVertex(DirectX::XMVectorAdd(trackRight, camRight));
        EmitVertex(DirectX::XMVectorAdd(cartLinkRight, camRight));
        EmitVertex(DirectX::XMVectorSubtract(cartLinkRight, camRight));
        glEnd();
        // END_PROFILE(g_profiler, "RenderLines" );

        // START_PROFILE(g_profiler, "RenderPolygons" );
        for (int i = 0; i < 3; ++i)
        {
          if (_cart->m_polygons[i])
          {
            DirectX::XMFLOAT4X4 polyMat = s_cartContents[i]->GetWorldMatrix(transform);
            s_polygon1->Render(0.0f, polyMat);
          }

          if (_cart->m_primitives[i])
          {
            DirectX::XMFLOAT4X4 polyMat = s_cartContents[i]->GetWorldMatrix(transform);
            s_primitive1->Render(0.0f, polyMat);

            if (buildingDetail < 3)
            {
              glEnable(GL_BLEND);
              glBlendFunc(GL_SRC_ALPHA, GL_ONE);
              glDepthMask(false);
              // glDisable( GL_DEPTH_TEST );
              glDisable(GL_LIGHTING);

              glColor4f(1.0f, 0.7f, 0.0f, 0.75f);

              float nearPlaneStart = g_renderer->GetNearPlane();
              g_camera->SetupProjectionMatrix(nearPlaneStart * 1.1f, g_renderer->GetFarPlane());

              DirectX::XMFLOAT3 const glowPos(polyMat._41, polyMat._42 - 25.0f, polyMat._43);
              Render3DSprite(glowPos, 50.0f, 50.0f, g_resource->GetTexture("Textures/Glow.bmp"));

              g_camera->SetupProjectionMatrix(nearPlaneStart, g_renderer->GetFarPlane());

              glEnable(GL_LIGHTING);
              glEnable(GL_DEPTH_TEST);
              glDepthMask(true);
              glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
              glDisable(GL_BLEND);
            }
          }
        }
        // END_PROFILE(g_profiler, "RenderPolygons" );
      }
    }

    // END_PROFILE(g_profiler, "Mine Cart");
  }


  bool MineBuilding::Advance()
  {
    float mineSpeed = RefinerySpeed();
    if (m_previousMineSpeed <= 0.1f && mineSpeed > 0.1f)
    {
      g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), "CogTurn");
    }
    m_previousMineSpeed = mineSpeed;

    if (mineSpeed > 0.0f)
    {
      for (int i = static_cast<int>(m_carts.size()) - 1; i >= 0; --i)
      {
        MineCart* thisCart = m_carts[i];
        thisCart->m_progress += mineSpeed * SERVER_ADVANCE_PERIOD;

        if (thisCart->m_progress >= 1.0f)
        {
          float remainder = thisCart->m_progress - 1.0f;
          m_carts.erase(m_carts.begin() + i);
          --i;

          Building* trackLink = g_location->GetBuilding(m_trackLink);
          if (trackLink)
          {
            MineBuilding* mineBuilding = (MineBuilding*)trackLink;
            mineBuilding->TriggerCart(thisCart, remainder);
          }
        }
      }
    }

    //
    // Rotate our wheels

    m_wheelRotate -= 3.0f * mineSpeed * SERVER_ADVANCE_PERIOD;

    return Building::Advance();
  }


  void MineBuilding::ListSoundEvents(std::vector<const char*>* _list)
  {
    Building::ListSoundEvents(_list);

    _list->push_back("CogTurn");
  }


  void MineBuilding::TriggerCart(MineCart* _cart, float _initValue)
  {
    _cart->m_progress = _initValue;
    m_carts.insert(m_carts.begin(), _cart);
  }


  DirectX::XMFLOAT3 MineBuilding::GetTrackMarker1()
  {
    if (!m_trackMarker1 || g_editing)
    {
      m_trackMarker1 = m_shape->m_rootFragment->LookupMarker("MarkerTrack1");
      DEBUG_ASSERT(m_trackMarker1);

      DirectX::XMFLOAT4X4 rootMat;
      DirectX::XMStoreFloat4x4(&rootMat,
                               BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

      // only the position was ever kept, which is why the member is one now.
      m_trackPosition1 = m_trackMarker1->GetWorldPosition(rootMat);
    }

    return m_trackPosition1;
  }


  DirectX::XMFLOAT3 MineBuilding::GetTrackMarker2()
  {
    if (!m_trackMarker2 || g_editing)
    {
      m_trackMarker2 = m_shape->m_rootFragment->LookupMarker("MarkerTrack2");
      DEBUG_ASSERT(m_trackMarker2);

      DirectX::XMFLOAT4X4 rootMat;
      DirectX::XMStoreFloat4x4(&rootMat,
                               BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

      m_trackPosition2 = m_trackMarker2->GetWorldPosition(rootMat);
    }

    return m_trackPosition2;
  }


  void MineBuilding::Read(TextReader* _in, bool _dynamic)
  {
    Building::Read(_in, _dynamic);
    m_trackLink = atoi(_in->GetNextToken());
  }


  void MineBuilding::Write(FileWriter* _out)
  {
    Building::Write(_out);
    _out->printf("{:<8d}", m_trackLink);
  }


  int MineBuilding::GetBuildingLink() { return m_trackLink; }


  void MineBuilding::SetBuildingLink(int _buildingId) { m_trackLink = _buildingId; }


  float MineBuilding::RefinerySpeed()
  {
    if (GetHighResTime() >= s_refineryRecalculateTimer)
    {
      //
      // Find the refinery
      // If not a refinery, look for a construction yard
      // If not, look for a fuel generator

      Building* driver = nullptr;

      int numFuelGenerators = 0;
      float fuelGeneratorFactor = 0.0f;

      for (int i = 0; i < g_location->m_buildings.Size(); ++i)
      {
        if (g_location->m_buildings.ValidIndex(i))
        {
          Building* building = g_location->m_buildings[i];
          if (building->m_type == TypeRefinery || building->m_type == TypeYard)
          {
            driver = building;
            break;
          }
        }
      }

      //
      // If we found a refinery calculate our speed based on that
      // If not, there may be a construction yard driving us
      // If not, there may be a Fuel Generator driving us
      // Otherwise assume the refinery is on a different level, and use global state data

      if (driver)
      {
        int numPorts = driver->GetNumPorts();
        int numPortsOccupied = driver->GetNumPortsOccupied();
        s_refineryPopulation = (float)numPortsOccupied / (float)numPorts;
      }
      else if (numFuelGenerators > 0)
      {
        s_refineryPopulation = fuelGeneratorFactor / (float)numFuelGenerators;
      }
      else
      {
        int mineLocationId = g_globalWorld->GetLocationId("mine");
        s_refineryPopulation = 0.0f;

        GlobalBuilding* globalRefinery = nullptr;
        for (auto const& gb : g_globalWorld->m_buildings)
        {
          if (gb && gb->m_locationId == mineLocationId && gb->m_type == TypeRefinery && gb->m_online)
          {
            s_refineryPopulation = 1.0f;
            break;
          }
        }
      }

      s_refineryRecalculateTimer = GetHighResTime() + 0.1f;
    }

    float speed = s_refineryPopulation * fabs(sinf(g_gameTime * 2.0f)) * 0.5f;

#ifdef CHEATMENU_ENABLED
    if (g_inputManager->controlEvent(ControlType::ControlScrollSpeedup))
    {
      speed *= 10.0f;
    }
#endif

  return speed;
}

// ****************************************************************************
// Class MineCart
// ****************************************************************************

MineCart::MineCart()
  : m_progress(0.0f)
{
  for (int i = 0; i < 3; ++i)
  {
    m_polygons[i] = false;
    m_primitives[i] = false;
  }
}


// ****************************************************************************
// Class TrackLink
// ****************************************************************************

TrackLink::TrackLink()
  : MineBuilding()
{
  m_type = TypeTrackLink;
  SetShape(g_resource->GetShape("TrackLink.shp"));
}


bool TrackLink::Advance() { return MineBuilding::Advance(); }


// ****************************************************************************
// Class TrackJunction
// ****************************************************************************

TrackJunction::TrackJunction()
  : MineBuilding()
{
  m_type = TypeTrackJunction;
  SetShape(g_resource->GetShape("TrackLink.shp"));
}


void TrackJunction::Initialise(Building* _template)
{
  MineBuilding::Initialise(_template);

  TrackJunction* trackJunction = (TrackJunction*)_template;
  for (int i = 0; i < static_cast<int>(trackJunction->m_trackLinks.size()); ++i)
  {
    int trackLink = trackJunction->m_trackLinks[i];
    m_trackLinks.push_back(trackLink);
  }
}


void TrackJunction::Render(float _predictionTime) { Building::Render(_predictionTime); }


void TrackJunction::RenderLink()
{
#ifdef DEBUG_RENDER_ENABLED
  for (int i = 0; i < static_cast<int>(m_trackLinks.size()); ++i)
  {
    int buildingId = m_trackLinks[i];
    if (buildingId != -1)
    {
      Building* linkBuilding = g_location->GetBuilding(buildingId);
      if (linkBuilding)
      {
        DirectX::XMFLOAT3 start = m_pos;
        start.y += 10.0f;
        DirectX::XMFLOAT3 end = linkBuilding->m_pos;
        end.y += 10.0f;
        RenderArrow(start, end, 6.0f, RGBAColour(255, 0, 255));
      }
    }
  }
#endif
}


void TrackJunction::TriggerCart(MineCart* _cart, float _initValue)
{
  if (static_cast<int>(m_trackLinks.size()) > 0)
  {
    int chosenLink = syncrand() % static_cast<int>(m_trackLinks.size());
    int buildingId = m_trackLinks[chosenLink];
    Building* linkBuilding = g_location->GetBuilding(buildingId);
    if (linkBuilding)
    {
      MineBuilding* mine = (MineBuilding*)linkBuilding;
      mine->TriggerCart(_cart, _initValue);
    }
  }
}


void TrackJunction::SetBuildingLink(int _buildingId) { m_trackLinks.push_back(_buildingId); }


void TrackJunction::Read(TextReader* _in, bool _dynamic)
{
  MineBuilding::Read(_in, _dynamic);

  while (_in->TokenAvailable())
  {
    int trackLink = atoi(_in->GetNextToken());
    m_trackLinks.push_back(trackLink);
  }
}


void TrackJunction::Write(FileWriter* _out)
{
  MineBuilding::Write(_out);

  for (int i = 0; i < static_cast<int>(m_trackLinks.size()); ++i)
  {
    _out->printf("{:<4d}", m_trackLinks[i]);
  }
}


// ****************************************************************************
// Class TrackStart
// ****************************************************************************

TrackStart::TrackStart()
  : MineBuilding(),
    m_reqBuildingId(-1)
{
  m_type = TypeTrackStart;
  SetShape(g_resource->GetShape("TrackLink.shp"));
}


void TrackStart::Initialise(Building* _template)
{
  MineBuilding::Initialise(_template);

  m_reqBuildingId = ((TrackStart*)_template)->m_reqBuildingId;
}


bool TrackStart::Advance()
{
  //
  // Is our required building online yet?
  // Fill carts with primitives when they reach 50%

  GlobalBuilding* globalBuilding = g_globalWorld->GetBuilding(m_reqBuildingId, g_locationId);

  if (globalBuilding && globalBuilding->m_online)
  {
    for (int i = 0; i < static_cast<int>(m_carts.size()); ++i)
    {
      MineCart* cart = m_carts[i];

      if (cart->m_progress > 0.5f)
      {
        bool primitiveFound = false;
        for (int j = 0; j < 3; ++j)
        {
          if (cart->m_primitives[j])
          {
            primitiveFound = true;
            break;
          }
        }

        if (!primitiveFound)
        {
          int primIndex = syncrand() % 3;
          cart->m_primitives[primIndex] = true;
        }
      }
    }
  }

  return MineBuilding::Advance();
}


void TrackStart::RenderAlphas(float _predictionTime)
{
  MineBuilding::RenderAlphas(_predictionTime);

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


void TrackStart::Read(TextReader* _in, bool _dynamic)
{
  MineBuilding::Read(_in, _dynamic);

  m_reqBuildingId = atoi(_in->GetNextToken());
}


void TrackStart::Write(FileWriter* _out)
{
  MineBuilding::Write(_out);

  _out->printf("{:<8d}", m_reqBuildingId);
}


// ****************************************************************************
// Class TrackEnd
// ****************************************************************************

TrackEnd::TrackEnd()
  : MineBuilding(),
    m_reqBuildingId(-1)
{
  m_type = TypeTrackEnd;
  SetShape(g_resource->GetShape("TrackLink.shp"));
}


void TrackEnd::Initialise(Building* _template)
{
  MineBuilding::Initialise(_template);

  m_reqBuildingId = ((TrackEnd*)_template)->m_reqBuildingId;
}


bool TrackEnd::Advance()
{
  //
  // Is our required building online yet?
  // Empty carts of primitives when they reach 50%

  // GlobalBuilding *globalBuilding = g_globalWorld->GetBuilding( m_reqBuildingId, g_locationId );
  Building* building = g_location->GetBuilding(m_reqBuildingId);

  bool online = false;
  if (building->m_type == TypeTrunkPort && ((TrunkPort*)building)->m_openTimer > 0.0f)
    online = true;
  if (building->m_type == TypeYard)
    online = true;
  if (building->m_type == TypeFuelGenerator)
    online = true;

  if (online)
  {
    for (int i = 0; i < static_cast<int>(m_carts.size()); ++i)
    {
      MineCart* cart = m_carts[i];

      if (cart->m_progress > 0.5f)
      {
        for (int j = 0; j < 3; ++j)
        {
          if (cart->m_primitives[j])
          {
            if (building && building->m_type == TypeYard)
            {
              ConstructionYard* yard = (ConstructionYard*)building;
              bool added = yard->AddPrimitive();
              if (added)
                cart->m_primitives[j] = false;
            }
            else
            {
              cart->m_primitives[j] = false;
            }
          }

          if (cart->m_polygons[j])
          {
            cart->m_polygons[j] = false;
          }
        }
      }
    }
  }

  return MineBuilding::Advance();
}


void TrackEnd::RenderAlphas(float _predictionTime)
{
  MineBuilding::RenderAlphas(_predictionTime);

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


void TrackEnd::Read(TextReader* _in, bool _dynamic)
{
  MineBuilding::Read(_in, _dynamic);

  m_reqBuildingId = atoi(_in->GetNextToken());
}


void TrackEnd::Write(FileWriter* _out)
{
  MineBuilding::Write(_out);

  _out->printf("{:<8d}", m_reqBuildingId);
}


// ****************************************************************************
// Class Refinery
// ****************************************************************************

Refinery::Refinery()
  : MineBuilding(),
    m_wheel1(nullptr),
    m_wheel2(nullptr),
    m_wheel3(nullptr),
    m_counter1(nullptr)
{
  m_type = TypeRefinery;
  SetShape(g_resource->GetShape("Refinery.shp"));

  m_wheel1 = m_shape->m_rootFragment->LookupMarker("MarkerWheel01");
  m_wheel2 = m_shape->m_rootFragment->LookupMarker("MarkerWheel02");
  m_wheel3 = m_shape->m_rootFragment->LookupMarker("MarkerWheel03");

  m_counter1 = m_shape->m_rootFragment->LookupMarker("MarkerCounter");
}


char const* Refinery::GetObjectiveCounter()
{
  GlobalBuilding* gb = g_globalWorld->GetBuilding(m_id.GetUniqueId(), g_locationId);
  int numRefined = 0;
  if (gb)
    numRefined = gb->m_link;

  static char result[256];
  sprintf(result, "%s : %d", LANGUAGEPHRASE("objective_refined"), numRefined);
  return result;
}


bool Refinery::Advance()
{
  GlobalBuilding* gb = g_globalWorld->GetBuilding(m_id.GetUniqueId(), g_locationId);

  if (gb && gb->m_link == -1)
  {
    // This occurs the first time
    gb->m_link = 0;
  }

  if (gb && gb->m_link >= 20)
  {
    if (!gb->m_online)
    {
      gb->m_online = true;
      g_globalWorld->EvaluateEvents();
    }
  }

  return MineBuilding::Advance();
}


void Refinery::TriggerCart(MineCart* _cart, float _initValue)
{
  // The refinery MUST be online at this point,
  // otherwise the carts would never have reached us

  bool polygonsFound = false;
  bool primitivesFound = false;

  for (int i = 0; i < 3; ++i)
  {
    if (_cart->m_polygons[i])
      polygonsFound = true;
    if (_cart->m_primitives[i])
      primitivesFound = true;
  }

  if (!primitivesFound && polygonsFound)
  {
    for (int i = 0; i < 3; ++i)
    {
      _cart->m_polygons[i] = false;
    }

    int primIndex = syncrand() % 3;
    _cart->m_primitives[primIndex] = true;

    GlobalBuilding* gb = g_globalWorld->GetBuilding(m_id.GetUniqueId(), g_locationId);

    if (gb)
      gb->m_link++;
  }

  MineBuilding::TriggerCart(_cart, _initValue);
}


void Refinery::Render(float _predictionTime)
{
  MineBuilding::Render(_predictionTime);

  //
  // Render wheels

  DirectX::XMFLOAT4X4 refineryMat;
  DirectX::XMStoreFloat4x4(&refineryMat,
                           BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

  DirectX::XMFLOAT4X4 wheel1Mat = m_wheel1->GetWorldMatrix(refineryMat);
  DirectX::XMFLOAT4X4 wheel2Mat = m_wheel2->GetWorldMatrix(refineryMat);
  DirectX::XMFLOAT4X4 wheel3Mat = m_wheel3->GetWorldMatrix(refineryMat);

  float refinerySpeed = RefinerySpeed();
  float predictedWheelRotate = m_wheelRotate - 3.0f * refinerySpeed * _predictionTime;

  wheel1Mat = RotatedAroundLocalFront(wheel1Mat, predictedWheelRotate);
  wheel2Mat = RotatedAroundLocalFront(wheel2Mat, predictedWheelRotate * -1.0f);
  wheel3Mat = RotatedAroundLocalFront(wheel3Mat, predictedWheelRotate);

  wheel1Mat._11 *= 1.3f;
  wheel1Mat._12 *= 1.3f;
  wheel1Mat._13 *= 1.3f;
  wheel1Mat._21 *= 1.3f;
  wheel1Mat._22 *= 1.3f;
  wheel1Mat._23 *= 1.3f;
  wheel1Mat._31 *= 1.3f;
  wheel1Mat._32 *= 1.3f;
  wheel1Mat._33 *= 1.3f;

  wheel2Mat._11 *= 0.8f;
  wheel2Mat._12 *= 0.8f;
  wheel2Mat._13 *= 0.8f;
  wheel2Mat._21 *= 0.8f;
  wheel2Mat._22 *= 0.8f;
  wheel2Mat._23 *= 0.8f;

  wheel3Mat._11 *= 1.6f;
  wheel3Mat._12 *= 1.6f;
  wheel3Mat._13 *= 1.6f;
  wheel3Mat._21 *= 1.6f;
  wheel3Mat._22 *= 1.6f;
  wheel3Mat._23 *= 1.6f;
  wheel3Mat._31 *= 1.6f;
  wheel3Mat._32 *= 1.6f;
  wheel3Mat._33 *= 1.6f;

  s_wheelShape->Render(_predictionTime, wheel1Mat);
  s_wheelShape->Render(_predictionTime, wheel2Mat);
  s_wheelShape->Render(_predictionTime, wheel3Mat);

  GlobalBuilding* gb = g_globalWorld->GetBuilding(m_id.GetUniqueId(), g_locationId);
  int numRefined = 0;
  if (gb)
    numRefined = gb->m_link;

  DirectX::XMFLOAT4X4 counterMat = m_counter1->GetWorldMatrix(refineryMat);
  glColor4f(0.6f, 0.8f, 0.9f, 1.0f);
  g_gameFont.DrawText3D(DirectX::XMFLOAT3(counterMat._41, counterMat._42, counterMat._43),
                        DirectX::XMFLOAT3(counterMat._31, counterMat._32, counterMat._33),
                        DirectX::XMFLOAT3(counterMat._21, counterMat._22, counterMat._23), 10.0f, "{}", numRefined);
  // The shadow offset: a tenth of a unit forward, a fifth right, a fifth up.
  // operator^ was the cross product, and front x up is the RIGHT vector -- the
  // opposite hand from GetRight()'s up x front, which is why it is spelled out
  // in this order rather than reached for from elsewhere.
  {
    DirectX::XMVECTOR const front = DirectX::XMVectorSet(counterMat._31, counterMat._32, counterMat._33, 0.0f);
    DirectX::XMVECTOR const up = DirectX::XMVectorSet(counterMat._21, counterMat._22, counterMat._23, 0.0f);
    DirectX::XMVECTOR pos = DirectX::XMVectorSet(counterMat._41, counterMat._42, counterMat._43, 0.0f);
    pos = DirectX::XMVectorAdd(pos, DirectX::XMVectorScale(front, 0.1f));
    pos = DirectX::XMVectorAdd(pos, DirectX::XMVectorScale(DirectX::XMVector3Cross(front, up), 0.2f));
    pos = DirectX::XMVectorAdd(pos, DirectX::XMVectorScale(up, 0.2f));
    counterMat._41 = DirectX::XMVectorGetX(pos);
    counterMat._42 = DirectX::XMVectorGetY(pos);
    counterMat._43 = DirectX::XMVectorGetZ(pos);
  }
  g_gameFont.SetRenderShadow(true);
  glColor4f(0.6f, 0.8f, 0.9f, 0.0f);
  g_gameFont.DrawText3D(DirectX::XMFLOAT3(counterMat._41, counterMat._42, counterMat._43),
                        DirectX::XMFLOAT3(counterMat._31, counterMat._32, counterMat._33),
                        DirectX::XMFLOAT3(counterMat._21, counterMat._22, counterMat._23), 10.0f, "{}", numRefined);
  g_gameFont.SetRenderShadow(false);
}


// ****************************************************************************
// Class Mine
// ****************************************************************************

Mine::Mine()
  : MineBuilding(),
    m_wheel1(nullptr),
    m_wheel2(nullptr)
{
  m_type = TypeMine;
  SetShape(g_resource->GetShape("Mine.shp"));

  m_wheel1 = m_shape->m_rootFragment->LookupMarker("MarkerWheel01");
  m_wheel2 = m_shape->m_rootFragment->LookupMarker("MarkerWheel02");
}


void Mine::Render(float _predictionTime)
{
  MineBuilding::Render(_predictionTime);

  //
  // Render wheels

  DirectX::XMFLOAT4X4 mineMat;
  DirectX::XMStoreFloat4x4(&mineMat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

  DirectX::XMFLOAT4X4 wheel1Mat = m_wheel1->GetWorldMatrix(mineMat);
  DirectX::XMFLOAT4X4 wheel2Mat = m_wheel2->GetWorldMatrix(mineMat);

  float refinerySpeed = RefinerySpeed();
  float predictedWheelRotate = m_wheelRotate - 3.0f * refinerySpeed * _predictionTime;

  wheel1Mat = RotatedAroundLocalFront(wheel1Mat, predictedWheelRotate * -1.0f);
  wheel2Mat = RotatedAroundLocalFront(wheel2Mat, predictedWheelRotate);

  wheel1Mat._11 *= 0.5f;
  wheel1Mat._12 *= 0.5f;
  wheel1Mat._13 *= 0.5f;
  wheel1Mat._21 *= 0.5f;
  wheel1Mat._22 *= 0.5f;
  wheel1Mat._23 *= 0.5f;

  wheel2Mat._11 *= 0.9f;
  wheel2Mat._12 *= 0.9f;
  wheel2Mat._13 *= 0.9f;
  wheel2Mat._21 *= 0.9f;
  wheel2Mat._22 *= 0.9f;
  wheel2Mat._23 *= 0.9f;

  s_wheelShape->Render(_predictionTime, wheel1Mat);
  s_wheelShape->Render(_predictionTime, wheel2Mat);
}


void Mine::TriggerCart(MineCart* _cart, float _initValue)
{
  //
  // If there is anything already in here, don't do anything

  bool somethingFound = false;
  for (int i = 0; i < 3; ++i)
  {
    if (_cart->m_primitives[i] || _cart->m_polygons[i])
    {
      somethingFound = true;
      break;
    }
  }


  if (!somethingFound)
  {
    float fractionOccupied = (float)GetNumPortsOccupied() / (float)GetNumPorts();

    if (syncfrand() <= fractionOccupied)
    {
      for (int i = 0; i < 3; ++i)
      {
        _cart->m_primitives[i] = false;
        int cartIndex = syncrand() % 3;
        _cart->m_polygons[cartIndex] = true;
      }
    }
  }

  MineBuilding::TriggerCart(_cart, _initValue);
}
} // namespace Species
