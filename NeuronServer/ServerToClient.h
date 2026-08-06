/* Represents one connection to one client. Server is expected to have a list of these. */


#pragma once

#include "UdpSocket.h"


namespace Neuron
{
  class ServerToClient
  {
    private:
      // WHERE this client is: the address and port its datagrams arrived from,
      // which is also where replies go. It replaces a dotted-quad string plus a
      // fixed reply port, and that pair is what limited a host to one client and
      // could not work through NAT — two players behind one router share an
      // address and differ only by port.
      //
      // It also replaces a socket. Each client used to own one.
      Endpoint m_endpoint;

      // WHO this client is, as far as the protocol is concerned. Assigned by the
      // server, sent to the client in HelloClient, and what a TeamAssign is
      // matched against.
      int m_connectionId;

    public:
      ServerToClient(Endpoint const& _endpoint, int _connectionId);

      [[nodiscard]] Endpoint const& GetEndpoint() const;
      [[nodiscard]] int GetConnectionId() const;

      int m_lastKnownSequenceId;

      // Server ticks since anything arrived from this client. Reset by every
      // datagram it sends, including the IAmAlive it sends when it has nothing
      // else to say.
      int m_ticksSinceHeardFrom;
  };
} // namespace Neuron
