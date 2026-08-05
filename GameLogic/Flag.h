
#pragma once

#include "NeuronMath.h"

#define FLAG_RESOLUTION 6


namespace Species
{
  class Flag
  {
    protected:
      // Flag's constructor assigns none of these, so every one relied on
      // Vector3's zeroing default constructor. The array matters most:
      // check_math_types deliberately does not check array members — that rule
      // was tried, measured and refused — so nothing but this initialiser
      // stands between it and stack garbage if Render runs before Initialise.
      DirectX::XMFLOAT3 m_flag[FLAG_RESOLUTION][FLAG_RESOLUTION]{};

      int m_texId;
      DirectX::XMFLOAT3 m_pos{0.0f, 0.0f, 0.0f};
      DirectX::XMFLOAT3 m_front{0.0f, 0.0f, 0.0f};
      DirectX::XMFLOAT3 m_up{0.0f, 0.0f, 0.0f};
      float m_size;

    public:
      Flag();

      void Initialise();

      void SetTexture(int _textureId);
      void SetPosition(DirectX::XMFLOAT3 const& _pos);
      void SetOrientation(DirectX::XMFLOAT3 const& _front, DirectX::XMFLOAT3 const& _up);
      void SetSize(float _size);

      void Render();
      void RenderText(int _posX, int _posY, char* _caption);
  };
} // namespace Species
