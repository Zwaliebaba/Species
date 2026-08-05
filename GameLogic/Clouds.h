
#pragma once

#include "NeuronMath.h"


class Clouds
{
  protected:
    DirectX::XMFLOAT3 m_offset{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 m_vel{0.0f, 0.0f, 0.0f};

    void RenderQuad(float posNorth, float posSouth, float posEast, float posWest, float height, float texNorth, float texSouth, float texEast,
                    float texWest);

  public:
    Clouds();

    void Advance();

    void Render(float _predictionTime);

    void RenderFlat(float _predictionTime);
    void RenderBlobby(float _predictionTime);
    void RenderSky();
};
