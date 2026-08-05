#pragma once

#ifdef DEBUG_RENDER_ENABLED

#include "RgbColour.h"
#include "NeuronMath.h"

void RenderSquare2d(float x, float y, float size, RGBAColour const& _col = RGBAColour(255, 255, 255));

void RenderCube(DirectX::XMFLOAT3 const& _centre, float _sizeX, float _sizeY, float _sizeZ, RGBAColour const& _col = RGBAColour(255, 255, 255));
void RenderSphereRings(DirectX::XMFLOAT3 const& _centre, float _radius, RGBAColour const& _col = RGBAColour(255, 255, 255));
void RenderSphere(DirectX::XMFLOAT3 const& _centre, float _radius, RGBAColour const& _col = RGBAColour(255, 255, 255));

void RenderVerticalCylinder(DirectX::XMFLOAT3 const& _centreBase, DirectX::XMFLOAT3 const& _verticalAxis, float _height, float _radius,
                            RGBAColour const& _col = RGBAColour(255, 255, 255));

void RenderArrow(DirectX::XMFLOAT3 const& start, DirectX::XMFLOAT3 const& end, float width, RGBAColour const& _col = RGBAColour(255, 255, 255));
// Takes the label itself, not a format for it. strings-modernised T18: this
// was `(point, char const* text, ...)` over a char[512] and a vsprintf, and
// its one caller has always passed a runtime label with no arguments. Making
// it a std::format entry point would have been the wrong shape twice over --
// nothing formats, and a label containing a brace would then throw where it
// used to print.
void RenderPointMarker(DirectX::XMFLOAT3 const& point, char const* _text);

void PrintMatrix(const char* _name, GLenum _whichMatrix);
void PrintMatrices(const char* _title);

#endif // DEBUG_RENDER_ENABLED
