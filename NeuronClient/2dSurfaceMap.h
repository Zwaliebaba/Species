#pragma once


#include "2dArray.h"
#include "NeuronMath.h"


// ****************************************************************************
// Class SurfaceMap2D
// Builds on Array2D to provide continuous interpolating 2D map, rather than a
// discrete sampled one. In addition this class performs a coordinate space
// transform so that data values can be looked up using world space co-ords
// rather than direct indices into the 2D array.
// ****************************************************************************


namespace Neuron
{
  template <class T> class SurfaceMap2D : public Array2D<T>
  {
    public:
      // These variables are used when converting from real co-ords into
      // array indices
      float m_x0;
      float m_y0;
      float m_cellSizeX;
      float m_cellSizeY;
      float m_invCellSizeX;
      float m_invCellSizeY;

    public:
      SurfaceMap2D();
      SurfaceMap2D(float _width, float _height, float _x0, float _y0, float _cellSizeX, float _cellSizeY, T _outsideValue);
      ~SurfaceMap2D();

      void Initialise(float _width, float _height, float _x0, float _y0, float _cellSizeX, float _cellSizeY, T _outsideValue);

      T GetValue(float _x, float _y) const;
      T const& GetValueNearest(float _x, float _y) const; // Like GetValue but without interpolation
      T* GetPointerNearest(float _x, float _y) const;
      T GetHighestValue() const;

      inline int GetMapIndexX(float _realX) const;
      inline int GetMapIndexY(float _realY) const;
      inline float GetRealX(int _mapIndexX) const;
      inline float GetRealY(int _mapIndexY) const;
  };


#include "2dSurfaceMap.h"


template <class T>
SurfaceMap2D<T>::SurfaceMap2D()
	: Array2D<T>(),
	m_x0(0.0f),
	m_y0(0.0f),
	m_cellSizeX(0.0f),
	m_cellSizeY(0.0f),
	m_invCellSizeX(0.0f),
	m_invCellSizeY(0.0f)
{}


template <class T>
SurfaceMap2D<T>::SurfaceMap2D(float _width, float _height,
	float _x0, float _y0,
	float _cellSizeX, float _cellSizeY,
	T _outsideValue)
	: Array2D<T>(ceilf(_width / _cellSizeX), ceilf(_height / _cellSizeY), _outsideValue),
	m_x0(_x0),
	m_y0(_y0),
	m_cellSizeX(_cellSizeX),
	m_cellSizeY(_cellSizeY),
	m_invCellSizeX(1.0f / _cellSizeX),
	m_invCellSizeY(1.0f / _cellSizeY)
{}


template <class T>
SurfaceMap2D<T>::~SurfaceMap2D()
{
	m_x0 = 0.0f;
	m_y0 = 0.0f;
	m_cellSizeX = 0.0f;
	m_cellSizeY = 0.0f;
	m_invCellSizeX = 0.0f;
	m_invCellSizeY = 0.0f;
}


template <class T>
void SurfaceMap2D<T>::Initialise(float _width, float _height,
	float _x0, float _y0,
	float _cellSizeX, float _cellSizeY,
	T _outsideValue)
{
	Array2D<T>::Initialise(ceilf(_width / _cellSizeX), ceilf(_height / _cellSizeY), _outsideValue);
	m_x0 = _x0;
	m_y0 = _y0;
	m_cellSizeX = _cellSizeX;
	m_cellSizeY = _cellSizeY;
	m_invCellSizeX = 1.0f / _cellSizeX;
	m_invCellSizeY = 1.0f / _cellSizeY;
}


template <class T>
T SurfaceMap2D<T>::GetValue(float _x, float _y) const
{
	_x -= m_x0;
	_y -= m_y0;

	float fractionalX = _x * m_invCellSizeX;
	float fractionalY = _y * m_invCellSizeY;

	unsigned short x1 = fractionalX;
	unsigned short y1 = fractionalY;
	unsigned short x2 = x1 + 1;
	unsigned short y2 = y1 + 1;

	fractionalX = fractionalX - floorf(fractionalX);
	fractionalY = fractionalY - floorf(fractionalY);

	if (x1 >= this->m_numColumns) x1 = 0;
	if (x2 >= this->m_numColumns) x2 = 0;
	if (y1 >= this->m_numRows) y1 = 0;
	if (y2 >= this->m_numRows) y2 = 0;

	T value11 = this->GetData(x1, y1);
	T value12 = this->GetData(x1, y2);
	T value21 = this->GetData(x2, y1);
	T value22 = this->GetData(x2, y2);

	float weight11 = (1.0f - fractionalX) * (1.0f - fractionalY);
	float weight12 = (1.0f - fractionalX) * (fractionalY);
	float weight21 = (fractionalX) * (1.0f - fractionalY);
	float weight22 = (fractionalX) * (fractionalY);

	T returnVal = value11 * weight11 +
		value12 * weight12 +
		value21 * weight21 +
		value22 * weight22;

	return returnVal;
}


// SurfaceMap2D<XMFLOAT3>, which exists for Landscape's normal map.
//
// The generic GetValue above interpolates with `T * float` and `T + T`, and
// XMFLOAT3 has neither -- that is the point of directxmath-migration, which
// keeps storage inert and does the arithmetic through XMVECTOR at the call
// site. So the ONE member that needs arithmetic is specialised here and the
// rest of the template is used unchanged. GetHighestValue would need `>` and
// is never instantiated for this element type.
//
// FOUND BY T18, WHICH THE PLAN DID NOT ANTICIPATE. No task owned this header,
// so Landscape::m_normalMap had no way to stop being a SurfaceMap2D<Vector3> --
// and T25 deletes Vector3 out from under it. See that task's notes.
//
// The index arithmetic below is copied from the generic version rather than
// tidied, including the unsigned short truncation and the wrap-to-zero clamps:
// changing any of it changes every landscape normal in the game. The four
// weighted terms are summed left to right in the same order, with separate
// multiplies and adds, because a fused multiply-add would round differently.
template <> inline DirectX::XMFLOAT3 SurfaceMap2D<DirectX::XMFLOAT3>::GetValue(float _x, float _y) const
{
  _x -= m_x0;
  _y -= m_y0;

  float fractionalX = _x * m_invCellSizeX;
  float fractionalY = _y * m_invCellSizeY;

  unsigned short x1 = fractionalX;
  unsigned short y1 = fractionalY;
  unsigned short x2 = x1 + 1;
  unsigned short y2 = y1 + 1;

  fractionalX = fractionalX - floorf(fractionalX);
  fractionalY = fractionalY - floorf(fractionalY);

  if (x1 >= this->m_numColumns)
    x1 = 0;
  if (x2 >= this->m_numColumns)
    x2 = 0;
  if (y1 >= this->m_numRows)
    y1 = 0;
  if (y2 >= this->m_numRows)
    y2 = 0;

  // Named apart from the generic version's value11..value22 above: those are
  // declared with the bare template parameter T, and a checker that sees an
  // XMFLOAT3 of the same name in this file cannot tell the two apart.
  DirectX::XMFLOAT3 const sample11 = this->GetData(x1, y1);
  DirectX::XMFLOAT3 const sample12 = this->GetData(x1, y2);
  DirectX::XMFLOAT3 const sample21 = this->GetData(x2, y1);
  DirectX::XMFLOAT3 const sample22 = this->GetData(x2, y2);

  float weight11 = (1.0f - fractionalX) * (1.0f - fractionalY);
  float weight12 = (1.0f - fractionalX) * (fractionalY);
  float weight21 = (fractionalX) * (1.0f - fractionalY);
  float weight22 = (fractionalX) * (fractionalY);

  DirectX::XMVECTOR sum = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&sample11), weight11);
  sum = DirectX::XMVectorAdd(sum, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&sample12), weight12));
  sum = DirectX::XMVectorAdd(sum, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&sample21), weight21));
  sum = DirectX::XMVectorAdd(sum, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&sample22), weight22));

  DirectX::XMFLOAT3 returnVal;
  DirectX::XMStoreFloat3(&returnVal, sum);
  return returnVal;
}


