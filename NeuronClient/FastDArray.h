//===============================================================//
//                        F D A R R A Y                          //
//                                                               //
//                   By Christopher Delay                        //
//                           V1.3                                //
//===============================================================//

#ifndef _included_fdarray_h
#define _included_fdarray_h


#include "DArray.h"


//=================================================================
// Fast Dynamic array object
// Use : A dynamically sized list of data
// Which can be indexed into - an entry's index never changes
// Same as DArray, but has new advantages: Insert is MUCH faster

template <class T>
class FastDArray: public DArray <T>
{
protected:
	int numused;
	int	*freelist;
	int firstfree;

	void RebuildFreeList();									// SLOW
	void RebuildNumUsed	();									// SLOW
	void Grow			();									// SLOW

public:
    FastDArray ();
    FastDArray ( int newstepsize );
	~FastDArray();

    void SetSize		( int newsize );					// SLOW

    inline int PutData	( const T &newdata );				// FAST Returns index used
    void PutData		( const T &newdata, int index );	// SLOW

	void MarkUsed		( int index );						// SLOW
	inline void MarkNotUsed	( int index );					// FAST

    inline T *GetPointer();                                 // FAST Returns next free element, sets to 'used'
    inline T *GetPointer(int index);                        // FAST Returns next free element, sets to 'used'
    inline int GetNextFree();								// FAST Sets the returned index to 'used'

    inline int NumUsed() const;								// FAST Returns the number of used entries

    void Empty();											// FAST Resets the array to empty
    void EmptyAndDelete();                                  // FAST
};

//  ===================================================================

#include "pch.h"


#include <stdlib.h>

#include "DebugUtils.h"
#include "FastDArray.h"


template <class T>
FastDArray <T>::FastDArray()
	: DArray <T>()
{
	numused = 0;
	freelist = NULL;
	firstfree = -1;
}


template <class T>
FastDArray <T>::FastDArray(int newstepsize)
	: DArray<T>(newstepsize)
{
	numused = 0;
	freelist = NULL;
	firstfree = -1;
}

template <class T>
FastDArray <T>::~FastDArray()
{
	Empty();
	// user must call EmptyAndDelete() by hand when T is pointer,
	// otherwise memory leaks
}

template <class T>
void FastDArray <T>::RebuildFreeList()
{
	//  Reset free list

	delete freelist;
	freelist = new int[this->m_arraySize];
	firstfree = -1;
	int lastknownfree = -1;

	// Step through, rebuilding

	for (int i = 0; i < this->m_arraySize; ++i) {
		if (this->shadow[i] == 0) {
			if (this->firstfree == -1) {
				this->firstfree = i;
			}
			else {
				if (lastknownfree != -1) {
					this->freelist[lastknownfree] = i;
				}
			}

			this->freelist[i] = -1;
			lastknownfree = i;
		}
		else {
			this->freelist[i] = -2;
		}
	}
}


template <class T>
void FastDArray <T>::RebuildNumUsed()
{
	this->numused = 0;
	for (int i = 0; i < this->m_arraySize; ++i)
	{
		if (this->shadow[i] == 1) ++this->numused;
	}
}


template <class T>
void FastDArray <T>::SetSize(int newsize)
{
	if (newsize > this->m_arraySize)
	{
		int oldarraysize = this->m_arraySize;

		this->m_arraySize = newsize;
		T* temparray = new T[this->m_arraySize];
		char* tempshadow = new char[this->m_arraySize];
		int* tempfreelist = new int[this->m_arraySize];

		int a;

		for (a = 0; a < oldarraysize; ++a)
		{
			temparray[a] = this->array[a];
			tempshadow[a] = this->shadow[a];
			tempfreelist[a] = this->freelist[a];
		}

		for (a = oldarraysize; a < this->m_arraySize; ++a)
			tempshadow[a] = 0;

		int oldfirstfree = this->firstfree;
		this->firstfree = oldarraysize;
		for (a = oldarraysize; a < this->m_arraySize - 1; ++a)
			tempfreelist[a] = a + 1;
		tempfreelist[this->m_arraySize - 1] = oldfirstfree;

		delete[] this->array;
		delete[] this->shadow;
		delete[] this->freelist;

		this->array = temparray;
		this->shadow = tempshadow;
		this->freelist = tempfreelist;
	}
	else if (newsize < this->m_arraySize)
	{
		this->m_arraySize = newsize;
		T* temparray = new T[this->m_arraySize];
		char* tempshadow = new char[this->m_arraySize];
		int* tempfreelist = new int[this->m_arraySize];

		for (int a = 0; a < this->m_arraySize; ++a)
		{
			temparray[a] = this->array[a];
			tempshadow[a] = this->shadow[a];
			tempfreelist[a] = this->freelist[a];
		}

		delete[] this->array;
		delete[] this->shadow;
		delete[] this->freelist;

		this->array = temparray;
		this->shadow = tempshadow;
		this->freelist = tempfreelist;

		RebuildNumUsed();
		RebuildFreeList();
	}
	else if (newsize == this->m_arraySize)
	{
		// Do nothing
	}
}


