#include "pch.h"
#include "GlVertex.h"
#include "SoundSources.h"

#include "FileWriter.h"
#include "Resource.h"
#include "Shape.h"
#include "HiResTime.h"
#include "MathUtils.h"
#include "TextStreamReaders.h"
#include "DebugRender.h"
#include "TextRenderer.h"
#include "Profiler.h"
#include "LanguageTable.h"

#include "SoundSystem.h"

#include "Triffid.h"

#include "Location.h"
#include "Explosion.h"
#include "Team.h"
#include "GameTime.h"
#include "ParticleSystem.h"
#include "EntityGrid.h"
#include "Unit.h"
#include "WorldPointers.h"
#include "AppState.h"


namespace Species
{
  Triffid::Triffid()
    : Building(),
      m_launchPoint(nullptr),
      m_stem(nullptr),
      m_timerSync(0.0f),
      m_pitch(0.6f),
      m_force(250.0f),
      m_variance(M_PI / 8.0f),
      m_reloadTime(60.0f),
      m_size(1.0f),
      m_damage(50.0f),
      m_useTrigger(0),
      m_triggerRadius(100.0f),
      m_triggered(false),
      m_triggerTimer(0.0f)
  {
    m_type = TypeTriffid;

    SetShape(g_resource->GetShape("TriffidHead.shp"));

    m_launchPoint = m_shape->m_rootFragment->LookupMarker("MarkerLaunchPoint");
    m_stem = m_shape->m_rootFragment->LookupMarker("MarkerTriffidStem");

    DEBUG_ASSERT(m_launchPoint);
    DEBUG_ASSERT(m_stem);

    m_triggerTimer = syncfrand(5.0f);

    memset(m_spawn, 0, NumSpawnTypes * sizeof(bool));
  }


