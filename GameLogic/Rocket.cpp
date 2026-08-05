#include "pch.h"
#include "SoundSources.h"
#include "Resource.h"
#include "GlVertex.h"
#include "FileWriter.h"
#include "TextStreamReaders.h"
#include "TextRenderer.h"
#include "Shape.h"
#include "DebugRender.h"
#include "Preferences.h"
#include "LanguageTable.h"

#ifdef CHEATMENU_ENABLED
#include "Input.h"
#include "InputTypes.h"
#endif

#include "Rocket.h"
#include "Citizen.h"

#include "Location.h"
#include "Team.h"
#include "GameTime.h"
#include "ParticleSystem.h"
#include "Explosion.h"
#include "GlobalWorld.h"
#include "EntityGrid.h"

#include "SoundSystem.h"
#include "WorldPointers.h"
#include "AppState.h"


Shape* FuelBuilding::s_fuelPipe = nullptr;


FuelBuilding::FuelBuilding()
  : Building(),
    m_fuelLink(-1),
    m_currentLevel(0.0f),
    m_fuelMarker(nullptr)
{
  if (!s_fuelPipe)
  {
    s_fuelPipe = g_resource->GetShape("FuelPipe.shp");
    DEBUG_ASSERT(s_fuelPipe);
  }
}


void FuelBuilding::Initialise(Building* _template)
{
  Building::Initialise(_template);

  m_fuelLink = ((FuelBuilding*)_template)->m_fuelLink;
}


DirectX::XMFLOAT3 FuelBuilding::GetFuelPosition()
{
  if (!m_fuelMarker)
  {
    m_fuelMarker = m_shape->m_rootFragment->LookupMarker("MarkerFuel");
    DEBUG_ASSERT(m_fuelMarker);
  }

  DirectX::XMFLOAT4X4 mat;
  DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::XMLoadFloat3(&m_up), DirectX::XMLoadFloat3(&m_pos)));

  // rows are Vector3. Returning .pos runs the seam's conversion on the way out.
  return m_fuelMarker->GetWorldPosition(mat);
}


void FuelBuilding::ProvideFuel(float _level)
{
  float factor2 = 0.2f * SERVER_ADVANCE_PERIOD;
  float factor1 = 1.0f - factor2;

  m_currentLevel = m_currentLevel * factor1 + _level * factor2;

  m_currentLevel = std::min(m_currentLevel, 1.0f);
  m_currentLevel = std::max(m_currentLevel, 0.0f);
}


FuelBuilding* FuelBuilding::GetLinkedBuilding()
{
  Building* building = g_location->GetBuilding(m_fuelLink);
  if (building)
  {
    if (building->m_type == TypeFuelGenerator || building->m_type == TypeFuelPipe || building->m_type == TypeEscapeRocket ||
        building->m_type == TypeFuelStation)
    {
      FuelBuilding* fuelBuilding = (FuelBuilding*)building;
      return fuelBuilding;
    }
  }

  return nullptr;
}


bool FuelBuilding::Advance()
{
  FuelBuilding* fuelBuilding = GetLinkedBuilding();
  if (fuelBuilding)
  {
    fuelBuilding->ProvideFuel(m_currentLevel);
  }

  return Building::Advance();
}


bool FuelBuilding::IsInView()
{
  FuelBuilding* fuelBuilding = GetLinkedBuilding();

  if (fuelBuilding)
  {
    DirectX::XMFLOAT3 const ourPipePosStore = GetFuelPosition();
    DirectX::XMFLOAT3 const theirPipePosStore = fuelBuilding->GetFuelPosition();
    DirectX::XMVECTOR const ourPipePos = DirectX::XMLoadFloat3(&ourPipePosStore);
    DirectX::XMVECTOR const theirPipePos = DirectX::XMLoadFloat3(&theirPipePosStore);

    DirectX::XMFLOAT3 midPoint;
    DirectX::XMStoreFloat3(&midPoint, DirectX::XMVectorScale(DirectX::XMVectorAdd(theirPipePos, ourPipePos), 0.5f));
    float radius = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(theirPipePos, ourPipePos))) / 2.0f;

    radius += m_radius;

    return (g_camera->SphereInViewFrustum(midPoint, radius));
  }
  else
  {
    return Building::IsInView();
  }
}


void FuelBuilding::Render(float _predictionTime)
{
  Building::Render(_predictionTime);

  FuelBuilding* fuelBuilding = GetLinkedBuilding();
  if (fuelBuilding)
  {
    DirectX::XMFLOAT3 const ourPipePosStore = GetFuelPosition();
    DirectX::XMFLOAT3 const theirPipePosStore = fuelBuilding->GetFuelPosition();
    DirectX::XMVECTOR const ourPipeStart = DirectX::XMLoadFloat3(&ourPipePosStore);

    // Native Normalise behaviour, per NeuronMath.h: no (0,0,1) fallback. The one
    // way this sees a zero-length input is a level whose m_fuelLink points a
    // building at itself, which would already be a broken link.
    DirectX::XMVECTOR const pipeVector =
      DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&theirPipePosStore), ourPipeStart));

    // operator^ was the cross product. g_upVector is still a Vector3 and cannot
    // be loaded by address; g_XMIdentityR1 is the (0,1,0,0) it holds.
    DirectX::XMVECTOR const right = DirectX::XMVector3Cross(pipeVector, DirectX::g_XMIdentityR1);
    DirectX::XMVECTOR const up = DirectX::XMVector3Cross(pipeVector, right);

    DirectX::XMVECTOR const pipePos = DirectX::XMVectorMultiplyAdd(pipeVector, DirectX::XMVectorReplicate(10.0f), ourPipeStart);

    // Matrix34(_f, _u, _pos): `up` is the FRONT argument here and pipeVector the
    // UP one, which is how the legacy call read. Not a transposition.
    DirectX::XMFLOAT4X4 pipeMat;
    DirectX::XMStoreFloat4x4(&pipeMat, BasisFromFrontAndUp(up, pipeVector, pipePos));
    DEBUG_ASSERT(s_fuelPipe);
    s_fuelPipe->Render(_predictionTime, pipeMat);
  }
}


