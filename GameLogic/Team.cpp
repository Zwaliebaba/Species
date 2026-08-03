#include "pch.h"
#include "Globals.h"

#include <math.h>

#include "HiResTime.h"
#include "MathUtils.h"
#include "Profiler.h"
#include "Resource.h"
#include "Debug.h"
#include "TextRenderer.h"
#include "Shape.h"
#include "Preferences.h"
#include "DebugRender.h"
#include "Bitmap.h"
#include "BinaryStreamReaders.h"

#include "ClientToServer.h"

#include "SoundSystem.h"

#include "Location.h"
#include "GlobalWorld.h"
#include "GameTime.h"
#include "EntityGrid.h"
#include "Team.h"
#include "Unit.h"
#include "TaskManager.h"

#include "Engineer.h"
#include "Entity.h"
#include "InsertionSquad.h"
#include "Virii.h"
#include "Airstrike.h"
#include "WorldObject.h"
#include "Citizen.h"
#include "WorldPointers.h"
#include "AppState.h"


// ****************************************************************************
//  Class Team
// ****************************************************************************

// *** Constructor
Team::Team()
  : m_teamId(-1),
    m_teamType(TeamTypeUnused),
    m_currentUnitId(-1),
    m_currentEntityId(-1),
    m_currentBuildingId(-1)
{
  m_others.SetTotalNumSlices(NUM_SLICES_PER_FRAME);
  m_others.SetStepSize(100);
  m_units.SetStepSize(5);
}


void Team::Initialise(int _teamId)
{
  m_teamId = _teamId;


  //
  // Generate the ViriiFull bmp

  if (!g_resource->DoesTextureExist("Sprites/viriifull.bmp"))
  {
    BinaryReader* reader = g_resource->GetBinaryReader("Sprites/Virii.bmp");
    BitmapRGBA little(reader, "bmp");
    delete reader;
    BitmapRGBA big(32 + 128, 512);
    big.Clear(RGBAColour(0, 0, 0));
    int destY = 0;
    float viriiWidth = 32.0f;

    for (int i = 0; i < 32; ++i)
    {
      int destWidth = 32 - i / 2;
      int destHeight = 32 - i / 2;

      if (destY + destHeight < 512)
      {
        int targetX = viriiWidth / 2 - destWidth / 2;
        big.Blit(0, little.m_height, little.m_width, -little.m_height, &little, targetX, destY, destWidth, destHeight, true);
        destY += destHeight;
      }
    }

    reader = g_resource->GetBinaryReader("Textures/Glow.bmp");
    BitmapRGBA glow(reader, "bmp");
    delete reader;
    big.Blit(0, 0, 128, 128, &glow, 32, 0, 128, 128, true);

    g_resource->AddBitmap("Sprites/viriifull.bmp", big, true);
  }
}


void Team::SetTeamType(int _teamType) { m_teamType = _teamType; }


void Team::RegisterSpecial(WorldObjectId _id) { m_specials.PutData(_id); }


void Team::UnRegisterSpecial(WorldObjectId _id)
{
  for (int i = 0; i < m_specials.Size(); ++i)
  {
    WorldObjectId id = *m_specials.GetPointer(i);
    if (id == _id)
    {
      m_specials.RemoveData(i);
      break;
    }
  }
}


void Team::SelectUnit(int _unitId, int _entityId, int _buildingId)
{
  m_currentBuildingId = _buildingId;
  m_currentUnitId = _unitId;
  m_currentEntityId = _entityId;

  if (m_teamId == g_globalWorld->m_myTeamId)
  {
    g_gameCursor->BoostSelectionArrows(2.0f);
  }

  if (m_currentUnitId == -1 && m_currentBuildingId == -1 && m_others.ValidIndex(m_currentEntityId))
  {
    Entity* entity = m_others[m_currentEntityId];
    if (entity && entity->m_type == Entity::TypeOfficer)
    {
      g_taskManager->SelectTask(-1);
    }
  }

  if (_unitId == -1 && _entityId == -1 && _buildingId == -1)
  {
    g_soundSystem->TriggerOtherEvent(nullptr, "TaskManagerDeselectTask", SoundSourceBlueprint::TypeInterface);
  }
  else
  {
    g_soundSystem->TriggerOtherEvent(nullptr, "TaskManagerSelectTask", SoundSourceBlueprint::TypeInterface);
  }

  //    if( m_teamId == g_globalWorld->m_myTeamId )
  //    {
  //        Vector3 worldpos;
  //        if( m_units.ValidIndex(_unitId) )
  //        {
  //            Unit *unit = m_units[_unitId];
  //            worldpos = unit->m_centrePos - g_camera->GetFront() * 200.0f;
  //        }
  //        else if( m_others.ValidIndex(_entityId) )
  //        {
  //            Entity *entity = m_others[_entityId];
  //            worldpos = entity->m_pos - g_camera->GetFront() * 200.0f;
  //        }
  //    }
}


