#pragma once

#include "Building.h"
#include "Landscape.h"
#include "LocationAccess.h"
#include "NeuronMath.h"
#include "SliceWalker.h"
#include "SlotMap.h"
#include "Spirit.h"
#include "Weapons.h"
#include "WorldObject.h"

class ServerToClientLetter;
class WorldObjectEffect;
class WorldObjectId;
class LaserGod;
class TeamControls;

// ****************************************************************************
//  Class Location
// ****************************************************************************

namespace Species
{
  class WorldObject;
  class Entity;
  class EntityGrid;
  class ObstructionGrid;
  class Unit;
  class LevelFile;
  class Clouds;
  class Water;
  class Light;
  class Team;

  class Location : public LocationAccess
  {
    protected:
      int m_lastSliceProcessed;
      bool m_missionComplete;

      void LoadLevel(const char* _missionFilename, const char* _mapFilename);

      void AdvanceWeapons(int _slice);
      void AdvanceBuildings(int _slice);
      void AdvanceTeams(int _slice);
      void AdvanceSpirits(int _slice);
      void AdvanceClouds(int _slice);

      void RenderLandscape();
      void RenderWeapons();
      void RenderBuildings();
      void RenderBuildingAlphas();
      void RenderParticles();
      void RenderTeams();
      void RenderSpirits();
      void RenderClouds();
      void RenderWater();

      void InitLandscape();
      void InitLights();
      void InitTeams();

      void DoMissionCompleteActions();

      DirectX::XMFLOAT3 FindValidSpawnPosition(const DirectX::XMFLOAT3& _pos, float _spread);

    public:
      Landscape m_landscape;
      EntityGrid* m_entityGrid;
      ObstructionGrid* m_obstructionGrid;
      LevelFile* m_levelFile;
      Clouds* m_clouds;
      Water* m_water;

      Team* m_teams;

      float m_christmasTimer;

      FastSlotMap<Light*> m_lights;
      FastSlotMap<Building*> m_buildings;
      FastSlotMap<Spirit> m_spirits;
      FastSlotMap<Laser> m_lasers;
      FastSlotMap<WorldObject*> m_effects;

      // One walker per sliced container — the bookkeeping the legacy sliced
      // array carried as a base class. m_lights is not advanced in slices and so
      // has none. Empty() resets all four alongside the containers they walk:
      // emptying a container out from under a walk is what the legacy Empty did,
      // and the walk has to begin at slice 0 again.
      SliceWalker m_buildingsWalker;
      SliceWalker m_spiritsWalker;
      SliceWalker m_lasersWalker;
      SliceWalker m_effectsWalker;

      Location();
      ~Location() override;

      void Init(const char* _missionFilename, const char* _mapFilename);
      void InitBuildings();
      void Empty();

      void Advance(int _slice);
      void Render(bool renderWaterAndClouds = true);

      void InitialiseTeam(unsigned char _teamId, unsigned char _teamType);

      void RemoveTeam(unsigned char _teamId);

      int GetBuildingId(const DirectX::XMFLOAT3& startRay, const DirectX::XMFLOAT3& direction, unsigned char teamId,
                        float _maxDistance = FLT_MAX, float* _range = nullptr);
      int GetUnitId(const DirectX::XMFLOAT3& startRay, const DirectX::XMFLOAT3& direction, unsigned char teamId, float* _range = nullptr);
      WorldObjectId GetEntityId(const DirectX::XMFLOAT3& startRay, const DirectX::XMFLOAT3& direction, unsigned char teamId,
                                float* _range = nullptr);

      bool IsWalkable(const DirectX::XMFLOAT3& _from, const DirectX::XMFLOAT3& _to, bool _evaluateCliffs = false);
      bool IsVisible(const DirectX::XMFLOAT3& _from, const DirectX::XMFLOAT3& _to);

      void UpdateTeam(unsigned char teamId, const TeamControls& teamControls);

      int SpawnSpirit(const DirectX::XMFLOAT3& _pos, const DirectX::XMFLOAT3& _vel, unsigned char _teamId, WorldObjectId _id);
      void ThrowWeapon(const DirectX::XMFLOAT3& _pos, const DirectX::XMFLOAT3& _target, int _type, unsigned char _fromTeamId);
      void FireRocket(const DirectX::XMFLOAT3& _pos, const DirectX::XMFLOAT3& _target, unsigned char _fromTeamId);
      void FireLaser(const DirectX::XMFLOAT3& _pos, const DirectX::XMFLOAT3& _vel, unsigned char _fromTeamId);
      void FireTurretShell(const DirectX::XMFLOAT3& _pos, const DirectX::XMFLOAT3& _vel);
      void Bang(const DirectX::XMFLOAT3& _pos, float _range, float _damage);
      void CreateShockwave(const DirectX::XMFLOAT3& _pos, float _size, unsigned char _teamId = 255);

      bool MissionComplete();

      void AdvanceChristmas();
      static int ChristmasModEnabled(); // 0 = unavailable, 1 = enabled, 2 = disabled

      WorldObjectId SpawnEntities(const DirectX::XMFLOAT3& _pos, unsigned char _teamId, int _unitId, unsigned char _type, int _numEntities,
                                  const DirectX::XMFLOAT3& _vel, float _spread, float _range = -1.0f, int _routeId = -1,
                                  int _routeWaypointId = -1);

      int GetSpirit(WorldObjectId _id);

      bool IsFriend(unsigned char _teamId1, unsigned char _teamId2);

      Team* GetMyTeam();
      Entity* GetEntity(const DirectX::XMFLOAT3& _rayStart, const DirectX::XMFLOAT3& _rayDir);
      Building* GetBuilding(const DirectX::XMFLOAT3& _rayStart, const DirectX::XMFLOAT3& _rayDir);

      WorldObject* GetWorldObject(WorldObjectId _id);
      Entity* GetEntity(WorldObjectId _id);
      Entity* GetEntitySafe(WorldObjectId _id, unsigned char _type); // Safe to cast
      Unit* GetUnit(WorldObjectId _id);
      WorldObject* GetEffect(WorldObjectId _id);
      Building* GetBuilding(int _id);
      Spirit* GetSpirit(int _index);

      void SetupFog();
      void SetupLights();

      void WaterReflect(); // inverts direction of all lights

      void FlushOpenGlState() override;
      void RegenerateOpenGlState() override;

      float GroundHeight(float _worldX, float _worldZ) override;
      bool WorldObjectExists(const WorldObjectId& _id) override;
      bool GetSoundSource(const WorldObjectId& _id, DirectX::XMFLOAT3* _pos, DirectX::XMFLOAT3* _vel) override;
  };
} // namespace Species