void FuelBuilding::RenderAlphas(float _predictionTime)
{
  Building::RenderAlphas(_predictionTime);

  if (m_currentLevel > 0.0f)
  {
    FuelBuilding* fuelBuilding = GetLinkedBuilding();
    if (fuelBuilding)
    {
      DirectX::XMFLOAT3 const startPosStore = GetFuelPosition();
      DirectX::XMFLOAT3 const endPosStore = fuelBuilding->GetFuelPosition();
      DirectX::XMVECTOR const startPos = DirectX::XMLoadFloat3(&startPosStore);
      DirectX::XMVECTOR const endPos = DirectX::XMLoadFloat3(&endPosStore);

      DirectX::XMVECTOR const midPos = DirectX::XMVectorScale(DirectX::XMVectorAdd(startPos, endPos), 0.5f);

      DirectX::XMFLOAT3 const camPos = g_camera->GetPos();

      // operator^ was the cross product; SetLength is normalise-then-scale.
      DirectX::XMVECTOR rightAngle =
        DirectX::XMVector3Cross(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&camPos), midPos), DirectX::XMVectorSubtract(startPos, endPos));
      rightAngle = DirectX::XMVectorScale(DirectX::XMVector3Normalize(rightAngle), 25.0f);

      glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Fuel.bmp"));
      glEnable(GL_TEXTURE_2D);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE);
      glDepthMask(false);

      float tx = g_gameTime * -0.5f;
      float tw = 1.0f;

      glColor4f(1.0f, 0.4f, 0.1f, 0.4f * m_currentLevel);

      float nearPlaneStart = g_renderer->GetNearPlane();
      g_camera->SetupProjectionMatrix(nearPlaneStart * 1.2f, g_renderer->GetFarPlane());

      int buildingDetail = g_prefsManager->GetInt("RenderBuildingDetail");
      float maxLoops = 4 - buildingDetail;
      maxLoops = std::max(maxLoops, 1.0f);
      maxLoops = std::min(maxLoops, 3.0f);

      for (int i = 0; i < maxLoops; ++i)
      {
        glBegin(GL_QUADS);
        glTexCoord2f(tx, 0);
        EmitVertex(DirectX::XMVectorSubtract(startPos, rightAngle));
        glTexCoord2f(tx, 1);
        EmitVertex(DirectX::XMVectorAdd(startPos, rightAngle));
        glTexCoord2f(tx + tw, 1);
        EmitVertex(DirectX::XMVectorAdd(endPos, rightAngle));
        glTexCoord2f(tx + tw, 0);
        EmitVertex(DirectX::XMVectorSubtract(endPos, rightAngle));
        glEnd();
        rightAngle = DirectX::XMVectorScale(rightAngle, 0.7f);
      }

      g_camera->SetupProjectionMatrix(nearPlaneStart, g_renderer->GetFarPlane());

      glEnable(GL_DEPTH_TEST);
      glDisable(GL_TEXTURE_2D);
    }
  }

  //    glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
  //    g_editorFont.DrawText3DCentre( m_pos+Vector3(0,70,0), 5, "Fuel Pressure : %2.2f", m_currentLevel );
}


void FuelBuilding::Read(TextReader* _in, bool _dynamic)
{
  Building::Read(_in, _dynamic);

  m_fuelLink = atoi(_in->GetNextToken());
}


void FuelBuilding::Write(FileWriter* _out)
{
  Building::Write(_out);

  _out->printf("%6d ", m_fuelLink);
}


int FuelBuilding::GetBuildingLink() { return m_fuelLink; }


void FuelBuilding::SetBuildingLink(int _buildingId) { m_fuelLink = _buildingId; }

void FuelBuilding::Destroy(float _intensity)
{
  Building::Destroy(_intensity);
  FuelBuilding* fuelBuilding = GetLinkedBuilding();

  if (fuelBuilding)
  {
    DirectX::XMFLOAT3 const ourPipePosStore = GetFuelPosition();
    DirectX::XMFLOAT3 const theirPipePosStore = fuelBuilding->GetFuelPosition();
    DirectX::XMVECTOR const ourPipeStart = DirectX::XMLoadFloat3(&ourPipePosStore);

    // Native Normalise behaviour; see FuelBuilding::Render for the one input
    // that could be zero-length.
    DirectX::XMVECTOR const pipeVector =
      DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&theirPipePosStore), ourPipeStart));

    // operator^ was the cross product; g_XMIdentityR1 is the (0,1,0,0) that
    // g_upVector holds.
    DirectX::XMVECTOR const right = DirectX::XMVector3Cross(pipeVector, DirectX::g_XMIdentityR1);
    DirectX::XMVECTOR const up = DirectX::XMVector3Cross(pipeVector, right);

    DirectX::XMVECTOR const pipePos = DirectX::XMVectorMultiplyAdd(pipeVector, DirectX::XMVectorReplicate(10.0f), ourPipeStart);

    // Matrix34(_f, _u, _pos): `up` is the FRONT argument, as in Render.
    DirectX::XMFLOAT4X4 pipeMat;
    DirectX::XMStoreFloat4x4(&pipeMat, BasisFromFrontAndUp(up, pipeVector, pipePos));
    g_explosionManager.AddExplosion(s_fuelPipe, pipeMat);
  }
}
// ============================================================================


FuelGenerator::FuelGenerator()
  : FuelBuilding(),
    m_surges(0.0f),
    m_pump(nullptr),
    m_pumpTip(nullptr),
    m_pumpMovement(0.0f),
    m_previousPumpPos(0.0f)
{
  m_type = TypeFuelGenerator;

  SetShape(g_resource->GetShape("FuelGenerator.shp"));

  m_pump = g_resource->GetShape("FuelGeneratorPump.shp");
  m_pumpTip = m_pump->m_rootFragment->LookupMarker("MarkerTip");
}


void FuelGenerator::ProvideSurge() { m_surges++; }


bool FuelGenerator::Advance()
{
  //
  // Advance surges

  m_surges -= SERVER_ADVANCE_PERIOD * 1.0f;
  m_surges = std::min(m_surges, 10.0f);
  m_surges = std::max(m_surges, 0.0f);

  if (m_surges > 8)
  {
    GlobalBuilding* gb = g_globalWorld->GetBuilding(m_id.GetUniqueId(), g_locationId);
    if (gb)
      gb->m_online = true;
  }


  //
  // Pump fuel

  float fuelVal = m_surges / 10.0f;
  ProvideFuel(fuelVal);


  //
  // Spawn particles

  float previousPumpPos = m_previousPumpPos;
  DirectX::XMFLOAT3 pumpPos = GetPumpPos();
  m_previousPumpPos = (pumpPos.y - m_pos.y) / -80.0f;

  if (fuelVal > 0.0f && pumpPos.y > m_pos.y - 20.0f)
  {
    pumpPos.x += sfrand(10.0f);
    pumpPos.z += sfrand(10.0f);

    for (int i = 0; i < int(m_surges); ++i)
    {
      // g_upVector is still a Vector3 and cannot be loaded by address;
      // g_XMIdentityR1 is the (0,1,0,0) it holds. Kept as two steps so the
      // single frand draw stays where it was.
      DirectX::XMVECTOR pumpVelVec = DirectX::XMVectorScale(DirectX::g_XMIdentityR1, 20.0f);
      pumpVelVec = DirectX::XMVectorAdd(pumpVelVec, DirectX::XMVectorScale(DirectX::g_XMIdentityR1, frand(10)));
      DirectX::XMFLOAT3 pumpVel;
      DirectX::XMStoreFloat3(&pumpVel, pumpVelVec);

      DirectX::XMFLOAT4X4 mat;
      DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&pumpPos)));

      DirectX::XMFLOAT3 const particlePos = m_pumpTip->GetWorldPosition(mat);
      float size = 150.0f + frand(150.0f);

      g_particleSystem->CreateParticle(particlePos, pumpVel, Particle::TypeCitizenFire, size);
    }
  }


  //
  // Play sounds

  if (previousPumpPos >= 0.1f && m_previousPumpPos < 0.1f)
  {
    g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), "PumpUp");
  }
  else if (previousPumpPos <= 0.9f && m_previousPumpPos > 0.9f)
  {
    g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), "PumpDown");
  }

  return FuelBuilding::Advance();
}


