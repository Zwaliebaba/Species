#pragma once

#include "Building.h"

namespace Neuron
{
  class Shape;
  class ShapeFragment;
  class TextReader;
} // namespace Neuron

#define LASERFENCE_RAISESPEED 0.3f

class LaserFence : public Building
{
  protected:
    float m_status; // 0=down, 1=up
    int m_nextLaserFenceId;
    float m_sparkTimer;

    bool m_radiusSet;

    ShapeMarker* m_marker1;
    ShapeMarker* m_marker2;

    bool m_nextToggled; // set to true when the fence has enabled/disabled the next fence in the line, to prevent constant enable calling

  public:
    enum
    {
      ModeDisabled,
      ModeEnabling,
      ModeEnabled,
      ModeDisabling,
      ModeNeverOn
    };

    int m_mode;
    float m_scale;

    LaserFence();

    void Initialise(Building* _template) override;
    void SetDetail(int _detail) override;

    bool Advance() override;
    void Render(float predictionTime) override;
    void RenderAlphas(float predictionTime) override;
    void RenderLights() override;

    bool PerformDepthSort(DirectX::XMFLOAT3& _centrePos) override;
    bool IsInView() override;

    void Read(TextReader* _in, bool _dynamic) override;
    void Write(FileWriter* _out) override;

    void Enable();
    void Disable();
    void Toggle();
    bool IsEnabled();

    void Spark();
    void Electrocute(DirectX::XMFLOAT3 const& _pos);

    int GetBuildingLink() override;
    void SetBuildingLink(int _buildingId) override;

    float GetFenceFullHeight();

    // Eight sites in the .cpp built a world matrix and then scaled its three
    // basis rows by m_scale. They are NOT all the same matrix: the render and
    // marker paths level the fence against the world up, while the hit tests
    // use the building's own m_up. Both are stated here rather than inline so
    // the difference is visible instead of buried in eight near-identical
    // blocks -- getting it wrong would tilt the hit volume away from the
    // rendered fence.
    DirectX::XMFLOAT4X4 GetScaledLevelMatrix() const; // front, WORLD up, pos
    DirectX::XMFLOAT4X4 GetScaledWorldMatrix() const; // front, m_up, pos

    bool DoesSphereHit(DirectX::XMFLOAT3 const& _pos, float _radius) override;
    bool DoesRayHit(DirectX::XMFLOAT3 const& _rayStart, DirectX::XMFLOAT3 const& _rayDir, float _rayLen = 1e10, DirectX::XMFLOAT3* _pos = nullptr,
                    DirectX::XMFLOAT3* _norm = nullptr) override;
    bool DoesShapeHit(Shape* _shape, DirectX::XMFLOAT4X4 _transform) override;

    void ListSoundEvents(std::vector<const char*>* _list) override;

    DirectX::XMFLOAT3 GetTopPosition();
};