  // Vector3::SetLength, fallback and all: a zero-length vector became (len, 0, 0)
  // rather than QNaN. Triffid's Spawn rolls its velocities from syncsfrand, which
  // can land on exactly zero, so the fallback is reachable and reproduced.
  static void SetLengthPreservingFallback(DirectX::XMFLOAT3& _vector, float _length)
  {
    float const magnitude = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&_vector)));
    if (NearlyEquals(magnitude, 0.0f))
    {
      _vector = DirectX::XMFLOAT3(_length, 0.0f, 0.0f);
      return;
    }
    DirectX::XMStoreFloat3(&_vector, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&_vector), _length / magnitude));
  }

  // TriffidEgg is drawn and exploded at m_size, scaling the three basis rows and
  // leaving the position row alone.
  static DirectX::XMFLOAT4X4 XM_CALLCONV ScaledEggBasis(DirectX::FXMVECTOR _front, DirectX::FXMVECTOR _up, DirectX::FXMVECTOR _position, float _size)
  {
    DirectX::XMMATRIX mat = BasisFromFrontAndUp(_front, _up, _position);
    DirectX::XMVECTOR const scale = DirectX::XMVectorReplicate(_size);
    mat.r[0] = DirectX::XMVectorMultiply(mat.r[0], scale);
    mat.r[1] = DirectX::XMVectorMultiply(mat.r[1], scale);
    mat.r[2] = DirectX::XMVectorMultiply(mat.r[2], scale);

    DirectX::XMFLOAT4X4 result;
    DirectX::XMStoreFloat4x4(&result, mat);
    return result;
  }

  DirectX::XMFLOAT4X4 Triffid::GetHead()
  {
    DirectX::XMVECTOR pos =
      DirectX::XMVectorMultiplyAdd(DirectX::g_XMIdentityR1, DirectX::XMVectorReplicate(m_size * 30.0f), DirectX::XMLoadFloat3(&m_pos));
    DirectX::XMVECTOR front = DirectX::XMLoadFloat3(&m_front);
    DirectX::XMVECTOR up = DirectX::g_XMIdentityR1;

    float timer = g_gameTime + m_id.GetUniqueId() * 10.0f;

    if (m_damage > 0.0f)
    {
      front = DirectX::XMVector3TransformNormal(front, DirectX::XMMatrixRotationY(sinf(timer) * m_variance * 0.5f));
      DirectX::XMVECTOR const right = DirectX::XMVector3Cross(front, up);

      float pitchVariation = 1.0f + sinf(timer) * 0.1f;
      {
        DirectX::XMVECTOR const rotation = DirectX::XMVectorScale(right, m_pitch * 0.5f * pitchVariation);
        float const lengthSquared = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(rotation));
        if (lengthSquared >= 1e-8f)
        {
          float const spin = sqrtf(lengthSquared);
          front = DirectX::XMVector3Transform(front, DirectX::XMMatrixRotationAxis(DirectX::XMVectorScale(rotation, 1.0f / spin), spin));
        }
      }

      pos = DirectX::XMVectorAdd(pos, DirectX::XMVectorSet(sinf(timer / 1.3f) * 4, sinf(timer / 1.5f), sinf(timer / 1.2f) * 3, 0.0f));

      float justFiredFraction = (m_timerSync - GetHighResTime()) / m_reloadTime;
      justFiredFraction = std::min(justFiredFraction, 1.0f);
      justFiredFraction = std::max(justFiredFraction, 0.0f);
      justFiredFraction = pow(justFiredFraction, 5);

      pos = DirectX::XMVectorNegativeMultiplySubtract(front, DirectX::XMVectorReplicate(justFiredFraction * 10.0f), pos);
    }
    else
    {
      // We are on fire, so thrash around a lot
      DirectX::XMVECTOR const right = DirectX::XMVector3Cross(front, up);
      front = DirectX::g_XMIdentityR1;
      up = DirectX::XMVector3Cross(right, front);

      {
        DirectX::XMVECTOR const rotation = DirectX::XMVectorScale(right, sinf(timer * 5.0f) * m_variance);
        float const lengthSquared = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(rotation));
        if (lengthSquared >= 1e-8f)
        {
          float const spin = sqrtf(lengthSquared);
          front = DirectX::XMVector3Transform(front, DirectX::XMMatrixRotationAxis(DirectX::XMVectorScale(rotation, 1.0f / spin), spin));
        }
      }

      up = DirectX::XMVector3TransformNormal(up, DirectX::XMMatrixRotationY(sinf(timer * 3.5f) * m_variance));

      pos = DirectX::XMVectorAdd(pos, DirectX::XMVectorSet(sinf(timer * 3.3f) * 8, sinf(timer * 4.5f), sinf(timer * 3.2f) * 6, 0.0f));
    }

    DirectX::XMMATRIX result = BasisFromFrontAndUp(front, up, pos);

    float size = m_size * (1.0f + sinf(timer) * 0.1f);

    DirectX::XMVECTOR const scale = DirectX::XMVectorReplicate(size);
    result.r[0] = DirectX::XMVectorMultiply(result.r[0], scale);
    result.r[1] = DirectX::XMVectorMultiply(result.r[1], scale);
    result.r[2] = DirectX::XMVectorMultiply(result.r[2], scale);

    DirectX::XMFLOAT4X4 head;
    DirectX::XMStoreFloat4x4(&head, result);
    return head;
  }


  bool Triffid::DoesRayHit(DirectX::XMFLOAT3 const& _rayStart, DirectX::XMFLOAT3 const& _rayDir, float _rayLen, DirectX::XMFLOAT3* _pos,
                           DirectX::XMFLOAT3* _norm)
  {
    DirectX::XMFLOAT4X4 mat = GetHead();

    RayPackage ray(_rayStart, _rayDir, _rayLen);

    return m_shape->RayHit(&ray, mat, true);
  }


  void Triffid::Render(float _predictionTime)
  {
    DirectX::XMFLOAT4X4 headMatrix = GetHead();
    DirectX::XMMATRIX head = DirectX::XMLoadFloat4x4(&headMatrix);

    // Row 3 of the head matrix is its position.
    DirectX::XMVECTOR const headPos = head.r[3];

    // RenderArrow( m_pos, headPos, 1.0f, RGBAColour(100,0,0,255) );

    // PRE-wobble head, as the legacy code had it: the stem line is drawn before
    // the damage flicker scales a row.
    DirectX::XMFLOAT3 const stemPos = m_stem->GetWorldPosition(headMatrix);

    // SetLength; rendering only, so this takes the native normalise.
    DirectX::XMVECTOR const midPoint = DirectX::XMVectorMultiplyAdd(
      DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&stemPos), headPos)), DirectX::XMVectorReplicate(10.0f), headPos);

    glColor4f(1.0f, 1.0f, 0.5f, 1.0f);
    glLineWidth(2.0f);
    glDisable(GL_TEXTURE_2D);
    glBegin(GL_LINES);
    EmitVertex(headPos);
    EmitVertex(midPoint);
    EmitVertex(midPoint);
    EmitVertex(DirectX::XMLoadFloat3(&m_pos));
    glEnd();

    //
    // If we are damaged, flicked in and out based on our health

    if (m_renderDamaged && !g_editing && m_damage > 0.0f)
    {
      float timeIndex = g_gameTime + m_id.GetUniqueId() * 10;
      float thefrand = frand();

      // r is row 0, u row 1, f row 2, in the row-vector convention.
      if (thefrand > 0.7f)
        head.r[2] = DirectX::XMVectorScale(head.r[2], 1.0f - sinf(timeIndex) * 0.5f);
      else if (thefrand > 0.4f)
        head.r[1] = DirectX::XMVectorScale(head.r[1], 1.0f - sinf(timeIndex) * 0.2f);
      else
        head.r[0] = DirectX::XMVectorScale(head.r[0], 1.0f - sinf(timeIndex) * 0.5f);
      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ONE);
    }

    // The wobble above scales a basis row IN PLACE, and both the shape below and
    // the launch marker after it use the wobbled matrix, exactly as the legacy
    // code did with its single mutable Matrix34.
    DirectX::XMFLOAT4X4 wobbledHead;
    DirectX::XMStoreFloat4x4(&wobbledHead, head);

    glEnable(GL_NORMALIZE);

    m_shape->Render(_predictionTime, wobbledHead);

    if (m_triggered && GetHighResTime() > m_timerSync - m_reloadTime * 0.25f)
    {
      // this one wants the marker's whole basis: the egg is drawn with the
      // launch point's UP as its front and its negated FRONT as its up.
      DirectX::XMFLOAT4X4 const launchMat = m_launchPoint->GetWorldMatrix(wobbledHead);
      Shape* eggShape = g_resource->GetShape("TriffidEgg.shp");

      DirectX::XMFLOAT3 const eggFront = DirectX::XMFLOAT3(launchMat._21, launchMat._22, launchMat._23);
      DirectX::XMFLOAT3 const eggPos = DirectX::XMFLOAT3(launchMat._41, launchMat._42, launchMat._43);
      DirectX::XMFLOAT3 const launchFront = DirectX::XMFLOAT3(launchMat._31, launchMat._32, launchMat._33);

      DirectX::XMMATRIX egg = BasisFromFrontAndUp(DirectX::XMLoadFloat3(&eggFront), DirectX::XMVectorNegate(DirectX::XMLoadFloat3(&launchFront)),
                                                  DirectX::XMLoadFloat3(&eggPos));
      DirectX::XMVECTOR const eggScale = DirectX::XMVectorReplicate(m_size);
      egg.r[0] = DirectX::XMVectorMultiply(egg.r[0], eggScale);
      egg.r[1] = DirectX::XMVectorMultiply(egg.r[1], eggScale);
      egg.r[2] = DirectX::XMVectorMultiply(egg.r[2], eggScale);

      DirectX::XMFLOAT4X4 eggMat;
      DirectX::XMStoreFloat4x4(&eggMat, egg);
      eggShape->Render(_predictionTime, eggMat);
    }

    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDisable(GL_NORMALIZE);
  }


  void Triffid::RenderAlphas(float _predictionTime)
  {
    if (g_editing)
    {
      glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
      glLineWidth(1.0f);

      DirectX::XMFLOAT4X4 const headMat = GetHead();
      DirectX::XMFLOAT3 const headPos(headMat._41, headMat._42, headMat._43);

      DirectX::XMFLOAT4X4 mat;
      DirectX::XMStoreFloat4x4(&mat, BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&headPos)));

      DirectX::XMFLOAT4X4 const launchMat = m_launchPoint->GetWorldMatrix(mat);
      DirectX::XMFLOAT3 const launchPos = DirectX::XMFLOAT3(launchMat._41, launchMat._42, launchMat._43);
      DirectX::XMFLOAT3 const launchFront = DirectX::XMFLOAT3(launchMat._31, launchMat._32, launchMat._33);

      DirectX::XMVECTOR const point1 = DirectX::XMLoadFloat3(&launchPos);

      // The two spread arms, mirrored about the launch direction. Written twice
      // in the legacy code with only the sign of m_variance differing.
      DirectX::XMVECTOR spread[2];
      float const variances[2] = {m_variance * 0.5f, m_variance * -0.5f};
      for (int arm = 0; arm < 2; ++arm)
      {
        // HorizontalAndNormalise: flatten to the XZ plane, then normalise.
        DirectX::XMVECTOR angle = DirectX::XMVector3Normalize(DirectX::XMVectorSetY(DirectX::XMLoadFloat3(&launchFront), 0.0f));
        angle = DirectX::XMVector3TransformNormal(angle, DirectX::XMMatrixRotationY(variances[arm]));

        DirectX::XMVECTOR const right = DirectX::XMVector3Cross(angle, DirectX::g_XMIdentityR1);
        {
          DirectX::XMVECTOR const rotation = DirectX::XMVectorScale(right, m_pitch * 0.5f);
          float const lengthSquared = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(rotation));
          if (lengthSquared >= 1e-8f)
          {
            float const spin = sqrtf(lengthSquared);
            angle = DirectX::XMVector3Transform(angle, DirectX::XMMatrixRotationAxis(DirectX::XMVectorScale(rotation, 1.0f / spin), spin));
          }
        }

        spread[arm] = DirectX::XMVectorMultiplyAdd(angle, DirectX::XMVectorReplicate(m_force * 3.0f), point1);
      }

      glBegin(GL_LINE_LOOP);
      EmitVertex(point1);
      EmitVertex(spread[0]);
      EmitVertex(spread[1]);
      glEnd();


      //
      // Create a fake egg and plot its course
      // Render our trigger location

