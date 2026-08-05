#pragma once

#include "NeuronMath.h"


namespace Species
{
  class Sierpinski3D
  {
    public:
      DirectX::XMFLOAT3* m_points;
      unsigned int m_numPoints;

      Sierpinski3D(unsigned int _numPoints);
      ~Sierpinski3D();

      void Render(float _scale);
  };
} // namespace Species
