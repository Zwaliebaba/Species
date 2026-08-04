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
void RenderPointMarker(DirectX::XMFLOAT3 const& point, char const* text, ...);

void PrintMatrix(const char* _name, GLenum _whichMatrix);
void PrintMatrices(const char* _title);

#endif // DEBUG_RENDER_ENABLED