#ifdef LOCATION_EDITOR
    if (g_locationEditor->GetMode() == LocationEditorAccess::ModeBuilding && g_locationEditor->GetSelectionId() == m_id.GetUniqueId())
    {
      // Row 2 of the head matrix is its front.
      DirectX::XMVECTOR const headFront = DirectX::XMVectorSet(headMat._31, headMat._32, headMat._33, 0.0f);

      TriffidEgg egg;
      egg.m_pos = headPos;
      DirectX::XMStoreFloat3(&egg.m_vel, DirectX::XMVectorScale(DirectX::XMVector3Normalize(headFront), m_force * m_size));
      DirectX::XMStoreFloat3(&egg.m_front, headFront);

      glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
      glLineWidth(2.0f);
      glBegin(GL_LINES);
      while (true)
      {
        EmitVertex(DirectX::XMLoadFloat3(&egg.m_pos));
        egg.Advance(nullptr);
        EmitVertex(DirectX::XMLoadFloat3(&egg.m_pos));

        if (DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&egg.m_vel))) < 20.0f)
          break;
      }
      glEnd();

      if (m_useTrigger)
      {
        DirectX::XMFLOAT3 triggerPos;
        DirectX::XMStoreFloat3(&triggerPos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&m_triggerLocation)));
        int numSteps = 20;
        glBegin(GL_LINE_LOOP);
        glLineWidth(1.0f);
        glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
        for (int i = 0; i < numSteps; ++i)
        {
          float angle = 2.0f * M_PI * (float)i / (float)numSteps;
          DirectX::XMFLOAT3 thisPos(triggerPos.x + sinf(angle) * m_triggerRadius, triggerPos.y, triggerPos.z + cosf(angle) * m_triggerRadius);
          thisPos.y = g_location->m_landscape.m_heightMap->GetValue(thisPos.x, thisPos.z);
          thisPos.y += 10.0f;
          glVertex3fv(&thisPos.x);
        }
        glEnd();

        g_editorFont.DrawText3DCentre(DirectX::XMFLOAT3(triggerPos.x, triggerPos.y + 50.0f, triggerPos.z), 10.0f, "UseTrigger: {}", m_useTrigger);
      }
    }
