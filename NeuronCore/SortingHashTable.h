#ifndef INCLUDED_SORTING_HASH_TABLE_H
#define INCLUDED_SORTING_HASH_TABLE_H

#include "HashTable.h"

// Extends HashTable to provide a mechanism to iterating over all the
// table elements in alphabetical order.
//
// - PutData and RemoveData are now O(n) rather than O(1)
// - It is now possible to iterate over all elements without
//   having to manually discard all the empty slots.

template <class T>
class SortingHashTable : public HashTable<T>
{
  protected:
    short* m_orderedIndices; // Stores a chain of indices that make it easy to walk through the table in alphabetical key order
    short m_firstOrderedIndex; // The index of the alphabetically first table element
    short m_nextOrderedIndex; // Used by GetNextOrderedIndex

    void Grow();
    short FindPrevKey(const char* _key) const;
    // Returns the index of the table element whose key is alphabetically previous to the specified key

  public:
    SortingHashTable();
    ~SortingHashTable();

    int PutData(const char* _key, const T& _data); // Returns slot used
    void RemoveData(const char* _key); // Client still MUST delete if data is a pointer
    void RemoveData(unsigned int _index); // Client still MUST delete if data is a pointer

    short StartOrderedWalk(); // These two are used to while loop across all elements
    short GetNextOrderedIndex(); // in alphabetical order. Both will return -1
    // when there are no more elements
};

#include "Debug.h"
#include "SortingHashTable.h"

//*****************************************************************************
// Protected Functions
//*****************************************************************************

template <class T>
void SortingHashTable<T>::Grow()
{
  unsigned int oldSize = this->m_size;
  this->m_size *= 2;
  this->m_mask = (this->m_mask << 1) + 1;

  // Copy pointers to existing data
  char** oldKeys = this->m_keys;
  T* oldData = this->m_data;
  short* oldOrderedIndices = this->m_orderedIndices;

  // Make new data
  this->m_keys = new char*[this->m_size];
  this->m_data = new T[this->m_size];
  this->m_orderedIndices = new short[this->m_size];

  // Set all new data to zero
  memset(this->m_keys, 0, sizeof(char*) * this->m_size);
  memset(this->m_data, 0, sizeof(T) * this->m_size);
  memset(this->m_orderedIndices, -2, sizeof(short) * this->m_size);

  //
  // Go through the existing ordered index list, inserting elements into the
  // new table as we go

  short oldI = this->m_firstOrderedIndex;
  short newI = this->GetInsertPos(oldKeys[oldI]);
  this->m_firstOrderedIndex = newI;
  while (oldI != -1)
  {
    short nextOldI = oldOrderedIndices[oldI];

    this->m_keys[newI] = oldKeys[oldI];
    this->m_data[newI] = oldData[oldI];

    short nextNewI = (nextOldI != -1) ? this->GetInsertPos(oldKeys[nextOldI]) : -1;

    this->m_orderedIndices[newI] = nextNewI;

    oldI = nextOldI;
    newI = nextNewI;
  }

  this->m_slotsFree += this->m_size - oldSize;

  delete[] oldKeys;
  delete[] oldData;
  delete[] oldOrderedIndices;
}

// See header for description
template <class T>
short SortingHashTable<T>::FindPrevKey(const char* _key) const
{
  short prevI = -1;
  short i = m_firstOrderedIndex;

  while (true)
  {
    if (i == -1)
      return prevI;

    if (stricmp(this->m_keys[i], _key) >= 0)
      return prevI;

    prevI = i;
    i = this->m_orderedIndices[i];
  }
}

//*****************************************************************************
// Public Functions
//*****************************************************************************

template <class T>
SortingHashTable<T>::SortingHashTable()
  : HashTable<T>(),
    m_firstOrderedIndex(-1)
{
  m_orderedIndices = new short[this->m_size];
  memset(m_orderedIndices, 0, sizeof(short) * this->m_size);
}

template <class T>
SortingHashTable<T>::~SortingHashTable() { delete[] m_orderedIndices; }

template <class T>
int SortingHashTable<T>::PutData(const char* _key, const T& _data)
{
  //
  // Make sure the table is big enough

  if (this->m_slotsFree * 2 <= this->m_size)
    Grow();

  //
  // Do the main insert

  unsigned int index = this->GetInsertPos(_key);
  DEBUG_ASSERT(this->m_keys[index] == NULL);
  this->m_keys[index] = strdup(_key);
  this->m_data[index] = _data;
  this->m_slotsFree--;

  //
  // Insert us into the alphabetically order index list

  short i = FindPrevKey(_key);
  if (i == -1)
  {
    // Handle the case when the table is empty, or the new element is going
    // to be the new first alphabetical element
    m_orderedIndices[index] = m_firstOrderedIndex;
    m_firstOrderedIndex = index;
  }
  else
  {
    m_orderedIndices[index] = m_orderedIndices[i];
    m_orderedIndices[i] = index;
  }

  return index;
}

template <class T>
void SortingHashTable<T>::RemoveData(const char* _key)
{
  int index = this->GetIndex(_key);
  if (index >= 0)
    RemoveData(index);
}

template <class T>
void SortingHashTable<T>::RemoveData(unsigned int _index)
{
  //
  // Remove data

  delete[] this->m_keys[_index];
  this->m_keys[_index] = NULL;
  ++this->m_slotsFree;

  //
  // Remove from ordered list

  short prevIndex = -1;
  short index = m_firstOrderedIndex;
  while (m_orderedIndices[index] != _index)
  {
    DEBUG_ASSERT(index != -1);
    prevIndex = index;
    index = m_orderedIndices[index];
  }

  if (prevIndex == -1)
    m_firstOrderedIndex = -1;
  else
    m_orderedIndices[prevIndex] = m_orderedIndices[index];
}

template <class T>
short SortingHashTable<T>::StartOrderedWalk()
{
  m_nextOrderedIndex = m_orderedIndices[m_firstOrderedIndex];
  return m_firstOrderedIndex;
}

template <class T>
short SortingHashTable<T>::GetNextOrderedIndex()
{
  short rv = m_nextOrderedIndex;
  m_nextOrderedIndex = m_orderedIndices[m_nextOrderedIndex];
  return rv;
}

#endif
