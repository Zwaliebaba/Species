#pragma once

#include <memory>
#include <vector>

#include "NeuronMath.h"

#include "Entity.h"
#include "WorldObject.h"

#include "ProtocolLimits.h"

namespace Neuron
{
  class Shape;
  class ShapeFragment;
  class ShapeMarker;
  class TextReader;
  class FileWriter;
} // namespace Neuron


namespace Species
{
  class BuildingPort;

  class Building : public WorldObject
  {
    public:
      enum
      {
        TypeInvalid, // When you add an entry here remember to update building.cpp
        TypeFactory, // 1
        TypeCave,    // 2
        TypeRadarDish,
        TypeLaserFence,
        TypeControlTower,
        TypeGunTurret,
        TypeBridge,
        TypePowerstation,
        TypeTree,
        TypeWall,
        TypeTrunkPort,
        TypeResearchItem,
        TypeLibrary,
        TypeGenerator,
        TypePylon,
        TypePylonStart,
        TypePylonEnd,
        TypeSolarPanel,
        TypeTrackLink,
        TypeTrackJunction,
        TypeTrackStart,
        TypeTrackEnd,
        TypeRefinery,
        TypeMine,
        TypeYard,
        TypeDisplayScreen,
        TypeUpgradePort,
        TypePrimaryUpgradePort,
        TypeIncubator,
        TypeAntHill,
        TypeSafeArea,
        TypeTriffid,
        TypeSpiritReceiver,
        TypeReceiverLink,
        TypeReceiverSpiritSpawner,
        TypeSpiritProcessor,
        TypeSpawnPoint,
        TypeSpawnPopulationLock,
        TypeSpawnPointMaster,
        TypeSpawnLink,
        TypeAITarget,
        TypeAISpawnPoint,
        TypeBlueprintStore,
        TypeBlueprintConsole,
        TypeBlueprintRelay,
        TypeScriptTrigger,
        TypeSpam,
        TypeGodDish,
        TypeStaticShape,
        TypeFuelGenerator,
        TypeFuelPipe,
        TypeFuelStation,
        TypeEscapeRocket,
        TypeFenceSwitch,
        TypeDynamicHub,
        TypeDynamicNode,
        TypeFeedingTube,
        NumBuildingTypes
      };

      // Braced to zero: Vector3's default constructor did it and XMFLOAT3's does
      // not. Building's constructor assigns m_front and m_up but not m_centrePos,
      // which Initialise accumulates into via the shape's centre.
      DirectX::XMFLOAT3 m_front{0.0f, 0.0f, 0.0f};
      DirectX::XMFLOAT3 m_up{0.0f, 0.0f, 0.0f};
      float m_timeOfDeath;
      bool m_dynamic; // Only appears on this level, not all levels for this map
      bool m_isGlobal;
      DirectX::XMFLOAT3 m_centrePos{0.0f, 0.0f, 0.0f};
      float m_radius;

      bool m_destroyed; // Building has been destroyed using the script command DestroyBuilding, remove it next Advance

      Shape* m_shape;
      std::vector<ShapeMarker*> m_lights;                 // Ownership lights
      std::vector<std::unique_ptr<BuildingPort>> m_ports; // Require Citizens in them to operate

      static Shape* s_controlPad;
      static ShapeMarker* s_controlPadStatus;

    public:
      Building();

      virtual void Initialise(Building* _template);
      virtual bool Advance();

      virtual void SetShape(Shape* _shape);
      void SetShapeLights(ShapeFragment* _fragment); // Recursivly search for lights
      void SetShapePorts(ShapeFragment* _fragment);

      virtual void SetDetail(int _detail);

      virtual bool IsInView();

      virtual void Render(float predictionTime);
      virtual void RenderAlphas(float predictionTime);
      virtual void RenderLights();
      virtual void RenderPorts();
      virtual void RenderHitCheck();
      virtual void RenderLink(); // ie link to another building

      // The building's own basis as a world matrix. Ten sites in Building.cpp
      // built it inline from m_front, m_up and m_pos; this states it once.
      DirectX::XMFLOAT4X4 GetWorldMatrix() const;

      virtual bool PerformDepthSort(DirectX::XMFLOAT3& _centrePos); // Return true if you plan to use transparencies

      virtual void SetTeamId(int _teamId);
      virtual void Reprogram(float _complete);
      virtual void ReprogramComplete();

      virtual void Damage(float _damage);
      virtual void Destroy(float _intensity);

      DirectX::XMFLOAT3 PushFromBuilding(DirectX::XMFLOAT3 const& _pos, float _radius);

      virtual void EvaluatePorts();
      virtual int GetNumPorts();
      virtual int GetNumPortsOccupied();
      virtual WorldObjectId GetPortOccupant(int _portId);
      virtual bool GetPortPosition(int _portId, DirectX::XMFLOAT3& _pos, DirectX::XMFLOAT3& _front);

      virtual void OperatePort(int _portId, int _teamId);
      virtual int GetPortOperatorCount(int _portId, int _teamId);

      virtual char const* GetObjectiveCounter();

      virtual bool DoesSphereHit(DirectX::XMFLOAT3 const& _pos, float _radius);
      virtual bool DoesShapeHit(Shape* _shape, DirectX::XMFLOAT4X4 _transform);
      virtual bool DoesRayHit(DirectX::XMFLOAT3 const& _rayStart, DirectX::XMFLOAT3 const& _rayDir, float _rayLen = 1e10,
                              DirectX::XMFLOAT3* _pos = nullptr,
                              DirectX::XMFLOAT3* _norm = nullptr); // pos/norm will not always be available

      virtual void ListSoundEvents(std::vector<const char*>* _list);

      virtual void Read(TextReader* _in, bool _dynamic); // Use these to read/write additional building-specific
      virtual void Write(FileWriter* _out);              // data to the level files

      virtual int GetBuildingLink();                 // Allows a building to link to another
      virtual void SetBuildingLink(int _buildingId); // eg control towers

      static char const* GetTypeName(int _type);
      static int GetTypeId(char const* _name);
      static Building* CreateBuilding(int _type);
      static Building* CreateBuilding(char* _name);

      static char const* GetTypeNameTranslated(int _type);
  };


  class BuildingPort
  {
    public:
      ShapeMarker* m_marker;
      WorldObjectId m_occupant;
      // Identity by default, for the reason spelled out on ShapeMarker::m_transform.
      // SetShapePorts assigns all sixteen floats before the port is published, so
      // unlike that one this is not a live bug -- but the class is constructed with
      // `new BuildingPort()` and read through raw row numbers, and the fourth column
      // is exactly the one nobody remembers to write.
      DirectX::XMFLOAT4X4 m_mat{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
      int m_counter[NUM_TEAMS];
  };
} // namespace Species