void FuelGenerator::ListSoundEvents(std::vector<const char*>* _list)
{
  FuelBuilding::ListSoundEvents(_list);

  _list->push_back("PumpUp");
  _list->push_back("PumpDown");
}


DirectX::XMFLOAT3 FuelGenerator::GetPumpPos()
{
  DirectX::XMFLOAT3 pumpPos = m_pos;
  float pumpHeight = 80;

  pumpPos.y -= pumpHeight;
  pumpPos.y += fabs(cosf(m_pumpMovement)) * pumpHeight;

  return pumpPos;
}

void FuelGenerator::Render(float _predictionTime)
{
  FuelBuilding::Render(_predictionTime);

  //
  // Render the pump

  float fuelVal = m_surges / 10.0f;
  m_pumpMovement += g_advanceTime * fuelVal * 2;

  DirectX::XMFLOAT3 const pumpPos = GetPumpPos();
  DirectX::XMFLOAT4X4 mat;
  DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&pumpPos)));

  m_pump->Render(_predictionTime, mat);
}


void FuelGenerator::RenderAlphas(float _predictionTime)
{
  FuelBuilding::RenderAlphas(_predictionTime);

  //    glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
  //    g_editorFont.DrawText3DCentre( m_pos+Vector3(0,90,0), 10, "Surges : %2.2f", m_surges );
}


char const* FuelGenerator::GetObjectiveCounter()
{
  static std::string buffer;
  buffer = std::format("{} {}%", LANGUAGEPHRASE("objective_fuelpressure"), int(m_currentLevel * 100));
  return buffer.c_str();
}


// ============================================================================


FuelPipe::FuelPipe()
  : FuelBuilding()
{
  m_type = TypeFuelPipe;

  SetShape(g_resource->GetShape("FuelPipeBase.shp"));
}


bool FuelPipe::Advance()
{
  //
  // Ensure our sound ambiences are playing

  int numInstances = g_soundSystem->NumInstances(m_id, "FuelPipe PumpFuel");

  if (m_currentLevel > 0.2f && numInstances == 0)
  {
    g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), "PumpFuel");
  }
  else if (m_currentLevel <= 0.2f && numInstances > 0)
  {
    g_soundSystem->StopAllSounds(m_id, "FuelPipe PumpFuel");
  }


  return FuelBuilding::Advance();
}


void FuelPipe::ListSoundEvents(std::vector<const char*>* _list)
{
  FuelBuilding::ListSoundEvents(_list);

  _list->push_back("PumpFuel");
}


// ============================================================================


FuelStation::FuelStation()
  : FuelBuilding(),
    m_entrance(nullptr)
{
  m_type = TypeFuelStation;

  SetShape(g_resource->GetShape("FuelStation.shp"));

  m_entrance = m_shape->m_rootFragment->LookupMarker("MarkerEntrance");
}


bool FuelStation::IsLoading()
{
  Building* building = g_location->GetBuilding(m_fuelLink);
  if (building && building->m_type == TypeEscapeRocket)
  {
    EscapeRocket* rocket = (EscapeRocket*)building;
    if (rocket->m_state == EscapeRocket::StateLoading)
    {
      return true;
    }
  }

  return false;
}


bool FuelStation::Advance()
{
  Building* building = g_location->GetBuilding(m_fuelLink);
  if (building && building->m_type == TypeEscapeRocket)
  {
    EscapeRocket* rocket = (EscapeRocket*)building;
    if (rocket->m_state == EscapeRocket::StateLoading && rocket->SafeToLaunch())
    {
      //
      // Find a random Citizen and make him board

      Team* team = &g_location->m_teams[0];
      int numOthers = team->m_others.Size();
      if (numOthers > 0)
      {
        int randomIndex = syncrand() % numOthers;
        if (team->m_others.ValidIndex(randomIndex))
        {
          Entity* entity = team->m_others[randomIndex];
          if (entity && entity->m_type == Entity::TypeCitizen)
          {
            Citizen* citizen = (Citizen*)entity;
            float distance = DirectX::XMVectorGetX(
              DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&entity->m_pos), DirectX::XMLoadFloat3(&m_pos))));
            if (distance < 300.0f && (citizen->m_state == Citizen::StateIdle || citizen->m_state == Citizen::StateWorshipSpirit))
            {
              citizen->BoardRocket(m_id.GetUniqueId());
            }
          }
        }
      }
    }
  }

  return FuelBuilding::Advance();
}


DirectX::XMFLOAT3 FuelStation::GetEntrance()
{
  DirectX::XMFLOAT4X4 mat;
  DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

  return m_entrance->GetWorldPosition(mat);
}


bool FuelStation::BoardRocket(WorldObjectId _id)
{
  Building* building = g_location->GetBuilding(m_fuelLink);
  if (building && building->m_type == TypeEscapeRocket)
  {
    EscapeRocket* rocket = (EscapeRocket*)building;
    bool result = rocket->BoardRocket(_id);

    if (result)
    {
      Entity* entity = g_location->GetEntity(_id);
      DirectX::XMFLOAT3 entityPos = entity ? entity->m_pos : DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
      entityPos.y += 2;

      int numFlashes = 4 + speciesRandom() % 4;
      for (int i = 0; i < numFlashes; ++i)
      {
        // Left as one constructor call on purpose: the three draws are ordered
        // by the compiler's argument evaluation, not by this line, and splitting
        // them into statements would fix that order and change the sequence.
        DirectX::XMFLOAT3 const vel(sfrand(15.0f), frand(35.0f), sfrand(15.0f));
        g_particleSystem->CreateParticle(entityPos, vel, Particle::TypeControlFlash);
      }

      g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), "LoadPassenger");
    }

    return result;
  }

  return false;
}


