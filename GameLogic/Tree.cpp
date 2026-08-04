#include "pch.h"
#include "GlVertex.h"
#include "SoundSources.h"

#include "DebugRender.h"
#include "FileWriter.h"
#include "MathUtils.h"
#include "Profiler.h"
#include "Resource.h"
#include "TextStreamReaders.h"
#include "Debug.h"
#include "Shape.h"
#include "HiResTime.h"
#include "TextRenderer.h"
#include "Preferences.h"

#include "Tree.h"

#include "SoundSystem.h"

#include "ProtocolLimits.h"
#include "ParticleSystem.h"
#include "Location.h"
#include "GameTime.h"
#include "WorldPointers.h"
#include "AppState.h"

Tree::Tree()
  : Building(),
    m_branchDisplayListId(-1),
    m_leafDisplayListId(-1),
    m_height(50.0f),
    m_iterations(6),
    m_budsize(1.0f),
    m_pushOut(1.0f),
    m_pushUp(1.0f),
    m_seed(0),
    m_leafColour(0xffffffff),
    m_branchColour(0xffffffff),
    m_onFire(0.0f),
    m_fireDamage(0.0f),
    m_burnSoundPlaying(false),
    m_leafDropRate(0)
{
  m_type = TypeTree;
}

Tree::~Tree() { DeleteDisplayLists(); }

void Tree::Initialise(Building* _template)
{
  Building::Initialise(_template);

  DEBUG_ASSERT(_template->m_type == TypeTree);
  Tree* tree = (Tree*)_template;

  m_height = tree->m_height;
  m_iterations = tree->m_iterations;
  m_budsize = tree->m_budsize;
  m_pushOut = tree->m_pushOut;
  m_pushUp = tree->m_pushUp;
  m_seed = tree->m_seed;
  m_branchColour = tree->m_branchColour;
  m_leafColour = tree->m_leafColour;
  m_leafDropRate = tree->m_leafDropRate;
}


void Tree::SetDetail(int _detail)
{
  Building::SetDetail(_detail);

  int oldIterations = m_iterations;
  if (_detail == 0)
    m_iterations = 0;
  else
    m_iterations -= (_detail - 1);
  m_iterations = std::max(m_iterations, 3);

  Generate();

  m_iterations = oldIterations;
  DirectX::XMStoreFloat3(&m_centrePos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_hitcheckCentre), DirectX::XMVectorReplicate(m_height),
                                                                    DirectX::XMLoadFloat3(&m_pos)));
  m_radius = m_hitcheckRadius * m_height * 1.5f;
}


