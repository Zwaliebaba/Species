/* Represents one connection to one client. Server is expected to have a list of these. */


#pragma once


// NeuronCore, still at global scope until namespace-migration T2 reaches it.
class NetSocket;


namespace Neuron
{
  class ServerToClient
  {
    private:
      char m_ip[16];
      NetSocket* m_socket;

    public:
      ServerToClient(char* _ip);

      char* GetIP();
      NetSocket* GetSocket();

      int m_lastKnownSequenceId;
  };
} // namespace Neuron
