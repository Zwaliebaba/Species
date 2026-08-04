#include "pch.h"
#include "SoundSources.h"

#include <math.h>

#include "MathUtils.h"
#include "Debug.h"
#include "FileWriter.h"
#include "Matrix33.h"
#include "Profiler.h"
#include "Shape.h"
#include "Resource.h"
#include "TextStreamReaders.h"
#include "OglExtensions.h"
#include "DebugRender.h"
#include "Preferences.h"

#include "GameTime.h"
#include "Location.h"
#include "ObstructionGrid.h"
#include "Team.h"
#include "ParticleSystem.h"

#include "ClientToServer.h"

#include "SoundSystem.h"

#include "Building.h"
#include "LaserFence.h"
#include "WorldPointers.h"
#include "AppState.h"

LaserFence::LaserFence()
  : Building(),
    m_nextLaserFenceId(-1),
    m_status(0.0f),
    m_mode(ModeDisabled),
    m_scale(1.0f),
    m_marker1(nullptr),
    m_marker2(nullptr),
    m_sparkTimer(0.0f),
    m_radiusSet(false),
    m_nextToggled(false)
{
  m_type = Building::TypeLaserFence;

  SetShape(g_resource->GetShape("LaserFence.shp"));

  m_marker1 = m_shape->m_rootFragment->LookupMarker("MarkerFence01");
  m_marker2 = m_shape->m_rootFragment->LookupMarker("MarkerFence02");

  ASSERT_TEXT(m_marker1, "MarkerFence01 not found in laserfence.shp");
  ASSERT_TEXT(m_marker2, "MarkerFence02 not found in laserfence.shp");
}


void LaserFence::Initialise(Building* _template)
{
  Building::Initialise(_template);
  DEBUG_ASSERT(_template->m_type == Building::TypeLaserFence);

  m_nextLaserFenceId = ((LaserFence*)_template)->m_nextLaserFenceId;
  m_scale = ((LaserFence*)_template)->m_scale;
  m_mode = ((LaserFence*)_template)->m_mode;

  m_sparkTimer = frand(10.0f);
}

void LaserFence::SetDetail(int _detail)
{
  m_radiusSet = false;
  Building::SetDetail(_detail);
}

void LaserFence::Spark()
{
  Vector3 sparkPos = m_pos;
  sparkPos.y += frand(m_scale * 50.0f);

  LaserFence* nextFence = (LaserFence*)g_location->GetBuilding(m_nextLaserFenceId);

  int numSparks = 5.0f + frand(5.0f);
  for (int i = 0; i < numSparks; ++i)
  {
    Vector3 particleVel;
    if (nextFence)
      particleVel = (AsLegacy(m_pos) - AsLegacy(nextFence->m_pos)) ^ g_upVector;
    else
      particleVel = Vector3(sfrand(10.0f), sfrand(5.0f), sfrand(10.0f));

    particleVel.SetLength(40.0f + frand(20.0f));
    particleVel += Vector3(frand() * 20.0f, sfrand() * 20.0f, sfrand() * 20.0f);
    float size = 25.0f + frand(25.0f);
    g_particleSystem->CreateParticle(sparkPos, particleVel, Particle::TypeSpark, size);
  }

  g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), "Spark");
}


