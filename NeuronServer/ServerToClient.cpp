#include "pch.h"

#include "ServerToClient.h"


namespace Neuron
{


  ServerToClient::ServerToClient(Endpoint const& _endpoint, int _connectionId)
    : m_endpoint(_endpoint),
      m_connectionId(_connectionId),
      m_lastKnownSequenceId(-1),
      m_ticksSinceHeardFrom(0)
  {
    // Constructing one of these used to open a socket and connect it to the
    // client, with a DEBUG_ASSERT if that failed. It is two values now.
  }


  Endpoint const& ServerToClient::GetEndpoint() const { return m_endpoint; }


  int ServerToClient::GetConnectionId() const { return m_connectionId; }
} // namespace Neuron