void FuelStation::ListSoundEvents(std::vector<const char*>* _list)
{
  FuelBuilding::ListSoundEvents(_list);

  _list->push_back("LoadPassenger");
}


void FuelStation::Render(float _predictionTime) { Building::Render(_predictionTime); }


void FuelStation::RenderAlphas(float _predictionTime)
{
  // Prevent FuelBuilding::RenderAlphas from being called


  //
  // Render countdown

  Building* building = g_location->GetBuilding(m_fuelLink);
  if (building && building->m_type == TypeEscapeRocket)
  {
    EscapeRocket* rocket = (EscapeRocket*)building;
    if ((rocket->m_state == EscapeRocket::StateCountdown && rocket->m_countdown <= 10.0f) || rocket->m_state == EscapeRocket::StateFlight)
    {
      float screenSize = 60.0f;
      DirectX::XMFLOAT3 const screenFrontStore = m_front;
      DirectX::XMVECTOR const screenFront = DirectX::XMLoadFloat3(&screenFrontStore);
      // screenFront.RotateAroundY( 0.33f * M_PI );

      // operator^ was the cross product; g_XMIdentityR1 is the (0,1,0,0) that
      // g_upVector holds.
      DirectX::XMVECTOR const screenRight = DirectX::XMVector3Cross(screenFront, DirectX::g_XMIdentityR1);
      DirectX::XMVECTOR screenPos = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMVectorSet(0.0f, 150.0f, 0.0f, 0.0f));
      screenPos = DirectX::XMVectorSubtract(screenPos, DirectX::XMVectorScale(DirectX::XMVectorScale(screenRight, screenSize), 0.5f));
      screenPos = DirectX::XMVectorMultiplyAdd(screenFront, DirectX::XMVectorReplicate(30.0f), screenPos);
      DirectX::XMVECTOR const screenUp = DirectX::g_XMIdentityR1;

      //
      // Render lines for over effect

      glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/InterfaceGrey.bmp"));
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
      glEnable(GL_TEXTURE_2D);
      glEnable(GL_BLEND);
      glDepthMask(false);
      glShadeModel(GL_SMOOTH);
      glDisable(GL_CULL_FACE);

      float texX = 0.0f;
      float texW = 3.0f;
      float texY = g_gameTime * 0.01f;
      float texH = 0.3f;

      glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

      DirectX::XMVECTOR const screenRightSized = DirectX::XMVectorScale(screenRight, screenSize);
      DirectX::XMVECTOR const screenUpSized = DirectX::XMVectorScale(screenUp, screenSize);

      glBegin(GL_QUADS);
      EmitVertex(screenPos);
      EmitVertex(DirectX::XMVectorAdd(screenPos, screenRightSized));
      EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(screenPos, screenRightSized), screenUpSized));
      EmitVertex(DirectX::XMVectorAdd(screenPos, screenUpSized));
      glEnd();

      glColor4f(1.0f, 0.4f, 0.2f, 1.0f);

      glBlendFunc(GL_SRC_ALPHA, GL_ONE);

      for (int i = 0; i < 2; ++i)
      {
        glBegin(GL_QUADS);
        glTexCoord2f(texX, texY);
        EmitVertex(screenPos);

        glTexCoord2f(texX + texW, texY);
        EmitVertex(DirectX::XMVectorAdd(screenPos, screenRightSized));

        glTexCoord2f(texX + texW, texY + texH);
        EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(screenPos, screenRightSized), screenUpSized));

        glTexCoord2f(texX, texY + texH);
        EmitVertex(DirectX::XMVectorAdd(screenPos, screenUpSized));
        glEnd();

        texY *= 1.5f;
        texH = 0.1f;
      }


      glDepthMask(false);

      //
      // Render countdown

      glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
      DirectX::XMFLOAT3 textPos;
      DirectX::XMStoreFloat3(&textPos, DirectX::XMVectorAdd(DirectX::XMVectorAdd(screenPos, DirectX::XMVectorScale(screenRightSized, 0.5f)),
                                                            DirectX::XMVectorScale(screenUpSized, 0.5f)));

      DirectX::XMFLOAT3 screenUpStore;
      DirectX::XMStoreFloat3(&screenUpStore, screenUp);

      if (rocket->m_state == EscapeRocket::StateCountdown)
      {
        int countdown = (int)rocket->m_countdown + 1;
        g_gameFont.DrawText3D(textPos, screenFrontStore, screenUpStore, 50, "%d", countdown);
      }
      else
      {
        if (fmod(g_gameTime, 2) < 1)
        {
          g_gameFont.DrawText3D(textPos, screenFrontStore, screenUpStore, 50, "0");
        }
      }


      //
      // Render projection effect

      glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Laser.bmp"));

      DirectX::XMVECTOR const ourPos = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMVectorSet(0.0f, 90.0f, 0.0f, 0.0f));
      DirectX::XMVECTOR theirPos = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMVectorSet(0.0f, 200.0f, 0.0f, 0.0f));
      theirPos = DirectX::XMVectorMultiplyAdd(screenFront, DirectX::XMVectorReplicate(30.0f), theirPos);

      DirectX::XMFLOAT3 const camPos = g_camera->GetPos();
      DirectX::XMVECTOR const camToTheirPos = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&camPos), theirPos);

      // operator^ was the cross product; SetLength is normalise-then-scale.
      DirectX::XMVECTOR const lineTheirPos = DirectX::XMVectorScale(
        DirectX::XMVector3Normalize(DirectX::XMVector3Cross(camToTheirPos, DirectX::XMVectorSubtract(ourPos, theirPos))), m_radius * 0.5f);

      for (int i = 0; i < 3; ++i)
      {
        DirectX::XMFLOAT3 pos;
        DirectX::XMStoreFloat3(&pos, theirPos);
        pos.x += sinf(g_gameTime + i) * 15;
        pos.y += sinf(g_gameTime + i) * 15;
        pos.z += cosf(g_gameTime + i) * 15;

        float blue = 0.5f + fabs(sinf(g_gameTime * i)) * 0.5f;

        DirectX::XMVECTOR const posVec = DirectX::XMLoadFloat3(&pos);

        glBegin(GL_QUADS);
        glColor4f(1.0f, 0.4f, 0.2f, 0.4f);
        glTexCoord2f(1, 0);
        EmitVertex(DirectX::XMVectorSubtract(ourPos, lineTheirPos));
        glTexCoord2f(1, 1);
        EmitVertex(DirectX::XMVectorAdd(ourPos, lineTheirPos));
        glColor4f(1.0f, 0.4f, 0.2f, 0.2f);
        glTexCoord2f(0, 1);
        EmitVertex(DirectX::XMVectorAdd(posVec, lineTheirPos));
        glTexCoord2f(0, 0);
        EmitVertex(DirectX::XMVectorSubtract(posVec, lineTheirPos));
        glEnd();
      }


      glDepthMask(true);
      glEnable(GL_DEPTH_TEST);
      glDisable(GL_TEXTURE_2D);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDisable(GL_BLEND);
      glShadeModel(GL_FLAT);
    }
  }
}