#endif
    }
}


void Triffid::Damage(float _damage)
{
  Building::Damage(_damage);

  bool dead = (m_damage <= 0.0f);

  m_damage += _damage;

  if (m_damage <= 0.0f && !dead)
  {
    g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), "Burn");
  }
}


void Triffid::Launch()
{
  //
  // Determine what sort of egg to launch

  std::vector<int> possibleSpawns;
  for (int i = 0; i < NumSpawnTypes; ++i)
  {
    if (m_spawn[i])
      possibleSpawns.push_back(i);
  }

  if (possibleSpawns.empty())
  {
    // Can't spawn anything
    return;
  }

  int spawnIndex = syncrand() % static_cast<int>(possibleSpawns.size());
  int spawnType = possibleSpawns[spawnIndex];


  //
  // Fire the egg

  DirectX::XMFLOAT4X4 mat = GetHead();

  DirectX::XMFLOAT4X4 const launchMat = m_launchPoint->GetWorldMatrix(mat);
  DirectX::XMFLOAT3 const launchFront = DirectX::XMFLOAT3(launchMat._31, launchMat._32, launchMat._33);
  DirectX::XMFLOAT3 const launchPos = DirectX::XMFLOAT3(launchMat._41, launchMat._42, launchMat._43);

  DirectX::XMFLOAT3 velocity;
  DirectX::XMStoreFloat3(&velocity, DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&launchFront)),
                                                           m_force * m_size * (1.0f + syncsfrand(0.2f))));

  WorldObjectId wobjId = g_location->SpawnEntities(launchPos, m_id.GetTeamId(), -1, Entity::TypeTriffidEgg, 1, velocity, 0.0f);
  TriffidEgg* triffidEgg = (TriffidEgg*)g_location->GetEntitySafe(wobjId, Entity::TypeTriffidEgg);
  if (triffidEgg)
  {
    triffidEgg->m_spawnType = spawnType;
    triffidEgg->m_size = 1.0f + syncsfrand(0.3f);
    DirectX::XMStoreFloat3(&triffidEgg->m_spawnPoint, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&m_triggerLocation)));
    triffidEgg->m_roamRange = m_triggerRadius;
  }

  g_soundSystem->TriggerBuildingEvent(SoundSourceOf(this), "LaunchEgg");
}


