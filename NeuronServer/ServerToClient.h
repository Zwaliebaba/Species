/* Represents one connection to one client. Server is expected to have a list of these. */


#pragma once

#include <string>
#include <string_view>


// NeuronCore, still at global scope until namespace-migration T2 reaches it.
class NetSocket;


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
      NetSocket* m_socket;

    public:
      ServerToClient(char* _ip);

      std::string_view GetIP();
      NetSocket* GetSocket();

      int m_lastKnownSequenceId;
  };
} // namespace Neuron
