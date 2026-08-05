
#pragma once

#include <string>

#include "Building.h"

#define SCRIPTRIGGER_RUNALWAYS 999
#define SCRIPTRIGGER_RUNNEVER 998
#define SCRIPTRIGGER_RUNCAMENTER 997
#define SCRIPTRIGGER_RUNCAMVIEW 996


class ScriptTrigger : public Building
{
  public:
    std::string m_scriptFilename;
    float m_range;
    int m_entityType;
    int m_linkId;

  protected:
    int m_triggered;
    float m_timer;

  public:
    ScriptTrigger();

    void Initialise(Building* _template);
    bool Advance();
    void RenderAlphas(float predictionTime);

    void Trigger();

    bool DoesSphereHit(DirectX::XMFLOAT3 const& _pos, float _radius);
    bool DoesShapeHit(Shape* _shape, DirectX::XMFLOAT4X4 _transform);
    bool DoesRayHit(DirectX::XMFLOAT3 const& _rayStart, DirectX::XMFLOAT3 const& _rayDir, float _rayLen = 1e10, DirectX::XMFLOAT3* _pos = nullptr,
                    DirectX::XMFLOAT3* _norm = nullptr);

    int GetBuildingLink();
    void SetBuildingLink(int _buildingId);

    void Read(TextReader* _in, bool _dynamic);
    void Write(FileWriter* _out);
};
