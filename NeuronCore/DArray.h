//===============================================================//
//                        D A R R A Y                            //
//                                                               //
//                   By Christopher Delay                        //
//                           V1.3                                //
//===============================================================//

#ifndef _included_darray_h
#define _included_darray_h


//=================================================================
// Dynamic array template class
// Use : A dynamically sized list of data
// Which can be indexed into - an entry's index never changes
// A record is kept of which elements are in use. Accesses to
// elements that are not in use will cause an assert.

template <class T>
class DArray
{
protected:
    int m_stepSize;								// -1 means each step doubles the array size
    int m_arraySize;

    T *array;
    char *shadow;								// 0=not used, 1=used

	void Grow			();

public:
    DArray();
    DArray( int newstepsize );
    ~DArray();

    void SetSize		( int newsize );
	void SetStepSize	( int newstepsize );
	void SetStepDouble	();						// Switches to array size doubling when growing, rather than incrementing by m_stepSize

	inline T GetData	( int index ) const;
	inline T *GetPointer( int index ) const;

    int  PutData		( const T &newdata );	// Returns index used
    inline void PutData	( const T &newdata, int index );

	void MarkUsed		( int index );
    void MarkNotUsed	( int index );

	int FindData		( const T &data ) const;// -1 means 'not found'

    int NumUsed			() const;				// Returns the number of used entries
    inline int Size		() const;				// Returns the total size of the array

    inline bool ValidIndex ( int index ) const;	// Returns true if the index contains used data

    void Empty			();						// Resets the array to empty
	void EmptyAndDelete ();						// Same as Empty() but deletes the elements that are pointed to as well

    inline T& operator [] (int index);
	inline const T& operator [] (int index) const;
};

//  ===================================================================

#include "pch.h"

// By Christopher Delay

#include <stdlib.h>

#include "Debug.h"
#include "DArray.h"


template <class T>
DArray<T>::DArray()
{
	m_stepSize = 1;
	m_arraySize = 0;
	array = nullptr;
	shadow = nullptr;
}


template <class T>
DArray<T>::DArray(int newstepsize)
{
	m_stepSize = newstepsize;
	m_arraySize = 0;
	array = nullptr;
	shadow = nullptr;
}


template <class T>
DArray<T>::~DArray()
{
	Empty();
}


template <class T>
void DArray<T>::SetSize(int newsize)
{
	if (newsize > m_arraySize)
	{
		int oldarraysize = m_arraySize;

		m_arraySize = newsize;
		T* temparray = new T[m_arraySize];
		char* tempshadow = new char[m_arraySize];

		int a;

		for (a = 0; a < oldarraysize; ++a)
		{
			temparray[a] = array[a];
			tempshadow[a] = shadow[a];
		}

		for (a = oldarraysize; a < m_arraySize; ++a)
		{
			tempshadow[a] = 0;
		}

		delete[] array;
		delete[] shadow;

		array = temparray;
		shadow = tempshadow;
	}
	else if (newsize < m_arraySize)
	{
		m_arraySize = newsize;
		T* temparray = new T[m_arraySize];
		char* tempshadow = new char[m_arraySize];

		for (int a = 0; a < m_arraySize; ++a)
		{
			temparray[a] = array[a];
			tempshadow[a] = shadow[a];
		}

		delete[] array;
		delete[] shadow;

		array = temparray;
		shadow = tempshadow;
	}
	else if (newsize == m_arraySize)
	{
		// Do nothing
	}
}


template <class T>
void DArray<T>::Grow()
{
	if (m_stepSize == -1)
	{
		// Double array size
		if (m_arraySize == 0)
		{
			SetSize(1);
		}
		else
		{
			SetSize(m_arraySize * 2);
		}
	}
	else
	{
		// Increase array size by fixed amount
		SetSize(m_arraySize + m_stepSize);
	}
}


template <class T>
void DArray<T>::SetStepSize(int newstepsize)
{
	m_stepSize = newstepsize;
}


template <class T>
void DArray<T>::SetStepDouble()
{
	m_stepSize = -1;
}


template <class T>
int DArray<T>::PutData(const T& newdata)
{
	int freeslot = -1;				 // Find a free space

	for (int a = 0; a < m_arraySize; ++a)
	{
		if (shadow[a] == 0)
		{
			freeslot = a;
			break;
		}
	}

	if (freeslot == -1)			// Must resize the array
	{
		freeslot = m_arraySize;
		Grow();
	}

	array[freeslot] = newdata;
	shadow[freeslot] = 1;

	return freeslot;
}


template <class T>
void DArray<T>::PutData(const T& newdata, int index)
{
	DEBUG_ASSERT(index < m_arraySize && index >= 0);

	array[index] = newdata;
	shadow[index] = 1;
}


template <class T>
void DArray<T>::Empty()
{
	delete[] array;
	delete[] shadow;

	array = nullptr;
	shadow = nullptr;

	m_arraySize = 0;
}


template <class T>
void DArray<T>::EmptyAndDelete()
{
	for (int i = 0; i < m_arraySize; ++i)
	{
		if (ValidIndex(i))
		{
			delete array[i];
		}
	}

	Empty();
}


template <class T>
T DArray<T>::GetData(int index) const
{
	DEBUG_ASSERT(index < m_arraySize && index >= 0);
	DEBUG_ASSERT(shadow[index] != 0);

	return array[index];
}


template <class T>
T* DArray<T>::GetPointer(int index) const
{
	DEBUG_ASSERT(index < m_arraySize && index >= 0);
	DEBUG_ASSERT(shadow[index] != 0);

	return &(array[index]);
}


template <class T>
inline T& DArray<T>::operator [] (int index)
{
	DEBUG_ASSERT(index < m_arraySize && index >= 0);
	DEBUG_ASSERT(shadow[index] != 0);

	return array[index];
}


template <class T>
inline const T& DArray<T>::operator [] (int _index) const
{
	DEBUG_ASSERT(_index < this->m_arraySize && _index >= 0);
	DEBUG_ASSERT(shadow[_index] != 0);

	return array[_index];
}


template <class T>
void DArray<T>::MarkUsed(int _index)
{
	DEBUG_ASSERT(_index < m_arraySize && _index >= 0);
	DEBUG_ASSERT(shadow[_index] == 0);

	shadow[_index] = 1;
}


template <class T>
void DArray<T>::MarkNotUsed(int index)
{
	DEBUG_ASSERT(index < m_arraySize && index >= 0);
	DEBUG_ASSERT(shadow[index] != 0);

	shadow[index] = 0;
}


template <class T>
int DArray<T>::NumUsed() const
{
	int count = 0;

	for (int a = 0; a < m_arraySize; ++a)
	{
		if (shadow[a] == 1)	++count;
	}

	return count;
}


template <class T>
int DArray<T>::Size() const
{
	return m_arraySize;
}


template <class T>
bool DArray<T>::ValidIndex(int index) const
{
	if (index >= m_arraySize || index < 0)
	{
		return false;
	}

	if (!shadow[index])
	{
		return false;
	}

	return true;
}


template <class T>
int DArray<T>::FindData(const T& newdata) const
{
	for (int a = 0; a < m_arraySize; ++a)
	{
		if (shadow[a])
		{
			if (array[a] == newdata)	return a;
		}
	}

	return -1;
}

#endif

