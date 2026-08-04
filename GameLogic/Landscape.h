#pragma once

#include "2dArray.h"
#include "2dSurfaceMap.h"

class BitmapRGBA;
class RGBAColour;
class LandscapeFlattenArea;
class Landscape;
class LandscapeDef;
class LandscapeRenderer;


// ****************************************************************************
// Class LandscapeTile
// ****************************************************************************

class LandscapeTile
{
  public:
    //          OUR DEFINITION DATA
    float m_fractalDimension;
    float m_heightScale;
    float m_desiredHeight;
    int m_generationMethod;
    int m_randomSeed;
    float m_lowlandSmoothingFactor;
    int m_posX; // In world space
    float m_posY;
    int m_posZ;
    float m_outsideHeight;
    int m_guideGridPower; // Log to the base 2 of the resolution of the guide grid
    int m_size;           // Size when copied into the main landscape,
                          // (obviously in world space)

    Array2D<unsigned char>* m_guideGrid;

    //          OUR GENERATED DATA
    SurfaceMap2D<float>* m_heightMap; // Width and height of this must be equal and must be a power of 2 + 1
    float m_compensatedHeightScale;

  protected:
    int GetPowerOfTwo(int x);
    float GenerateNoise(float _halfSize, float _height);
    void GenerateDiamondMidpoint(int _x, int _z, int _halfSize);
    void GenerateSquareMidpoint(int _x, int _z, int _halfSize);
    void GenerateMidpoints(int _x1, int _x2, int _z1, int _z2);

  public:
    LandscapeTile();
    ~LandscapeTile();

    void GuideGridSetPower(int _power);
    int GuideGridGetPower();
    char* GuideGridToString();
    void GuideGridFromString(char* _hex);

    void Generate(LandscapeDef* _def);
};

#define LIGHTMAP_SCALEFACTOR 1


// ****************************************************************************
// Class Landscape
// ****************************************************************************

class Landscape
{
    friend class LocationEditor;

  public:
    SurfaceMap2D<float>* m_heightMap;
    SurfaceMap2D<DirectX::XMFLOAT3>* m_normalMap;
    float m_outsideHeight;
    LandscapeRenderer* m_renderer;

  private:
    float m_worldSizeX; // Updated in GenerateHeightMap
    float m_worldSizeZ; // Updated in GenerateHeightMap

    void MergeTileIntoLandscape(LandscapeTile const* _tile);
    void GenerateHeightMap(LandscapeDef* _def);
    void GenerateNormals();

    void FlattenArea(LandscapeFlattenArea const* _area);

    void RenderHitNormals() const;

    // THE RAY AND SPHERE API STAYS LEGACY, and this is a plan gap rather than
    // an oversight. Every one of these hands its Vector3* out-parameter
    // straight to MathUtils -- RayTriIntersection and SegRayIntersection2D --
    // which still take Vector3 const& and Vector3*, because T7 and T8 rebuilt
    // their bodies natively while deliberately keeping the signatures so
    // callers compiled unchanged. The seam converts VALUES and REFERENCES, not
    // POINTERS, so these cannot move until MathUtils does, and no task in
    // directxmath-migration.yaml owns that. Seventeen call sites in ten files
    // pass &someVector3 to RayHit alone. See T18's notes.
    bool UnsafeRayHit(Vector3 const& _rayStart, Vector3 const& _rayEnd, Vector3* _result) const;

  public:
    Landscape();
    Landscape(float _cellSize, int universeSizeX, int universeSizeZ);
    ~Landscape();

    void BuildOpenGlState();

    void Init(LandscapeDef* _def, bool _justMakeTheHeightMap = false);
    void Empty();

    void Render();

    void DeleteTile(int tileId);

    float GetWorldSizeX() const;
    float GetWorldSizeZ() const;
    bool IsInLandscape(Vector3 const& _pos);

    bool RayHit(Vector3 const& _rayStart, Vector3 const& _rayDir, Vector3* _result) const;
    bool RayHitCell(int x0, int z0, Vector3 const& _rayStart, Vector3 const& _rayDir, Vector3* _result) const;
    float SphereHit(Vector3 const& _centre, float _radius) const;
};
