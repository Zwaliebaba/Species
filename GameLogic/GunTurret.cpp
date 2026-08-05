#include "pch.h"
#include "GlVertex.h"
#include "SoundSources.h"

#include <math.h>

#include "Resource.h"
#include "Shape.h"
#include "MathUtils.h"
#include "TextRenderer.h"
#include "DebugRender.h"

#include "Input.h"
#include "InputTypes.h"

#include "ProtocolLimits.h"
#include "Location.h"
#include "Team.h"
#include "EntityGrid.h"
#include "GameTime.h"
#include "GlobalWorld.h"
#include "Explosion.h"

#include "GunTurret.h"
#include "Citizen.h"
#include "Ai.h"

#include "SoundSystem.h"
#include "WorldPointers.h"
#include "AppState.h"


GunTurret::GunTurret()
  : Building(),
    m_retargetTimer(0.0f),
    m_fireTimer(0.0f),
    m_nextBarrel(0),
    m_turret(nullptr),
    m_ownershipTimer(0.0f),
    m_health(60.0f),
    m_targetCreated(false),
    m_aiTargetCreated(false)
{
  SetShape(g_resource->GetShape("BattleCannonBase.shp"));
  m_type = TypeGunTurret;

  m_turret = g_resource->GetShape("BattleCannonTurret.shp");
  m_barrel = g_resource->GetShape("BattleCannonBarrel.shp");

  m_barrelMount = m_turret->m_rootFragment->LookupMarker("MarkerBarrel");

  for (int i = 0; i < GUNTURRET_NUMBARRELS; ++i)
  {
    char name[64];
    sprintf(name, "MarkerBarrelEnd0%d", i + 1);
    m_barrelEnd[i] = m_barrel->m_rootFragment->LookupMarker(name);
  }

  for (int i = 0; i < GUNTURRET_NUMSTATUSMARKERS; ++i)
  {
    char name[64];
    sprintf(name, "MarkerStatus0%d", i + 1);
    m_statusMarkers[i] = m_shape->m_rootFragment->LookupMarker(name);
  }
}


void GunTurret::Initialise(Building* _template)
{
  _template->m_up = g_location->m_landscape.m_normalMap->GetValue(_template->m_pos.x, _template->m_pos.z);
  DirectX::XMStoreFloat3(&_template->m_front, DirectX::XMVector3Cross(DirectX::g_XMIdentityR0, DirectX::XMLoadFloat3(&_template->m_up)));

  Building::Initialise(_template);

  m_turretFront = m_front;
  m_barrelUp = g_upVector;
  m_centrePos = m_pos;
  m_radius = 30.0f;

  SearchForRandomPos();
}


void GunTurret::SetDetail(int _detail)
{
  Building::SetDetail(_detail);

  m_centrePos = m_pos;
  m_radius = 30.0f;
}


void GunTurret::Damage(float _damage)
{
  m_health += _damage;

  if (m_health <= 0.0f)
  {
    ExplodeBody();
  }
}


void GunTurret::ExplodeBody()
{
  DirectX::XMFLOAT4X4 turretPos;
  DirectX::XMStoreFloat4x4(&turretPos,
                           BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_turretFront), DirectX::XMLoadFloat3(&m_up), DirectX::XMLoadFloat3(&m_pos)));

  // ShapeMarker::GetWorldMatrix still returns Matrix34 -- T10's seam -- and its
  // rows are Vector3. XMLoadFloat3 needs an XMFLOAT3*, and &row is a Vector3*;
  // the seam's conversion is to a REFERENCE, so it does not apply to taking an
  // address. Copy-initialising these two locals is what runs it.
  Matrix34 const barrelMount = m_barrelMount->GetWorldMatrix(turretPos);
  DirectX::XMFLOAT3 const barrelMountFront = barrelMount.f;
  DirectX::XMFLOAT3 const barrelMountPos = barrelMount.pos;

  DirectX::XMFLOAT4X4 barrelMat;
  DirectX::XMStoreFloat4x4(
    &barrelMat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&barrelMountFront), DirectX::XMLoadFloat3(&m_up), DirectX::XMLoadFloat3(&barrelMountPos)));

  g_explosionManager.AddExplosion(m_turret, turretPos);
  g_explosionManager.AddExplosion(m_barrel, barrelMat);
}


