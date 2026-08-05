#pragma once

#include "NeuronMath.h"


namespace Neuron
{
  class Triangle
  {
    public:
      DirectX::XMFLOAT3 m_corner[3];

      Triangle() {}
      Triangle(DirectX::XMFLOAT3 const& c1, DirectX::XMFLOAT3 const& c2, DirectX::XMFLOAT3 const& c3);
  };


  class Sphere
  {
    public:
      Sphere();
      void Render(DirectX::XMFLOAT3 const& pos, float radius);
      void RenderLong();

    private:
      Triangle m_topLevelTriangle[20];
      int m_displayListId;

      void ConsiderTriangle(int level, DirectX::XMFLOAT3 const& a, DirectX::XMFLOAT3 const& b, DirectX::XMFLOAT3 const& c);
  };
} // namespace Neuron
