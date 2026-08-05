#include "pch.h"
#include "GlVertex.h"
#include "AppCommands.h"
#include "Debug.h"
#include "LanguageTable.h"
#include "FilesysUtils.h"
#include "FileWriter.h"
#include "Input.h"
#include "TargetCursor.h"
#include "MathUtils.h"
#include "Profiler.h"
#include "Resource.h"
#include "Shape.h"
#include "StringUtils.h"
#include "TextRenderer.h"
#include "TextStreamReaders.h"
#include "Eclipse.h"
#include "GlobalInternet.h"
#include "GlobalWorld.h"
#include "Landscape.h"
#include "LevelFile.h"
#include "Location.h"
#include "GameTime.h"
#include "Building.h"
#include "TrunkPort.h"
#include "BuyNowWindow.h"
#include "WorldPointers.h"
#include "AppState.h"

// ****************************************************************************
// Class GlobalLocation
// ****************************************************************************

GlobalLocation::GlobalLocation()
  : m_id(-1),
    m_available(false),
    m_name("none"),
    m_mapFilename("none"),
    m_missionFilename("none"),
    m_missionCompleted(false),
    m_numSpirits(0)
{
}

void GlobalLocation::AddSpirits(int _count) { m_numSpirits += _count; }

// ****************************************************************************
// Class GlobalBuilding
// ****************************************************************************

GlobalBuilding::GlobalBuilding()
  : m_id(-1),
    m_teamId(-1),
    m_locationId(-1),
    m_type(Building::TypeTrunkPort),
    m_online(false),
    m_link(-1),
    m_shape(nullptr)
{
  m_shape = g_resource->GetShape("TrunkPort.shp");
}

// ****************************************************************************
// Class GlobalEventCondition
// ****************************************************************************

GlobalEventCondition::GlobalEventCondition()
  : m_type(-1),
    m_id(-1),
    m_locationId(-1),
    m_stringId(nullptr),
    m_cutScene(nullptr)
{
}

GlobalEventCondition::GlobalEventCondition(const GlobalEventCondition& _other)
  : m_type(_other.m_type),
    m_id(_other.m_id),
    m_locationId(_other.m_locationId),
    m_stringId(NewStr(_other.m_stringId)),
    m_cutScene(NewStr(_other.m_cutScene))
{
}

GlobalEventCondition::~GlobalEventCondition()
{
  delete[] m_stringId;
  delete[] m_cutScene;
}

void GlobalEventCondition::SetStringId(const char* _stringId)
{
  delete[] m_stringId;
  m_stringId = NewStr(_stringId);
}

void GlobalEventCondition::SetCutScene(char* _cutScene)
{
  delete[] m_cutScene;
  m_cutScene = NewStr(_cutScene);
}

const char* GlobalEventCondition::GetTypeName(int _type)
{
  static const char* names[] = {"AlwaysTrue", "BuildingOnline", "BuildingOffline", "ResearchOwned", "NotInLocation", "DebugKey", "NeverTrue"};

  DEBUG_ASSERT(_type >= 0 && _type < NumConditions);

  return names[_type];
}

int GlobalEventCondition::GetType(const char* _typeName)
{
  for (int i = 0; i < NumConditions; ++i)
  {
    if (stricmp(_typeName, GetTypeName(i)) == 0)
      return i;
  }

  return -1;
}

bool GlobalEventCondition::Evaluate()
{
  switch (m_type)
  {
  case AlwaysTrue:
    return true;

  case BuildingOnline:
  {
    GlobalBuilding* building = g_globalWorld->GetBuilding(m_id, m_locationId);
    if (building)
      return building->m_online;
    break;
  }

  case BuildingOffline:
  {
    GlobalBuilding* building = g_globalWorld->GetBuilding(m_id, m_locationId);
    if (building)
      return !building->m_online;
    break;
  }

  case ResearchOwned:
    return (g_globalWorld->m_research->HasResearch(m_id));

  case NotInLocation:
    return (g_location == nullptr);

  case NeverTrue:
    return false;

  default:
    DEBUG_ASSERT(false);
  }

  return false;
}

void GlobalEventCondition::Save(FileWriter* _out)
{
  _out->printf("%s ", GetTypeName(m_type));

  switch (m_type)
  {
  case AlwaysTrue:
    break;

  case BuildingOnline:
  case BuildingOffline:
    _out->printf(":%s,%d ", g_globalWorld->GetLocationName(m_locationId), m_id);
    break;

  case ResearchOwned:
    _out->printf(":%s ", GlobalResearch::GetTypeName(m_id));
    break;

  case DebugKey:
    _out->printf(":%d ", m_id);
    break;
  }
}

// ****************************************************************************
// Class GlobalEventAction
// ****************************************************************************

const char* GlobalEventAction::GetTypeName(int _type)
{
  static const char* names[] = {"SetMission", "RunScript", "MakeAvailable"};

  DEBUG_ASSERT(_type >= 0 && _type < NumActionTypes);

  return names[_type];
}

GlobalEventAction::GlobalEventAction()
  : m_filename("null")
{
}

void GlobalEventAction::Read(TextReader* _in)
{
  char* action = _in->GetNextToken();

  if (stricmp(action, "SetMission") == 0)
  {
    m_type = SetMission;
    m_locationId = g_globalWorld->GetLocationId(_in->GetNextToken());
    DEBUG_ASSERT(m_locationId != -1);
    m_filename = _in->GetNextToken();
  }
  else if (stricmp(action, "RunScript") == 0)
  {
    m_type = RunScript;
    m_filename = _in->GetNextToken();
  }
  else if (stricmp(action, "MakeAvailable") == 0)
  {
    m_type = MakeAvailable;
    m_locationId = g_globalWorld->GetLocationId(_in->GetNextToken());
    DEBUG_ASSERT(m_locationId != -1);
  }
  else
    DEBUG_ASSERT(false);
}

void GlobalEventAction::Write(FileWriter* _out)
{
  _out->printf("\t\tAction %-10s ", GetTypeName(m_type));

  char const* locationName = g_globalWorld->GetLocationName(m_locationId);

  switch (m_type)
  {
  case SetMission:
    _out->printf("%s %s", locationName, m_filename.c_str());
    break;
  case RunScript:
    _out->printf("%s", m_filename.c_str());
    break;
  case MakeAvailable:
    _out->printf("%s", locationName);
    break;

  default:
    DEBUG_ASSERT(false);
  }

  _out->printf("\n");
}

void GlobalEventAction::Execute()
{
  switch (m_type)
  {
  case SetMission:
  {
    GlobalLocation* loc = g_globalWorld->GetLocation(m_locationId);
    DEBUG_ASSERT(loc);
    loc->m_missionFilename = m_filename;
    break;
  }
  case RunScript:
  {
    g_script->RunScript(m_filename.c_str());
    break;
  }
  case MakeAvailable:
  {
    GlobalLocation* loc = g_globalWorld->GetLocation(m_locationId);
    DEBUG_ASSERT(loc);
    loc->m_available = true;
    break;
  }

  default:
    DEBUG_ASSERT(false);
  }
}

// ****************************************************************************
// Class GlobalEvent
// ****************************************************************************

GlobalEvent::GlobalEvent() {}

GlobalEvent::GlobalEvent(GlobalEvent& _other)
{
  for (GlobalEventCondition* condition : _other.m_conditions)
  {
    m_conditions.push_back(new GlobalEventCondition(*condition));
  }

  for (GlobalEventAction* action : _other.m_actions)
  {
    m_actions.push_back(new GlobalEventAction(*action));
  }
}

bool GlobalEvent::Evaluate()
{
  bool success = true;

  for (GlobalEventCondition* condition : m_conditions)
  {
    if (!condition->Evaluate())
    {
      success = false;
      break;
    }
  }

  return success;
}

bool GlobalEvent::Execute()
{
  if (m_actions.empty())
    return true;

  GlobalEventAction* action = m_actions[0];
  m_actions.erase(m_actions.begin());
  action->Execute();
  delete action;

  if (m_actions.empty())
    return true;

  return false;
}

