/* Represents one connection to one client. Server is expected to have a list of these. */


#pragma once

#include <string>
#include <string_view>

#include "UdpSocket.h"


namespace Neuron
{
  class ServerToClient
  {
    private:
      // std::string since strings-modernised T9. Was char[16] with an unbounded
      // strcpy into it — wide enough for dotted IPv4 and nothing else, which
      // strings-modernised T1 recorded as a hazard the day a hostname reaches
      // it. It is a per-connection registry key and is never serialised, so
      // there is no wire format to keep.
      std::string m_ip;

      // Where replies go. It replaces a NetSocket per client — one OS socket
      // each, "connected" to the client's address at construction, so a
      // registry of clients was also a registry of sockets. The server has one
      // socket now and this is just an address.
      Endpoint m_endpoint;

    public:
      ServerToClient(char* _ip);

      std::string_view GetIP();
      [[nodiscard]] Endpoint const& GetEndpoint() const;

      int m_lastKnownSequenceId;
  };
} // namespace Neuron
