#include "pch.h"

#include "FileWriter.h"
#include "Resource.h"
#include "TextStreamReaders.h"
#include "Shape.h"
#include "DebugRender.h"
#include "LanguageTable.h"

#include "StaticShape.h"

#include "Location.h"
#include "WorldPointers.h"


StaticShape::StaticShape()
  : Building(),
    m_scale(1.0f)
{
  m_type = TypeStaticShape;

  m_shapeName = "none";
}


void StaticShape::Initialise(Building* _template)
{
  Building::Initialise(_template);

  StaticShape* staticShape = (StaticShape*)_template;

  m_scale = staticShape->m_scale;
  SetShapeName(staticShape->m_shapeName);
}


// The basis Building::GetWorldMatrix states, with m_scale folded into the three
// basis rows. Row 3 is the position and is deliberately left unscaled -- the
// legacy Matrix34 scaled only r, u and f, so scaling it here would move every
// static shape away from the origin by a factor of m_scale.
DirectX::XMFLOAT4X4 StaticShape::GetScaledWorldMatrix() const
{
  DirectX::XMFLOAT4X4 const basis = GetWorldMatrix();
  DirectX::XMMATRIX mat = DirectX::XMLoadFloat4x4(&basis);
  DirectX::XMVECTOR const scale = DirectX::XMVectorReplicate(m_scale);
  mat.r[0] = DirectX::XMVectorMultiply(mat.r[0], scale);
  mat.r[1] = DirectX::XMVectorMultiply(mat.r[1], scale);
  mat.r[2] = DirectX::XMVectorMultiply(mat.r[2], scale);

  DirectX::XMFLOAT4X4 result;
  DirectX::XMStoreFloat4x4(&result, mat);
  return result;
}


void StaticShape::SetDetail(int _detail)
{
  m_pos.y = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);

  if (m_shape)
  {
    DirectX::XMFLOAT4X4 mat = GetScaledWorldMatrix();

    m_centrePos = m_shape->CalculateCentre(mat);
    m_radius = m_shape->CalculateRadius(mat, m_centrePos);
  }
}


void StaticShape::SetShapeName(std::string_view _shapeName)
{
  m_shapeName = _shapeName;

  if (m_shapeName != "none")
  {
    SetShape(g_resource->GetShape(m_shapeName.c_str()));

    DirectX::XMFLOAT4X4 mat = GetScaledWorldMatrix();

    m_centrePos = m_shape->CalculateCentre(mat);
    m_radius = m_shape->CalculateRadius(mat, m_centrePos);
  }
}


bool StaticShape::DoesRayHit(DirectX::XMFLOAT3 const& _rayStart, DirectX::XMFLOAT3 const& _rayDir, float _rayLen, DirectX::XMFLOAT3* _pos,
                             DirectX::XMFLOAT3* norm)
{
  if (m_shape)
  {
    RayPackage ray(_rayStart, _rayDir, _rayLen);
    DirectX::XMFLOAT4X4 transform = GetScaledWorldMatrix();
    return m_shape->RayHit(&ray, transform, true);
  }
  else
  {
    return RaySphereIntersection(_rayStart, _rayDir, m_pos, m_radius, _rayLen);
  }
}

bool StaticShape::DoesSphereHit(DirectX::XMFLOAT3 const& _pos, float _radius)
{
  if (m_shape)
  {
    SpherePackage sphere(_pos, _radius);
    DirectX::XMFLOAT4X4 transform = GetScaledWorldMatrix();
    return m_shape->SphereHit(&sphere, transform);
  }
  else
  {
    float distance =
      DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&_pos), DirectX::XMLoadFloat3(&m_pos))));
    return (distance <= _radius + m_radius);
  }
}

bool StaticShape::DoesShapeHit(Shape* _shape, DirectX::XMFLOAT4X4 _theTransform)
{
  if (m_shape)
  {
    DirectX::XMFLOAT4X4 ourTransform = GetScaledWorldMatrix();

    // return m_shape->ShapeHit( _shape, _theTransform, ourTransform, true );
    return _shape->ShapeHit(m_shape, ourTransform, _theTransform, true);
  }
  else
  {
    SpherePackage package(m_pos, m_radius);
    return _shape->SphereHit(&package, _theTransform, true);
  }
}


void StaticShape::Render(float _predictionTime)
{
  if (m_shape)
  {
    DirectX::XMFLOAT4X4 mat = GetScaledWorldMatrix();

    glEnable(GL_NORMALIZE);
    m_shape->Render(_predictionTime, mat);
    glDisable(GL_NORMALIZE);
  }
  else
  {
    RenderSphere(m_pos, 40.0f);
  }
}


bool StaticShape::Advance() { return Building::Advance(); }


void StaticShape::Read(TextReader* _in, bool _dynamic)
{
  Building::Read(_in, _dynamic);

  m_scale = atof(_in->GetNextToken());

  SetShapeName(_in->GetNextToken());
}


void StaticShape::Write(FileWriter* _out)
{
  Building::Write(_out);

  _out->printf("%6.2f  ", m_scale);
  _out->printf("%s  ", m_shapeName.c_str());
}
