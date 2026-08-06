
/*
 * BYTE STREAM
 *
 * Bounded reading and writing of the scalars a datagram is made of,
 * for network transmission via UDP.
 *
 */

#pragma once

#include <cstddef>
#include <cstring>
#include <type_traits>

namespace Neuron
{
  // ****************************************************************************
  //  Class ByteReader
  // ****************************************************************************

  // Reads scalars out of a datagram somebody else sent.
  //
  // What this replaces was a pair of macros — `*((int*)_stream); _stream +=
  // sizeof(int);` — with no notion of where the buffer ended, so every read in
  // the protocol was a read of whatever the sender chose to make it.
  // ServerToClientLetter took a length parameter and never consulted it, and a
  // datagram claiming more updates than it carried walked off the end of the
  // receive buffer at the first one that did not fit.
  //
  // OVERRUN IS STICKY, NOT FATAL. A read that does not fit returns a zeroed
  // value, leaves the cursor alone, and latches Ok() false for good. That is
  // what lets a parser run its switch to the bottom and ask once, at the end,
  // whether any of what it read was real — rather than checking a return value
  // after every field, which is the shape nobody maintains.
  class ByteReader
  {
    public:
      ByteReader(char const* _data, size_t _length)
        : m_data(_data),
          m_length(_data ? _length : 0)
      {
      }

      // The width is spelled at the call site and never deduced, because the
      // width IS the wire format. READ_UNSIGNED_CHAR said so in its name; a
      // deduced Read() would let the type of whatever member receives the value
      // decide how many bytes come off the wire, and change the protocol on the
      // day somebody widens a field.
      template <typename T> [[nodiscard]] T Read()
      {
        static_assert(std::is_trivially_copyable_v<T>, "the wire carries object representations, so only trivially copyable types");

        T value{};

        if (sizeof(T) > m_length - m_cursor)
        {
          m_overran = true;
          return value;
        }

        // memcpy rather than a cast: the cursor is a byte offset into a
        // datagram, so nothing makes it aligned for T — a float after an odd
        // number of chars is the ordinary case here — and the cast form was
        // undefined behaviour every time it happened to land unaligned.
        std::memcpy(&value, m_data + m_cursor, sizeof(T));
        m_cursor += sizeof(T);
        return value;
      }

      [[nodiscard]] bool Ok() const { return !m_overran; }
      [[nodiscard]] size_t BytesRead() const { return m_cursor; }
      [[nodiscard]] size_t BytesRemaining() const { return m_length - m_cursor; }

    private:
      char const* m_data;
      size_t m_length;
      size_t m_cursor = 0;
      bool m_overran = false;
  };


  // ****************************************************************************
  //  Class ByteWriter
  // ****************************************************************************

  // Writes scalars into a datagram this end is about to send.
  //
  // The bound matters less here than on the reading side — a sender controls
  // what it emits — but only until the buffer it emits into is smaller than what
  // it was asked to emit, which is exactly the state the send and receive
  // buffers are in today (see tasks/network-transport.yaml T3). A write that
  // does not fit is dropped and latched instead of running off the end.
  class ByteWriter
  {
    public:
      ByteWriter(char* _data, size_t _capacity)
        : m_data(_data),
          m_capacity(_data ? _capacity : 0)
      {
      }

      // Non-deduced for the same reason as ByteReader::Read, and it matters
      // more here: an integer literal deduces to int, so a deduced
      // Write(m_teamId) after somebody changes m_teamId's type silently emits a
      // different number of bytes and nothing fails to compile.
      template <typename T> void Write(std::type_identity_t<T> _value)
      {
        static_assert(std::is_trivially_copyable_v<T>, "the wire carries object representations, so only trivially copyable types");

        if (sizeof(T) > m_capacity - m_cursor)
        {
          m_overran = true;
          return;
        }

        std::memcpy(m_data + m_cursor, &_value, sizeof(T));
        m_cursor += sizeof(T);
      }

      // For a block already in wire form — one update's serialised bytes being
      // copied into the letter that carries it.
      void WriteBytes(char const* _bytes, size_t _count)
      {
        if (!_bytes || _count > m_capacity - m_cursor)
        {
          m_overran = true;
          return;
        }

        std::memcpy(m_data + m_cursor, _bytes, _count);
        m_cursor += _count;
      }

      [[nodiscard]] bool Ok() const { return !m_overran; }
      [[nodiscard]] size_t BytesWritten() const { return m_cursor; }
      [[nodiscard]] size_t BytesRemaining() const { return m_capacity - m_cursor; }

    private:
      char* m_data;
      size_t m_capacity;
      size_t m_cursor = 0;
      bool m_overran = false;
  };
} // namespace Neuron