bool LaserFence::Advance()
{
  if (!m_radiusSet)
  {
    Building* building = g_location->GetBuilding(m_nextLaserFenceId);
    if (building)
    {
      m_centrePos = (AsLegacy(building->m_pos) + AsLegacy(m_pos)) / 2.0f;
      m_radius = (AsLegacy(building->m_pos) - AsLegacy(m_pos)).Mag() / 2.0f + m_radius;
    }
    m_radiusSet = true;
  }

  if (m_mode != ModeDisabled)
  {
    m_sparkTimer -= SERVER_ADVANCE_PERIOD;
    if (m_sparkTimer < 0.0f)
    {
      m_sparkTimer = 8.0f + syncfrand(4.0f);
      Spark();
    }
  }

  switch (m_mode)
  {
  case ModeEnabling:
    m_status += LASERFENCE_RAISESPEED * SERVER_ADVANCE_PERIOD;
    if (m_status >= 0.5f && m_nextLaserFenceId != -1 && !m_nextToggled)
    {
      LaserFence* nextFence = (LaserFence*)g_location->GetBuilding(m_nextLaserFenceId);
      if (nextFence)
      {
        nextFence->Enable();
        m_nextToggled = true;
      }
    }
    if (m_status >= 1.0f)
    {
      m_status = 1.0f;
      m_mode = ModeEnabled;
      if (m_nextLaserFenceId == -1)
      {
        g_location->m_obstructionGrid->CalculateAll();
      }
    }
    break;

  case ModeDisabling:
    if (m_status <= 0.5f && m_nextLaserFenceId != -1 && !m_nextToggled)
    {
      LaserFence* nextFence = (LaserFence*)g_location->GetBuilding(m_nextLaserFenceId);
      if (nextFence)
      {
        nextFence->Disable();
        m_nextToggled = true;
      }
    }
    m_status -= LASERFENCE_RAISESPEED * SERVER_ADVANCE_PERIOD;
    if (m_status <= 0.0f)
    {
      m_status = 0.0f;
      m_mode = ModeDisabled;
      if (m_nextLaserFenceId == -1)
      {
        g_location->m_obstructionGrid->CalculateAll();
      }
    }
    break;

  case ModeEnabled:
  {
    if (m_status < 1.0f)
    {
      m_status = 1.0f;
      if (m_nextLaserFenceId == -1)
      {
        g_location->m_obstructionGrid->CalculateAll();
      }
    }
    break;
  }
  case ModeDisabled:
    m_status = 0.0f;
    break;
  }

  return Building::Advance();
}


void LaserFence::Render(float predictionTime)
{
  Matrix34 mat(m_front, g_upVector, m_pos);
  mat.f *= m_scale;
  mat.u *= m_scale;
  mat.r *= m_scale;

  glEnable(GL_NORMALIZE);
  m_shape->Render(predictionTime, mat);
  glDisable(GL_NORMALIZE);
}


float LaserFence::GetFenceFullHeight()
{
  Matrix34 mat(m_front, g_upVector, m_pos);
  mat.u *= m_scale;
  mat.r *= m_scale;
  mat.f *= m_scale;
  Vector3 marker1 = m_marker1->GetWorldMatrix(mat).pos;
  Vector3 marker2 = m_marker2->GetWorldMatrix(mat).pos;

  return (marker2.y - marker1.y);
}


bool LaserFence::IsInView()
{
  return Building::IsInView();

  // No need to do the usual midpoint / radius calculation here
  // as it has already been done in LaserFence::Advance.
  // Our midpoint and radius values are frigged in order to
  // correctly fill the obstruction grid.
}


bool LaserFence::PerformDepthSort(DirectX::XMFLOAT3& _centrePos)
{
  if (m_mode == ModeDisabled)
    return false;


  if (m_nextLaserFenceId != -1)
  {
    _centrePos = m_centrePos;
    return true;
  }


  return false;
  ;
}


