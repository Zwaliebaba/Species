#pragma once

#include "NeuronMath.h"


// What the layers below Species ask the renderer to do.
//
// Renderer itself lives in Species, so entity and window code reaching it had
// to include Renderer.h upward — 61 allowlist entries, the third largest
// cluster. This is the surface those files actually use, taken from the T1
// catalogue: lighting and culling for entity render bodies, screen size for
// windows, and a few frame-level calls.
//
// The seam is the pointer's TYPE rather than a second global: g_renderer in
// WorldPointers.h is a RendererAccess*, so dereferencing it needs only this
// header. Code that constructs a Renderer, or reaches a member not listed
// here, still includes Renderer.h and is still recorded in the allowlist.
// See tasks/layering-inversion.yaml T9.


namespace Neuron
{
  class Shape;
  class ShapeFragment;

  class RendererAccess
  {
    public:
      virtual ~RendererAccess() = default;

      virtual void Initialise() = 0;
      virtual void BuildOpenGlState() = 0;

      virtual float GetNearPlane() const = 0;
      virtual float GetFarPlane() const = 0;

      virtual void SetObjectLighting() const = 0;
      virtual void UnsetObjectLighting() const = 0;

      virtual int ScreenW() const = 0;
      virtual int ScreenH() const = 0;

      virtual void SetupMatricesFor3D() const = 0;
      virtual void SetupMatricesFor2D() const = 0;

      virtual void RasteriseSphere(DirectX::XMFLOAT3 const& _pos, float _radius) = 0;
      virtual void MarkUsedCells(ShapeFragment const* _frag, DirectX::XMFLOAT4X4 const& _transform) = 0;
      virtual void MarkUsedCells(Shape const* _shape, DirectX::XMFLOAT4X4 const& _transform) = 0;

      virtual void StartFadeOut() = 0;

      // Read by the world code that waits for a fade before switching level.
      virtual bool IsFadeComplete() const = 0;

      // Read by the debug overlay in GameLogic; owned by Renderer.
      virtual int Fps() const = 0;
      virtual bool DisplayFps() const = 0;
      virtual void SetDisplayFps(bool _display) = 0;
  };
} // namespace Neuron