Unit* Team::GetMyUnit()
{
  if (m_currentUnitId == -1 || !m_units.ValidIndex(m_currentUnitId))
  {
    return nullptr;
  }
  else if (m_units.ValidIndex(m_currentUnitId))
  {
    return m_units[m_currentUnitId];
  }
  else
  {
    return nullptr;
  }
}


Entity* Team::RayHitEntity(Vector3 const& _rayStart, Vector3 const& _rayEnd)
{
  // Hit against Units
  for (unsigned int i = 0; i < m_units.Size(); ++i)
  {
    if (m_units.ValidIndex(i))
    {
      Entity* result = m_units[i]->RayHit(_rayStart, _rayEnd);
      if (result)
      {
        return result;
      }
    }
  }

  // Hit against Others
  for (unsigned int i = 0; i < m_others.Size(); ++i)
  {
    if (m_others.ValidIndex(i))
    {
      if (m_others[i]->RayHit(_rayStart, _rayEnd))
      {
        return m_others[i];
      }
    }
  }

  return nullptr;
}


Entity* Team::GetMyEntity()
{
  if (m_currentEntityId == -1)
  {
    return nullptr;
  }
  else if (m_others.ValidIndex(m_currentEntityId))
  {
    return m_others[m_currentEntityId];
  }
  else
  {
    return nullptr;
  }
}


Unit* Team::NewUnit(int _troopType, int _numEntities, int* _unitId, Vector3 const& _pos)
{
  *_unitId = m_units.GetNextFree();
  Unit* unit = nullptr;

  if (_troopType == Entity::TypeInsertionSquadie)
  {
    unit = new InsertionSquad(m_teamId, *_unitId, _numEntities, _pos);
  }
  else if (_troopType == Entity::TypeSpaceInvader)
  {
    unit = new AirstrikeUnit(m_teamId, *_unitId, _numEntities, _pos);
  }
  else if (_troopType == Entity::TypeVirii)
  {
    unit = new ViriiUnit(m_teamId, *_unitId, _numEntities, _pos);
  }
  else
  {
    unit = new Unit(_troopType, m_teamId, *_unitId, _numEntities, _pos);
  }

  m_units.PutData(unit, *_unitId);
  unit->Begin();
  return unit;
}

Entity* Team::NewEntity(int _troopType, int _unitId, int* _index)
{
  if (_unitId == -1)
  {
    Entity* entity = Entity::NewEntity(_troopType);
    DEBUG_ASSERT(entity);
    *_index = m_others.PutData(entity);
    return entity;
  }
  else
  {
    if (m_units.ValidIndex(_unitId))
    {
      Unit* unit = m_units.GetData(_unitId);
      return unit->NewEntity(_index);
    }
  }

  return nullptr;
}

int Team::NumEntities(int _troopType)
{
  int result = 0;
  int i;

  for (i = 0; i < m_units.Size(); ++i)
  {
    if (m_units.ValidIndex(i))
    {
      Unit* unit = m_units[i];
      if (unit->m_troopType == _troopType)
      {
        result += unit->NumEntities();
      }
    }
  }

  for (i = 0; i < m_others.Size(); ++i)
  {
    if (m_others.ValidIndex(i))
    {
      Entity* ent = m_others[i];
      if (ent->m_type == _troopType)
      {
        ++result;
      }
    }
  }

  return result;
}