void GlobalEvent::MakeAlwaysTrue()
{
  if (m_conditions.size() == 1 && m_conditions[0]->m_type == GlobalEventCondition::AlwaysTrue)
    return;

  for (GlobalEventCondition* condition : m_conditions)
    delete condition;
  m_conditions.clear();

  auto cond = new GlobalEventCondition();
  cond->m_type = GlobalEventCondition::AlwaysTrue;
  m_conditions.push_back(cond);
}

void GlobalEvent::Read(TextReader* _in)
{
  //
  // Parse conditions line

  while (_in->TokenAvailable())
  {
    char* conditionTypeName = _in->GetNextToken();

    auto condition = new GlobalEventCondition;
    condition->m_type = condition->GetType(conditionTypeName);
    DEBUG_ASSERT(condition->m_type != -1);

    switch (condition->m_type)
    {
    case GlobalEventCondition::AlwaysTrue:
    case GlobalEventCondition::NotInLocation:
      break;

    case GlobalEventCondition::BuildingOffline:
    case GlobalEventCondition::BuildingOnline:
      condition->m_locationId = g_globalWorld->GetLocationId(_in->GetNextToken());
      condition->m_id = atoi(_in->GetNextToken());
      DEBUG_ASSERT(condition->m_locationId != -1);
      break;

    case GlobalEventCondition::ResearchOwned:
      condition->m_id = GlobalResearch::GetType(_in->GetNextToken());
      DEBUG_ASSERT(condition->m_id != -1);
      break;

    case GlobalEventCondition::DebugKey:
      condition->m_id = atoi(_in->GetNextToken());
      break;
    }

    m_conditions.push_back(condition);
  }

  //
  // Parse actions

  while (_in->ReadLine())
  {
    if (_in->TokenAvailable())
    {
      char* word = _in->GetNextToken();
      if (stricmp(word, "end") == 0)
        break;
      DEBUG_ASSERT(stricmp(word, "action") == 0);

      auto action = new GlobalEventAction;
      action->Read(_in);
      m_actions.push_back(action);
    }
  }
}

void GlobalEvent::Write(FileWriter* _out)
{
  _out->printf("\tEvent ");

  for (GlobalEventCondition* gec : m_conditions)
  {
    gec->Save(_out);
  }

  _out->printf("\n");

  for (GlobalEventAction* gea : m_actions)
  {
    gea->Write(_out);
  }

  _out->printf("\t\tEnd\n");
}

// ****************************************************************************
// Class GlobalResearch
// ****************************************************************************

GlobalResearch::GlobalResearch()
  : m_researchPoints(0),
    m_researchTimer(0.0f)
{
  for (int i = 0; i < NumResearchItems; ++i)
  {
    m_researchLevel[i] = 0;
    m_researchProgress[i] = 0;
  }

  m_currentResearch = TypeSquad;
}

void GlobalResearch::AddResearch(int _type)
{
  DEBUG_ASSERT(_type >= 0 && _type < NumResearchItems);

  if (m_researchLevel[_type] < 1)
  {
    m_researchProgress[_type] = RequiredProgress(0);
    EvaluateLevel(_type);
  }
}

bool GlobalResearch::HasResearch(int _type)
{
  DEBUG_ASSERT(_type >= 0 && _type < NumResearchItems);

  return (m_researchLevel[_type] > 0);
}

int GlobalResearch::CurrentProgress(int _type)
{
  DEBUG_ASSERT(_type >= 0 && _type < NumResearchItems);

  return m_researchProgress[_type];
}

int GlobalResearch::RequiredProgress(int _level)
{
  DEBUG_ASSERT(_level >= 0 && _level < 4);

  static int s_requiredProgress[] = {1, 50, 100, 200};

  return s_requiredProgress[_level];
}

void GlobalResearch::EvaluateLevel(int _type)
{
  int currentLevel = m_researchLevel[_type];

  if (currentLevel < 4)
  {
    int newLevel = 0;

    int requiredProgress = RequiredProgress(currentLevel);

    if (m_researchProgress[_type] >= requiredProgress)
    {
      // Level change has just occurred
      m_researchLevel[_type]++;
      m_researchProgress[_type] -= requiredProgress;

      std::string sepStringId = std::format("research_{}_v{}", GetTypeName(_type), m_researchLevel[_type]);
      StrToLower(sepStringId.data());

      if (ISLANGUAGEPHRASE(sepStringId.c_str()))
      {
      }

      if (currentLevel > 0)
      {
        // This action should only go off if a player UPGRADES an existing research item
        // Not if he finds a new one
        g_taskManagerInterface->SetCurrentMessage(TaskManagerInterfaceAccess::MessageResearchUpgrade, _type, 4.0f);
      }
    }
  }
}

void GlobalResearch::SetCurrentResearch(int _type)
{
  int currentLevel = m_researchLevel[_type];

  if (currentLevel == 4)
  {
    // Fully researched already
    return;
  }

  if (m_currentResearch != _type)
  {
    m_currentResearch = _type;

    std::string sepStringId = std::format("research_{}", GetTypeName(_type));
    StrToLower(sepStringId.data());

    if (ISLANGUAGEPHRASE(sepStringId.c_str()))
    {
    }
  }
}

void GlobalResearch::GiveResearchPoints(int _numPoints) { m_researchPoints += _numPoints; }

void GlobalResearch::AdvanceResearch()
{
  if (m_researchPoints > 0 && CurrentLevel(m_currentResearch) < 4)
  {
    m_researchTimer -= g_advanceTime;
    if (m_researchTimer <= 0.0f)
    {
      m_researchTimer = GLOBALRESEARCH_TIMEPERPOINT;
      IncreaseProgress(1);
      --m_researchPoints;
    }
  }
}

void GlobalResearch::SetCurrentProgress(int _type, int _progress)
{
  DEBUG_ASSERT(_type >= 0 && _type < NumResearchItems);

  m_researchProgress[_type] = _progress;
  EvaluateLevel(_type);
}

void GlobalResearch::IncreaseProgress(int _amount)
{
  m_researchProgress[m_currentResearch] += _amount;
  EvaluateLevel(m_currentResearch);
}

void GlobalResearch::DecreaseProgress(int _amount)
{
  m_researchProgress[m_currentResearch] -= _amount;
  EvaluateLevel(m_currentResearch);
}

int GlobalResearch::CurrentLevel(int _type)
{
  DEBUG_ASSERT(_type >= 0 && _type < NumResearchItems);

  return m_researchLevel[_type];
}

void GlobalResearch::Write(FileWriter* _out)
{
  _out->printf("Research_StartDefinition\n");

  for (int i = 0; i < NumResearchItems; ++i)
    _out->printf("\tResearch %s %d %d\n", GetTypeName(i), CurrentProgress(i), CurrentLevel(i));

  _out->printf("\tCurrentResearch %s\n", GetTypeName(m_currentResearch));
  _out->printf("\tCurrentPoints %d\n", m_researchPoints);
  _out->printf("Research_EndDefinition\n\n");
}

void GlobalResearch::Read(TextReader* _in)
{
  while (_in->ReadLine())
  {
    if (!_in->TokenAvailable())
      continue;
    char* word = _in->GetNextToken();

    if (stricmp(word, "research_enddefinition") == 0)
      return;
    if (stricmp(word, "Research") == 0)
    {
      char* type = _in->GetNextToken();
      int progress = atoi(_in->GetNextToken());
      int level = atoi(_in->GetNextToken());

      int researchType = GetType(type);
      m_researchLevel[researchType] = level;
      SetCurrentProgress(researchType, progress);
    }
    else if (stricmp(word, "CurrentResearch") == 0)
    {
      char* type = _in->GetNextToken();
      int researchType = GetType(type);
      m_currentResearch = researchType;
    }
    else if (stricmp(word, "CurrentPoints") == 0)
    {
      int points = atoi(_in->GetNextToken());
      m_researchPoints = points;
    }
    else
      ASSERT_TEXT(false, "Error loading GlobalResearch");
  }
}