bool GunTurret::SearchForTargets()
{
  WorldObjectId previousTarget = m_targetId;
  m_targetId.SetInvalid();

  if (m_id.GetTeamId() != 255)
  {
    m_targetId = g_location->m_entityGrid->GetBestEnemy(m_pos.x, m_pos.z, GUNTURRET_MINRANGE, GUNTURRET_MAXRANGE, m_id.GetTeamId());
  }

  Entity* entity = g_location->GetEntity(m_targetId);

  if (entity && m_targetId != previousTarget)
  {
    g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), "TargetSighted");
  }

  return (m_targetId.IsValid());
}


void GunTurret::SearchForRandomPos()
{
  float angle = syncfrand(2.0f * M_PI);
  float distance = 200.0f + syncfrand(200.0f);
  float height = syncfrand(100.0f);

  m_target = DirectX::XMFLOAT3(m_pos.x + cosf(angle) * distance, m_pos.y + height, m_pos.z + sinf(angle) * distance);
}


void GunTurret::RecalculateOwnership()
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
    if (teamCount[i] > 0 && winningTeam == -1)
    {
      winningTeam = i;
    }
    else if (winningTeam != -1 && teamCount[i] > 0 && teamCount[i] > teamCount[winningTeam])
    {
      winningTeam = i;
    }
  }

  if (winningTeam == -1)
  {
    SetTeamId(255);
  }
  else if (winningTeam == 0)
  {
    // Green own the building, but set to yellow so the player
    // can control the building
    SetTeamId(2);
  }
  else
  {
    // Red own the building
    SetTeamId(winningTeam);
  }
}


void GunTurret::PrimaryFire()
{
  bool fired = false;

  for (int i = 0; i < 2; ++i)
  {
    if (GetPortOccupant(m_nextBarrel).IsValid())
    {
      DirectX::XMFLOAT4X4 turretMat;
      DirectX::XMStoreFloat4x4(&turretMat,
                               BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_turretFront), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

      // ShapeMarker::GetWorldMatrix still returns Matrix34 -- T10's seam, whose
      // rows are Vector3; see ExplodeBody for why these copy through locals.
      Matrix34 const barrelMount = m_barrelMount->GetWorldMatrix(turretMat);
      DirectX::XMFLOAT3 const markerFront = barrelMount.f;
      DirectX::XMFLOAT3 const markerPos = barrelMount.pos;

      DirectX::XMVECTOR const barrelUp = DirectX::XMLoadFloat3(&m_barrelUp);
      DirectX::XMVECTOR const barrelRight = DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&markerFront), barrelUp);
      DirectX::XMVECTOR const barrelMountFront = DirectX::XMVector3Cross(barrelUp, barrelRight);

      DirectX::XMFLOAT4X4 barrelMountMat;
      DirectX::XMStoreFloat4x4(&barrelMountMat, BasisFromFrontAndUp(barrelMountFront, barrelUp, DirectX::XMLoadFloat3(&markerPos)));
      Matrix34 const barrelMat = m_barrelEnd[m_nextBarrel]->GetWorldMatrix(barrelMountMat);

      DirectX::XMFLOAT3 const shellPos = barrelMat.pos;
      DirectX::XMFLOAT3 shellVel;
      DirectX::XMStoreFloat3(&shellVel, DirectX::XMVectorScale(DirectX::XMVector3Normalize(barrelMountFront), 500.0f));


      g_location->FireTurretShell(shellPos, shellVel);
      fired = true;
    }

    ++m_nextBarrel;
    if (m_nextBarrel >= GUNTURRET_NUMBARRELS)
    {
      m_nextBarrel = 0;
    }
  }

  if (fired)
  {
    g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), "FireShell");
  }
}