bool Tree::Advance()
{
  m_fireDamage -= SERVER_ADVANCE_PERIOD;
  if (m_fireDamage < 0.0f)
    m_fireDamage = 0.0f;
  if (m_fireDamage > 100.0f)
    m_fireDamage = 100.0f;

  if (m_onFire > 0.0f)
  {
    //
    // Spawn fire particle

    float actualHeight = GetActualHeight(0.0f);
    int numFire = actualHeight / 5;
    for (int i = 0; i < numFire; ++i)
    {
      // The three sfrand calls stay in this order: they advance the RNG.
      DirectX::XMFLOAT3 const scatter(sfrand(actualHeight * 1.0f), sfrand(actualHeight * 0.5f), sfrand(actualHeight * 1.0f));
      DirectX::XMFLOAT3 const fireSpawn(m_pos.x + scatter.x, m_pos.y + actualHeight + scatter.y, m_pos.z + scatter.z);
      float fireSize = actualHeight * 2.0f;
      fireSize *= (1.0f + sfrand(0.5f));
      g_particleSystem->CreateParticle(fireSpawn, g_zeroVector, Particle::TypeFire, fireSize);
    }

    if (frand(100.0f) < 10.0f)
    {
      // The three sfrand calls stay in this order: they advance the RNG.
      DirectX::XMFLOAT3 const scatter(sfrand(actualHeight * 0.75f), sfrand(actualHeight * 0.75f), sfrand(actualHeight * 0.75f));
      DirectX::XMFLOAT3 const fireSpawn(m_pos.x + scatter.x, m_pos.y + actualHeight + scatter.y, m_pos.z + scatter.z);
      g_particleSystem->CreateParticle(fireSpawn, g_zeroVector, Particle::TypeExplosionDebris);
    }

    //
    // Burn down

    m_onFire += SERVER_ADVANCE_PERIOD * 0.01f;
    if (m_onFire > 1.0f)
    {
      m_onFire = -1.0f;
    }

    //
    // Spread to nearby trees

    DirectX::XMFLOAT3 hitCentre;
    DirectX::XMStoreFloat3(&hitCentre, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_hitcheckCentre),
                                                                    DirectX::XMVectorReplicate(actualHeight), DirectX::XMLoadFloat3(&m_pos)));
    float hitRadius = m_hitcheckRadius * actualHeight;

    for (int b = 0; b < g_location->m_buildings.Size(); ++b)
    {
      if (g_location->m_buildings.ValidIndex(b))
      {
        Building* building = g_location->m_buildings[b];
        if (building != this && building->m_type == TypeTree)
        {
          Tree* tree = (Tree*)building;
          float distance = DirectX::XMVectorGetX(
            DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&tree->m_pos), DirectX::XMLoadFloat3(&m_pos))));
          float theirActualHeight = tree->GetActualHeight(0.0f);
          float theirRadius = theirActualHeight * tree->m_hitcheckRadius * 1.5f;
          float ourRadius = actualHeight * m_hitcheckRadius * 1.5f;

          if (theirRadius + ourRadius >= distance)
          {
            tree->Damage(-1.0f);
          }
        }
      }
    }
  }
  else if (m_onFire < 0.0f)
  {
    if (m_burnSoundPlaying)
    {
      g_soundSystem->StopAllSounds(m_id, "Tree Burn");
      g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), "Create");
      m_burnSoundPlaying = false;
    }

    //
    // Regrow

    m_onFire += SERVER_ADVANCE_PERIOD * 0.01f;
    if (m_onFire > 0.0f)
    {
      m_onFire = 0.0f;
    }
  }

  if (m_onFire == 0.0f && m_leafDropRate > 0)
  {
    // drop some leaves
    if (rand() % (51 - m_leafDropRate) == 0)
    {
      float actualHeight = GetActualHeight(0.0f);
      // The three sfrand calls stay in this order: they advance the RNG.
      DirectX::XMFLOAT3 const scatter(sfrand(actualHeight * 1.0f), sfrand(actualHeight * 0.25f), sfrand(actualHeight * 1.0f));
      DirectX::XMFLOAT3 const fireSpawn(m_pos.x + scatter.x, m_pos.y + actualHeight + scatter.y, m_pos.z + scatter.z);
      g_particleSystem->CreateParticle(fireSpawn, g_zeroVector, Particle::TypeLeaf, -1.0f,
                                       RGBAColour(m_leafColourArray[0], m_leafColourArray[1], m_leafColourArray[2]));
    }
  }

  return false;
}

float Tree::GetActualHeight(float _predictionTime)
{
  float predictedOnFire = m_onFire;
  if (predictedOnFire != 0.0f)
  {
    predictedOnFire += _predictionTime * 0.01f;
  }

  float actualHeight = m_height * (1.0f - fabs(predictedOnFire));
  if (actualHeight < m_height * 0.1f)
    actualHeight = m_height * 0.1f;

  return actualHeight;
}

static void intToArray(unsigned x, unsigned char* a)
{
  a[0] = x & 0xFF;
  x >>= 8;
  a[1] = x & 0xFF;
  x >>= 8;
  a[2] = x & 0xFF;
  x >>= 8;
  a[3] = x & 0xFF;
}

void Tree::DeleteDisplayLists()
{
  if (m_branchDisplayListId != -1)
  {
    glDeleteLists(m_branchDisplayListId, 1);
    m_branchDisplayListId = -1;
  }

  if (m_leafDisplayListId != -1)
  {
    glDeleteLists(m_leafDisplayListId, 1);
    m_leafDisplayListId = -1;
  }
}

