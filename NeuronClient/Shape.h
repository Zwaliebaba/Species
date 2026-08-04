#pragma once

#include <memory>
#include <vector>

#include <stdio.h>

#include "NeuronMath.h"
#include "RgbColour.h"


class TextReader;
class ShapeFragment;
class Shape;


// ****************
// Class RayPackage
// ****************

class RayPackage
{
  public:
    DirectX::XMFLOAT3 m_rayStart;
    DirectX::XMFLOAT3 m_rayEnd;
    DirectX::XMFLOAT3 m_rayDir;
    float m_rayLen;

    RayPackage(DirectX::XMFLOAT3 const& _start, DirectX::XMFLOAT3 const& _dir, float _length = 1e10)
      : m_rayStart(_start),
        m_rayDir(_dir)
    {
      m_rayLen = _length;
      DirectX::XMStoreFloat3(&m_rayEnd, DirectX::XMVectorMultiplyAdd(DirectX::XMLoadFloat3(&m_rayDir), DirectX::XMVectorReplicate(_length),
                                                                     DirectX::XMLoadFloat3(&m_rayStart)));
    }
};


// *******************
// Class SpherePackage
// *******************

class SpherePackage
{
  public:
    DirectX::XMFLOAT3 m_pos;
    float m_radius;

    SpherePackage(DirectX::XMFLOAT3 const& _pos, float _radius)
      : m_pos(_pos),
        m_radius(_radius)
    {
    }
};


// *****************
// Class ShapeMarker
// *****************

class ShapeMarker
{
  public:
    DirectX::XMFLOAT4X4 m_transform;
    char* m_name;
    char* m_parentName;
    int m_depth; // Number of levels in the shape fragment tree from root to self
    // Observers into the fragment tree, which owns them — the commented-out
    // "should we?" delete loop that used to sit in the destructor is answered
    // here: no. It was a `new ShapeFragment*[m_depth]` released with plain
    // `delete`, which is undefined behaviour; a vector removes both questions.
    std::vector<ShapeFragment*> m_parents;

    ShapeMarker(char const* _name, char* _parentName, int _depth, DirectX::XMFLOAT4X4 const& _transform);
    ShapeMarker(TextReader* _in, char const* _name);
    ~ShapeMarker();

    DirectX::XMFLOAT4X4 GetWorldMatrix(DirectX::XMFLOAT4X4 const& _rootTransform);

    void WriteToFile(FILE* _out) const;
};


// ******************
// Class VertexPosCol
// ******************

class VertexPosCol
{
  public:
    unsigned short m_posId;
    unsigned short m_colId;
};


// *******************
// Class ShapeTriangle
// *******************

class ShapeTriangle
{
  public:
    unsigned short v1, v2, v3; // Vertex indices (into ShapeFragment::m_vertices)
};


// *******************
// Class ShapeFragment
// *******************

class ShapeFragment
{
    friend class ShapeExporter;

  protected:
    char* m_displayListName;

    void ParsePositionBlock(TextReader* in, unsigned int numPositions);
    void ParseNormalBlock(TextReader* in, unsigned int numNorms);
    void ParseColourBlock(TextReader* in, unsigned int numColours);
    void ParseVertexBlock(TextReader* in, unsigned int numVerts);
    void ParseStripBlock(TextReader* in);
    void ParseAllStripBlocks(TextReader* in, unsigned int numStrips);
    void ParseTriangleBlock(TextReader* in, unsigned int numTriangles);

    void GenerateNormals();

  public:
    unsigned int m_numPositions;
    DirectX::XMFLOAT3* m_positions;
    DirectX::XMFLOAT3* m_positionsInWS; // Temp storage space used to cache World Space versions of all the vertex positions in the hit check routines
    unsigned int m_numNormals;
    DirectX::XMFLOAT3* m_normals;
    unsigned int m_numColours;
    RGBAColour* m_colours;
    unsigned int m_numVertices; // Each element contains an index into m_positions and an index into m_colours
    VertexPosCol* m_vertices;
    unsigned int m_numTriangles;
    unsigned int m_maxTriangles;
    ShapeTriangle* m_triangles;

