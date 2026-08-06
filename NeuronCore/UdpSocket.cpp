#include "pch.h"

#include <format>

#include <ws2tcpip.h>

#include "Generic.h"
#include "UdpSocket.h"

namespace Neuron
{
  namespace
  {
    // Winsock's errors are ints from WSAGetLastError. Wrapping them in
    // system_category is what lets a caller print one, compare it against
    // std::errc, or propagate it without every layer above knowing it came from
    // Winsock — which is the point, because T7 replaces this class with an
    // interface that has other implementations.
    std::error_code LastError() { return std::error_code(WSAGetLastError(), std::system_category()); }
  } // namespace

  std::string Endpoint::ToString() const
  {
    in_addr address;
    address.s_addr = m_address;
    return std::format("{}:{}", IpToString(address), m_port);
  }


  std::error_code ResolveEndpoint(char const* _host, unsigned short _port, Endpoint& _endpoint)
  {
    _endpoint = Endpoint();

    if (!_host || !*_host)
      return std::make_error_code(std::errc::invalid_argument);

    addrinfo hints = {};
    hints.ai_family = AF_INET; // IPv4 only, which is what the protocol's packed addresses are
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    addrinfo* results = nullptr;
    const int failed = getaddrinfo(_host, nullptr, &hints, &results);
    if (failed != 0 || !results)
      return std::error_code(failed, std::system_category());

    // The first IPv4 answer, which is what NetGetHostByName's caller took —
    // h_addr_list[0] — so a multi-homed name resolves the same way it did.
    const sockaddr_in* address = reinterpret_cast<sockaddr_in const*>(results->ai_addr);
    _endpoint = Endpoint(address->sin_addr.s_addr, _port);

    freeaddrinfo(results);
    return std::error_code();
  }


  UdpSocket::~UdpSocket() { Close(); }

  bool UdpSocket::IsOpen() const { return m_socket != INVALID_SOCKET; }

  std::error_code UdpSocket::Open(unsigned short _port)
  {
    Close();

    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == INVALID_SOCKET)
      return LastError();

    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(_port);

    if (bind(m_socket, reinterpret_cast<sockaddr*>(&address), static_cast<int>(sizeof(address))) == SOCKET_ERROR)
    {
      const std::error_code failure = LastError();
      Close();
      return failure;
    }

    unsigned long nonBlocking = 1;
    if (ioctlsocket(m_socket, FIONBIO, &nonBlocking) == SOCKET_ERROR)
    {
      const std::error_code failure = LastError();
      Close();
      return failure;
    }

    return std::error_code();
  }

  void UdpSocket::Close()
  {
    if (m_socket != INVALID_SOCKET)
    {
      closesocket(m_socket);
      m_socket = INVALID_SOCKET;
    }
  }

  std::error_code UdpSocket::SendTo(Endpoint const& _to, char const* _data, int _length)
  {
    if (!IsOpen())
      return std::make_error_code(std::errc::not_connected);

    if (!_data || _length <= 0)
      return std::make_error_code(std::errc::invalid_argument);

    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = _to.m_address;
    address.sin_port = htons(_to.m_port);

    const int sent = sendto(m_socket, _data, _length, 0, reinterpret_cast<sockaddr*>(&address), static_cast<int>(sizeof(address)));
    if (sent == SOCKET_ERROR)
      return LastError();

    // A datagram is sent whole or not at all — there is no partial send to loop
    // over. NetSocket carried a select-plus-partial-write loop inherited from
    // TCP that could not execute for a datagram socket, and T6 deletes it.
    if (sent != _length)
      return std::make_error_code(std::errc::message_size);

    return std::error_code();
  }

  bool UdpSocket::TryReceive(char* _buffer, int _capacity, int& _bytesReceived, Endpoint& _from)
  {
    _bytesReceived = 0;
    _from = Endpoint();

    if (!IsOpen() || !_buffer || _capacity <= 0)
      return false;

    // Loops rather than returning false on the first error, because two of the
    // errors below mean "that one is not worth having" rather than "the socket
    // is empty" — and returning false for those would end the caller's drain
    // with datagrams still queued behind them.
    while (true)
    {
      sockaddr_in from = {};
      int fromLength = static_cast<int>(sizeof(from));

      const int received = recvfrom(m_socket, _buffer, _capacity, 0, reinterpret_cast<sockaddr*>(&from), &fromLength);

      if (received != SOCKET_ERROR)
      {
        _bytesReceived = received;
        _from = Endpoint(from.sin_addr.s_addr, ntohs(from.sin_port));
        return true;
      }

      const int error = WSAGetLastError();

      if (error == WSAEWOULDBLOCK)
        return false; // nothing waiting, which is most ticks

      if (error == WSAEMSGSIZE)
        continue; // longer than MaxDatagramSize; the excess is gone and so is the datagram

      // Windows reports a previous send's ICMP port-unreachable as a receive
      // error on a bound UDP socket. It says nothing about the socket's health
      // and nothing about the datagrams still queued behind it.
      if (error == WSAECONNRESET)
        continue;

      return false;
    }
  }
} // namespace Neuron