const char* GlobalResearch::GetTypeName(int _type)
{
  const char* names[] = {"Citizen", "Officer", "Squad", "Laser", "Grenade", "Rocket", "Controller", "AirStrike", "Armour", "TaskManager", "Engineer"};

  DEBUG_ASSERT(_type >= 0 && _type < NumResearchItems);
  return names[_type];
}

const char* GlobalResearch::GetTypeNameTranslated(int _type)
{
  const char* typeName = GetTypeName(_type);

  const std::string stringId = std::format("researchname_{}", typeName);

  if (ISLANGUAGEPHRASE(stringId.c_str()))
    return LANGUAGEPHRASE(stringId.c_str());
  return typeName;
}

int GlobalResearch::GetType(char* _name)
{
  for (int i = 0; i < NumResearchItems; ++i)
  {
    if (stricmp(_name, GetTypeName(i)) == 0)
      return i;
  }

  return -1;
}

// ****************************************************************************
// Class GlobalWorld
// ****************************************************************************

void ColourShapeFragment(ShapeFragment* _frag, const RGBAColour& _colour)
{
  if (_frag->m_numColours == 0)
    _frag->m_colours = new RGBAColour[1];
  _frag->m_colours[0] = _colour;

  for (int i = 0; i < _frag->m_numVertices; ++i)
  {
    VertexPosCol* vert = &_frag->m_vertices[i];
    vert->m_colId = 0;
  }

  for (int i = 0; i < static_cast<int>(_frag->m_childFragments.size()); ++i)
    ColourShapeFragment(_frag->m_childFragments[i].get(), _colour);
}

SphereWorld::SphereWorld()
  : m_shapeOuter(nullptr),
    m_shapeMiddle(nullptr),
    m_shapeInner(nullptr)
{
  m_shapeOuter = g_resource->GetShape("GlobalWorldOuter.shp");
  m_shapeMiddle = g_resource->GetShape("GlobalWorldMiddle.shp");
  m_shapeInner = g_resource->GetShape("GlobalWorldInner.shp");
}

void SphereWorld::AddLocation(int _locationId)
{
  // Initialise the sphere world to
  if (_locationId < static_cast<int>(m_spirits.size()))
    return;

  const int oldNumLocations = static_cast<int>(m_spirits.size());
  m_spirits.resize(_locationId + 1);

  // Initialise the spirits for the new worlds. resize kept the existing lists,
  // so only the ones past the old end need filling.
  for (int locationId = oldNumLocations; locationId < static_cast<int>(m_spirits.size()); ++locationId)
  {
    // Generate some new spirits
    GlobalLocation* loc = g_globalWorld->GetLocation(locationId);
    if (loc)
    {
      const int numSpirits = stricmp(loc->m_name.c_str(), "Receiver") == 0 ? 60 : 10;
      for (int i = 0; i < numSpirits; ++i)
      {
        float value = frand();
        m_spirits[locationId].push_back(value);
      }
    }
  }
}

void SphereWorld::Render()
{
  // For some reason this fixes a slight glitch in the first few frames of the
  // start sequence on my machine. God knows why, but it won't cause any harm.
  static int frameCount = 0;
  if (frameCount < 10)
  {
    frameCount++;
    return;
  }

  RenderWorldShape();
  RenderIslands();
  RenderTrunkLinks();

  if (!g_editing)
  {
    RenderSpirits();
    RenderHeaven();
  }

  glEnable(GL_CULL_FACE); // CRASH WORKAROUND - FIX AND DELETE ASAP
}

void SphereWorld::RenderSpirits()
{
  START_PROFILE(g_profiler, "Spirits");

  //
  // Advance all spirits

  for (int locationId = 0; locationId < static_cast<int>(m_spirits.size()); ++locationId)
  {
    GlobalLocation* location = g_globalWorld->GetLocation(locationId);
    if (location)
    {
      bool isReceiver = (stricmp(location->m_name.c_str(), "Receiver") == 0);

      if (isReceiver && frand(30) < 1.0f)
        m_spirits[locationId].insert(m_spirits[locationId].begin(), 0.0f);
      else if (!isReceiver && frand(300) < 1.0f)
        m_spirits[locationId].insert(m_spirits[locationId].begin(), 1.0f);

      if (location->m_numSpirits > 0 && frand(20) < 1.0f)
      {
        m_spirits[locationId].insert(m_spirits[locationId].begin(), 1.0f);
        location->m_numSpirits--;
      }

      for (int i = 0; i < static_cast<int>(m_spirits[locationId].size()); ++i)
      {
        float* thisSpirit = &m_spirits[locationId][i];

        if (isReceiver)
          *thisSpirit += g_advanceTime * 0.02f;
        else
          *thisSpirit -= g_advanceTime * 0.02f;

        if (*thisSpirit >= 1.0f || *thisSpirit <= 0.0f)
        {
          m_spirits[locationId].erase(m_spirits[locationId].begin() + i);
          --i;
        }
      }
    }
  }

  //
  // Render all spirits

  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glDepthMask(false);

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Glow.bmp"));

  DirectX::XMFLOAT3 const cameraRight = g_camera->GetRight();
  DirectX::XMFLOAT3 const cameraUp = g_camera->GetUp();
  DirectX::XMVECTOR const camRight = DirectX::XMLoadFloat3(&cameraRight);
  DirectX::XMVECTOR const camUp = DirectX::XMLoadFloat3(&cameraUp);

  for (int locationId = 0; locationId < static_cast<int>(m_spirits.size()); ++locationId)
  {
    GlobalLocation* location = g_globalWorld->GetLocation(locationId);
    if (location)
    {
      for (int i = 0; i < static_cast<int>(m_spirits[locationId].size()); ++i)
      {
        float* thisSpirit = &m_spirits[locationId][i];

        DirectX::XMFLOAT3 const fromPos = g_globalWorld->GetLocationPosition(locationId);

        float alphaValue = *thisSpirit * 3.0f;
        if (alphaValue > 1.0f)
          alphaValue = 1.0f;

        DirectX::XMFLOAT3 positionStore;
        DirectX::XMStoreFloat3(&positionStore, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&fromPos), *thisSpirit));
        float timeOffset = g_gameTime / 2.0f;
        float posOffset = 1000;

        positionStore.x += sinf(*thisSpirit * 14 + timeOffset) * posOffset;
        positionStore.y += sinf(*thisSpirit * 15 + timeOffset) * posOffset;
        positionStore.z += sinf(*thisSpirit * 16 + timeOffset) * posOffset;
        DirectX::XMVECTOR const position = DirectX::XMLoadFloat3(&positionStore);

        float scale = 0.4f;

        glColor4f(0.6f, 0.2f, 0.1f, alphaValue);
        glBegin(GL_QUADS);
        glTexCoord2f(0.5f, 0.5f);
        EmitVertex(DirectX::XMVectorAdd(position, DirectX::XMVectorScale(camUp, 300.0f * scale)));
        glTexCoord2f(0.5f, 0.5f);
        EmitVertex(DirectX::XMVectorAdd(position, DirectX::XMVectorScale(camRight, 300.0f * scale)));
        glTexCoord2f(0.5f, 0.5f);
        EmitVertex(DirectX::XMVectorSubtract(position, DirectX::XMVectorScale(camUp, 300.0f * scale)));
        glTexCoord2f(0.5f, 0.5f);
        EmitVertex(DirectX::XMVectorSubtract(position, DirectX::XMVectorScale(camRight, 300.0f * scale)));
        glEnd();

        glColor4f(0.6f, 0.2f, 0.1f, alphaValue);
        glBegin(GL_QUADS);
        glTexCoord2f(0.5f, 0.5f);
        EmitVertex(DirectX::XMVectorAdd(position, DirectX::XMVectorScale(camUp, 100.0f * scale)));
        glTexCoord2f(0.5f, 0.5f);
        EmitVertex(DirectX::XMVectorAdd(position, DirectX::XMVectorScale(camRight, 100.0f * scale)));
        glTexCoord2f(0.5f, 0.5f);
        EmitVertex(DirectX::XMVectorSubtract(position, DirectX::XMVectorScale(camUp, 100.0f * scale)));
        glTexCoord2f(0.5f, 0.5f);
        EmitVertex(DirectX::XMVectorSubtract(position, DirectX::XMVectorScale(camRight, 100.0f * scale)));
        glEnd();

        glColor4f(0.6f, 0.2f, 0.1f, alphaValue / 4.0f);
        glBegin(GL_QUADS);
        glTexCoord2i(0, 0);
        EmitVertex(DirectX::XMVectorAdd(position, DirectX::XMVectorScale(camUp, 6000.0f * scale)));
        glTexCoord2i(1, 0);
        EmitVertex(DirectX::XMVectorAdd(position, DirectX::XMVectorScale(camRight, 6000.0f * scale)));
        glTexCoord2i(1, 1);
        EmitVertex(DirectX::XMVectorSubtract(position, DirectX::XMVectorScale(camUp, 6000.0f * scale)));
        glTexCoord2i(0, 1);
        EmitVertex(DirectX::XMVectorSubtract(position, DirectX::XMVectorScale(camRight, 6000.0f * scale)));
        glEnd();
      }
    }
  }

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glDisable(GL_TEXTURE_2D);

  glDepthMask(true);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_BLEND);
  glEnable(GL_CULL_FACE);

  END_PROFILE(g_profiler, "Spirits");
}