bool FuelStation::PerformDepthSort(DirectX::XMFLOAT3& _centrePos)
{
  _centrePos = m_centrePos;
  return true;
}


// ============================================================================


EscapeRocket::EscapeRocket()
  : FuelBuilding(),
    m_fuel(0.0f),
    m_pipeCount(0),
    m_passengers(0),
    m_countdown(-1.0f),
    m_booster(nullptr),
    m_shadowTimer(0.0f),
    m_state(StateRefueling),
    m_damage(0.0f),
    m_spawnBuildingId(-1),
    m_spawnCompleted(false),
    m_cameraShake(0.0f)
{
  m_type = TypeEscapeRocket;

  SetShape(g_resource->GetShape("Rocket.shp"));

  m_booster = m_shape->m_rootFragment->LookupMarker("MarkerBooster");
  ASSERT_TEXT(m_booster, "MarkerBooster not found in rocket.shp");

  for (int i = 0; i < 3; ++i)
  {
    const std::string name = std::format("MarkerWindow0{}", i + 1);
    m_window[i] = m_shape->m_rootFragment->LookupMarker(name.c_str());
    ASSERT_TEXT(m_window[i], "{} not found", name.c_str());
  }
}


void EscapeRocket::ListSoundEvents(std::vector<const char*>* _list)
{
  FuelBuilding::ListSoundEvents(_list);

  _list->push_back("Refueling");
  _list->push_back("Happy");
  _list->push_back("Unhappy");
  _list->push_back("Flight");
  _list->push_back("Malfunction");
  _list->push_back("Explode");
  _list->push_back("EngineBurn");
}


void EscapeRocket::SetupSounds()
{
  char const* requiredSoundName = nullptr;

  //
  // What ambience should be playing?

  switch (m_state)
  {
  case StateRefueling:
    if (m_currentLevel > 0.2f)
      requiredSoundName = "Refueling";
    else
      requiredSoundName = "Unhappy";
    break;

  case StateLoading:
  case StateIgnition:
  case StateReady:
  case StateCountdown:
    if (m_damage < 10)
      requiredSoundName = "Happy";
    else
      requiredSoundName = "Malfunction";
    break;

  case StateExploding:
    if (m_fuel > 0.0f)
      requiredSoundName = "Malfunction";
    else
      requiredSoundName = "Unhappy";
    break;

  case StateFlight:
    requiredSoundName = "Flight";
    break;
  }

  std::string fullName;
  if (requiredSoundName)
    fullName = std::format("EscapeRocket {}", requiredSoundName);


  //
  // If we're not set up right, kill all sounds first


  int numInstances = requiredSoundName ? g_soundSystem->NumInstances(m_id, fullName.c_str()) : 0;

  if (!requiredSoundName || numInstances == 0)
  {
    g_soundSystem->StopAllSounds(m_id, "EscapeRocket Refueling");
    g_soundSystem->StopAllSounds(m_id, "EscapeRocket Happy");
    g_soundSystem->StopAllSounds(m_id, "EscapeRocket Unhappy");
    g_soundSystem->StopAllSounds(m_id, "EscapeRocket Malfunction");
    g_soundSystem->StopAllSounds(m_id, "EscapeRocket Flight");
  }


  //
  // Spawn the correct sound

  if (requiredSoundName && numInstances == 0)
  {
    g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), requiredSoundName);
  }


  //
  // If our engines are on then trigger the event

  int numEngineInstances = g_soundSystem->NumInstances(m_id, "EscapeRocket EngineBurn");

  if (m_state == StateReady || m_state == StateCountdown || m_state == StateFlight)
  {
    if (numEngineInstances == 0)
    {
      g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), "EngineBurn");
    }
  }
  else
  {
    if (numEngineInstances > 0)
    {
      g_soundSystem->StopAllSounds(m_id, "EscapeRocket EngineBurn");
    }
  }
}


void EscapeRocket::Initialise(Building* _template)
{
  FuelBuilding::Initialise(_template);

  m_fuel = ((EscapeRocket*)_template)->m_fuel;
  m_passengers = ((EscapeRocket*)_template)->m_passengers;
  m_spawnBuildingId = ((EscapeRocket*)_template)->m_spawnBuildingId;
  m_spawnCompleted = ((EscapeRocket*)_template)->m_spawnCompleted;
}


char const* EscapeRocket::GetObjectiveCounter()
{
  static std::string buffer;
  buffer = std::format("{} {}%, {} {}%", LANGUAGEPHRASE("objective_fuel"), (int)m_fuel, LANGUAGEPHRASE("objective_passengers"), (int)m_passengers);
  return buffer.c_str();
}


bool EscapeRocket::BoardRocket(WorldObjectId _id)
{
  if (m_state == StateLoading)
  {
    ++m_passengers;
    return true;
  }

  return false;
}


void EscapeRocket::ProvideFuel(float _level)
{
#ifdef CHEATMENU_ENABLED
  if (g_inputManager->controlEvent(ControlScrollSpeedup))
  {
    _level *= 100;
  }
#endif

  FuelBuilding::ProvideFuel(_level);

  if (_level > 0.1f)
  {
    ++m_pipeCount;
  }
}


void EscapeRocket::Refuel()
{
  switch (m_pipeCount)
  {
  case 0: // No incoming fuel
  case 1: // 1 pipe providing fuel
  {
    float targetFuel = 50.0f;
    if (m_fuel < targetFuel)
    {
      float factor1 = m_currentLevel * SERVER_ADVANCE_PERIOD * 0.005f;
      float factor2 = 1.0f - factor1;
      m_fuel = m_fuel * factor2 + targetFuel * factor1;
    }
    break;
  }

  case 2: // 2 pipes providing fuel
  {
    float targetFuel = 100.0f;
    float factor1 = m_currentLevel * SERVER_ADVANCE_PERIOD * 0.01f;
    float factor2 = 1.0f - factor1;
    m_fuel = m_fuel * factor2 + targetFuel * factor1;
    m_fuel = std::min(m_fuel, 100.0f);
    break;
  }

  case 3: // 3 pipes providing fuel
  {
    float targetFuel = 100.0f;
    float factor1 = m_currentLevel * SERVER_ADVANCE_PERIOD * 0.1;
    float factor2 = 1.0f - factor1;
    m_fuel = m_fuel * factor2 + targetFuel * factor1;
    m_fuel = std::min(m_fuel, 100.0f);
    break;
  }
  };

  m_pipeCount = 0;
}