template <class T>
void FastDArray <T>::Grow()
{
	if (this->m_stepSize == -1)
	{
		if (this->m_arraySize == 0)
		{
			SetSize(1);
		}
		else
		{
			// Double array size
			SetSize(this->m_arraySize * 2);
		}
	}
	else
	{
		// Increase array size by fixed amount
		SetSize(this->m_arraySize + this->m_stepSize);
	}
}


template <class T>
int FastDArray <T>::PutData(const T& newdata)
{
	if (this->firstfree == -1)
	{
		// Must resize the array
		this->Grow();
	}

	DarwiniaDebugAssert(this->firstfree != -1);
	if (this->firstfree == -1)
	{
		// Must resize the array
		this->Grow();
	}

	int freeslot = this->firstfree;
	int nextfree = this->freelist[freeslot];

	this->array[freeslot] = newdata;
	if (this->shadow[freeslot] == 0) ++this->numused;
	this->shadow[freeslot] = 1;
	this->freelist[freeslot] = -2;
	this->firstfree = nextfree;

	return freeslot;
}


template <class T>
void FastDArray <T>::PutData(const T& newdata, int index)
{
	DarwiniaDebugAssert(index < this->m_arraySize && index >= 0);

	this->array[index] = newdata;

	if (this->shadow[index] == 0)
	{
		this->shadow[index] = 1;
		++this->numused;
		this->RebuildFreeList();
	}
}


template <class T>
void FastDArray <T>::EmptyAndDelete()
{
	delete[] this->freelist;
	this->freelist = NULL;

	this->firstfree = -1;
	this->numused = 0;

	DArray<T>::EmptyAndDelete();
}

template <class T>
void FastDArray <T>::Empty()
{
	delete[] this->freelist;
	this->freelist = NULL;

	this->firstfree = -1;
	this->numused = 0;

	DArray<T>::Empty();
}


template <class T>
void FastDArray <T>::MarkUsed(int index)
{
	DarwiniaDebugAssert(index < this->m_arraySize && index >= 0);
	DarwiniaDebugAssert(this->shadow[index] == 0);

	this->shadow[index] = 1;
	++this->numused;
	this->RebuildFreeList();
}


template <class T>
void FastDArray <T>::MarkNotUsed(int index)
{
	DarwiniaDebugAssert(index < this->m_arraySize && index >= 0);
	DarwiniaDebugAssert(this->shadow[index] != 0);

	--this->numused;
	this->shadow[index] = 0;
	this->freelist[index] = this->firstfree;
	this->firstfree = index;
}


template <class T>
T* FastDArray<T>::GetPointer()
{
	return GetPointer(GetNextFree());
}


template <class T>
T* FastDArray<T>::GetPointer(int index)
{
	return DArray<T>::GetPointer(index);
}


template <class T>
int FastDArray<T>::GetNextFree()
{
	if (this->firstfree == -1)
	{
		// Must resize the array
		this->Grow();
	}

	int freeslot = this->firstfree;
	int nextfree = this->freelist[freeslot];

	if (this->shadow[freeslot] == 0)
	{
		++this->numused;
	}

	this->shadow[freeslot] = 1;
	this->freelist[freeslot] = -2;
	this->firstfree = nextfree;

	return freeslot;
}


template <class T>
int FastDArray <T>::NumUsed() const
{
	return this->numused;
}

#endif