void SphereWorld::RenderWorldShape()
{
  START_PROFILE(g_profiler, "Shape");

  g_globalWorld->SetupLights();

  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);

  float spec = 0.5f;
  float diffuse = 1.0f;
  float amb = 0.0f;
  GLfloat materialShininess[] = {10.0f};
  GLfloat materialSpecular[] = {spec, spec, spec, 1.0f};
  GLfloat materialDiffuse[] = {diffuse, diffuse, diffuse, 1.0f};
  GLfloat ambCol[] = {amb, amb, amb, 1.0f};

  glMaterialfv(GL_FRONT, GL_SPECULAR, materialSpecular);
  glMaterialfv(GL_FRONT, GL_DIFFUSE, materialDiffuse);
  glMaterialfv(GL_FRONT, GL_SHININESS, materialShininess);
  glMaterialfv(GL_FRONT, GL_AMBIENT, ambCol);

  glPushMatrix();
  glScalef(120.0f, 120.0f, 120.0f);
  glEnable(GL_NORMALIZE);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glDisable(GL_CULL_FACE);

  //
  // Render outer

  DirectX::XMFLOAT4X4 identity; // was g_identityMatrix34
  DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixIdentity());
  m_shapeOuter->Render(0.0f, identity);
  m_shapeMiddle->Render(0.0f, identity);
  m_shapeInner->Render(0.0f, identity);

  glDisable(GL_NORMALIZE);
  glPopMatrix();

  glDisable(GL_COLOR_MATERIAL);
  glDisable(GL_LIGHTING);
  glDisable(GL_LIGHT0);
  glDisable(GL_LIGHT1);

  END_PROFILE(g_profiler, "Shape");
}

void SphereWorld::RenderTrunkLinks()
{
  // if( g_editing ) return;

  // Matrix34(0) was the identity, whatever the argument looked like.
  DirectX::XMFLOAT4X4 rootMat;
  DirectX::XMStoreFloat4x4(&rootMat, DirectX::XMMatrixIdentity());

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glDepthMask(false);

  glBegin(GL_QUADS);

  for (GlobalBuilding* building : g_globalWorld->m_buildings)
  {
    if (building->m_type == Building::TypeTrunkPort && building->m_link != -1)
    {
      GlobalLocation* fromLoc = g_globalWorld->GetLocation(building->m_locationId);
      GlobalLocation* toLoc = g_globalWorld->GetLocation(building->m_link);

      if (fromLoc && toLoc && (fromLoc->m_available && toLoc->m_available) || g_editing)
      {
        DirectX::XMFLOAT3 const fromPosStore = g_globalWorld->GetLocationPosition(building->m_locationId);
        DirectX::XMFLOAT3 const toPosStore = g_globalWorld->GetLocationPosition(building->m_link);
        DirectX::XMVECTOR const fromPos = DirectX::XMLoadFloat3(&fromPosStore);
        DirectX::XMVECTOR const toPos = DirectX::XMLoadFloat3(&toPosStore);

        if (building->m_online)
          glColor4f(0.4f, 0.3f, 1.0f, 1.0f);
        else
          glColor4f(0.4f, 0.3f, 1.0f, 0.4f);

        // fromPos *= 120.0f;
        // toPos *= 120.0f;

        DirectX::XMVECTOR const midPoint = DirectX::XMVectorAdd(fromPos, DirectX::XMVectorScale(DirectX::XMVectorSubtract(toPos, fromPos), 0.5f));
        DirectX::XMFLOAT3 const cameraPos = g_camera->GetPos();
        DirectX::XMVECTOR const camToMidPoint = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&cameraPos), midPoint);
        DirectX::XMVECTOR const rightAngle = DirectX::XMVectorScale(
          DirectX::XMVector3Normalize(DirectX::XMVector3Cross(camToMidPoint, DirectX::XMVectorSubtract(midPoint, toPos))), 200.0f);

        EmitVertex(DirectX::XMVectorSubtract(fromPos, rightAngle));
        EmitVertex(DirectX::XMVectorAdd(fromPos, rightAngle));
        EmitVertex(DirectX::XMVectorAdd(toPos, rightAngle));
        EmitVertex(DirectX::XMVectorSubtract(toPos, rightAngle));
      }
    }
  }

  glEnd();
  glDepthMask(true);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_BLEND);
}

void SphereWorld::RenderHeaven()
{
  START_PROFILE(g_profiler, "Heaven");

  g_globalWorld->SetupLights();

  //
  // Render the central repository of spirits

  glPushMatrix();
  glScalef(120.0f, 120.0f, 120.0f);

  DirectX::XMFLOAT3 const cameraUp = g_camera->GetUp();
  DirectX::XMFLOAT3 const cameraRight = g_camera->GetRight();
  DirectX::XMVECTOR const camUp = DirectX::XMLoadFloat3(&cameraUp);
  DirectX::XMVECTOR const camRight = DirectX::XMLoadFloat3(&cameraRight);

  glDepthMask(false);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Glow.bmp"));

  for (int i = 0; i < 50; ++i)
  {
    DirectX::XMVECTOR const pos = DirectX::XMVectorSet(sinf(i / g_gameTime + i) * 20, sinf(g_gameTime + i) * i, cosf(i / g_gameTime + i) * 20, 0.0f);

    float size = i;

    glColor4f(0.6f, 0.2f, 0.1f, 0.9f);

    glBegin(GL_QUADS);
    glTexCoord2i(0, 0);
    EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorSubtract(pos, DirectX::XMVectorScale(camRight, size)), DirectX::XMVectorScale(camUp, size)));
    glTexCoord2i(1, 0);
    EmitVertex(DirectX::XMVectorAdd(DirectX::XMVectorAdd(pos, DirectX::XMVectorScale(camRight, size)), DirectX::XMVectorScale(camUp, size)));
    glTexCoord2i(1, 1);
    EmitVertex(DirectX::XMVectorSubtract(DirectX::XMVectorAdd(pos, DirectX::XMVectorScale(camRight, size)), DirectX::XMVectorScale(camUp, size)));
    glTexCoord2i(0, 1);
    EmitVertex(
      DirectX::XMVectorSubtract(DirectX::XMVectorSubtract(pos, DirectX::XMVectorScale(camRight, size)), DirectX::XMVectorScale(camUp, size)));
    glEnd();
  }

  glPopMatrix();

  //
  // Render god rays going down

  /*
      glBindTexture   ( GL_TEXTURE_2D, g_resource->GetTexture( "Textures/GodRay.bmp" ) );

    for (int i = 0; i < g_globalWorld->m_locations.Size(); ++i)
    {
      GlobalLocation *loc = g_globalWorld->m_locations.GetData(i);
          Vector3 islandPos = g_globalWorld->GetLocationPosition(loc->m_id );
          Vector3 centrePos = g_zeroVector;

          for( int j = 0; j < 6; ++j )
          {
              Vector3 godRayPos = islandPos;

              godRayPos.x += sinf( g_gameTime + i + j/2 ) * 1000;
              godRayPos.y += sinf( g_gameTime + i + j/2 ) * 1000;
              godRayPos.z += cosf( g_gameTime + i + j/2 ) * 1000;

              Vector3 camToCentre = g_camera->GetPos() - centrePos;
              Vector3 lineToCentre = camToCentre ^ ( centrePos - godRayPos );
              lineToCentre.Normalise();

              glColor4f( 0.6f, 0.2f, 0.1f, 0.8f);

              glBegin( GL_QUADS );
                  glTexCoord2f(0.75f,0);      glVertex3fv( (centrePos - lineToCentre * 1000).GetData() );
                  glTexCoord2f(0.75f,1);      glVertex3fv( (centrePos + lineToCentre * 1000).GetData() );
                  glTexCoord2f(0.05f,1);      glVertex3fv( (godRayPos + lineToCentre * 1000).GetData() );
                  glTexCoord2f(0.05f,0);      glVertex3fv( (godRayPos - lineToCentre * 1000).GetData() );
              glEnd();
          }
      }

  */

  glDisable(GL_TEXTURE_2D);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_BLEND);
  glDepthMask(true);

  END_PROFILE(g_profiler, "Heaven");
}