void EscapeRocket::AdvanceRefueling()
{
  Refuel();

  m_damage = 0.0f;

  if (m_fuel >= 95.0f)
  {
    m_state = StateLoading;
  }
}


void EscapeRocket::AdvanceLoading()
{
  Refuel();

  if (m_fuel > 95.0f && m_passengers >= 100)
  {
    m_state = StateIgnition;
    m_countdown = 18.0f;
  }
}


void EscapeRocket::AdvanceIgnition()
{
  m_countdown -= SERVER_ADVANCE_PERIOD;

  SetupSpectacle();

  if (m_countdown <= 0.0f)
  {
    m_state = StateReady;
    m_countdown = 120.0f;

    m_cameraShake = 5.0f;

    if (m_spawnCompleted)
    {
      m_countdown = 10.0f;
      g_taskManagerInterface->SetVisible(false);
      if (g_script->IsRunningScript())
      {
        g_script->Skip();
      }
      g_script->RunScript("LaunchpadVictory.txt");
    }
  }
}


void EscapeRocket::AdvanceReady()
{
  //
  // Spawn attacking Citizens

  if (!m_spawnCompleted)
  {
    if (m_countdown > 100.0f && m_countdown < 110.0f)
    {
      Building* spawnBuilding = g_location->GetBuilding(m_spawnBuildingId);
      if (spawnBuilding)
      {
        DirectX::XMFLOAT3 spawnPos;
        DirectX::XMStoreFloat3(&spawnPos,
                               DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&spawnBuilding->m_front), DirectX::XMVectorReplicate(40.0f),
                                                            DirectX::XMLoadFloat3(&spawnBuilding->m_pos)));

        // g_zeroVector is still a Vector3 and cannot be loaded by address.
        DirectX::XMFLOAT3 const noVelocity(0.0f, 0.0f, 0.0f);
        g_location->SpawnEntities(spawnPos, 1, -1, Entity::TypeCitizen, 1, noVelocity, 40.0f);
      }
    }
  }

  SetupAttackers();
  SetupSpectacle();

  m_countdown -= SERVER_ADVANCE_PERIOD;

  if (m_countdown <= 0.0f)
  {
    m_state = StateCountdown;
    m_countdown = 10.0f;
  }
}


void EscapeRocket::AdvanceCountdown()
{
  m_countdown -= SERVER_ADVANCE_PERIOD * 0.5f;
  m_countdown = std::max(m_countdown, 0.0f);

  SetupAttackers();
  SetupSpectacle();

  if (m_countdown == 0.0f)
  {
    m_state = StateFlight;

    GlobalBuilding* gb = g_globalWorld->GetBuilding(m_id.GetUniqueId(), g_locationId);
    // if( gb ) gb->m_online = true;
  }
}


void EscapeRocket::AdvanceFlight()
{
  float landHeight = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);
  float thrust = sqrtf(m_pos.y - landHeight) * 2;
  thrust = std::max(thrust, 0.1f);

  m_vel = DirectX::XMFLOAT3(0.0f, thrust, 0.0f);

  DirectX::XMVECTOR const step = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), SERVER_ADVANCE_PERIOD);
  DirectX::XMStoreFloat3(&m_pos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), step));
  DirectX::XMStoreFloat3(&m_centrePos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_centrePos), step));

  SetupSpectacle();
}


void EscapeRocket::AdvanceExploding()
{
  m_damage -= SERVER_ADVANCE_PERIOD * 1;

  //
  // Burn fuel

  m_fuel -= SERVER_ADVANCE_PERIOD * 10;
  m_fuel = std::max(m_fuel, 0.0f);

  //
  // Kill passengers

  if (m_passengers)
  {
    m_passengers--;

    int windowIndex = syncrand() % 3;
    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::XMLoadFloat3(&m_up), DirectX::XMLoadFloat3(&m_pos)));

    // rows are Vector3. XMLoadFloat3 needs an XMFLOAT3*, and &row is a Vector3*;
    // the seam converts by reference, so these locals are what run it.
    DirectX::XMFLOAT4X4 const windowMat = m_window[windowIndex]->GetWorldMatrix(mat);
    DirectX::XMFLOAT3 const windowFront = DirectX::XMFLOAT3(windowMat._31, windowMat._32, windowMat._33);
    DirectX::XMFLOAT3 const windowUp = DirectX::XMFLOAT3(windowMat._21, windowMat._22, windowMat._23);
    DirectX::XMFLOAT3 const windowPos = DirectX::XMFLOAT3(windowMat._41, windowMat._42, windowMat._43);

    DirectX::XMVECTOR vel = DirectX::XMLoadFloat3(&windowFront);
    float angle = syncsfrand(M_PI * 0.25f);

    // RotateAround took its angle from the axis magnitude and did nothing below
    // 1e-8 squared.
    DirectX::XMVECTOR const rotation = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&windowUp), angle);
    float const rotationLengthSquared = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(rotation));
    if (rotationLengthSquared >= 1e-8f)
    {
      float const spin = sqrtf(rotationLengthSquared);
      vel = DirectX::XMVector3Transform(vel, DirectX::XMMatrixRotationAxis(DirectX::XMVectorScale(rotation, 1.0f / spin), spin));
    }

    // SetLength on a marker's front row, so never zero-length.
    vel = DirectX::XMVectorScale(DirectX::XMVector3Normalize(vel), 10.0f + syncfrand(30.0f));

    DirectX::XMFLOAT3 velStore;
    DirectX::XMStoreFloat3(&velStore, vel);

    WorldObjectId id = g_location->SpawnEntities(windowPos, 0, -1, Entity::TypeCitizen, 1, velStore, 0.0f);
    Citizen* citizen = (Citizen*)g_location->GetEntity(id);
    citizen->m_onGround = false;
    citizen->SetFire();
  }


  if (m_fuel > 0.0f)
  {
    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));
    g_explosionManager.AddExplosion(m_shape, mat, 0.001f);
  }


  //
  // Spawn smoke and fire

  for (int i = 0; i < 3; ++i)
  {
    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::XMLoadFloat3(&m_up), DirectX::XMLoadFloat3(&m_pos)));

    DirectX::XMFLOAT4X4 const windowMat = m_window[i]->GetWorldMatrix(mat);
    DirectX::XMFLOAT3 const windowFront = DirectX::XMFLOAT3(windowMat._31, windowMat._32, windowMat._33);
    DirectX::XMFLOAT3 const windowUp = DirectX::XMFLOAT3(windowMat._21, windowMat._22, windowMat._23);
    DirectX::XMFLOAT3 const windowPos = DirectX::XMFLOAT3(windowMat._41, windowMat._42, windowMat._43);

    DirectX::XMVECTOR vel = DirectX::XMLoadFloat3(&windowFront);
    float angle = syncsfrand(M_PI * 0.25f);

    // RotateAround took its angle from the axis magnitude.
    DirectX::XMVECTOR const rotation = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&windowUp), angle);
    float const rotationLengthSquared = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(rotation));
    if (rotationLengthSquared >= 1e-8f)
    {
      float const spin = sqrtf(rotationLengthSquared);
      vel = DirectX::XMVector3Transform(vel, DirectX::XMMatrixRotationAxis(DirectX::XMVectorScale(rotation, 1.0f / spin), spin));
    }

    // SetLength on a marker's front row, so never zero-length.
    vel = DirectX::XMVectorScale(DirectX::XMVector3Normalize(vel), 5.0f + syncfrand(10.0f));
    float fireSize = 150 + syncfrand(150.0f);

    DirectX::XMFLOAT3 velStore;
    DirectX::XMStoreFloat3(&velStore, vel);

    DirectX::XMFLOAT3 const smokeVel = velStore;
    float smokeSize = fireSize;

    if (m_fuel > 0.0f)
      g_particleSystem->CreateParticle(windowPos, velStore, Particle::TypeFire, fireSize);
    g_particleSystem->CreateParticle(windowPos, smokeVel, Particle::TypeMissileTrail, smokeSize);
  }


  if (m_damage <= 0.0f)
  {
    m_damage = 0.0f;
    m_spawnCompleted = true;
    m_state = StateRefueling;
  }
}