void LaserFence::RenderAlphas(float predictionTime)
{
  Building::RenderAlphas(predictionTime);

  if (m_mode != ModeDisabled && m_mode != ModeNeverOn)
  {
    //
    // Draw the laser fence connecting to the next laser fence

    if (m_nextLaserFenceId != -1)
    {
      Building* nextFence = g_location->GetBuilding(m_nextLaserFenceId);
      if (!nextFence || nextFence->m_type != Building::TypeLaserFence)
      {
        m_nextLaserFenceId = -1;
        return;
      }
      LaserFence* nextLaserFence = (LaserFence*)nextFence;

      int buildingDetail = g_prefsManager->GetInt("RenderBuildingDetail", 1);

      glDisable(GL_CULL_FACE);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDepthMask(false);

      unsigned char alpha = 150;
      if (m_id.GetTeamId() == 255)
      {
        glColor4ub(100, 100, 100, alpha);
      }
      else
      {
        RGBAColour* colour = &g_location->m_teams[m_id.GetTeamId()].m_colour;
        glColor4ub(colour->r, colour->g, colour->b, alpha);
      }

      float ourFenceMaxHeight = GetFenceFullHeight();
      float theirFenceMaxHeight = nextLaserFence->GetFenceFullHeight();
      float predictedStatus = m_status;
      if (m_mode == ModeDisabling)
        predictedStatus -= LASERFENCE_RAISESPEED * predictionTime;
      if (m_mode == ModeEnabling)
        predictedStatus += LASERFENCE_RAISESPEED * predictionTime;
      if (g_editing)
        predictedStatus = 1.0f;

      float ourFenceHeight = ourFenceMaxHeight * predictedStatus;
      float theirFenceHeight = theirFenceMaxHeight * predictedStatus;
      float distance = (AsLegacy(m_pos) - AsLegacy(nextFence->m_pos)).Mag();
      float dx = distance / (ourFenceMaxHeight * 2.0f);
      float dz = ourFenceHeight / ourFenceMaxHeight;
      float timeOff = g_gameTime / 15.0f;

      if (buildingDetail < 3)
      {
        glEnable(GL_TEXTURE_2D);

        gglActiveTextureARB(GL_TEXTURE0_ARB);
        glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/LaserFence.bmp"));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
        glTexEnvf(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_REPLACE);
        glEnable(GL_TEXTURE_2D);

        gglActiveTextureARB(GL_TEXTURE1_ARB);
        glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/LaserFence2.bmp"));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
        glTexEnvf(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE);
        glEnable(GL_TEXTURE_2D);


        glBegin(GL_QUADS);
        gglMultiTexCoord2fARB(GL_TEXTURE0_ARB, timeOff, 0.0f);
        gglMultiTexCoord2fARB(GL_TEXTURE1_ARB, 0, 0);
        glVertex3fv((AsLegacy(m_pos) - Vector3(0, ourFenceHeight / 3, 0)).GetData());

        gglMultiTexCoord2fARB(GL_TEXTURE0_ARB, timeOff, dz);
        gglMultiTexCoord2fARB(GL_TEXTURE1_ARB, 0, 1);
        glVertex3fv((AsLegacy(m_pos) + Vector3(0, ourFenceHeight, 0)).GetData());

        gglMultiTexCoord2fARB(GL_TEXTURE0_ARB, timeOff + dx, dz);
        gglMultiTexCoord2fARB(GL_TEXTURE1_ARB, 1, 1);
        glVertex3fv((AsLegacy(nextFence->m_pos) + Vector3(0, theirFenceHeight, 0)).GetData());

        gglMultiTexCoord2fARB(GL_TEXTURE0_ARB, timeOff + dx, 0.0f);
        gglMultiTexCoord2fARB(GL_TEXTURE1_ARB, 1, 0);
        glVertex3fv((AsLegacy(nextFence->m_pos) - Vector3(0, theirFenceHeight / 3, 0)).GetData());
        glEnd();

        gglActiveTextureARB(GL_TEXTURE1_ARB);
        glDisable(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

        gglActiveTextureARB(GL_TEXTURE0_ARB);
        glDisable(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
      }

      //
      // Blend another poly over the top for burn effect

      glEnable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/LaserFence2.bmp"));
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE);

      glBegin(GL_QUADS);
      glTexCoord2f(0, 0);
      glVertex3fv((AsLegacy(m_pos) - Vector3(0, ourFenceHeight / 3, 0)).GetData());

      glTexCoord2f(0, 1);
      glVertex3fv((AsLegacy(m_pos) + Vector3(0, ourFenceHeight, 0)).GetData());

      glTexCoord2f(1, 1);
      glVertex3fv((AsLegacy(nextFence->m_pos) + Vector3(0, theirFenceHeight, 0)).GetData());

      glTexCoord2f(1, 0);
      glVertex3fv((AsLegacy(nextFence->m_pos) - Vector3(0, theirFenceHeight / 3, 0)).GetData());
      glEnd();

      glDisable(GL_TEXTURE_2D);


      //
      // Gimme a line across the top
      glLineWidth(2.0f);
      glEnable(GL_LINE_SMOOTH);

      glBegin(GL_LINES);
      glVertex3fv((AsLegacy(m_pos) + Vector3(0, ourFenceHeight, 0)).GetData());
      glVertex3fv((AsLegacy(nextFence->m_pos) + Vector3(0, theirFenceHeight, 0)).GetData());
      glEnd();

      glDepthMask(true);
      glDisable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glEnable(GL_CULL_FACE);
      glDisable(GL_LINE_SMOOTH);
    }
  }
}


void LaserFence::RenderLights() {}


void LaserFence::Enable()
{
  if (m_mode != ModeEnabled && m_mode != ModeNeverOn)
  {
    m_mode = ModeEnabling;
  }

  if (m_mode == ModeNeverOn)
  {
    LaserFence* nextFence = (LaserFence*)g_location->GetBuilding(m_nextLaserFenceId);
    if (nextFence)
      nextFence->Toggle();
  }

  m_nextToggled = false;
}


void LaserFence::Disable()
{
  if (m_mode != ModeDisabled && m_mode != ModeNeverOn)
  {
    m_mode = ModeDisabling;
  }

  if (m_mode == ModeNeverOn)
  {
    LaserFence* nextFence = (LaserFence*)g_location->GetBuilding(m_nextLaserFenceId);
    if (nextFence)
      nextFence->Toggle();
  }

  m_nextToggled = false;
}


void LaserFence::Toggle()
{
  switch (m_mode)
  {
  case ModeDisabled:
  case ModeDisabling:
    Enable();
    break;

  case ModeEnabled:
  case ModeEnabling:
    Disable();
    break;
  }

  m_nextToggled = false;
}


bool LaserFence::IsEnabled() { return (m_mode == ModeEnabled) || (m_mode == ModeEnabling); }


void LaserFence::Read(TextReader* _in, bool _dynamic)
{
  Building::Read(_in, _dynamic);

  char* word = _in->GetNextToken();
  m_nextLaserFenceId = atoi(word);

  if (_in->TokenAvailable())
    m_scale = atof(_in->GetNextToken());
  if (_in->TokenAvailable())
    m_mode = atof(_in->GetNextToken());
}


void LaserFence::Write(FileWriter* _out)
{
  Building::Write(_out);

  _out->printf("%-8d", m_nextLaserFenceId);
  _out->printf("%-6.2f", m_scale);
  _out->printf("%d", m_mode);
}


int LaserFence::GetBuildingLink() { return m_nextLaserFenceId; }


void LaserFence::SetBuildingLink(int _buildingId) { m_nextLaserFenceId = _buildingId; }


void LaserFence::Electrocute(Vector3 const& _pos) { g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), "Electrocute"); }