void SphereWorld::RenderIslands()
{
  if (g_camera->IsInMode(CameraAccess::ModeSphereWorldIntro) || g_camera->IsInMode(CameraAccess::ModeSphereWorldOutro))
    return;

  //
  // Render the islands

  START_PROFILE(g_profiler, "Islands");

  glMatrixMode(GL_MODELVIEW);

  DirectX::XMFLOAT3 rayStart{0.0f, 0.0f, 0.0f};
  DirectX::XMFLOAT3 rayDir{0.0f, 0.0f, 0.0f};
  g_camera->GetClickRay(g_target->X(), g_target->Y(), &rayStart, &rayDir);

  DirectX::XMFLOAT3 const cameraRight = g_camera->GetRight();
  DirectX::XMFLOAT3 const cameraUp = g_camera->GetUp();
  DirectX::XMVECTOR const camRight = DirectX::XMLoadFloat3(&cameraRight);
  DirectX::XMVECTOR const camUp = DirectX::XMLoadFloat3(&cameraUp);

  //    glColor4f       ( 1.0f, 1.0f, 1.0f, 1.0f );
  glColor4f(0.6f, 0.2f, 0.1f, 1.0f);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(false);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glEnable(GL_BLEND);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Starburst.bmp"));

  for (GlobalLocation* loc : g_globalWorld->m_locations)
  {
    if (loc->m_available || g_editing)
    {
      DirectX::XMFLOAT3 const islandPosStore = g_globalWorld->GetLocationPosition(loc->m_id);
      DirectX::XMVECTOR const islandPos = DirectX::XMLoadFloat3(&islandPosStore);

      int numRedraws = 5;
      if (!loc->m_missionCompleted && stricmp(loc->m_missionFilename.c_str(), "null") != 0 && fmodf(g_gameTime, 1.0f) < 0.7f)
        numRedraws = 10;

      glBegin(GL_QUADS);
      for (int j = 0; j <= numRedraws; ++j)
      {
        glTexCoord2i(0, 0);
        EmitVertex(DirectX::XMVectorAdd(islandPos, DirectX::XMVectorScale(camUp, 1000.0f * j)));
        glTexCoord2i(1, 0);
        EmitVertex(DirectX::XMVectorAdd(islandPos, DirectX::XMVectorScale(camRight, 1000.0f * j)));
        glTexCoord2i(1, 1);
        EmitVertex(DirectX::XMVectorSubtract(islandPos, DirectX::XMVectorScale(camUp, 1000.0f * j)));
        glTexCoord2i(0, 1);
        EmitVertex(DirectX::XMVectorSubtract(islandPos, DirectX::XMVectorScale(camRight, 1000.0f * j)));
      }
      glEnd();
    }
  }

  glDisable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(true);
  glEnable(GL_DEPTH_TEST);

  //
  // Render the islands names

  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

  for (GlobalLocation* loc : g_globalWorld->m_locations)
  {
    if (loc->m_available || g_editing)
    {
      DirectX::XMFLOAT3 islandPos = g_globalWorld->GetLocationPosition(loc->m_id);
      char* islandName = strdup(g_globalWorld->GetLocationNameTranslated(loc->m_id));
      strupr(islandName);

      // size is overwritten on the next line; the sqrtf is kept because it is
      // what the legacy code did, dead or not.
      DirectX::XMFLOAT3 const cameraPos = g_camera->GetPos();
      float size = 5.0f * sqrtf(DirectX::XMVectorGetX(DirectX::XMVector3Length(
                            DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&cameraPos), DirectX::XMLoadFloat3(&islandPos)))));
      size = 1000.0f;

      DirectX::XMFLOAT3 titlePos;
      DirectX::XMFLOAT3 belowPos;
      DirectX::XMStoreFloat3(&titlePos,
                             DirectX::XMVectorMultiplyAdd(camUp, DirectX::XMVectorReplicate(size * 1.5f), DirectX::XMLoadFloat3(&islandPos)));
      DirectX::XMStoreFloat3(&belowPos,
                             DirectX::XMVectorNegativeMultiplySubtract(camUp, DirectX::XMVectorReplicate(size), DirectX::XMLoadFloat3(&islandPos)));

      g_gameFont.SetRenderShadow(true);
      glColor4f(0.7f, 0.7f, 0.7f, 0.0f);
      g_gameFont.DrawText3DCentre(titlePos, size * 3.0f, islandName);

      if (g_editing)
      {
        g_gameFont.DrawText3DCentre(islandPos, size, loc->m_mapFilename.c_str());
        g_gameFont.DrawText3DCentre(belowPos, size, loc->m_missionFilename.c_str());
      }

      // The shadow pass above draws at islandPos; the lit pass below draws at
      // the same point nudged up and right, exactly as the legacy code did by
      // mutating islandPos between the two.
      DirectX::XMVECTOR nudged = DirectX::XMVectorMultiplyAdd(camUp, DirectX::XMVectorReplicate(size * 0.3f), DirectX::XMLoadFloat3(&islandPos));
      nudged = DirectX::XMVectorMultiplyAdd(camRight, DirectX::XMVectorReplicate(size * 0.1f), nudged);
      DirectX::XMStoreFloat3(&islandPos, nudged);
      DirectX::XMStoreFloat3(&titlePos, DirectX::XMVectorMultiplyAdd(camUp, DirectX::XMVectorReplicate(size * 1.5f), nudged));
      DirectX::XMStoreFloat3(&belowPos, DirectX::XMVectorNegativeMultiplySubtract(camUp, DirectX::XMVectorReplicate(size), nudged));

      g_gameFont.SetRenderShadow(false);
      glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
      if (stricmp(loc->m_missionFilename.c_str(), "null") == 0)
        glColor4f(0.5f, 0.5f, 0.5f, 1.0f);

      g_gameFont.DrawText3DCentre(titlePos, size * 3.0f, islandName);

      if (g_editing)
      {
        g_gameFont.DrawText3DCentre(islandPos, size, loc->m_mapFilename.c_str());
        g_gameFont.DrawText3DCentre(belowPos, size, loc->m_missionFilename.c_str());
      }

      free(islandName);
    }
  }

  END_PROFILE(g_profiler, "Islands");
}