void Triffid::Initialise(Building* _template)
{
  Building::Initialise(_template);

  Triffid* triffid = (Triffid*)_template;

  m_size = triffid->m_size;
  m_pitch = triffid->m_pitch;
  m_force = triffid->m_force;
  m_variance = triffid->m_variance;
  m_reloadTime = triffid->m_reloadTime;

  m_useTrigger = triffid->m_useTrigger;
  m_triggerLocation.x = triffid->m_triggerLocation.x;
  m_triggerLocation.z = triffid->m_triggerLocation.z;
  m_triggerRadius = triffid->m_triggerRadius;

  m_triggerLocation.y = g_location->m_landscape.m_heightMap->GetValue(m_triggerLocation.x, m_triggerLocation.z);

  memcpy(m_spawn, triffid->m_spawn, NumSpawnTypes * sizeof(bool));

  float timeAdd = syncrand() % (int)m_reloadTime;
  timeAdd += 10.0f;
  m_timerSync = GetHighResTime() + timeAdd;
}


bool Triffid::Advance()
{
  //
  // Burn if we have been damaged

  if (m_damage <= 0.0f)
  {
    DirectX::XMFLOAT4X4 const headMat = GetHead();

    // Every sfrand call below stays in this order: they advance the RNG.
    DirectX::XMFLOAT3 const headScatter(sfrand(10.0f * m_size), sfrand(10.0f * m_size), sfrand(10.0f * m_size));
    DirectX::XMFLOAT3 fireSpawn(headMat._41 + headScatter.x, headMat._42 + headScatter.y, headMat._43 + headScatter.z);
    float fireSize = 100.0f + sfrand(100.0f * m_size);
    g_particleSystem->CreateParticle(fireSpawn, g_zeroVector, Particle::TypeFire, fireSize);

    DirectX::XMFLOAT3 const baseScatter(sfrand(10.0f * m_size), sfrand(10.0f * m_size), sfrand(10.0f * m_size));
    fireSpawn = DirectX::XMFLOAT3(m_pos.x + baseScatter.x, m_pos.y + baseScatter.y, m_pos.z + baseScatter.z);
    g_particleSystem->CreateParticle(fireSpawn, g_zeroVector, Particle::TypeFire, fireSize);

    if (frand(100.0f) < 10.0f)
    {
      g_particleSystem->CreateParticle(fireSpawn, g_zeroVector, Particle::TypeExplosionDebris);
    }

    m_size -= 0.006f;
    if (m_size <= 0.3f)
    {
      g_explosionManager.AddExplosion(m_shape, headMat);
      return true;
    }
    m_timerSync -= 0.5f;
  }


  //
  // Look in our trigger area if required

  bool oldTriggered = m_triggered;

  if (m_useTrigger == 0 || m_damage <= 0.0f)
  {
    m_triggered = true;
  }

  if (m_useTrigger > 0 && GetHighResTime() > m_triggerTimer)
  {
    START_PROFILE(g_profiler, "CheckTrigger");
    DirectX::XMFLOAT3 triggerPos;
    DirectX::XMStoreFloat3(&triggerPos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_pos), DirectX::XMLoadFloat3(&m_triggerLocation)));
    bool enemiesFound = g_location->m_entityGrid->AreEnemiesPresent(triggerPos.x, triggerPos.z, m_triggerRadius, m_id.GetTeamId());
    m_triggered = enemiesFound;
    m_triggerTimer = GetHighResTime() + 5.0f;
    END_PROFILE(g_profiler, "CheckTrigger");
  }


  //
  // If we have just triggered, start our random timer

  if (!oldTriggered && m_triggered)
  {
    float timeAdd = syncrand() % (int)m_reloadTime;
    timeAdd += 10.0f;
    m_timerSync = GetHighResTime() + timeAdd;
  }


  //
  // Launch an egg if it is time

  if (m_triggered && GetHighResTime() > m_timerSync)
  {
    Launch();
    m_timerSync = GetHighResTime() + m_reloadTime * (1.0f + syncsfrand(0.1f));
  }


  //
  // Flicker if we are damaged

  float healthFraction = (float)m_damage / 50.0f;
  float timeIndex = g_gameTime + m_id.GetUniqueId() * 10;
  m_renderDamaged = (frand(0.75f) * (1.0f - fabs(sinf(timeIndex)) * 1.2f) > healthFraction);

  return false;
}


