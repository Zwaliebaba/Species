
#pragma once

#include "Building.h"


class SafeArea : public Building
{
public:
    float       m_size;
    int         m_entitiesRequired;
    int         m_entityTypeRequired;

    float       m_recountTimer;
    int         m_entitiesCounted;

public:
    SafeArea();

    void Initialise ( Building *_template );
    bool Advance    ();
    void Render     ( float predictionTime );

    bool DoesSphereHit(DirectX::XMFLOAT3 const& _pos, float _radius);
    bool DoesShapeHit(Shape* _shape, DirectX::XMFLOAT4X4 _transform);
    bool DoesRayHit(DirectX::XMFLOAT3 const& _rayStart, DirectX::XMFLOAT3 const& _rayDir, float _rayLen = 1e10, DirectX::XMFLOAT3* _pos = nullptr,
                    DirectX::XMFLOAT3* _norm = nullptr);

    char const *GetObjectiveCounter();

    void Read       ( TextReader *_in, bool _dynamic );
    void Write      ( FileWriter *_out );
};