void Team::Advance(int _slice)
{
  //
  // Advance all Units

  if (m_teamType > TeamTypeUnused)
  {
    START_PROFILE(g_profiler, "Advance Unit Entities");
    for (int unit = 0; unit < m_units.Size(); ++unit)
    {
      if (m_units.ValidIndex(unit))
      {
        Unit* theUnit = m_units.GetData(unit);
        theUnit->AdvanceEntities(_slice);
      }
    }
    END_PROFILE(g_profiler, "Advance Unit Entities");

    if (_slice == 0)
    {
      START_PROFILE(g_profiler, "Advance Units");
      for (int unit = 0; unit < m_units.Size(); ++unit)
      {
        if (m_units.ValidIndex(unit))
        {
          Unit* theUnit = m_units.GetData(unit);
          bool amIDead = theUnit->Advance(unit);
          if (amIDead)
          {
            m_units.MarkNotUsed(unit);
            delete theUnit;
          }
        }
      }
      END_PROFILE(g_profiler, "Advance Units");
    }
  }


  //
  // Advance all Other entities

  if (m_teamType > TeamTypeUnused)
  {
    START_PROFILE(g_profiler, "Advance Others");
    int startIndex, endIndex;
    m_others.GetNextSliceBounds(_slice, &startIndex, &endIndex);

    for (int i = startIndex; i <= endIndex; i++)
    {
      if (m_others.ValidIndex(i))
      {
        Entity* ent = m_others[i];
        if (ent->m_enabled)
        {
          Vector3 oldPos(ent->m_pos);
          WorldObjectId myId(m_teamId, -1, i, ent->m_id.GetUniqueId());

          char const* entityName = Entity::GetTypeName(ent->m_type);
          START_PROFILE(g_profiler, entityName);
          bool amIdead = ent->Advance(nullptr);
          END_PROFILE(g_profiler, entityName);

#ifdef PROFILER_ENABLED
          DEBUG_ASSERT(strcmp(g_profiler->m_currentElement->m_name, "Advance Others") == 0);
#endif

          if (amIdead)
          {
            g_location->m_entityGrid->RemoveObject(myId, oldPos.x, oldPos.z, ent->m_radius);
            m_others.MarkNotUsed(i);
            delete ent;
          }
          else if (!ent->m_enabled)
          {
            g_location->m_entityGrid->RemoveObject(myId, oldPos.x, oldPos.z, ent->m_radius);
          }
          else
          {
            g_location->m_entityGrid->UpdateObject(myId, oldPos.x, oldPos.z, ent->m_pos.x, ent->m_pos.z, ent->m_radius);
          }
        }
      }
    }

    END_PROFILE(g_profiler, "Advance Others");
  }
}

void Team::Render()
{
  //
  // Render Units

  START_PROFILE(g_profiler, "Render Units");

  float timeSinceAdvance = g_predictionTime;

  glEnable(GL_TEXTURE_2D);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glEnable(GL_BLEND);
  glEnable(GL_ALPHA_TEST);
  glAlphaFunc(GL_GREATER, 0.02f);

  for (int i = 0; i < m_units.Size(); ++i)
  {
    if (m_units.ValidIndex(i))
    {
      Unit* unit = m_units[i];
      if (unit->IsInView())
      {
        START_PROFILE(g_profiler, Entity::GetTypeName(unit->m_troopType));
        unit->Render(timeSinceAdvance);
        END_PROFILE(g_profiler, Entity::GetTypeName(unit->m_troopType));
      }
    }
  }

  glDisable(GL_TEXTURE_2D);
  glDisable(GL_ALPHA_TEST);
  glDisable(GL_BLEND);
  glDisable(GL_TEXTURE_2D);
  glAlphaFunc(GL_GREATER, 0.01f);

  END_PROFILE(g_profiler, "Render Units");


  //
  // Render Others

  START_PROFILE(g_profiler, "Render Others");
  RenderOthers(timeSinceAdvance);
  END_PROFILE(g_profiler, "Render Others");


  //
  // Render Virii

  if (m_teamId == 1 && m_teamType == TeamTypeCPU)
  {
    START_PROFILE(g_profiler, "Render Virii");
    RenderVirii(timeSinceAdvance);
    END_PROFILE(g_profiler, "Render Virii");
  }


  //
  // Render Citizens

  START_PROFILE(g_profiler, "Render Citizens");
  RenderCitizens(timeSinceAdvance);
  END_PROFILE(g_profiler, "Render Citizens");
}


