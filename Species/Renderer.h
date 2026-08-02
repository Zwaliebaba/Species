#pragma once

#define CHECK_OPENGL_STATE()

#define PIXEL_EFFECT_GRID_RES	16

class Shape;
class ShapeFragment;
class Vector3;
class Matrix34;

class Renderer
{
  public:
    int m_fps;
    bool m_displayFPS;
    bool m_renderDebug;
    bool m_displayInputMode;

    float m_renderDarwinLogo;

  private:
    float m_nearPlane;
    float m_farPlane;
    int m_screenW;
    int m_screenH;
    int m_tileIndex; // Used when rendering a poster

    double m_totalMatrix[16]; // Modelview matrix * Projection matrix

    float m_fadedness; // 1.0 means black screen. 0.0 means not fade out at all.
    float m_fadeRate; // +ve means fading out, -ve means fading in
    float m_fadeDelay; // Amount of time left to wait before starting fade

    unsigned int m_pixelEffectTexId;
    float m_pixelEffectGrid[PIXEL_EFFECT_GRID_RES][PIXEL_EFFECT_GRID_RES]; // -1.0 means cell not used
    int m_pixelSize;

    int GetGLStateInt(int pname) const;
    float GetGLStateFloat(int pname) const;

    void RenderFlatTexture();
    void RenderLogo();
    void RenderHelp();

    void PaintPixels();
    void PreRenderPixelEffect();
    void ApplyPixelEffect();

    void RenderFadeOut();
    void RenderFrame(bool withFlip = true);
    void RenderPaused();

  public:
    Renderer();

    void Initialise();
    void Restart();

    void Render();
    void FPSMeterAdvance();

    void BuildOpenGlState();

    float GetNearPlane() const;
    float GetFarPlane() const;
    void SetNearAndFar(float _nearPlane, float _farPlane);

    void CheckOpenGLState() const;
    void SetOpenGLState() const;

    void SetObjectLighting() const;
    void UnsetObjectLighting() const;

    int ScreenW() const;
    int ScreenH() const;

    void SetupProjMatrixFor3D() const;
    void SetupMatricesFor3D() const;
    void SetupMatricesFor2D() const;

    void UpdateTotalMatrix();
    void Get2DScreenPos(const Vector3& _in, Vector3* _out);
    const double* GetTotalMatrix();

    void RasteriseSphere(const Vector3& _pos, float _radius);
    void MarkUsedCells(const ShapeFragment* _frag, const Matrix34& _transform);
    void MarkUsedCells(const Shape* _shape, const Matrix34& _transform);

    bool IsFadeComplete() const;
    void StartFadeOut();
    void StartFadeIn(float _delay);
};