bool GunTurret::Advance()
{
  if (m_health <= 0.0f)
    return true;

  //
  // Create an AI target on top of us,
  // to get Citizens to fight hard for control

  if (!m_aiTargetCreated)
  {
    Building* aiTarget = Building::CreateBuilding(TypeAITarget);
    aiTarget->m_pos = m_pos;
    aiTarget->m_front = m_front;
    g_location->m_buildings.PutData(aiTarget);
    int uniqueId = g_globalWorld->GenerateBuildingId();
    aiTarget->m_id.SetUniqueId(uniqueId);
    m_aiTargetCreated = true;
  }

  //
  // Create a GunTurret target that citizens can run from

  if (!m_targetCreated)
  {
    GunTurretTarget* target = new GunTurretTarget(m_id.GetUniqueId());
    int index = g_location->m_effects.PutData(target);
    target->m_id.Set(m_id.GetTeamId(), UNIT_EFFECTS, index, -1);
    target->m_id.GenerateUniqueId();
    m_targetCreated = true;
  }


  //
  // Time to recalculate ownership?

  m_ownershipTimer -= SERVER_ADVANCE_PERIOD;
  if (m_ownershipTimer <= 0.0f)
  {
    RecalculateOwnership();
    m_ownershipTimer = GUNTURRET_OWNERSHIPTIMER;
  }


  Team* team = g_location->GetMyTeam();
  bool underPlayerControl = (team && team->m_currentBuildingId == m_id.GetUniqueId());


  //
  // Look for a new target

  bool primaryFire = false;
  bool secondaryFire = false;

  if (underPlayerControl)
  {
    if (m_id.GetTeamId() != 2)
    {
      // Player has lost control of the building
      team->SelectUnit(-1, -1, -1);
      g_camera->RequestMode(CameraAccess::ModeFreeMovement);
      return Building::Advance();
    }
    m_target = g_userInput->GetMousePos3d();

    primaryFire = g_inputManager->controlEvent(ControlUnitPrimaryFireTarget) || g_inputManager->controlEvent(ControlUnitStartSecondaryFireDirected);
    secondaryFire = g_inputManager->controlEvent(ControlUnitSecondaryFireTarget);
  }
  else
  {
    m_retargetTimer -= SERVER_ADVANCE_PERIOD;
    if (m_retargetTimer <= 0.0f)
    {
      bool targetFound = SearchForTargets();
      if (!targetFound && m_id.GetTeamId() != 255)
        SearchForRandomPos();
      m_retargetTimer = GUNTURRET_RETARGETTIMER;
    }

    if (m_targetId.IsValid())
    {
      // Check our target is still alive
      Entity* target = g_location->GetEntity(m_targetId);
      if (!target)
      {
        m_targetId.SetInvalid();
        return Building::Advance();
      }
      m_target = DirectX::XMFLOAT3(target->m_pos.x + sinf(g_gameTime * 3) * 20, target->m_pos.y + fabs(sinf(g_gameTime * 2) * 10),
                                   target->m_pos.z + cosf(g_gameTime * 3) * 20);

      // HorizontalAndNormalise: flatten to the XZ plane, then normalise.
      DirectX::XMVECTOR const targetFront = DirectX::XMVector3Normalize(
        DirectX::XMVectorSetY(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_target), DirectX::XMLoadFloat3(&m_pos)), 0.0f));
      float angle = acosf(DirectX::XMVectorGetX(DirectX::XMVector3Dot(targetFront, DirectX::XMLoadFloat3(&m_turretFront))));

      primaryFire = (angle < 0.25f);
    }
  }


  DirectX::XMVECTOR const turretPosVec = DirectX::XMLoadFloat3(&m_pos);
  DirectX::XMVECTOR const toTargetFull = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_target), turretPosVec);
  float distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(toTargetFull));
  if (distance < 75.0f)
  {
    // SetLength, whose zero-length fallback left (75, 0, 0). Reproduced rather
    // than taken natively: distance can be exactly zero here, and the native
    // normalise would store QNaN into m_target and never recover.
    DirectX::XMVECTOR const direction =
      NearlyEquals(distance, 0.0f) ? DirectX::g_XMIdentityR0 : DirectX::XMVectorScale(toTargetFull, 1.0f / distance);
    DirectX::XMStoreFloat3(&m_target, DirectX::XMVectorMultiplyAdd(direction, DirectX::XMVectorReplicate(75.0f), turretPosVec));
  }


  //
  // Face our target

  DirectX::XMFLOAT4X4 turretPos;
  DirectX::XMStoreFloat4x4(&turretPos,
                           BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_turretFront), DirectX::XMLoadFloat3(&m_up), DirectX::XMLoadFloat3(&m_pos)));

  // ShapeMarker::GetWorldMatrix still returns Matrix34 -- T10's seam.
  DirectX::XMFLOAT3 const barrelPosStore = m_barrelMount->GetWorldMatrix(turretPos).pos;
  DirectX::XMVECTOR const barrelPos = DirectX::XMLoadFloat3(&barrelPosStore);

  DirectX::XMVECTOR const targetVec = DirectX::XMLoadFloat3(&m_target);
  DirectX::XMVECTOR turretFront = DirectX::XMLoadFloat3(&m_turretFront);

  // HorizontalAndNormalise: flatten to the XZ plane, then normalise.
  DirectX::XMVECTOR toTarget = DirectX::XMVector3Normalize(DirectX::XMVectorSetY(DirectX::XMVectorSubtract(targetVec, barrelPos), 0.0f));
  float angle = acosf(DirectX::XMVectorGetX(DirectX::XMVector3Dot(toTarget, turretFront)));

  // SetLength followed by RotateAround, and the zero-length fallback here is
  // load-bearing rather than cosmetic. When the turret points exactly away from
  // its target the cross product is zero and SetLength left (angle * 0.2, 0, 0)
  // -- a real rotation about world X, which is what breaks the deadlock and
  // lets the turret come round. Normalising natively would give QNaN, the 1e-8
  // guard below would then skip the rotation, and the turret would stay aimed
  // backwards for the rest of the game.
  DirectX::XMVECTOR const rotationAxis = DirectX::XMVector3Cross(turretFront, toTarget);
  float const axisLength = DirectX::XMVectorGetX(DirectX::XMVector3Length(rotationAxis));
  DirectX::XMVECTOR const rotation = NearlyEquals(axisLength, 0.0f) ? DirectX::XMVectorSet(angle * 0.2f, 0.0f, 0.0f, 0.0f)
                                                                    : DirectX::XMVectorScale(rotationAxis, (angle * 0.2f) / axisLength);

  // RotateAround's angle is the vector's magnitude, with the same 1e-8 guard.
  float const rotationLengthSquared = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(rotation));
  if (rotationLengthSquared >= 1e-8f)
  {
    float const spin = sqrtf(rotationLengthSquared);
    turretFront = DirectX::XMVector3Transform(turretFront, DirectX::XMMatrixRotationAxis(DirectX::XMVectorScale(rotation, 1.0f / spin), spin));
  }
  DirectX::XMStoreFloat3(&m_turretFront, DirectX::XMVector3Normalize(turretFront));

  toTarget = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(targetVec, barrelPos));
  DirectX::XMVECTOR const barrelRight = DirectX::XMVector3Cross(toTarget, DirectX::g_XMIdentityR1);
  DirectX::XMVECTOR const targetBarrelUp = DirectX::XMVector3Cross(toTarget, barrelRight);
  float factor = SERVER_ADVANCE_PERIOD * 2.0f;
  DirectX::XMStoreFloat3(&m_barrelUp, DirectX::XMVectorLerp(DirectX::XMLoadFloat3(&m_barrelUp), targetBarrelUp, factor));

  //
  // Open fire

  if (primaryFire)
    PrimaryFire();

  return Building::Advance();
}


