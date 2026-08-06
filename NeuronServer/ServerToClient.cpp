#include "pch.h"


#include "Debug.h"
#include "Generic.h"
#include "ProtocolLimits.h"
#include "ServerToClient.h"


namespace Neuron
{


  ServerToClient::ServerToClient(char* _ip)
    : m_ip(_ip),
      // ConvertIPToInt produces the octets in the order sockaddr_in holds
      // them, so this is the address to send back to with no further
      // conversion. Constructing one can no longer fail: it used to open and
      // connect a socket, and assert if that did not work.
      m_endpoint(static_cast<unsigned long>(ConvertIPToInt(_ip)), ClientPort)
  {
    m_lastKnownSequenceId = -1;
  }


  std::string_view ServerToClient::GetIP() { return m_ip; }


  Endpoint const& ServerToClient::GetEndpoint() const { return m_endpoint; }
} // namespace Neuron