    char* m_name;
    char* m_parentName;
    DirectX::XMFLOAT4X4 m_transform;
    DirectX::XMFLOAT3 m_angVel;
    DirectX::XMFLOAT3 m_vel;

    bool m_useCylinder; // If true then use cylinder hit check instead of sphere
    DirectX::XMFLOAT3 m_centre;
    float m_radius;
    float m_mostPositiveY;
    float m_mostNegativeY;

    // The fragment tree owns its children. ShapeMarker::m_parents points back
    // up into it and does not.
    std::vector<std::unique_ptr<ShapeFragment>> m_childFragments;
    std::vector<std::unique_ptr<ShapeMarker>> m_childMarkers;

    ShapeFragment(TextReader* _in, char const* _name);
    ShapeFragment(char const* _name, char const* _parentName);
    ~ShapeFragment();

    void BuildDisplayList();

    void RegisterPositions(DirectX::XMFLOAT3* positions, unsigned int numPositions);
    void RegisterNormals(DirectX::XMFLOAT3* norms, unsigned int numNorms);
    void RegisterColours(RGBAColour* colours, unsigned int numColours);
    void RegisterVertices(VertexPosCol* verts, unsigned int numVerts);
    void RegisterTriangles(ShapeTriangle* tris, unsigned int numTris);

    void WriteToFile(FILE* _out) const;

    void Render(float _predictionTime); // Uses display list
    void RenderSlow();                  // Doesn't use display list
    void RenderHitCheck(DirectX::XMFLOAT4X4 const& _transform);
    void RenderMarkers(DirectX::XMFLOAT4X4 const& _transform);

    ShapeFragment* LookupFragment(char const* _name); // Recurses into child fragments
    ShapeMarker* LookupMarker(char const* _name);     // Recurses into child fragments

    void CalculateCentre(DirectX::XMFLOAT4X4 const& _transform, DirectX::XMFLOAT3& _centre, int& _numFragments);   // Recursive
    void CalculateRadius(DirectX::XMFLOAT4X4 const& _transform, DirectX::XMFLOAT3 const& _centre, float& _radius); // Recursive

    bool RayHit(RayPackage* _package, DirectX::XMFLOAT4X4 const& _transform, bool _accurate = false);
    bool SphereHit(SpherePackage* _package, DirectX::XMFLOAT4X4 const& _transform, bool _accurate = false);
    bool ShapeHit(Shape* _shape, DirectX::XMFLOAT4X4 const& _theTransform, // Transform of _shape
                  DirectX::XMFLOAT4X4 const& _ourTransform,                // Transform of this
                  bool _accurate = false);
};


// ***********
// Class Shape
// ***********

class Shape
{
  protected:
    void Load(TextReader* _in);
    bool m_animating; // If false then whole shape in one display list otherwise one display list per fragment
    char* m_displayListName;

  public:
    std::unique_ptr<ShapeFragment> m_rootFragment;
    char* m_name;

    Shape();
    Shape(char const* filename, bool _animating);
    Shape(TextReader* in, bool _animating);
    ~Shape();

    void BuildDisplayList();

    void WriteToFile(FILE* _out) const;

    void Render(float _predictionTime, DirectX::XMFLOAT4X4 const& _transform);
    void RenderHitCheck(DirectX::XMFLOAT4X4 const& _transform);
    void RenderMarkers(DirectX::XMFLOAT4X4 const& _transform);

    bool RayHit(RayPackage* _package, DirectX::XMFLOAT4X4 const& _transform, bool _accurate = false);
    bool SphereHit(SpherePackage* _package, DirectX::XMFLOAT4X4 const& _transform, bool _accurate = false);
    bool ShapeHit(Shape* _shape, DirectX::XMFLOAT4X4 const& _theTransform, // Transform of _shape
                  DirectX::XMFLOAT4X4 const& _ourTransform,                // Transform of this
                  bool _accurate = false);

    DirectX::XMFLOAT3 CalculateCentre(DirectX::XMFLOAT4X4 const& _transform);
    float CalculateRadius(DirectX::XMFLOAT4X4 const& _transform, DirectX::XMFLOAT3 const& _centre);
};