void Team::RenderVirii(float _predictionTime)
{
  if (m_others.Size() == 0)
    return;

  int lastUpdated = m_others.GetLastUpdated();

  float nearPlaneStart = g_renderer->GetNearPlane();
  g_camera->SetupProjectionMatrix(nearPlaneStart * 1.05f, g_renderer->GetFarPlane());

  //
  // Render Red Virii shapes

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Sprites/viriifull.bmp"));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glDepthMask(false);
  glDisable(GL_CULL_FACE);
  glBegin(GL_QUADS);

  int entityDetail = g_prefsManager->GetInt("RenderEntityDetail");

  for (int i = 0; i <= m_others.Size(); i++)
  {
    if (m_others.ValidIndex(i))
    {
      Entity* entity = m_others.GetData(i);
      if (entity->m_type == Entity::TypeVirii)
      {
        Virii* virii = (Virii*)entity;
        if (virii->IsInView())
        {
          float rangeToCam = (virii->m_pos - g_camera->GetPos()).Mag();
          int viriiDetail = 1;
          if (entityDetail == 1 && rangeToCam > 1000.0f)
            viriiDetail = 2;
          else if (entityDetail == 2 && rangeToCam > 1000.0f)
            viriiDetail = 3;
          else if (entityDetail == 2 && rangeToCam > 500.0f)
            viriiDetail = 2;
          else if (entityDetail == 3 && rangeToCam > 1000.0f)
            viriiDetail = 4;
          else if (entityDetail == 3 && rangeToCam > 600.0f)
            viriiDetail = 3;
          else if (entityDetail == 3 && rangeToCam > 300.0f)
            viriiDetail = 2;

          if (i <= lastUpdated)
          {
            virii->Render(_predictionTime, m_teamId, viriiDetail);
          }
          else
          {
            virii->Render(_predictionTime + SERVER_ADVANCE_PERIOD, m_teamId, viriiDetail);
          }
        }
      }
    }
  }

  glEnd();
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);
  glEnable(GL_CULL_FACE);
  glDepthMask(true);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

  g_camera->SetupProjectionMatrix(nearPlaneStart, g_renderer->GetFarPlane());
}


void Team::RenderCitizens(float _predictionTime)
{
  if (m_others.Size() == 0)
    return;

  int lastUpdated = m_others.GetLastUpdated();

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Sprites/Citizen.bmp"));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glEnable(GL_BLEND);
  glEnable(GL_ALPHA_TEST);
  glAlphaFunc(GL_GREATER, 0.04f);
  glDisable(GL_CULL_FACE);

  int entityDetail = g_prefsManager->GetInt("RenderEntityDetail", 1);
  float highDetailDistanceSqd = 0.0f;
  if (entityDetail <= 1)
    highDetailDistanceSqd = 99999.9f;
  else if (entityDetail == 2)
    highDetailDistanceSqd = 1000.0f;
  else if (entityDetail >= 3)
    highDetailDistanceSqd = 500.0f;

  highDetailDistanceSqd *= highDetailDistanceSqd;

  for (int i = 0; i <= m_others.Size(); i++)
  {
    if (m_others.ValidIndex(i))
    {
      Entity* entity = m_others.GetData(i);
      if (entity->m_type == Entity::TypeCitizen)
      {
        Citizen* citizen = (Citizen*)entity;
        if (citizen->IsInView())
        {
          float camDistSqd = (citizen->m_pos - g_camera->GetPos()).MagSquared();
          float highDetail = 1.0f - (camDistSqd / highDetailDistanceSqd);
          highDetail = max(highDetail, 0.0f);
          highDetail = min(highDetail, 1.0f);

          if (i <= lastUpdated)
          {
            citizen->Render(_predictionTime, highDetail);
          }
          else
          {
            citizen->Render(_predictionTime + SERVER_ADVANCE_PERIOD, highDetail);
          }
        }
      }
    }
  }

  glDisable(GL_ALPHA_TEST);
  glAlphaFunc(GL_GREATER, 0.01);
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);
  glEnable(GL_CULL_FACE);
}


