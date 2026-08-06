#pragma once

#include "RendererAccess.h"
#include "WorldPointers.h"

#define PIXEL_EFFECT_GRID_RES 16

namespace Neuron
{
  class Shape;
  class ShapeFragment;
} // namespace Neuron


namespace Species
{
  class Renderer : public RendererAccess
  {
    public:
      int m_fps;
      bool m_displayFPS;

      int Fps() const override { return m_fps; }
      bool DisplayFps() const override { return m_displayFPS; }
      void SetDisplayFps(bool _display) override { m_displayFPS = _display; }
      bool m_renderDebug;

      float m_renderDarwinLogo;

    private:
      float m_nearPlane;
      float m_farPlane;
      int m_screenW;
      int m_screenH;
      int m_tileIndex; // Used when rendering a poster

      double m_totalMatrix[16]; // Modelview matrix * Projection matrix

      float m_fadedness; // 1.0 means black screen. 0.0 means not fade out at all.
      float m_fadeRate;  // +ve means fading out, -ve means fading in
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

      void Initialise() override;
      void Restart();

      void Render();
      void FPSMeterAdvance();

      void BuildOpenGlState() override;

      float GetNearPlane() const override;
      float GetFarPlane() const override;
      void SetNearAndFar(float _nearPlane, float _farPlane);

      // Disabled: the body returns immediately. Kept because those eighty lines
      // are the only written statement of the renderer's GL-state contract, and
      // deleting them would delete that. Nothing calls it.
      void CheckOpenGLState() const;
      void SetOpenGLState() const;

      void SetObjectLighting() const override;
      void UnsetObjectLighting() const override;

      int ScreenW() const override;
      int ScreenH() const override;

      void SetupProjMatrixFor3D() const;
      void SetupMatricesFor3D() const override;
      void SetupMatricesFor2D() const override;

      void UpdateTotalMatrix();
      void Get2DScreenPos(DirectX::XMFLOAT3 const& _in, DirectX::XMFLOAT3* _out);
      const double* GetTotalMatrix();

      void RasteriseSphere(DirectX::XMFLOAT3 const& _pos, float _radius) override;
      void MarkUsedCells(ShapeFragment const* _frag, DirectX::XMFLOAT4X4 const& _transform) override;
      void MarkUsedCells(Shape const* _shape, DirectX::XMFLOAT4X4 const& _transform) override;

      bool IsFadeComplete() const override;
      void StartFadeOut() override;
      void StartFadeIn(float _delay);
  };

  // g_renderer is a RendererAccess* so the layers below Species need only the
  // interface. Species owns the concrete type and drives the frame, so it
  // reaches the rest of the API — Render, the fade and OpenGL state, the
  // projection setup — through here. The cast is safe because App is the only
  // thing that ever assigns g_renderer, and it assigns a Renderer.
  inline Renderer* TheRenderer() { return static_cast<Renderer*>(g_renderer); }
} // namespace Species
