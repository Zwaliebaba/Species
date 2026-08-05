#pragma once

#include "Debug.h"


namespace Neuron
{
  template <class T, unsigned int N> class PresizedArray
  {
    public:
      T m_data[N]; // fixed-size array of elements of type T

    public:
      T& operator[](unsigned int i)
      {
        DEBUG_ASSERT(i < N);
        return m_data[i];
      }
      const T& operator[](unsigned int i) const
      {
        DEBUG_ASSERT(i < N);
        return m_data[i];
      }

      static unsigned int Size() { return N; }

      // Assign one value to all elements
      void SetAll(const T& _value)
      {
        for (unsigned int i = 0; i < N; ++i)
        {
          m_data[i] = _value;
        }
      }
  };


  // Comparisons
  // template <class T, std::size_t N>
  // bool operator == (const PresizedArray<T,N>& x, const PresizedArray<T,N>& y)
  //{
  //    return std::equal(x.begin(), x.end(), y.begin());
  //}
  //
  // template <class T, std::size_t N>
  // bool operator < (const PresizedArray<T,N>& x, const PresizedArray<T,N>& y)
  //{
  //    return std::lexicographical_compare(x.begin(),x.end(),y.begin(),y.end());
  //}
  //
  // template <class T, std::size_t N>
  // bool operator != (const PresizedArray<T,N>& x, const PresizedArray<T,N>& y)
  //{
  //    return !(x == y);
  //}
  //
  // template <class T, std::size_t N>
  // bool operator > (const PresizedArray<T,N>& x, const PresizedArray<T,N>& y)
  //{
  //    return y < x;
  //}
  //
  // template <class T, std::size_t N>
  // bool operator <= (const PresizedArray<T,N>& x, const PresizedArray<T,N>& y)
  //{
  //    return !(y < x);
  //}
  //
  // template < class T, std::size_t N>
  // bool operator >= (const PresizedArray<T,N>& x, const PresizedArray<T,N>& y)
  //{
  //    return !(x < y);
  //}
} // namespace Neuron
