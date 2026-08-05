
#pragma once

#include <string>
#include <string_view>

#include "Building.h"


namespace Species
{
  class StaticShape : public Building
  {
    public:
      std::string m_shapeName;
      float m_scale;

    public:
      StaticShape();

      void Initialise(Building* _template);
      void SetDetail(int _detail);

      void SetShapeName(std::string_view _shapeName);
      void SetStringId(char* _stringId);

      bool Advance();
      void Render(float _predictionTime);

      // Building::GetWorldMatrix with this class's uniform scale folded into the
      // three basis rows. All six render and hit-test paths below built the same
      // matrix inline and then scaled r, u and f by m_scale; stated once here.
      DirectX::XMFLOAT4X4 GetScaledWorldMatrix() const;

      bool DoesSphereHit(DirectX::XMFLOAT3 const& _pos, float _radius);
      bool DoesShapeHit(Shape* _shape, DirectX::XMFLOAT4X4 _transform);
      bool DoesRayHit(DirectX::XMFLOAT3 const& _rayStart, DirectX::XMFLOAT3 const& _rayDir, float _rayLen = 1e10, DirectX::XMFLOAT3* _pos = nullptr,
                      DirectX::XMFLOAT3* _norm = nullptr); // pos/norm will not always be available

      void Read(TextReader* _in, bool _dynamic);
      void Write(FileWriter* _out);
  };
} // namespace Species