void Tree::Generate()
{
  float timeNow = GetHighResTime();

  m_hitcheckCentre = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
  m_hitcheckRadius = 0.0f;
  m_numLeafs = 0;

  DeleteDisplayLists();

  intToArray(m_branchColour, m_branchColourArray);
  intToArray(m_leafColour, m_leafColourArray);

  int treeDetail = g_prefsManager->GetInt("RenderTreeDetail", 1);
  if (treeDetail > 1)
  {
    int alpha = m_leafColourArray[3];
    alpha *= pow(1.3f, treeDetail);
    alpha = std::min(alpha, 255);
    m_leafColourArray[3] = alpha;
  }

  speciesSeedRandom(m_seed);
  m_branchDisplayListId = glGenLists(1);
  glNewList(m_branchDisplayListId, GL_COMPILE);
  glBegin(GL_QUADS);
  RenderBranch(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f), m_iterations, false, true, false);
  glEnd();
  glEndList();

  speciesSeedRandom(m_seed);
  m_leafDisplayListId = glGenLists(1);
  glNewList(m_leafDisplayListId, GL_COMPILE);
  glBegin(GL_QUADS);
  RenderBranch(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f), m_iterations, false, false, true);
  glEnd();
  glEndList();


  //
  // We now have all the leaf positions accumulated in m_hitcheckCentre
  // So we can calculate the actual centre position, then the radius

  DirectX::XMStoreFloat3(&m_hitcheckCentre, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_hitcheckCentre), 1.0f / (float)m_numLeafs));
  RenderBranch(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f), m_iterations, true, false, true);
  m_hitcheckRadius *= 0.8f;

  float totalTime = GetHighResTime() - timeNow;
  DebugTrace("Tree generated in %dms\n", int(totalTime * 1000.0f));
}

void Tree::Render(float _predictionTime)
{
  // RenderHitCheck();
}


bool Tree::PerformDepthSort(DirectX::XMFLOAT3& _centrePos)
{
  DirectX::XMStoreFloat3(&_centrePos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_hitcheckCentre), DirectX::XMVectorReplicate(m_height),
                                                                   DirectX::XMLoadFloat3(&m_pos)));
  return true;
}


void Tree::RenderAlphas(float _predictionTime)
{
  if (m_branchDisplayListId == -1 || m_leafDisplayListId == -1)
  {
    Generate();
  }

  if (g_editing)
  {
    intToArray(m_branchColour, m_branchColourArray);
    intToArray(m_leafColour, m_leafColourArray);
  }

  float actualHeight = GetActualHeight(_predictionTime);

  glEnable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glBindTexture(GL_TEXTURE_2D, g_resource->GetTexture("Textures/Laser.bmp"));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glDisable(GL_CULL_FACE);
  glDepthMask(false);

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  DirectX::XMFLOAT4X4 mat;
  DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_pos)));
  glMultMatrixf(mat.ConvertToOpenGLFormat());
  glScalef(actualHeight, actualHeight, actualHeight);

  if (Location::ChristmasModEnabled() == 1)
  {
    m_branchColourArray[0] = 180;
    m_branchColourArray[1] = 100;
    m_branchColourArray[2] = 50;
  }

  glColor4ubv(m_branchColourArray);
  glCallList(m_branchDisplayListId);

  if (Location::ChristmasModEnabled() != 1)
  {
    glColor4ubv(m_leafColourArray);
    glCallList(m_leafDisplayListId);
  }

  glPopMatrix();

  glDepthMask(true);
  glEnable(GL_CULL_FACE);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);
}

void Tree::RenderHitCheck()
{
#ifdef DEBUG_RENDER_ENABLED
  float actualHeight = GetActualHeight(0.0f);

  RenderSphere(m_pos, 10.0f);
  DirectX::XMFLOAT3 debugCentre;
  DirectX::XMStoreFloat3(&debugCentre, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_hitcheckCentre),
                                                                    DirectX::XMVectorReplicate(actualHeight), DirectX::XMLoadFloat3(&m_pos)));
  RenderSphere(debugCentre, m_hitcheckRadius * actualHeight);
#endif
}

void Tree::Damage(float _damage)
{
  Building::Damage(_damage);

  if (m_fireDamage < 50.0f)
  {
    m_fireDamage += _damage * -1.0f;

    if (m_fireDamage > 50.0f)
    {
      if (m_onFire == 0.0f)
      {
        m_onFire = 0.01f;
      }
      else if (m_onFire < 0.0f)
      {
        m_onFire = m_onFire * -1.0f;
      }

      if (!m_burnSoundPlaying)
      {
        g_soundSystem->StopAllSounds(m_id, "Tree Create");
        g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), "Burn");
        m_burnSoundPlaying = true;
      }
    }
  }
}

