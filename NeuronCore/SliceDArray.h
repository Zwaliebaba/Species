//===============================================================//
//						  SLICE DARRAY                           //
//                                                               //
//                   By Christopher Delay                        //
//                           V1.3                                //
//===============================================================//

#pragma once


#include "FastDArray.h"


//=================================================================
// Slice Dynamic array object
// Use : A dynamically sized list of data
// Which can be indexed into - an entry's index never changes
// Same as Fast DArray, but has data and methods to help "slicing" up a for loop
// across all the members.

template <class T>
class SliceDArray : public FastDArray <T>
{
protected:
    int totalNumSlices;
    int lastSlice;          // Slice number used last time GetNextSliceBounds was used
	int lastUpdated;        // Previous result returned as the "upper" value from GetNextSliceBounds

public:
    SliceDArray ();
    SliceDArray (int _totalNumSlices);
    SliceDArray ( int newstepsize, int _totalNumSlices );

    void Empty ();
    void GetNextSliceBounds (int slice, int *lower, int *upper);
    void SetTotalNumSlices(int _slices);
    int GetLastUpdated();
};


//  ===================================================================

#include "pch.h"


#include <stdlib.h>

#include "Debug.h"
#include "SliceDArray.h"

template <class T>
SliceDArray <T> ::SliceDArray()
    : FastDArray <T>(),
    totalNumSlices(0),
    lastSlice(-1),
    lastUpdated(0)
{}

template <class T>
SliceDArray <T> ::SliceDArray(int _totalNumSlices)
    : FastDArray <T>(),
    totalNumSlices(_totalNumSlices),
    lastSlice(-1),
    lastUpdated(0)
{}


template <class T>
SliceDArray <T> ::SliceDArray(int _totalNumSlices, int newstepsize)
    : FastDArray<T>(newstepsize),
    totalNumSlices(_totalNumSlices),
    lastSlice(-1),
    lastUpdated(0)
{}


template <class T>
void SliceDArray<T>::Empty()
{
    lastSlice = -1;
    lastUpdated = 0;
    FastDArray<T>::Empty();
}


template <class T>
void SliceDArray <T>::GetNextSliceBounds(int _slice, int* _lower, int* _upper)
{
    DEBUG_ASSERT(lastSlice == -1 ||
        _slice == lastSlice + 1 ||
        (_slice == 0 && lastSlice == totalNumSlices - 1));

    if (_slice == 0)
    {
        *_lower = 0;
    }
    else
    {
        *_lower = lastUpdated + 1;
    }

    if (_slice == totalNumSlices - 1)
    {
        *_upper = this->Size() - 1;
    }
    else
    {
        int numPerSlice = int(this->Size() / (float)totalNumSlices);
        *_upper = *_lower + numPerSlice;
    }

    lastUpdated = *_upper;
    lastSlice = _slice;
}


template <class T>
void SliceDArray <T>::SetTotalNumSlices(int _slices)
{
    totalNumSlices = _slices;
}


template <class T>
int SliceDArray <T>::GetLastUpdated()
{
    return lastUpdated;
}

