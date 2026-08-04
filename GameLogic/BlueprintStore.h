#pragma once

#include "Building.h"


class BlueprintBuilding : public Building
{
  public:
    int m_buildingLink;
    float m_infected;
    int m_segment;

  protected:
    ShapeMarker* m_marker;
    DirectX::XMFLOAT3 m_vel{0.0f, 0.0f, 0.0f};

  public:
    BlueprintBuilding();

    void Initialise(Building* _template);
    bool Advance();
    bool IsInView();
    void Render(float _predictionTime);
    void RenderAlphas(float _predictionTime);

    virtual void SendBlueprint(int _segment, bool _infected);

    DirectX::XMFLOAT4X4 GetMarker(float _predictionTime);

    void Read(TextReader* _in, bool _dynamic);
    void Write(FileWriter* _out);

    int GetBuildingLink();
    void SetBuildingLink(int _buildingId);
};


// ============================================================================

#define BLUEPRINTSTORE_NUMSEGMENTS 4

class BlueprintStore : public BlueprintBuilding
{
  public:
    float m_segments[BLUEPRINTSTORE_NUMSEGMENTS];

  public:
    BlueprintStore();

    void GetDisplay(DirectX::XMFLOAT3& _pos, DirectX::XMFLOAT3& _right, DirectX::XMFLOAT3& _up, float& _size);

    void Initialise(Building* _template);
    bool Advance();
    void Render(float _predictionTime);
    void RenderAlphas(float _predictionTime);
    void SendBlueprint(int _segment, bool _infected);

    char const* GetObjectiveCounter();

    int GetNumInfected(); // Returns number of segments totally infected ie == 100.0f
    int GetNumClean();    // Returns number of segments totally clean ie == 0.0f
};


// ============================================================================


class BlueprintConsole : public BlueprintBuilding
{
  public:
    BlueprintConsole();

    void Initialise(Building* _template);

    void RecalculateOwnership();
    bool Advance();
    void Render(float _predictionTime);
    void RenderPorts();

    void Read(TextReader* _in, bool _dynamic);
    void Write(FileWriter* _out);
};


// ============================================================================


class BlueprintRelay : public BlueprintBuilding
{
  public:
    float m_altitude;

  public:
    BlueprintRelay();

    void Initialise(Building* _template);
    void SetDetail(int _detail);

    bool Advance();

    void Render(float _predictionTime);

    void Read(TextReader* _in, bool _dynamic);
    void Write(FileWriter* _out);
};
