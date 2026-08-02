/* Represents one connection to one client. Server is expected to have a list of these. */


#ifndef _SERVERTOCLIENT_H
#define _SERVERTOCLIENT_H


class NetSocket;


class ServerToClient
{
private:
    char		m_ip[16];
    NetSocket	*m_socket;

public:
  // Told whether to open a socket rather than reading it off the application
  // object; Server is the only thing that constructs these and already knows.
  ServerToClient(char* _ip, bool _bypassNetworking);

  char* GetIP();
  NetSocket* GetSocket();

  int m_lastKnownSequenceId;
};



#endif