bool LaserFence::DoesSphereHit(DirectX::XMFLOAT3 const& _pos, float _radius)
{
  if (m_mode == ModeDisabled || g_editing)
  {
    SpherePackage sphere(_pos, _radius);
    Matrix34 transform(m_front, m_up, m_pos);
    transform.f *= m_scale;
    transform.u *= m_scale;
    transform.r *= m_scale;
    return m_shape->SphereHit(&sphere, transform);
  }

  //
  // Fake this for now using 2 ray hits
  // One running across the sphere in the x direction, the other in the z direction

  Vector3 ray1Start = _pos - Vector3(_radius, 0, 0);
  Vector3 ray1Dir(1, 0, 0);
  float ray1Len = _radius * 2;
  if (DoesRayHit(ray1Start, ray1Dir, ray1Len))
    return true;

  Vector3 ray2Start = _pos - Vector3(0, 0, _radius);
  Vector3 ray2Dir(0, 0, 1);
  float ray2Len = _radius * 2;
  if (DoesRayHit(ray2Start, ray2Dir, ray2Len))
    return true;

  //
  // Fence is ok
  // Test against the shape

  SpherePackage sphere(_pos, _radius);
  Matrix34 transform(m_front, m_up, m_pos);
  transform.f *= m_scale;
  transform.u *= m_scale;
  transform.r *= m_scale;
  return m_shape->SphereHit(&sphere, transform);
}