void Triffid::Read(TextReader* _in, bool _dynamic)
{
  Building::Read(_in, _dynamic);

  m_size = atof(_in->GetNextToken());
  m_pitch = atof(_in->GetNextToken());
  m_force = atof(_in->GetNextToken());
  m_variance = atof(_in->GetNextToken());
  m_reloadTime = atof(_in->GetNextToken());
  m_useTrigger = atoi(_in->GetNextToken());

  m_triggerLocation.x = atof(_in->GetNextToken());
  m_triggerLocation.z = atof(_in->GetNextToken());
  m_triggerRadius = atof(_in->GetNextToken());

  for (int i = 0; i < NumSpawnTypes; ++i)
  {
    m_spawn[i] = (atoi(_in->GetNextToken()) == 1);
  }
}


void Triffid::Write(FileWriter* _out)
{
  Building::Write(_out);

  _out->printf("{:<6.2f} {:<6.2f} {:<6.2f} {:<6.2f} {:<6.2f} ", m_size, m_pitch, m_force, m_variance, m_reloadTime);
  _out->printf("{:d} {:<8.2f} {:<8.2f} {:<6.2f} ", m_useTrigger, m_triggerLocation.x, m_triggerLocation.z, m_triggerRadius);

  for (int i = 0; i < NumSpawnTypes; ++i)
  {
    if (m_spawn[i])
      _out->printf("1 ");
    else
      _out->printf("0 ");
  }
}


char const* Triffid::GetSpawnName(int _spawnType)
{
  static char const* names[NumSpawnTypes] = {"SpawnVirii", "SpawnCentipedes",  "SpawnSpider",  "SpawnSpirits",
                                             "SpawnEggs",  "SpawnTriffidEggs", "SpawnCitizens"};

  return names[_spawnType];
}


char const* Triffid::GetSpawnNameTranslated(int _spawnType)
{
  char const* spawnName = GetSpawnName(_spawnType);

  const std::string stringId = std::format("spawnname_{}", spawnName);

  if (ISLANGUAGEPHRASE(stringId.c_str()))
  {
    return LANGUAGEPHRASE(stringId.c_str());
  }
  else
  {
    return spawnName;
  }
}


void Triffid::ListSoundEvents(std::vector<const char*>* _list)
{
  Building::ListSoundEvents(_list);

  _list->push_back("LaunchEgg");
  _list->push_back("Burn");
}


// ============================================================================


TriffidEgg::TriffidEgg()
  : Entity(),
    m_force(1.0f),
    m_spawnType(Triffid::SpawnVirii),
    m_size(1.0f)
{
  SetType(TypeTriffidEgg);

  m_up = g_upVector;
  m_shape = g_resource->GetShape("TriffidEgg.shp");

  m_life = 20.0f + syncfrand(10.0f);
  m_timerSync = GetHighResTime() + m_life;
}


void TriffidEgg::ChangeHealth(int _amount)
{
  bool dead = m_dead;
  Entity::ChangeHealth(_amount);

  if (m_dead && !dead)
  {
    // We just died
    g_explosionManager.AddExplosion(
      m_shape, ScaledEggBasis(DirectX::XMLoadFloat3(&m_front), DirectX::XMLoadFloat3(&m_up), DirectX::XMLoadFloat3(&m_pos), m_size));
  }
}