bool Tree::DoesSphereHit(DirectX::XMFLOAT3 const& _pos, float _radius)
{
  if (SphereSphereIntersection(m_pos, 10.0f, _pos, _radius))
  {
    return true;
  }

  float actualHeight = GetActualHeight(0.0f);

  DirectX::XMFLOAT3 hitCentre;
  DirectX::XMStoreFloat3(&hitCentre, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_hitcheckCentre), DirectX::XMVectorReplicate(actualHeight),
                                                                  DirectX::XMLoadFloat3(&m_pos)));
  if (SphereSphereIntersection(hitCentre, m_hitcheckRadius * actualHeight, _pos, _radius))
  {
    return true;
  }

  return false;
}

bool Tree::DoesShapeHit(Shape* _shape, DirectX::XMFLOAT4X4 _transform)
{
  SpherePackage packageA(m_pos, 10.0f);
  if (_shape->SphereHit(&packageA, _transform))
  {
    return true;
  }

  float actualHeight = GetActualHeight(0.0f);

  DirectX::XMFLOAT3 hitCentre;
  DirectX::XMStoreFloat3(&hitCentre, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_hitcheckCentre), DirectX::XMVectorReplicate(actualHeight),
                                                                  DirectX::XMLoadFloat3(&m_pos)));
  SpherePackage packageB(hitCentre, m_hitcheckRadius * actualHeight);
  if (_shape->SphereHit(&packageB, _transform))
  {
    return true;
  }

  return false;
}

bool Tree::DoesRayHit(DirectX::XMFLOAT3 const& _rayStart, DirectX::XMFLOAT3 const& _rayDir, float _rayLen, DirectX::XMFLOAT3* _pos,
                      DirectX::XMFLOAT3* _norm)
{
  // Same seam as LaserFence: MathUtils kept its Vector3* out-parameters.
  Vector3* const legacyHit = _pos ? &AsLegacy(*_pos) : nullptr;
  Vector3* const legacyNorm = _norm ? &AsLegacy(*_norm) : nullptr;

  if (RaySphereIntersection(_rayStart, _rayDir, m_pos, 10.00f, _rayLen, legacyHit, legacyNorm))
  {
    return true;
  }

  float actualHeight = GetActualHeight(0.0f);

  DirectX::XMFLOAT3 hitCentre;
  DirectX::XMStoreFloat3(&hitCentre, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_hitcheckCentre), DirectX::XMVectorReplicate(actualHeight),
                                                                  DirectX::XMLoadFloat3(&m_pos)));
  if (RaySphereIntersection(_rayStart, _rayDir, hitCentre, m_hitcheckRadius * actualHeight, _rayLen, legacyHit, legacyNorm))
  {
    return true;
  }

  return false;
}