bool LaserFence::DoesRayHit(DirectX::XMFLOAT3 const& _rayStart, DirectX::XMFLOAT3 const& _rayDir, float _rayLen, DirectX::XMFLOAT3* _pos,
                            DirectX::XMFLOAT3* _norm)
{
  if (m_mode == ModeDisabled || g_editing)
  {
    RayPackage ray(_rayStart, _rayDir, _rayLen);
    Matrix34 transform(m_front, m_up, m_pos);
    transform.f *= m_scale;
    transform.u *= m_scale;
    transform.r *= m_scale;
    return m_shape->RayHit(&ray, transform, true);
  }

  if (m_nextLaserFenceId != -1 && m_status > 0.0f)
  {
    Building* nextFence = g_location->GetBuilding(m_nextLaserFenceId);
    float maxHeight = GetFenceFullHeight();
    float fenceHeight = maxHeight * m_status;
    Vector3 pos1 = AsLegacy(m_pos) - Vector3(0, fenceHeight / 3, 0);
    Vector3 pos2 = AsLegacy(m_pos) + Vector3(0, fenceHeight, 0);
    Vector3 pos3 = AsLegacy(nextFence->m_pos) - Vector3(0, fenceHeight / 3, 0);
    Vector3 pos4 = AsLegacy(nextFence->m_pos) + Vector3(0, fenceHeight, 0);

    bool hitTri1 = false;
    bool hitTri2 = false;

    hitTri1 = RayTriIntersection(_rayStart, _rayDir, pos1, pos2, pos4, _rayLen, _pos);
    if (!hitTri1)
      hitTri2 = RayTriIntersection(_rayStart, _rayDir, pos4, pos3, pos1, _rayLen, _pos);

    if (hitTri1 || hitTri2)
    {
      if (_norm)
      {
        Vector3 v1 = pos1 - pos2;
        Vector3 v2 = pos1 - pos3;
        *_norm = (v2 ^ v1).Normalise();
      }
      return true;
    }
  }

  // return Building::DoesRayHit( _rayStart, _rayDir, _rayLen, _pos, _norm );

  RayPackage ray(_rayStart, _rayDir, _rayLen);
  Matrix34 transform(m_front, m_up, m_pos);
  transform.f *= m_scale;
  transform.u *= m_scale;
  transform.r *= m_scale;
  return m_shape->RayHit(&ray, transform, true);
}


bool LaserFence::DoesShapeHit(Shape* _shape, DirectX::XMFLOAT4X4 _transform)
{
  return DoesSphereHit(_transform.pos, _shape->m_rootFragment->m_radius);
}


void LaserFence::ListSoundEvents(std::vector<const char*>* _list)
{
  Building::ListSoundEvents(_list);

  _list->push_back("Spark");
  _list->push_back("Electrocute");
}

Vector3 LaserFence::GetTopPosition()
{
  Matrix34 mat(m_front, g_upVector, m_pos);
  mat.u *= m_scale;
  mat.r *= m_scale;
  mat.f *= m_scale;
  return m_marker2->GetWorldMatrix(mat).pos;
}