void TriffidEgg::Spawn()
{
  int teamId = m_id.GetTeamId();

  switch (m_spawnType)
  {
  case Triffid::SpawnVirii:
  {
    int numVirii = 5 + syncrand() % 5;
    g_location->SpawnEntities(m_pos, teamId, -1, TypeVirii, numVirii, g_zeroVector, 0.0f, 100.0f);
    break;
  }

  case Triffid::SpawnCentipede:
  {
    int size = 5 + syncrand() % 5;
    Team* team = &g_location->m_teams[m_id.GetTeamId()];
    int unitId;
    Unit* unit = team->NewUnit(TypeCentipede, size, &unitId, m_pos);
    g_location->SpawnEntities(m_pos, teamId, unitId, TypeCentipede, size, g_zeroVector, 0.0f, 200.0f);
    break;
  }

  case Triffid::SpawnSpider:
  {
    WorldObjectId id = g_location->SpawnEntities(m_pos, teamId, -1, TypeSpider, 1, g_zeroVector, 0.0f, 150.0f);
    break;
  }

  case Triffid::SpawnSpirits:
  {
    int numSpirits = 5 + syncrand() % 5;
    for (int i = 0; i < numSpirits; ++i)
    {
      // The RNG calls stay in this order. SetLength on a vector that can be
      // exactly zero (both syncsfrand rolls landing on 0), where the legacy
      // fallback left (len, 0, 0) -- kept rather than taking a QNaN.
      DirectX::XMFLOAT3 vel(syncsfrand(), 0.0f, syncsfrand());
      SetLengthPreservingFallback(vel, 20.0f + syncfrand(20.0f));
      g_location->SpawnSpirit(m_pos, vel, teamId, WorldObjectId());
    }
    break;
  }

  case Triffid::SpawnEggs:
  {
    int numEggs = 5 + syncrand() % 5;
    for (int i = 0; i < numEggs; ++i)
    {
      // The RNG calls stay in this order.
      DirectX::XMFLOAT3 vel(syncsfrand(), 1.0f, syncsfrand());
      SetLengthPreservingFallback(vel, 20.0f + syncfrand(20.0f));
      g_location->SpawnEntities(m_pos, teamId, -1, TypeEgg, 1, vel, 0.0f, 0.0f);
    }
    break;
  }

  case Triffid::SpawnTriffidEggs:
  {
    int numEggs = 2 + syncrand() % 3;
    for (int i = 0; i < numEggs; ++i)
    {
      // The RNG calls stay in this order.
      DirectX::XMFLOAT3 vel(syncsfrand(), 0.5f + syncfrand(), syncsfrand());
      SetLengthPreservingFallback(vel, 75.0f + syncfrand(50.0f));
      WorldObjectId id = g_location->SpawnEntities(m_pos, teamId, -1, TypeTriffidEgg, 1, vel, 0.0f, 0.0f);
      TriffidEgg* egg = (TriffidEgg*)g_location->GetEntitySafe(id, TypeTriffidEgg);
      if (egg)
        egg->m_spawnType = syncrand() % (Triffid::NumSpawnTypes - 1);
      // The NumSpawnTypes-1 prevents Citizens from coming out
    }
    break;
  }

  case Triffid::SpawnCitizens:
  {
    int numCitizens = 10 + syncrand() % 10;
    for (int i = 0; i < numCitizens; ++i)
    {
      // The RNG calls stay in this order.
      DirectX::XMFLOAT3 vel(syncsfrand(), 1.0f, syncsfrand());
      SetLengthPreservingFallback(vel, 10.0f + syncfrand(20.0f));
      WorldObjectId id = g_location->SpawnEntities(m_pos, teamId, -1, TypeCitizen, 1, vel, 0.0f, 0.0f);
      Entity* entity = g_location->GetEntity(id);
      entity->m_front.y = 0.0f;
      DirectX::XMStoreFloat3(&entity->m_front, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&entity->m_front)));
      entity->m_onGround = false;
    }
    break;
  }
  }
}