DirectX::XMFLOAT3 GunTurret::GetTarget() { return m_target; }


bool GunTurret::IsInView()
{
  Team* team = g_location->GetMyTeam();
  bool underPlayerControl = (team && team->m_currentBuildingId == m_id.GetUniqueId());

  if (underPlayerControl)
  {
    return true;
  }
  else
  {
    return Building::IsInView();
  }
}


void GunTurret::Render(float _predictionTime)
{
  if (g_editing)
  {
    m_turretFront = m_front;
  }

  DirectX::XMFLOAT4X4 turretPos;
  DirectX::XMStoreFloat4x4(&turretPos,
                           BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_turretFront), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));
  m_turret->Render(_predictionTime, turretPos);

  // ShapeMarker::GetWorldMatrix still returns Matrix34 -- T10's seam, whose
  // rows are Vector3; see ExplodeBody for why these copy through locals.
  Matrix34 const barrelMount = m_barrelMount->GetWorldMatrix(turretPos);
  DirectX::XMFLOAT3 const markerFront = barrelMount.f;
  DirectX::XMFLOAT3 const markerPos = barrelMount.pos;

  DirectX::XMVECTOR const barrelUp = DirectX::XMLoadFloat3(&m_barrelUp);
  DirectX::XMVECTOR const barrelRight = DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&markerFront), barrelUp);
  DirectX::XMVECTOR const barrelFront = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(barrelUp, barrelRight));

  DirectX::XMFLOAT4X4 barrelMat;
  DirectX::XMStoreFloat4x4(&barrelMat, BasisFromFrontAndUp(barrelFront, barrelUp, DirectX::XMLoadFloat3(&markerPos)));
  m_barrel->Render(_predictionTime, barrelMat);

  // RenderArrow( barrelPos, barrelPos + barrelFront * 1000.0f, 1.0f );

  //
  // Render targetting crosshair

  /*
      Team *team = g_location->GetMyTeam();
      bool underPlayerControl = ( team && team->m_currentBuildingId == m_id.GetUniqueId() );

      if( underPlayerControl )
      {
          Vector3 targetPos = barrelPos + Vector3(0,10,0) + barrelFront.SetLength( 300.0f );
          float size = 20.0f;
          Vector3 camUp = g_camera->GetUp();
          Vector3 camRight = g_camera->GetRight();
          targetPos += camUp * 5.0f;

          glBindTexture( GL_TEXTURE_2D, g_resource->GetTexture( "Icons/MouseMissileTarget.bmp" ) );
          glEnable( GL_TEXTURE_2D );
          glDisable( GL_CULL_FACE );
          glBlendFunc( GL_SRC_ALPHA, GL_ONE );
          glEnable( GL_BLEND );
          glDepthMask( false );
          glDisable( GL_DEPTH_TEST );

          glBegin( GL_QUADS );
              glTexCoord2i(0,0);      glVertex3fv( (targetPos - camRight * size - camUp * size).GetData() );
              glTexCoord2i(1,0);      glVertex3fv( (targetPos + camRight * size - camUp * size).GetData() );
              glTexCoord2i(1,1);      glVertex3fv( (targetPos + camRight * size + camUp * size).GetData() );
              glTexCoord2i(0,1);      glVertex3fv( (targetPos - camRight * size + camUp * size).GetData() );
          glEnd();

          glEnable( GL_DEPTH_TEST );
          glDepthMask( true );
          glDisable( GL_BLEND );
          glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
          glEnable( GL_CULL_FACE );
          glDisable( GL_TEXTURE_2D );
      }*/
}


