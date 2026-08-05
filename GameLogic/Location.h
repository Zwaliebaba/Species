#pragma once

#include <float.h>

#include "SliceWalker.h"
#include "SlotMap.h"
#include "NeuronMath.h"

#include "Landscape.h"
#include "Building.h"
#include "WorldObject.h"
#include "Weapons.h"
#include "Spirit.h"
#include "LocationAccess.h"


class ServerToClientLetter;
class WorldObject;
class WorldObjectEffect;
class Entity;
class EntityGrid;
class ObstructionGrid;
class WorldObjectId;
class Unit;
class LaserGod;
class LevelFile;
class Clouds;
class Water;
class Light;
class Team;
class TeamControls;


// ****************************************************************************
//  Class Location
// ****************************************************************************

class Location : public LocationAccess
{
  protected:
    int m_lastSliceProcessed;
    bool m_missionComplete;

    void SetMyTeamId(unsigned char _teamId);
    void LoadLevel(char const* _missionFilename, char const* _mapFilename);

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
    void RenderMagic();
    void RenderSpirits();
    void RenderClouds();
    void RenderWater();

    void InitLandscape();
    void InitLights();
    void InitTeams();

    void DoMissionCompleteActions();

    DirectX::XMFLOAT3 FindValidSpawnPosition(DirectX::XMFLOAT3 const& _pos, float _spread);

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

  public:
    Location();
    ~Location();

    void Init(char const* _missionFilename, char const* _mapFilename);
    void InitBuildings();
    void Empty();

    void Advance(int _slice);
    void Render(bool renderWaterAndClouds = true);

    void InitialiseTeam(unsigned char _teamId, unsigned char _teamType);

    void RemoveTeam(unsigned char _teamId);

    int GetBuildingId(DirectX::XMFLOAT3 const& startRay, DirectX::XMFLOAT3 const& direction, unsigned char teamId, float _maxDistance = FLT_MAX,
                      float* _range = nullptr);
    int GetUnitId(DirectX::XMFLOAT3 const& startRay, DirectX::XMFLOAT3 const& direction, unsigned char teamId, float* _range = nullptr);
    WorldObjectId GetEntityId(DirectX::XMFLOAT3 const& startRay, DirectX::XMFLOAT3 const& direction, unsigned char teamId, float* _range = nullptr);

    bool IsWalkable(DirectX::XMFLOAT3 const& _from, DirectX::XMFLOAT3 const& _to, bool _evaluateCliffs = false);
    bool IsVisible(DirectX::XMFLOAT3 const& _from, DirectX::XMFLOAT3 const& _to);

    void UpdateTeam(unsigned char teamId, TeamControls const& teamControls);

    int SpawnSpirit(DirectX::XMFLOAT3 const& _pos, DirectX::XMFLOAT3 const& _vel, unsigned char _teamId, WorldObjectId _id);
    void ThrowWeapon(DirectX::XMFLOAT3 const& _pos, DirectX::XMFLOAT3 const& _target, int _type, unsigned char _fromTeamId);
    void FireRocket(DirectX::XMFLOAT3 const& _pos, DirectX::XMFLOAT3 const& _target, unsigned char _fromTeamId);
    void FireLaser(DirectX::XMFLOAT3 const& _pos, DirectX::XMFLOAT3 const& _vel, unsigned char _fromTeamId);
    void FireTurretShell(DirectX::XMFLOAT3 const& _pos, DirectX::XMFLOAT3 const& _vel);
    void Bang(DirectX::XMFLOAT3 const& _pos, float _range, float _damage);
    void CreateShockwave(DirectX::XMFLOAT3 const& _pos, float _size, unsigned char _teamId = 255);

    bool MissionComplete();

    void AdvanceChristmas();
    static int ChristmasModEnabled(); // 0 = unavailable, 1 = enabled, 2 = disabled

    WorldObjectId SpawnEntities(DirectX::XMFLOAT3 const& _pos, unsigned char _teamId, int _unitId, unsigned char _type, int _numEntities,
                                DirectX::XMFLOAT3 const& _vel, float _spread, float _range = -1.0f, int _routeId = -1, int _routeWaypointId = -1);

    int GetSpirit(WorldObjectId _id);

    bool IsFriend(unsigned char _teamId1, unsigned char _teamId2);

    Team* GetMyTeam();
    Entity* GetEntity(DirectX::XMFLOAT3 const& _rayStart, DirectX::XMFLOAT3 const& _rayDir);
    Building* GetBuilding(DirectX::XMFLOAT3 const& _rayStart, DirectX::XMFLOAT3 const& _rayDir);

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
    bool WorldObjectExists(WorldObjectId const& _id) override;
    bool GetSoundSource(WorldObjectId const& _id, DirectX::XMFLOAT3* _pos, DirectX::XMFLOAT3* _vel) override;
};