void EscapeRocket::SetupSpectacle()
{
  m_shadowTimer -= SERVER_ADVANCE_PERIOD;
  if (m_shadowTimer <= 0.0f)
  {
    for (int t = 0; t < NUM_TEAMS; ++t)
    {
      Team* team = &g_location->m_teams[t];
      for (int i = 0; i < team->m_others.Size(); ++i)
      {
        if (team->m_others.ValidIndex(i))
        {
          Entity* entity = team->m_others[i];
          if (entity && entity->m_type == Entity::TypeCitizen)
          {
            Citizen* citizen = (Citizen*)entity;
            // if( m_state == StateReady ) citizen->CastShadow( m_id.GetUniqueId() );
            //  Causes too much of a slow down, and doesn't add much visually to the scene
            if (t == 0 && citizen->m_state == Citizen::StateIdle && (syncrand() % 10) < 2)
            {
              citizen->WatchSpectacle(m_id.GetUniqueId());
            }
          }
        }
      }
    }

    m_shadowTimer = 1.0f;
  }
}


bool EscapeRocket::IsSpectacle()
{
  return (m_state == StateIgnition || m_state == StateReady || m_state == StateCountdown || m_state == StateFlight);
}


bool EscapeRocket::IsInView() { return FuelBuilding::IsInView() || m_state == StateFlight; }


void EscapeRocket::SetupAttackers()
{
  if (!m_spawnCompleted && syncfrand() < 0.2f)
  {
    Team* team = &g_location->m_teams[1];
    int numOthers = team->m_others.Size();
    if (numOthers > 0)
    {
      int randomIndex = syncrand() % numOthers;
      if (team->m_others.ValidIndex(randomIndex))
      {
        Entity* entity = team->m_others[randomIndex];
        if (entity && entity->m_type == Entity::TypeCitizen)
        {
          Citizen* citizen = (Citizen*)entity;
          float range = DirectX::XMVectorGetX(
            DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&citizen->m_pos), DirectX::XMLoadFloat3(&m_pos))));
          if (range < 350.0f)
          {
            citizen->AttackBuilding(m_id.GetUniqueId());
          }
        }
      }
    }
  }
}


void EscapeRocket::Damage(float _damage)
{
  FuelBuilding::Damage(_damage);

  if (m_state != StateExploding)
  {
    m_damage -= _damage;

    if (m_damage > 100.0f)
    {
      m_state = StateExploding;
      g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), "Explode");
    }
  }
}


bool EscapeRocket::Advance()
{
  switch (m_state)
  {
  case StateRefueling:
    AdvanceRefueling();
    break;
  case StateLoading:
    AdvanceLoading();
    break;
  case StateIgnition:
    AdvanceIgnition();
    break;
  case StateReady:
    AdvanceReady();
    break;
  case StateCountdown:
    AdvanceCountdown();
    break;
  case StateFlight:
    AdvanceFlight();
    break;
  case StateExploding:
    AdvanceExploding();
    break;
  }


  SetupSounds();


  //
  // Create rocket flames
  // Shake the camera

  if (m_cameraShake > 0.0)
  {
    m_cameraShake -= SERVER_ADVANCE_PERIOD;

    float actualShake = m_cameraShake / 5.0f;
    g_camera->CreateCameraShake(actualShake);
  }


  if (m_state == StateReady || m_state == StateCountdown || m_state == StateFlight)
  {
    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));

    DirectX::XMFLOAT3 const boosterPos = m_booster->GetWorldPosition(mat);

    for (int i = 0; i < 15; ++i)
    {
      // Both constructor calls are left whole: their draws are ordered by the
      // compiler's argument evaluation, and splitting them would fix that order.
      DirectX::XMFLOAT3 const jitter(sfrand(20), 10, sfrand(20));
      DirectX::XMFLOAT3 pos = boosterPos;
      pos.x += jitter.x;
      pos.y += jitter.y;
      pos.z += jitter.z;

      DirectX::XMFLOAT3 vel(sfrand(50), -frand(150), sfrand(50));
      float size = 500.0f;

      if (i > 10)
      {
        vel.x *= 0.75f;
        vel.z *= 0.75f;
        g_particleSystem->CreateParticle(pos, vel, Particle::TypeMissileTrail, size);
      }
      else
      {
        g_particleSystem->CreateParticle(pos, vel, Particle::TypeMissileFire, size);
      }
    }
  }

  return FuelBuilding::Advance();
}


bool EscapeRocket::SafeToLaunch()
{
  DirectX::XMFLOAT3 testPos = m_pos;
  testPos.x += 330.0f;
  testPos.z += 50.0f;
  float testRadius = 100.0f;

  int numEnemies = g_location->m_entityGrid->GetNumEnemies(testPos.x, testPos.z, testRadius, 0);

  return (numEnemies < 2);
}