void Tree::RenderBranch(DirectX::XMFLOAT3 _from, DirectX::XMFLOAT3 _to, int _iterations, bool _calcRadius, bool _renderBranch, bool _renderLeaf)
{
  if (_iterations == 0)
    return;
  _iterations--;

  DirectX::XMVECTOR const from = DirectX::XMLoadFloat3(&_from);
  DirectX::XMVECTOR const to = DirectX::XMLoadFloat3(&_to);

  if (_iterations == 0)
  {
    if (_calcRadius)
    {
      float distToCentre = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(to, DirectX::XMLoadFloat3(&m_hitcheckCentre))));
      if (distToCentre > m_hitcheckRadius)
        m_hitcheckRadius = distToCentre;
    }
    else if (_renderLeaf)
    {
      DirectX::XMStoreFloat3(&m_hitcheckCentre, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_hitcheckCentre), to));
      m_numLeafs++;
    }
  }

  /*
  Make the tree seem more alive animation
  _to += Vector3(sinf(g_gameTime*(7-_iterations))*0.005f * _iterations,
                 sinf(g_gameTime) * 0.01f,
                 sinf(g_gameTime * (7-_iterations))*0.005f * _iterations );
  */

  DirectX::XMVECTOR const thisBranch = DirectX::XMVectorSubtract(to, from);

  DirectX::XMVECTOR const rightAngleA = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(thisBranch, to));
  DirectX::XMVECTOR const rightAngleB = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(rightAngleA, thisBranch));

  float thickness = DirectX::XMVectorGetX(DirectX::XMVector3Length(thisBranch));

  float budsize = 0.1f;

  if (_iterations == 0)
  {
    budsize *= m_budsize;
  }

  DirectX::XMVECTOR const camRightA = DirectX::XMVectorScale(rightAngleA, thickness * budsize);
  DirectX::XMVECTOR const camRightB = DirectX::XMVectorScale(rightAngleB, thickness * budsize);


  if ((_iterations == 0 && _renderLeaf) || (_iterations != 0 && _renderBranch))
  {
    glTexCoord2f(0.0f, 0.0f);
    EmitVertex(DirectX::XMVectorSubtract(from, camRightA));
    glTexCoord2f(0.0f, 1.0f);
    EmitVertex(DirectX::XMVectorAdd(from, camRightA));
    glTexCoord2f(1.0f, 1.0f);
    EmitVertex(DirectX::XMVectorAdd(to, camRightA));
    glTexCoord2f(1.0f, 0.0f);
    EmitVertex(DirectX::XMVectorSubtract(to, camRightA));

    glTexCoord2f(0.0f, 0.0f);
    EmitVertex(DirectX::XMVectorSubtract(from, camRightB));
    glTexCoord2f(0.0f, 1.0f);
    EmitVertex(DirectX::XMVectorAdd(from, camRightB));
    glTexCoord2f(1.0f, 1.0f);
    EmitVertex(DirectX::XMVectorAdd(to, camRightB));
    glTexCoord2f(1.0f, 0.0f);
    EmitVertex(DirectX::XMVectorSubtract(to, camRightB));
  }

  int numBranches = 4;

  for (int i = 0; i < numBranches; ++i)
  {
    DirectX::XMVECTOR thisRightAngle = DirectX::XMVectorZero();
    if (i == 0)
      thisRightAngle = rightAngleA;
    if (i == 1)
      thisRightAngle = DirectX::XMVectorNegate(rightAngleA);
    if (i == 2)
      thisRightAngle = rightAngleB;
    if (i == 3)
      thisRightAngle = DirectX::XMVectorNegate(rightAngleB);

    // frand stays inside the loop: it advances the RNG once per branch.
    float distance = 0.3f + frand(0.6f);
    DirectX::XMVECTOR const thisFromVec = DirectX::XMVectorMultiplyAdd(thisBranch, DirectX::XMVectorReplicate(distance), from);
    DirectX::XMVECTOR const thisToVec = DirectX::XMVectorMultiplyAdd(
      thisBranch, DirectX::XMVectorReplicate((1.0f - distance) * m_pushUp),
      DirectX::XMVectorMultiplyAdd(thisRightAngle, DirectX::XMVectorReplicate(thickness * 0.4f * m_pushOut), thisFromVec));

    DirectX::XMFLOAT3 thisFrom, thisTo;
    DirectX::XMStoreFloat3(&thisFrom, thisFromVec);
    DirectX::XMStoreFloat3(&thisTo, thisToVec);
    RenderBranch(thisFrom, thisTo, _iterations, _calcRadius, _renderBranch, _renderLeaf);
  }
}


void Tree::ListSoundEvents(std::vector<const char*>* _list)
{
  Building::ListSoundEvents(_list);

  _list->push_back("Burn");
}


void Tree::Read(TextReader* _in, bool _dynamic)
{
  Building::Read(_in, _dynamic);

  m_height = atof(_in->GetNextToken());
  m_budsize = atof(_in->GetNextToken());
  m_pushOut = atof(_in->GetNextToken());
  m_pushUp = atof(_in->GetNextToken());
  m_iterations = atoi(_in->GetNextToken());
  m_seed = atoi(_in->GetNextToken());
  m_branchColour = atoi(_in->GetNextToken());
  m_leafColour = atoi(_in->GetNextToken());
  if (_in->TokenAvailable())
  {
    m_leafDropRate = atoi(_in->GetNextToken());
  }
}

void Tree::Write(FileWriter* _out)
{
  Building::Write(_out);

  _out->printf("%-8.2f", m_height);
  _out->printf("%-8.2f", m_budsize);
  _out->printf("%-8.2f", m_pushOut);
  _out->printf("%-8.2f", m_pushUp);
  _out->printf("%-8d", m_iterations);
  _out->printf("%-8d", m_seed);
  _out->printf("%-12d", m_branchColour);
  _out->printf("%-12d", m_leafColour);
  _out->printf("%-8d", m_leafDropRate);
}
