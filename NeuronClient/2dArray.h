#ifndef INCLUDED_2D_ARRAY_H
#define INCLUDED_2D_ARRAY_H


// ****************************************************************************
// Class Array2D
//
// Provides a mechanism to store a fixed number of elements arranged as a 2D
// array. Get and Set methods assume that values for X and Y are within bounds.
// ****************************************************************************

template <class T>
class Array2D
{
protected:
	T				*m_data;
	T				m_outsideValue;
	unsigned short	m_numColumns;
	unsigned short	m_numRows;

public:
	Array2D();
	Array2D(unsigned short _numColumns, unsigned short _numRows, T _outsideValue);
	~Array2D();

	// Only need to call Initialise if you used the default constructor
	void					Initialise		(unsigned short _numCols, unsigned short _numRows, T _outsideValue);

	inline T const			&GetData		(unsigned short _x, unsigned short _y) const;
	inline T				*GetPointer		(unsigned short _x, unsigned short _y);
	inline T const			*GetConstPointer(unsigned short _x, unsigned short _y) const;

	inline void				PutData			(unsigned short _x, unsigned short _y, T const &_value);
	inline void				AddToData		(unsigned short _x, unsigned short _y, T const &_value);

	inline unsigned short	GetNumRows		() const;
	inline unsigned short	GetNumColumns	() const;

	void					SetAll			(T const &_value);
};


#include "2dArray.h"
#include "DebugUtils.h"

template <class T>
Array2D<T>::Array2D()
	: m_numColumns(0),
	m_numRows(0),
	m_data(NULL)
{}


template <class T>
Array2D<T>::Array2D(unsigned short _numColumns, unsigned short _numRows, T _outsideValue)
	: m_numColumns(_numColumns),
	m_numRows(_numRows),
	m_outsideValue(_outsideValue)
{
	m_data = new T[_numColumns * _numRows];
}


template <class T>
Array2D<T>::~Array2D()
{
	delete[] m_data;
	m_data = NULL;
}


template <class T>
void Array2D<T>::Initialise(unsigned short _numColumns, unsigned short _numRows, T _outsideValue)
{
	DarwiniaDebugAssert(m_numColumns == 0 && m_numRows == 0);
	m_numColumns = _numColumns;
	m_numRows = _numRows;
	m_outsideValue = _outsideValue;
	m_data = new T[_numColumns * _numRows];
}


template <class T>
inline T const& Array2D<T>::GetData(unsigned short _x, unsigned short _y) const
{
	if (_x >= m_numColumns || _y >= m_numRows) return m_outsideValue;
	return m_data[_y * m_numColumns + _x];
}


template <class T>
inline T* Array2D<T>::GetPointer(unsigned short _x, unsigned short _y)
{
	if (_x >= m_numColumns || _y >= m_numRows) return &m_outsideValue;
	return &(m_data[_y * m_numColumns + _x]);
}


template <class T>
inline T const* Array2D<T>::GetConstPointer(unsigned short _x, unsigned short _y) const
{
	if (_x >= m_numColumns || _y >= m_numRows) return &m_outsideValue;
	return &(m_data[_y * m_numColumns + _x]);
}


template <class T>
inline void Array2D<T>::PutData(unsigned short _x, unsigned short _y, T const& _value)
{
	if (_x >= m_numColumns || _y >= m_numRows) return;
	m_data[_y * m_numColumns + _x] = _value;
}


template <class T>
inline void Array2D<T>::AddToData(unsigned short _x, unsigned short _y, T const& _value)
{
	if (_x >= m_numColumns || _y >= m_numRows) return;
	m_data[_y * m_numColumns + _x] += _value;
}


template <class T>
inline unsigned short Array2D<T>::GetNumRows() const
{
	return m_numRows;
}


template <class T>
inline unsigned short Array2D<T>::GetNumColumns() const
{
	return m_numColumns;
}


template <class T>
void Array2D<T>::SetAll(T const& _value)
{
	for (unsigned short z = 0; z < m_numRows; z++)
	{
		for (unsigned short x = 0; x < m_numColumns; x++)
		{
			PutData(x, z, _value);
		}
	}
}


#endif