// ****************************************************************************
// Class SphereWorld
// ****************************************************************************

GlobalWorld::GlobalWorld()
  : m_myTeamId(255),
    m_editorMode(0),
    m_editorSelectionId(-1),
    m_nextLocationId(0),
    m_nextBuildingId(0),
    m_locationRequested(-1)
{
  m_globalInternet = new GlobalInternet();
  m_sphereWorld = new SphereWorld();
  m_research = new GlobalResearch();
}

GlobalWorld::GlobalWorld(GlobalWorld& _other)
  : m_globalInternet(nullptr),
    m_sphereWorld(nullptr),
    m_myTeamId(_other.m_myTeamId),
    m_nextLocationId(0),
    m_nextBuildingId(0),
    m_locationRequested(-1)
{
  m_research = new GlobalResearch();

  for (GlobalLocation* location : _other.m_locations)
  {
    m_locations.push_back(new GlobalLocation(*location));
  }
  for (GlobalBuilding* building : _other.m_buildings)
  {
    m_buildings.push_back(new GlobalBuilding(*building));
  }
  for (GlobalEvent* event : _other.m_events)
  {
    m_events.push_back(new GlobalEvent(*event));
  }
}

GlobalWorld::~GlobalWorld()
{
  for (GlobalLocation* location : m_locations)
    delete location;
  m_locations.clear();
  for (GlobalBuilding* building : m_buildings)
    delete building;
  m_buildings.clear();
  for (GlobalEvent* event : m_events)
    delete event;
  m_events.clear();

  delete m_globalInternet;
  delete m_sphereWorld;
  delete m_research;
}

void GlobalWorld::Advance()
{
  if (g_editing)
  {
    if (m_editorMode == 0)
    {
      // Edit locations
      if (g_inputManager->controlEvent(ControlSelectLocation))
      {
        DirectX::XMFLOAT3 rayStart{0.0f, 0.0f, 0.0f};
        DirectX::XMFLOAT3 rayDir{0.0f, 0.0f, 0.0f};
        g_camera->GetClickRay(g_target->X(), g_target->Y(), &rayStart, &rayDir);
        int locId = LocationHit(rayStart, rayDir);
        if (locId != -1)
        {
          GlobalLocation* loc = GetLocation(locId);
          g_requestedLocationId = locId;
          g_requestedMission = loc->m_missionFilename;
          g_requestedMap = loc->m_mapFilename;
        }
      }
    }
    else
    {
      // Move locations
      if (g_inputManager->controlEvent(ControlSelectLocation))
      {
        DirectX::XMFLOAT3 rayStart{0.0f, 0.0f, 0.0f};
        DirectX::XMFLOAT3 rayDir{0.0f, 0.0f, 0.0f};
        g_camera->GetClickRay(g_target->X(), g_target->Y(), &rayStart, &rayDir);
        m_editorSelectionId = LocationHit(rayStart, rayDir);
      }
      else if (g_inputManager->controlEvent(ControlLocationDragActive))
      {
        GlobalLocation* loc = GetLocation(m_editorSelectionId);
        if (loc)
        {
          DirectX::XMFLOAT3 const mousePos3D = g_userInput->GetMousePos3d();
          DirectX::XMStoreFloat3(&loc->m_pos, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&mousePos3D), 1.0f / 120.0f));
        }
      }
      else if (g_inputManager->controlEvent(ControlDeselectLocation))
        m_editorSelectionId = -1;
    }
  }
  else
  {
    bool chatLog = false;

    // Has the user clicked on a location?
    if (g_inputManager->controlEvent(ControlSelectLocation) && m_locationRequested == -1 && EclGetWindows()->size() == 0 && !chatLog)
    {
      DirectX::XMFLOAT3 rayStart{0.0f, 0.0f, 0.0f};
      DirectX::XMFLOAT3 rayDir{0.0f, 0.0f, 0.0f};
      g_camera->GetClickRay(g_target->X(), g_target->Y(), &rayStart, &rayDir);
      int locId = LocationHit(rayStart, rayDir);
      if (locId != -1)
      {
        GlobalLocation* loc = GetLocation(locId);
        if (loc->m_missionFilename != "null" && loc->m_available)
        {
          if (!g_script->IsRunningScript())
          {
            if (!g_appCommands->HasBoughtGame())
            {
              // We're not registered, we should run a script to end
              if (!(loc->m_mapFilename == "MapGarden.txt" || loc->m_mapFilename == "MapContainment.txt"))
              {
                // Buy me URL
                EclRegisterWindow(std::make_unique<BuyNowWindow>());

                // Bar Location
                return;
              }
            }
          }

          // Default behaviour is to go the location
          m_locationRequested = locId;
          g_renderer->StartFadeOut();
        }
      }
    }
    // Is the cursor attracted to a point?
    else if (m_locationRequested == -1 && EclGetWindows()->size() == 0 && !chatLog)
    {
      DirectX::XMFLOAT3 rayStart{0.0f, 0.0f, 0.0f};
      DirectX::XMFLOAT3 rayDir{0.0f, 0.0f, 0.0f};
      g_camera->GetClickRay(g_target->X(), g_target->Y(), &rayStart, &rayDir);
      int locId = LocationHit(rayStart, rayDir, 10000.0f);
      if (locId != -1)
      {
        // We're close to a location, but not there, so drag the pointer towards it
        GlobalLocation* loc = GetLocation(locId);
        float locX, locY;
        g_camera->Get2DScreenPos(loc->m_pos, &locX, &locY);
        locY = g_renderer->ScreenH() - locY;
        int movX = static_cast<int>(locX - g_target->X());
        int movY = static_cast<int>(locY - g_target->Y());
        int movMag2 = movX * movX + movY * movY;
        int movFactor = 30 / movMag2;
        if (movFactor > 0)
          g_target->MoveCursor(movX * movFactor, movY * movFactor);
      }
    }

    // Has the fade out finished?
    if (m_locationRequested != -1 && g_renderer->IsFadeComplete())
    {
      GlobalLocation* loc = GetLocation(m_locationRequested);
      g_requestedLocationId = m_locationRequested;
      g_requestedMission = loc->m_missionFilename;
      g_requestedMap = loc->m_mapFilename;

      m_locationRequested = -1;
    }
  }
}

void GlobalWorld::Render()
{
  START_PROFILE(g_profiler, "Render Global World");

  if (!g_editing)
    m_globalInternet->Render();
  m_sphereWorld->Render();

  END_PROFILE(g_profiler, "Render Global World");
}

// Returns the ID of the location the line intersects. Returns -1 if line
// does not intersect any location
int GlobalWorld::LocationHit(DirectX::XMFLOAT3 const& _pos, DirectX::XMFLOAT3 const& _dir, float locationRadius)
{
  // float locationRadius = 5000.0f;

  for (GlobalLocation* gl : m_locations)
  {
    DirectX::XMFLOAT3 const locPos = GetLocationPosition(gl->m_id);

    bool hit = RaySphereIntersection(_pos, _dir, locPos, locationRadius);
    if (hit)
      return gl->m_id;
  }

  return -1;
}

GlobalLocation* GlobalWorld::GetLocation(int _id)
{
  for (GlobalLocation* loc : m_locations)
  {
    if (loc->m_id == _id)
      return loc;
  }

  return nullptr;
}

GlobalLocation* GlobalWorld::GetHighlightedLocation()
{
  int screenX = g_target->X();
  int screenY = g_target->Y();

  DirectX::XMFLOAT3 rayStart{0.0f, 0.0f, 0.0f};
  DirectX::XMFLOAT3 rayDir{0.0f, 0.0f, 0.0f};
  g_camera->GetClickRay(screenX, screenY, &rayStart, &rayDir);
  int locId = g_globalWorld->LocationHit(rayStart, rayDir);

  GlobalLocation* loc = GetLocation(locId);

  if (loc && loc->m_available)
    return loc;
  if (loc && g_editing)
    return loc;

  return nullptr;
}

