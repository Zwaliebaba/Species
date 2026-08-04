
#pragma once

#include <vector>

#include "Building.h"


class ResearchItem : public Building
{
  protected:
    float m_reprogrammed;
    ShapeMarker* m_end1;
    ShapeMarker* m_end2;

  public:
    int m_researchType; // indexes into GlobalResearch::m_type
    int m_level;
    bool m_inLibrary;

  public:
    ResearchItem();

    void Initialise(Building* _template);
    void SetDetail(int _detail);

    bool Advance();
    void Render(float _predictionTime);
    void RenderAlphas(float _predictionTime);
    bool RenderPixelEffect(float _predictionTime);

    bool NeedsReprogram();
    bool Reprogram();

    void Read(TextReader* _in, bool _dynamic);
    void Write(FileWriter* _out);

    void GetEndPositions(Vector3& _end1, Vector3& _end2);

    bool DoesSphereHit(DirectX::XMFLOAT3 const& _pos, float _radius);
    bool DoesShapeHit(Shape* _shape, DirectX::XMFLOAT4X4 _transform);
    bool DoesRayHit(DirectX::XMFLOAT3 const& _rayStart, DirectX::XMFLOAT3 const& _rayDir, float _rayLen, DirectX::XMFLOAT3* _pos,
                    DirectX::XMFLOAT3* norm);

    bool IsInView();

    void ListSoundEvents(std::vector<const char*>* _list);
};