template <class T>
T const& SurfaceMap2D<T>::GetValueNearest(float _x, float _y) const
{
	return this->GetData(floorf(_x * m_invCellSizeX), floorf(_y * m_invCellSizeY));
}


template <class T>
T* SurfaceMap2D<T>::GetPointerNearest(float _x, float _y) const
{
	return this->GetPointer(floorf(_x * m_invCellSizeX), floorf(_y * m_invCellSizeY));
}


template <class T>
T SurfaceMap2D<T>::GetHighestValue() const
{
	T highest = this->GetData(0, 0);
	for (unsigned short y = 0; y < this->m_numRows; ++y)
	{
		for (unsigned short x = 0; x < this->m_numRows; ++x)
		{
			T val = this->GetData(x, y);
			if (val > highest)
			{
				highest = val;
			}
		}
	}

	return highest;
}


template <class T>
inline int SurfaceMap2D<T>::GetMapIndexX(float _realX) const
{
	return (_realX - m_x0) * m_invCellSizeX;
}


template <class T>
inline int SurfaceMap2D<T>::GetMapIndexY(float _realY) const
{
	return (_realY - m_y0) * m_invCellSizeY;
}


template <class T>
inline float SurfaceMap2D<T>::GetRealX(int _mapIndexX) const
{
	return _mapIndexX * m_cellSizeX + m_x0;
}


template <class T>
inline float SurfaceMap2D<T>::GetRealY(int _mapIndexY) const
{
	return _mapIndexY * m_cellSizeY + m_y0;
}
} // namespace Neuron
