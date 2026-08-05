
#pragma once

#include <vector>

#include "Building.h"


class FileWriter;


class ControlTower : public Building
{
  public:
    float m_ownership; // 100 = strongly owned, 5 = nearly lost, 0 = neutral

  protected:
    static Shape* s_dishShape;
    ShapeMarker* m_lightPos;
    ShapeMarker* m_reprogrammer[3];
    ShapeMarker* m_console[3];
    ShapeMarker* m_dishPos;

    // Braced to zero, and this one is load-bearing rather than tidiness.
    // Advance tests it against zero to decide whether the dish basis has been
    // computed yet; the legacy code wrote that test as `== Matrix34()`, which
    // compared indeterminate memory on both sides and only worked because a
    // fresh allocation happens to be zeroed. The braces make it true.
    DirectX::XMFLOAT4X4 m_dishMatrix{};

    bool m_beingReprogrammed[3]; // One bool for each slot
    int m_controlBuildingId;     // Whom I affect

    float m_checkTargetTimer;

  public:
    ControlTower();

    void Initialise(Building* _template);

    bool Advance();

    bool IsInView();
    void Render(float _predictionTime);
    void RenderAlphas(float _predictionTime);

    int GetAvailablePosition(DirectX::XMFLOAT3& _pos, DirectX::XMFLOAT3& _front); // Finds place for reprogrammer
    void GetConsolePosition(int _position, DirectX::XMFLOAT3& _pos);

    void BeginReprogram(int _position);
    bool Reprogram(int _teamId); // Returns true if job completed
    void EndReprogram(int _position);

    void ListSoundEvents(std::vector<const char*>* _list);

    void Read(TextReader* _in, bool _dynamic);
    void Write(FileWriter* _out);

    int GetBuildingLink();
    void SetBuildingLink(int _buildingId);
};
