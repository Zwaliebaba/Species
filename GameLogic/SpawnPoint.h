#pragma once

#include <memory>

#include "Building.h"


// ============================================================================


namespace Species
{
  class SpawnBuildingSpirit
  {
    public:
      int m_targetBuildingId;
      float m_currentProgress;
  };


  class SpawnBuildingLink
  {
    public:
      int m_targetBuildingId; // Who I am directly linked to

      std::vector<int> m_targets; // List of all SpawnPoint buildings reachable down this route
      std::vector<SpawnBuildingSpirit*> m_spirits;
  };


  class SpawnBuilding : public Building
  {
    protected:
      std::vector<std::unique_ptr<SpawnBuildingLink>> m_links;
      ShapeMarker* m_spiritLink;

      // Braced to zero: Vector3's default constructor did it, XMFLOAT3's does
      // not, and SpawnBuilding's constructor never assigned it.
      DirectX::XMFLOAT3 m_visibilityMidpoint{0.0f, 0.0f, 0.0f};
      float m_visibilityRadius;

    public:
      SpawnBuilding();
      ~SpawnBuilding();

      void Initialise(Building* _template);
      bool Advance();

      virtual void TriggerSpirit(SpawnBuildingSpirit* _spirit);

      bool IsInView();
      void Render(float _predictionTime);
      void RenderAlphas(float _predictionTime);
      void RenderSpirit(DirectX::XMFLOAT3 const& _pos);

      DirectX::XMFLOAT3 GetSpiritLink();
      void SetBuildingLink(int _buildingId);
      void ClearLinks();

      std::vector<int>* ExploreLinks(); // Returns a list of all SpawnBuildings accessable

      void Read(TextReader* _in, bool _dynamic);
      void Write(FileWriter* _out);
  };

  // ============================================================================

  class SpawnLink : public SpawnBuilding
  {
    public:
      SpawnLink();
  };


  // ============================================================================

  class MasterSpawnPoint : public SpawnBuilding
  {
    protected:
      static int s_masterSpawnPointId;
      bool m_exploreLinks;

    public:
      MasterSpawnPoint();

      bool Advance();
      void Render(float _predictionTime);
      void RenderAlphas(float _predictionTime);

      void RequestSpirit(int _targetBuildingId);

      char const* GetObjectiveCounter();

      static MasterSpawnPoint* GetMasterSpawnPoint();
  };


  // ============================================================================


  class SpawnPoint : public SpawnBuilding
  {
    protected:
      void RecalculateOwnership();
      bool PopulationLocked();

    protected:
      float m_evaluateTimer;
      float m_spawnTimer;
      int m_populationLock; // Building ID (if found), -1 = not yet searched, -2 = nothing found
      int m_numFriendsNearby;
      ShapeMarker* m_doorMarker;

    public:
      SpawnPoint();

      bool Advance();

      bool PerformDepthSort(DirectX::XMFLOAT3& _centrePos);

      void TriggerSpirit(SpawnBuildingSpirit* _spirit);

      void Render(float _predictionTime);
      void RenderAlphas(float _predictionTime);
      void RenderPorts();
  };


  // ============================================================================

  class SpawnPopulationLock : public Building
  {
    public:
      float m_searchRadius;
      int m_maxPopulation;
      int m_teamCount[NUM_TEAMS];

    protected:
      static float s_overpopulationTimer;
      static int s_overpopulation;
      int m_originalMaxPopulation;
      float m_recountTimer;
      int m_recountTeamId;

    public:
      SpawnPopulationLock();

      void Initialise(Building* _template);
      bool Advance();
      void Render(float _predictionTime);
      void RenderAlphas(float _predictionTime);

      void Read(TextReader* _in, bool _dynamic);
      void Write(FileWriter* _out);

      bool DoesSphereHit(DirectX::XMFLOAT3 const& _pos, float _radius);
      bool DoesShapeHit(Shape* _shape, DirectX::XMFLOAT4X4 _transform);
  };
} // namespace Species