bool TriffidEgg::Advance(Unit* _unit)
{
  if (m_dead)
    return true;

  DirectX::XMStoreFloat3(&m_pos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(SERVER_ADVANCE_PERIOD),
                                                              DirectX::XMLoadFloat3(&m_pos)));

  // Fly through the air, bounce
  m_vel.y -= 9.8f * m_force;
  DirectX::XMVECTOR const right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_vel)));

  DirectX::XMVECTOR up = DirectX::XMLoadFloat3(&m_up);
  {
    DirectX::XMVECTOR const rotation = DirectX::XMVectorScale(right, SERVER_ADVANCE_PERIOD * m_force * m_force * 30.0f);
    float const lengthSquared = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(rotation));
    if (lengthSquared >= 1e-8f)
    {
      float const spin = sqrtf(lengthSquared);
      up = DirectX::XMVector3Transform(up, DirectX::XMMatrixRotationAxis(DirectX::XMVectorScale(rotation, 1.0f / spin), spin));
    }
  }

  // m_front is built from the UNNORMALISED up, then both are normalised.
  DirectX::XMVECTOR const front = DirectX::XMVector3Cross(right, up);
  DirectX::XMStoreFloat3(&m_up, DirectX::XMVector3Normalize(up));
  DirectX::XMStoreFloat3(&m_front, DirectX::XMVector3Normalize(front));

  float landHeight = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);
  if (m_pos.y < landHeight + 3.0f)
  {
    BounceOffLandscape();
    m_force *= TRIFFIDEGG_BOUNCEFRICTION;
    DirectX::XMStoreFloat3(&m_vel, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_vel), m_force));
    if (m_pos.y < landHeight + 3.0f)
      m_pos.y = landHeight + 3.0f;
    if (m_force > 0.1f)
    {
      g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "Bounce");
    }
  }

  // Self right ourselves

  if (m_up.y < 0.3f && m_force < 0.4f)
  {
    DirectX::XMVECTOR const up = DirectX::XMVectorMultiplyAdd(DirectX::g_XMIdentityR1, DirectX::XMVectorReplicate(0.05f),
                                                              DirectX::XMVectorScale(DirectX::XMLoadFloat3(&m_up), 0.95f));
    DirectX::XMVECTOR const right = DirectX::XMVector3Cross(up, DirectX::XMLoadFloat3(&m_front));
    DirectX::XMVECTOR const front = DirectX::XMVector3Cross(right, up);
    DirectX::XMStoreFloat3(&m_up, DirectX::XMVector3Normalize(up));
    DirectX::XMStoreFloat3(&m_front, DirectX::XMVector3Normalize(front));
  }

  //
  // Break open if it is time

  if (GetHighResTime() > m_timerSync)
  {
    DirectX::XMFLOAT4X4 transform;
    DirectX::XMStoreFloat4x4(&transform,
                             BasisFromFrontAndUp(DirectX::XMLoadFloat3(&m_front), DirectX::XMLoadFloat3(&m_up), DirectX::XMLoadFloat3(&m_pos)));
    g_explosionManager.AddExplosion(m_shape, transform);
    Spawn();
    g_soundSystem->TriggerEntityEvent(SoundSourceOf(this), "BurstOpen");
    return true;
  }

  return Entity::Advance(_unit);
}


void TriffidEgg::Render(float _predictionTime)
{
  if (m_dead)
    return;

  DirectX::XMFLOAT3 predictedPos;
  DirectX::XMStoreFloat3(&predictedPos, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_vel), DirectX::XMVectorReplicate(_predictionTime),
                                                                     DirectX::XMLoadFloat3(&m_pos)));

  DirectX::XMVECTOR const right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::g_XMIdentityR1, DirectX::XMLoadFloat3(&m_vel)));

  DirectX::XMVECTOR up = DirectX::XMLoadFloat3(&m_up);
  {
    DirectX::XMVECTOR const rotation = DirectX::XMVectorScale(right, _predictionTime * m_force * m_force * 30.0f);
    float const lengthSquared = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(rotation));
    if (lengthSquared >= 1e-8f)
    {
      float const spin = sqrtf(lengthSquared);
      up = DirectX::XMVector3Transform(up, DirectX::XMMatrixRotationAxis(DirectX::XMVectorScale(rotation, 1.0f / spin), spin));
    }
  }

  // front is built from the UNNORMALISED up, then both are normalised.
  DirectX::XMVECTOR front = DirectX::XMVector3Cross(right, up);
  up = DirectX::XMVector3Normalize(up);
  front = DirectX::XMVector3Normalize(front);

  //
  // Make our size pulsate a little
  float age = (m_timerSync - GetHighResTime()) / m_life;
  age = std::max(age, 0.0f);
  age = std::min(age, 1.0f);
  float size = m_size + fabs(sinf(g_gameTime * 2.0f)) * (1.0f - age) * 0.4f;

  predictedPos.y -= size;
  DirectX::XMFLOAT4X4 const transform = ScaledEggBasis(front, up, DirectX::XMLoadFloat3(&predictedPos), size);

  glEnable(GL_NORMALIZE);
  g_renderer->SetObjectLighting();
  m_shape->Render(_predictionTime, transform);
  g_renderer->UnsetObjectLighting();
  glDisable(GL_NORMALIZE);

  g_renderer->MarkUsedCells(m_shape, transform);

  //
  // Render our shadow

  BeginRenderShadow();
  RenderShadow(predictedPos, size * 10.0f);
  EndRenderShadow();
}


bool TriffidEgg::RenderPixelEffect(float _predictionTime)
{
  Render(_predictionTime);

  return true;
}


void TriffidEgg::ListSoundEvents(std::vector<const char*>* _list)
{
  Entity::ListSoundEvents(_list);

  _list->push_back("Bounce");
  _list->push_back("BurstOpen");
}
} // namespace Species