void Team::RenderOthers(float _predictionTime)
{
  int lastUpdated = m_others.GetLastUpdated();

  for (int i = 0; i <= lastUpdated; i++)
  {
    if (m_others.ValidIndex(i))
    {
      Entity* entity = m_others.GetData(i);
      if (entity->m_type != Entity::TypeVirii && entity->m_type != Entity::TypeCitizen && entity->IsInView())
      {
        START_PROFILE(g_profiler, Entity::GetTypeName(entity->m_type));
        entity->Render(_predictionTime);
        END_PROFILE(g_profiler, Entity::GetTypeName(entity->m_type));
      }
    }
  }

  int size = m_others.Size();
  _predictionTime += SERVER_ADVANCE_PERIOD;
  for (int i = lastUpdated + 1; i < size; i++)
  {
    if (m_others.ValidIndex(i))
    {
      Entity* entity = m_others.GetData(i);
      if (entity->m_type != Entity::TypeVirii && entity->m_type != Entity::TypeCitizen && entity->IsInView())
      {
        START_PROFILE(g_profiler, Entity::GetTypeName(entity->m_type));
        entity->Render(_predictionTime);
        END_PROFILE(g_profiler, Entity::GetTypeName(entity->m_type));
      }
    }
  }
}

// ****************************************************************************
//  Class TeamControls
// ****************************************************************************

#include "Input.h"

// TeamControls' data and its flags encoding live in NeuronCore, because the wire
// protocol serialises them. Advance() stays here: filling the struct in means
// polling the camera, the mouse and the input manager, which is client work and
// has no place in the foundation.
void TeamControls::Advance()
{
  if (g_camera->IsInMode(CameraAccess::ModeBuildingFocus))
    return;

  m_mousePos = g_userInput->GetMousePos3d();

  m_primaryFireTarget |= g_inputManager->controlEvent(ControlUnitPrimaryFireTarget);
  m_secondaryFireTarget |= g_inputManager->controlEvent(ControlUnitSecondaryFireTarget);
  m_primaryFireDirected |= g_inputManager->controlEvent(ControlUnitPrimaryFireDirected) && !g_inputManager->controlEvent(ControlCameraRotate);
  m_secondaryFireDirected |=
    g_inputManager->controlEvent(ControlUnitSecondaryFireDirected) /* && g_inputManager->controlEvent( ControlUnitStartSecondaryFireDirected ) */;
  m_cameraEntityTracking |= g_camera->IsInMode(CameraAccess::ModeEntityTrack);
  m_unitMove |= g_inputManager->controlEvent(ControlUnitSetTarget) && !m_secondaryFireTarget;
  m_unitSecondaryMode |= g_inputManager->controlEvent(ControlUnitStartSecondaryFireDirected);
  m_endSetTarget |= g_inputManager->controlEvent(ControlUnitEndSetTarget);

  InputDetails details;
  if (g_inputManager->controlEvent(ControlUnitMove, details))
  {
    Vector3 right = g_camera->GetControlVector();
    Vector3 front = g_upVector ^ -right;

    Vector3 waypoint = right * -details.x;
    waypoint += front * -details.y;

    m_directUnitMove = true;
    m_directUnitMoveDx = waypoint.x;
    m_directUnitMoveDy = waypoint.z;

    g_controlHelpSystem->RecordCondUsed(ControlHelpAccess::CondMoveCameraOrUnit);
  }

  if (g_inputManager->controlEvent(ControlUnitPrimaryFireDirected, details) && !g_inputManager->controlEvent(ControlCameraRotate))
  {
    m_primaryFireDirected = true;
    m_directUnitFireDx = details.x;
    m_directUnitFireDy = details.y;

    g_controlHelpSystem->RecordCondUsed(ControlHelpAccess::CondSquaddieFire);
  }

  if (m_secondaryFireDirected)
  {
    g_controlHelpSystem->RecordCondUsed(ControlHelpAccess::CondFireAirstrike);
    g_controlHelpSystem->RecordCondUsed(ControlHelpAccess::CondFireGrenades);
    g_controlHelpSystem->RecordCondUsed(ControlHelpAccess::CondFireRocket);
  }
}