GlobalLocation* GlobalWorld::GetLocation(const char* _name)
{
  int locationId = GetLocationId(_name);
  if (locationId != -1)
    return GetLocation(locationId);
  return nullptr;
}

int GlobalWorld::GetLocationId(const char* _name)
{
  for (GlobalLocation* loc : m_locations)
  {
    DEBUG_ASSERT(loc);
    if (stricmp(loc->m_name.c_str(), _name) == 0)
      return loc->m_id;
  }

  return -1;
}

int GlobalWorld::GetLocationIdFromMapFilename(const char* _mapFilename)
{
  // Skip a leading "Map" and drop the four-character extension. The old code
  // did that with strstr(buf, "Map") + 3 and then indexed [strlen - 4], so a
  // filename carrying neither read and WROTE outside the buffer — the +3
  // happened before the null test, exactly as FilesysUtils' path helpers did
  // before strings-modernised T4 defined them. A name that does not have both
  // now answers "no such location", which is what GetLocationId returns for an
  // unknown name anyway.
  const std::string filename = _mapFilename;
  const size_t nameStart = filename.find("Map");
  if (nameStart == std::string::npos || filename.size() < nameStart + 3 + 4)
    return -1;

  const std::string mapName = filename.substr(nameStart + 3, filename.size() - nameStart - 3 - 4);
  return GetLocationId(mapName.c_str());
}

char const* GlobalWorld::GetLocationName(int _id)
{
  GlobalLocation* loc = GetLocation(_id);
  if (loc)
    return loc->m_name.c_str();
  return nullptr;
}

char const* GlobalWorld::GetLocationNameTranslated(int _id)
{
  GlobalLocation* loc = GetLocation(_id);
  if (!loc)
    return nullptr;

  const std::string stringId = std::format("location_{}", loc->m_name);

  if (ISLANGUAGEPHRASE(stringId.c_str()))
    return LANGUAGEPHRASE(stringId.c_str());
  return loc->m_name.c_str();
}

DirectX::XMFLOAT3 GlobalWorld::GetLocationPosition(int _id)
{
  GlobalLocation* location = GetLocation(_id);
  if (!location)
    return DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

  DirectX::XMFLOAT3 result;
  DirectX::XMStoreFloat3(&result, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&location->m_pos), 120.0f));
  return result;
}

GlobalBuilding* GlobalWorld::GetBuilding(int _id, int _locationId)
{
  if (_id == -1 || _locationId == -1)
    return nullptr;

  for (GlobalBuilding* buil : m_buildings)
  {
    if (buil->m_id == _id && buil->m_locationId == _locationId)
      return buil;
  }

  return nullptr;
}

void GlobalWorld::AddLocation(GlobalLocation* location)
{
  if (location->m_id == -1)
  {
    location->m_id = m_nextLocationId;
    m_nextLocationId++;
  }
  else if (location->m_id >= m_nextLocationId)
    m_nextLocationId = location->m_id + 1;

  m_locations.push_back(location);
  m_sphereWorld->AddLocation(location->m_id);
}

void GlobalWorld::AddBuilding(GlobalBuilding* building)
{
  if (building->m_id == -1)
  {
    building->m_id = m_nextBuildingId;
    m_nextBuildingId++;
  }
  else if (building->m_id >= m_nextBuildingId)
    m_nextBuildingId = building->m_id + 1;

  m_buildings.push_back(building);
}

void GlobalWorld::WriteLocations(FileWriter* _out)
{
  _out->printf("Locations_StartDefinition\n");
  _out->printf("\t# Id  Avail                   mapFile                    missionFile\n");
  _out->printf("\t# ==================================================================\n");

  for (GlobalLocation* location : m_locations)
  {
    _out->printf("\t%4d %4d %30s %40s\n", location->m_id, static_cast<int>(location->m_available), location->m_mapFilename.c_str(),
                 location->m_missionFilename.c_str());
  }

  _out->printf("Locations_EndDefinition\n\n");
}

void GlobalWorld::WriteBuildings(FileWriter* _out)
{
  _out->printf("Buildings_StartDefinition\n");
  _out->printf("\t# Id  teamId  locId   type   link  online\n");
  _out->printf("\t# =======================================\n");

  for (GlobalBuilding* building : m_buildings)
  {
    _out->printf("\t%4d %4d %6d %6d %6d %6d\n", building->m_id, building->m_teamId, building->m_locationId, building->m_type, building->m_link,
                 building->m_online);
  }

  _out->printf("Buildings_EndDefinition\n\n");
}

void GlobalWorld::WriteEvents(FileWriter* _out)
{
  _out->printf("Events_StartDefinition\n");

  for (GlobalEvent* ge : m_events)
  {
    ge->Write(_out);
  }

  _out->printf("Events_EndDefinition\n");
}

void GlobalWorld::ParseLocations(TextReader* _in)
{
  while (_in->ReadLine())
  {
    if (!_in->TokenAvailable())
      continue;

    char* word = _in->GetNextToken();

    if (stricmp(word, "Locations_EndDefinition") == 0)
      return;

    auto location = new GlobalLocation();

    location->m_id = atoi(word);
    location->m_available = static_cast<bool>(atoi(_in->GetNextToken()));

    location->m_mapFilename = _in->GetNextToken();
    location->m_missionFilename = _in->GetNextToken();

    // The name is the map filename without its "Map" prefix and its ".txt" —
    // written as a copy from + 3 followed by [strlen - 4] = 0, which indexed
    // before the start of the buffer for anything shorter than seven
    // characters. Same arithmetic, and a name too short to carry both is left
    // as the filename rather than read out of bounds.
    location->m_name =
      location->m_mapFilename.size() >= 7 ? location->m_mapFilename.substr(3, location->m_mapFilename.size() - 7) : location->m_mapFilename;

    AddLocation(location);
  }
}

void GlobalWorld::ParseBuildings(TextReader* _in)
{
  while (_in->ReadLine())
  {
    if (!_in->TokenAvailable())
      continue;

    char* word = _in->GetNextToken();

    if (stricmp(word, "buildings_enddefinition") == 0)
      return;

    auto building = new GlobalBuilding();

    building->m_id = atoi(word);
    building->m_teamId = atoi(_in->GetNextToken());
    building->m_locationId = atoi(_in->GetNextToken());
    building->m_type = atoi(_in->GetNextToken());
    building->m_link = atoi(_in->GetNextToken());
    building->m_online = atoi(_in->GetNextToken());

    AddBuilding(building);
  }
}

void GlobalWorld::ParseEvents(TextReader* _in)
{
  while (_in->ReadLine())
  {
    if (!_in->TokenAvailable())
      continue;
    char* word = _in->GetNextToken();

    if (stricmp(word, "events_enddefinition") == 0)
      return;

    DEBUG_ASSERT(stricmp(word, "Event") == 0);

    auto event = new GlobalEvent();
    event->Read(_in);
    m_events.push_back(event);
  }
}

void GlobalWorld::AddLevelBuildingToGlobalBuildings(Building* _building, int _locId)
{
  if (_building->m_isGlobal)
  {
    GlobalBuilding* gb = GetBuilding(_building->m_id.GetUniqueId(), _locId);
    if (!gb)
    {
      gb = new GlobalBuilding();
      gb->m_type = _building->m_type;
      gb->m_locationId = _locId;
      gb->m_id = _building->m_id.GetUniqueId();
      gb->m_teamId = _building->m_id.GetTeamId();
      m_buildings.push_back(gb);

      if (_building->m_type == Building::TypeTrunkPort)
        gb->m_link = static_cast<TrunkPort*>(_building)->m_targetLocationId;
    }
    gb->m_pos = _building->m_pos;
  }
}

