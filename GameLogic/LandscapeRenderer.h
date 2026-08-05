#pragma once


#include "2dSurfaceMap.h"
#include "RgbColour.h"
#include "TextureUv.h"
#include "NeuronMath.h"


class BitmapRGBA;


class LandVertex
{
  public:
    DirectX::XMFLOAT3 m_pos{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 m_norm{0.0f, 0.0f, 0.0f};
    RGBAColour m_col;
    TextureUV m_uv;
};


//*****************************************************************************
// Class LandTriangleStrip
//*****************************************************************************

class LandTriangleStrip
{
  public:
    int m_firstVertIndex;
    int m_numVerts;

    LandTriangleStrip()
      : m_firstVertIndex(-1),
        m_numVerts(-2)
    {
    }
};


//*****************************************************************************
// Class LandscapeRenderer
//*****************************************************************************

struct IDirect3DVertexBuffer9;

class LandscapeRenderer
{
  protected:
    enum
    {
      RenderModeVertexArray,
      RenderModeDisplayList,
      RenderModeVertexBufferObject,
      RenderModeVertexBufferDirect3D
    };

    BitmapRGBA* m_landscapeColour;
    float m_highest;
    int m_renderMode;

    std::vector<LandVertex> m_verts;

    unsigned int m_vertexBuffer;

    std::vector<LandTriangleStrip*> m_strips;

    void BuildVertArrayAndTriStrip(SurfaceMap2D<float>* _heightMap);
    void BuildNormArray();
    void BuildUVArray(SurfaceMap2D<float>* _heightMap);
    void GetLandscapeColour(float _height, float _gradient, unsigned int _x, unsigned int _y, RGBAColour* _colour);
    void BuildColourArray();

  public:
    static const unsigned int m_posOffset;
    static const unsigned int m_normOffset;
    static const unsigned int m_colOffset;
    static const unsigned int m_uvOffset;

  public:
    int m_numTriangles;

    LandscapeRenderer(SurfaceMap2D<float>* _heightMap);
    ~LandscapeRenderer();

    void BuildOpenGlState(SurfaceMap2D<float>* _heightMap);

    void Initialise();

    void RenderMainSlow();
    void RenderOverlaySlow();
    void Render();
};
