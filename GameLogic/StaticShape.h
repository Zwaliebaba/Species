
#pragma once

#include "Building.h"


class StaticShape : public Building
{
  public:
    char m_shapeName[256];
    float m_scale;

  public:
    StaticShape();

    void Initialise(Building* _template);
    void SetDetail(int _detail);

    void SetShapeName(char* _shapeName);
    void SetStringId(char* _stringId);

    bool Advance();
    void Render(float _predictionTime);

    bool DoesSphereHit(DirectX::XMFLOAT3 const& _pos, float _radius);
    bool DoesShapeHit(Shape* _shape, DirectX::XMFLOAT4X4 _transform);
    bool DoesRayHit(DirectX::XMFLOAT3 const& _rayStart, DirectX::XMFLOAT3 const& _rayDir, float _rayLen = 1e10, DirectX::XMFLOAT3* _pos = nullptr,
                    DirectX::XMFLOAT3* _norm = nullptr); // pos/norm will not always be available

    void Read(TextReader* _in, bool _dynamic);
    void Write(FileWriter* _out);
};