void GunTurret::RenderPorts()
{
  glDisable(GL_CULL_FACE);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Starburst.bmp"));
  glDepthMask(false);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);

  for (int i = 0; i < GetNumPorts(); ++i)
  {
    DirectX::XMFLOAT4X4 rootMat = GetWorldMatrix();

    // ShapeMarker::GetWorldMatrix still returns Matrix34 -- T10's seam.
    DirectX::XMFLOAT3 const statusPos = m_statusMarkers[i]->GetWorldMatrix(rootMat).pos;

    //
    // Render the status light

    float size = 6.0f;
    DirectX::XMFLOAT3 const camRightStore = g_camera->GetRight();
    DirectX::XMFLOAT3 const camUpStore = g_camera->GetUp();
    DirectX::XMVECTOR const camR = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&camRightStore), size);
    DirectX::XMVECTOR const camU = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&camUpStore), size);

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
  }

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_BLEND);
  glDepthMask(true);
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_CULL_FACE);
}

bool GunTurret::DoesRayHit(DirectX::XMFLOAT3 const& _rayStart, DirectX::XMFLOAT3 const& _rayDir, float _rayLen, DirectX::XMFLOAT3* _pos,
                           DirectX::XMFLOAT3* norm)
{
  if (g_editing)
  {
    return RaySphereIntersection(_rayStart, _rayDir, m_pos, m_radius, _rayLen);
  }
  else
  {
    return Building::DoesRayHit(_rayStart, _rayDir, _rayLen, _pos, norm);
  }
}


void GunTurret::ListSoundEvents(std::vector<const char*>* _list)
{
  Building::ListSoundEvents(_list);

  _list->push_back("TargetSighted");
  _list->push_back("FireShell");
}


GunTurretTarget::GunTurretTarget(int _buildingId)
  : WorldObject(),
    m_buildingId(_buildingId)
{
  m_type = EffectGunTurretTarget;
}


bool GunTurretTarget::Advance()
{
  GunTurret* turret = (GunTurret*)g_location->GetBuilding(m_buildingId);
  if (!turret || turret->m_type != Building::TypeGunTurret)
  {
    return true;
  }

  if (turret->GetNumPortsOccupied() > 0)
  {
    m_pos = turret->GetTarget();
  }
  else
  {
    m_pos = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
  }

  return false;
}


void GunTurretTarget::Render(float _time)
{
  // RenderSphere( m_pos, 10.0f );
}