void GlobalWorld::LoadGame(const char* _filename)
{
  TextReader* in = nullptr;

  if (!g_editing)
  {
    const std::string fullFilename = std::format("{}users/{}/{}", g_appCommands->ProfileDirectory(), g_userProfileName, _filename);
    if (DoesFileExist(fullFilename.c_str()))
      in = new TextFileReader(fullFilename.c_str());
  }

  if (!in)
    in = g_resource->GetTextReader(_filename);

  if (in)
  {
    while (in->ReadLine())
    {
      if (!in->TokenAvailable())
        continue;
      char* word = in->GetNextToken();

      if (stricmp("locations_startdefinition", word) == 0)
        ParseLocations(in);
      else if (stricmp("buildings_startdefinition", word) == 0)
        ParseBuildings(in);
      else if (stricmp("events_startdefinition", word) == 0)
        ParseEvents(in);
      else if (stricmp("research_startdefinition", word) == 0)
        m_research->Read(in);
      else if (stricmp("tutorial_startdefinition", word) == 0)
        ParseTutorial(in);
    }
  }

  delete in;
  in = nullptr;

  //
  // Load locations

  LoadLocations("Locations.txt");

  //
  // Load all map files into memory

  for (GlobalLocation* loc : m_locations)
  {
    // Load all the level files for the location
    LevelFile levFile("null", loc->m_mapFilename.c_str());
    for (auto const& building : levFile.m_buildings)
    {
      AddLevelBuildingToGlobalBuildings(building.get(), loc->m_id);
    }

    const std::string filter = std::format("Mission{}*.txt", GetLocationName(loc->m_id));
    std::vector<char*>* missionFileNames = g_resource->ListResources("Levels/", filter.c_str(), false);
    for (const char* missionFileName : *missionFileNames)
    {
      LevelFile levFile(missionFileName, loc->m_mapFilename.c_str());

      for (auto const& building : levFile.m_buildings)
      {
        AddLevelBuildingToGlobalBuildings(building.get(), loc->m_id);

        if (building->m_type == Building::TypeAntHill || building->m_type == Building::TypeTriffid || building->m_type == Building::TypeIncubator)
        {
          if (!building->m_dynamic)
          {
            DebugTrace("{} found on level {} should be dynamic (otherwise save games wont work)\n", Building::GetTypeName(building->m_type),
                       GetLocationName(loc->m_id));
          }
        }
      }

      bool objectivesComplete = true;
      for (auto const& gec : levFile.m_primaryObjectives)
      {
        if (!gec->Evaluate())
        {
          objectivesComplete = false;
          break;
        }
      }

      if (objectivesComplete)
        loc->m_missionCompleted = true;
    }
    for (char* missionFileName : *missionFileNames)
      delete[] missionFileName;
    delete missionFileNames;
  }

  EvaluateEvents();
}

void GlobalWorld::SaveGame(const char* _filename)
{
  FileWriter* out = nullptr;

  if (!g_editing && stricmp(g_userProfileName.c_str(), "none") != 0)
  {
    const std::string fullFilename = std::format("{}users/{}/{}", g_appCommands->ProfileDirectory(), g_userProfileName, _filename);
#ifdef TARGET_DEBUG
    out = new FileWriter(fullFilename.c_str(), false);
#else
    out = new FileWriter(fullFilename.c_str(), true);
#endif
  }

  if (!out)
    out = g_resource->GetFileWriter(_filename, false);

  WriteLocations(out);
  WriteBuildings(out);
  m_research->Write(out);
  WriteTutorial(out);
  WriteEvents(out);

  delete out;
}

void GlobalWorld::WriteTutorial(FileWriter* _out) {}

void GlobalWorld::ParseTutorial(TextReader* _in)
{
  while (_in->ReadLine())
  {
    if (!_in->TokenAvailable())
      continue;

    char* word = _in->GetNextToken();

    if (stricmp(word, "tutorial_enddefinition") == 0)
      return;

    char* temp = _in->GetNextToken();
    int chapter = atoi(_in->GetNextToken());
  }
}

void GlobalWorld::LoadLocations(const char* _filename)
{
  TextReader* in = g_resource->GetTextReader(_filename);

  while (in->ReadLine())
  {
    if (!in->TokenAvailable())
      continue;

    int locIndex = atoi(in->GetNextToken());
    float posX = atof(in->GetNextToken());
    float posY = atof(in->GetNextToken());
    float posZ = atof(in->GetNextToken());

    GlobalLocation* location = GetLocation(locIndex);
    if (location)
      location->m_pos = DirectX::XMFLOAT3(posX, posY, posZ);
  }

  delete in;
}

void GlobalWorld::SaveLocations(const char* _filename)
{
  FileWriter* out = g_resource->GetFileWriter(_filename, false);

  out->printf("# ================================\n");
  out->printf("# id   x        y        z\n");
  out->printf("# ================================\n\n");

  for (GlobalLocation* loc : m_locations)
  {
    out->printf("%-6d %-8.2f %-8.2f %-8.2f\n", loc->m_id, loc->m_pos.x, loc->m_pos.y, loc->m_pos.z);
  }

  delete out;
}

// Find the lowest unused building ID in the current location
int GlobalWorld::GenerateBuildingId()
{
  int id = 0;
  while (true)
  {
    if (!g_location->GetBuilding(id))
      break;
    ++id;
  }
  return id;
}

// Checks to see if any event's conditions are true. If they are, the first action
// for that event will be executed. That event action is then deleted, and if there
// are no more actions for the event, then the event is deleted too.
// Returns true if actions remain to be completed
bool GlobalWorld::EvaluateEvents()
{
  if (g_script && g_script->IsRunningScript())
    return true;

  for (int i = 0; i < static_cast<int>(m_events.size()); ++i)
  {
    GlobalEvent* event = m_events[i];

    if (event->Evaluate())
    {
      event->MakeAlwaysTrue();
      bool amIDone = event->Execute();
      if (amIDone)
      {
        m_events.erase(m_events.begin() + i);
        delete event;
        --i;
      }
      return true;
    }
  }

  return false;
}

void GlobalWorld::TransferSpirits(int _locationId)
{
  //
  // Count how many spirits remain on the location

  DEBUG_ASSERT(g_location);
  int remainingSpirits = g_location->m_spirits.NumUsed();

  GlobalLocation* location = GetLocation(_locationId);
  ASSERT_TEXT(location, "GlobalWorld::TransferSpirits, failed to lookup location {}", _locationId);

  int spiritCount = location->m_numSpirits + remainingSpirits / 2;
  location->m_numSpirits = remainingSpirits / 2;

  float position = 1.0f;
  for (int i = 0; i < spiritCount; ++i)
  {
    std::vector<float>& spirits = m_sphereWorld->m_spirits[_locationId];
    spirits.insert(spirits.begin(), position);
    position -= frand(0.04f);
  }
}

void GlobalWorld::SetupLights()
{
  float black[] = {0, 0, 0, 0};
  float colour1[] = {2.0f, 1.5f, 0.75f, 1.0f};

  DirectX::XMFLOAT3 light0;
  DirectX::XMStoreFloat3(&light0, DirectX::XMVector3Normalize(DirectX::g_XMIdentityR1));
  GLfloat light0AsFourFloats[] = {light0.x, light0.y, light0.z, 0.0f};

  glLightfv(GL_LIGHT0, GL_POSITION, light0AsFourFloats);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, colour1);
  glLightfv(GL_LIGHT0, GL_SPECULAR, colour1);
  glLightfv(GL_LIGHT0, GL_AMBIENT, black);

  glDisable(GL_LIGHT0);
  glDisable(GL_LIGHT1);
  glDisable(GL_LIGHTING);
}

void GlobalWorld::SetupFog()
{
  float fog = 0.1f;
  float fogCol[] = {fog, fog, fog, fog};

  glFogf(GL_FOG_DENSITY, 1.0f);
  glFogf(GL_FOG_START, 0.0f);
  glFogf(GL_FOG_END, 19000.0f);
  glFogfv(GL_FOG_COLOR, fogCol);
  glFogi(GL_FOG_MODE, GL_LINEAR);
  // glEnable    (GL_FOG);
}

float GlobalWorld::GetSize()
{
  if (g_location)
    return g_location->m_landscape.GetWorldSizeX();

  return 2e5;
}
