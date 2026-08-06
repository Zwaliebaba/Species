// ****************************************************************************
//  One non-blocking UDP socket, polled from the tick.
// ****************************************************************************

#pragma once

#include <string>
#include <system_error>

#include <winsock2.h>

namespace Neuron
{
  // Where a datagram came from, and therefore where a reply goes.
  //
  // The address is held exactly as sockaddr_in holds it — IPv4, network byte
  // order — because that is what it is read from and written back into.
  // ConvertIPToInt in Generic.h produces the same layout from a dotted quad.
  class Endpoint
  {
    public:
      unsigned long m_address = 0;
      unsigned short m_port = 0;

      Endpoint() = default;
      Endpoint(unsigned long _address, unsigned short _port)
        : m_address(_address),
          m_port(_port)
      {
      }

      // A port of zero is not a port anything can reply to, which makes it the
      // natural "no endpoint" — an address of zero is INADDR_ANY and legitimate.
      [[nodiscard]] bool IsValid() const { return m_port != 0; }

      bool operator==(Endpoint const& _other) const = default;

      [[nodiscard]] std::string ToString() const;
  };


  // Resolves a host — a dotted quad or a name — and a port to an endpoint.
  //
  // It exists because the client's server address is a preferences string, so
  // "myserver.local" is a thing a user can write. The legacy transport resolved
  // it on the way to connecting, and T6 deleted that; without this the address
  // would silently have to be numeric, which is the kind of regression a build
  // cannot notice.
  [[nodiscard]] std::error_code ResolveEndpoint(char const* _host, unsigned short _port, Endpoint& _endpoint);


  // The whole transport, and deliberately small: open, send, try to receive.
  //
  // ONE SOCKET, POLLED, NOT A THREAD PER ENDPOINT. What this replaces was a
  // socket per client for sending, plus a listener on its own blocking thread
  // for receiving, with a mutex between that thread and the tick — and the
  // thread bought nothing, because the inbox it filled was only ever drained
  // once per tick anyway. Polling from the tick deletes the thread, the mutex
  // and every cross-thread hazard, and changes no timing.
  //
  // It stays one socket at any client count. IOCP and RIO earn their complexity
  // at thousands of sockets; a UDP game server is one socket talking to
  // everybody, and Transport (T7) is where a different implementation would
  // slot in without the protocol noticing.
  class UdpSocket
  {
    public:
      UdpSocket() = default;
      ~UdpSocket();

      // A socket is an OS handle with an owner. Copying one would close it
      // twice.
      UdpSocket(UdpSocket const&) = delete;
      UdpSocket& operator=(UdpSocket const&) = delete;

      // Binds to _port on every interface and switches to non-blocking. An
      // empty error_code is success.
      [[nodiscard]] std::error_code Open(unsigned short _port);
      void Close();
      [[nodiscard]] bool IsOpen() const;

      [[nodiscard]] std::error_code SendTo(Endpoint const& _to, char const* _data, int _length);

      // True if a datagram was read into _buffer. False means nothing was
      // waiting — the ordinary case on a quiet tick — or that what was waiting
      // was not worth handing on: an oversized datagram, or the stale ICMP
      // port-unreachable Windows reports on a bound UDP socket. Neither of
      // those is a reason to stop draining, so both are consumed and the drain
      // continues; the caller loops until this returns false.
      [[nodiscard]] bool TryReceive(char* _buffer, int _capacity, int& _bytesReceived, Endpoint& _from);

    private:
      SOCKET m_socket = INVALID_SOCKET;
  };
} // namespace Neuron
