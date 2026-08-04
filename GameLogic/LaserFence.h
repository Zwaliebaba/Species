#pragma once

#include "Building.h"

class Shape;
class ShapeFragment;
class TextReader;

#define LASERFENCE_RAISESPEED       0.3f

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

    bool PerformDepthSort(Vector3& _centrePos) override;
    bool IsInView() override;

    void Read(TextReader* _in, bool _dynamic) override;
    void Write(FileWriter* _out) override;

    void Enable();
    void Disable();
    void Toggle();
    bool IsEnabled();

    void Spark();
    void Electrocute(const Vector3& _pos);

    int GetBuildingLink() override;
    void SetBuildingLink(int _buildingId) override;

    float GetFenceFullHeight();

    bool DoesSphereHit(const Vector3& _pos, float _radius) override;
    bool DoesRayHit(const Vector3& _rayStart, const Vector3& _rayDir, float _rayLen = 1e10, Vector3* _pos = nullptr,
                    Vector3* _norm = nullptr) override;
    bool DoesShapeHit(Shape* _shape, Matrix34 _transform) override;

    void ListSoundEvents(std::vector<const char*>* _list) override;

    Vector3 GetTopPosition();
};