void EscapeRocket::Render(float _predictionTime)
{
  DirectX::XMFLOAT3 predictedPos;
  DirectX::XMStoreFloat3(&predictedPos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(_predictionTime),
                                                                     DirectX::XMLoadFloat3(&m_pos)));

  DirectX::XMFLOAT4X4 mat;
  DirectX::XMStoreFloat4x4(&mat,
                           BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::XMLoadFloat3(&m_up), DirectX::XMLoadFloat3(&predictedPos)));

  m_shape->Render(0.0f, mat);
}


void EscapeRocket::RenderAlphas(float _predictionTime)
{
  FuelBuilding::RenderAlphas(_predictionTime);

  if (g_editing)
  {
    Building* spawnBuilding = g_location->GetBuilding(m_spawnBuildingId);
    if (spawnBuilding)
    {
      RenderArrow(m_pos, spawnBuilding->m_pos, 1.0f);
    }

    return;
  }

  DirectX::XMFLOAT3 predictedPos;
  DirectX::XMStoreFloat3(&predictedPos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(_predictionTime),
                                                                     DirectX::XMLoadFloat3(&m_pos)));

  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
  //    g_editorFont.DrawText3DCentre( predictedPos+Vector3(0,120,0), 10, "Fuel : %2.2f", m_fuel );
  //    g_editorFont.DrawText3DCentre( predictedPos+Vector3(0,130,0), 10, "Passengers : %d", m_passengers );
  //    g_editorFont.DrawText3DCentre( predictedPos+Vector3(0,140,0), 10, "Damage : %2.2f", m_damage );
  //    g_editorFont.DrawText3DCentre( predictedPos+Vector3(0,150,0), 10, "Timer : %2.2f", m_countdown );

  //    if( m_state == StateCountdown && m_countdown <= 10.0f )
  //    {
  //        g_editorFont.DrawText3DCentre( predictedPos+Vector3(0,200,0), 100, "%d", int(m_countdown) );
  //    }


  //
  // Render rocket glow
  // Calculate alpha value

  float alpha = m_fuel / 100.0f;
  alpha = std::min(alpha, 1.0f);
  alpha = std::max(alpha, 0.0f);

  if (alpha > 0.0f)
  {
    DirectX::XMFLOAT3 const camUpStore = g_camera->GetUp();
    DirectX::XMFLOAT3 const camRightStore = g_camera->GetRight();
    DirectX::XMVECTOR const camUp = DirectX::XMLoadFloat3(&camUpStore);
    DirectX::XMVECTOR const camRight = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&camRightStore), 0.75f);

    glDepthMask(false);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Starburst.bmp"));
    // glDisable       ( GL_DEPTH_TEST );

    float timeIndex = g_gameTime * 2;

    int buildingDetail = g_prefsManager->GetInt("RenderBuildingDetail", 1);
    int maxBlobs = 50;
    if (buildingDetail == 2)
      maxBlobs = 20;
    if (buildingDetail == 3)
      maxBlobs = 10;

    DirectX::XMFLOAT3 boosterPos = predictedPos;
    boosterPos.y += 100;


    //
    // Central glow effect

    for (int i = 30; i < maxBlobs; ++i)
    {
      DirectX::XMFLOAT3 pos = boosterPos;
      pos.x += sinf(timeIndex * 0.5 + i) * i * 3.7f;
      pos.y += cosf(timeIndex * 0.5 + i) * cosf(i * 20) * 50;
      pos.y += 40;
      pos.z += cosf(timeIndex * 0.3 + i) * i * 3.7f;

      float size = 20.0f * sinf(timeIndex + i * 2);
      size = std::max(size, 5.0f);

      DirectX::XMVECTOR const posVec = DirectX::XMLoadFloat3(&pos);

      for (int j = 0; j < 2; ++j)
      {
        size *= 0.75f;

        // Inside the j loop because size changes on each pass.
        DirectX::XMVECTOR const rightSized = DirectX::XMVectorScale(camRight, size);
        DirectX::XMVECTOR const upSized = DirectX::XMVectorScale(camUp, size);

        glColor4f(1.0f, 0.6f, 0.2f, alpha);
        glBegin(GL_QUADS);
        glTexCoord2i(0, 0);
        EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorSubtract(posVec, rightSized), upSized));
        glTexCoord2i(1, 0);
        EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(posVec, rightSized), upSized));
        glTexCoord2i(1, 1);
        EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorAdd(posVec, rightSized), upSized));
        glTexCoord2i(0, 1);
        EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorSubtract(posVec, rightSized), upSized));
        glEnd();
      }
    }


    //
    // Central starbursts

    glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Starburst.bmp"));

    int numStars = 10;
    if (buildingDetail == 2)
      numStars = 5;
    if (buildingDetail == 3)
      numStars = 2;

    for (int i = 8; i < numStars; ++i)
    {
      DirectX::XMFLOAT3 pos = boosterPos;
      pos.x += sinf(timeIndex + i) * i * 1.7f;
      pos.y += fabs(cosf(timeIndex + i) * cosf(i * 20) * 64);
      pos.z += cosf(timeIndex + i) * i * 1.7f;

      float size = i * 30.0f;

      DirectX::XMVECTOR const posVec = DirectX::XMLoadFloat3(&pos);
      DirectX::XMVECTOR const rightSized = DirectX::XMVectorScale(camRight, size);
      DirectX::XMVECTOR const upSized = DirectX::XMVectorScale(camUp, size);

      glColor4f(1.0f, 0.4f, 0.2f, alpha);
      glBegin(GL_QUADS);
      glTexCoord2i(0, 0);
      EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorSubtract(posVec, rightSized), upSized));
      glTexCoord2i(1, 0);
      EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(posVec, rightSized), upSized));
      glTexCoord2i(1, 1);
      EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorAdd(posVec, rightSized), upSized));
      glTexCoord2i(0, 1);
      EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorSubtract(posVec, rightSized), upSized));
      glEnd();
    }

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
  }
}


void EscapeRocket::Read(TextReader* _in, bool _dynamic)
{
  FuelBuilding::Read(_in, _dynamic);

  m_fuel = atof(_in->GetNextToken());
  m_passengers = atoi(_in->GetNextToken());
  m_spawnBuildingId = atoi(_in->GetNextToken());
  m_spawnCompleted = atoi(_in->GetNextToken());
}


void EscapeRocket::Write(FileWriter* _out)
{
  FuelBuilding::Write(_out);

  _out->printf("%6.2f %6d %6d %6d", m_fuel, m_passengers, m_spawnBuildingId, (int)m_spawnCompleted);
}


int EscapeRocket::GetStateId(char* _state)
{
  static char const* stateNames[] = {"Refueling", "Loading", "Ignition", "Ready", "Countdown", "Exploding", "Flight"};

  for (int i = 0; i < NumStates; ++i)
  {
    if (stricmp(stateNames[i], _state) == 0)
    {
      return i;
    }
  }

  return -1;
}
