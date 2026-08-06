// ****************************************************************************
//  How a datagram leaves and arrives, with the sockets left out of it.
// ****************************************************************************

#pragma once

#include <system_error>

#include "UdpSocket.h"

namespace Neuron
{
  // The seam. Two methods, and they are the two UdpSocket already had.
  //
  // IT EXISTS SO THE PROTOCOL CAN BE TESTED. Everything under Tests/ up to now
  // covers encodings — what a letter's bytes are — and nothing covers the
  // conversation those letters make: join, hello, team assignment, sequenced
  // updates, acknowledgement, retransmission. That is where the defects
  // actually live, and it was unreachable because exercising it meant opening a
  // socket, which docs/TESTING.md forbids and a CI runner cannot be relied on
  // to provide. LoopbackTransport is a fake wire in memory; the game injects
  // UdpTransport and behaves exactly as it did.
  //
  // It is also where a different implementation would go if one is ever wanted.
  // That is NOT why it is here — a UDP game server is one socket at any client
  // count, and IOCP or RIO earn their complexity at thousands — but it costs
  // nothing to leave the door open once the tests need the door anyway.
  class Transport
  {
    public:
      virtual ~Transport() = default;

      [[nodiscard]] virtual std::error_code Send(Endpoint const& _to, char const* _data, int _length) = 0;

      // True if a datagram was read into _buffer. False means nothing is
      // waiting; the caller drains by looping until it says so.
      [[nodiscard]] virtual bool TryReceive(char* _buffer, int _capacity, int& _bytesReceived, Endpoint& _from) = 0;
  };


  // The real wire, behind the seam. It owns its socket rather than wrapping a
  // borrowed one, so "the transport" is one object to hand about and one thing
  // to close.
  //
  // It lives here rather than in UdpSocket.h so the dependency runs one way:
  // Transport.h knows what a UdpSocket is, and UdpSocket.h does not know a
  // Transport exists. The other arrangement compiles only when the two headers
  // are included in one particular order, which is the kind of thing that works
  // until somebody adds an include somewhere else.
  class UdpTransport : public Transport
  {
    public:
      [[nodiscard]] std::error_code Open(unsigned short _port) { return m_socket.Open(_port); }
      [[nodiscard]] bool IsOpen() const { return m_socket.IsOpen(); }
      void Close() { m_socket.Close(); }

      [[nodiscard]] std::error_code Send(Endpoint const& _to, char const* _data, int _length) override
      {
        return m_socket.SendTo(_to, _data, _length);
      }

      [[nodiscard]] bool TryReceive(char* _buffer, int _capacity, int& _bytesReceived, Endpoint& _from) override
      {
        return m_socket.TryReceive(_buffer, _capacity, _bytesReceived, _from);
      }

    private:
      UdpSocket m_socket;
  };
} // namespace Neuron
