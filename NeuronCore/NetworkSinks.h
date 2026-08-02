#pragma once

// Where a delivered message goes, expressed so the sender does not have to know
// who is on the other end.
//
// The client can host a server in its own process, and the two used to find each
// other through the application singleton — Server reaching for
// g_app->m_clientToServer, ClientToServer reaching for g_app->m_server. That
// works only while both live in the same binary as the game, which is exactly
// what stops a headless server from being buildable.
//
// Each side now holds the other as a sink and is handed one at construction.
// A listen-server wires the two together; a dedicated host leaves the client
// sink null and lets the sockets carry everything.

class NetworkUpdate;
class ServerToClientLetter;

// Implemented by Server. Fed by a locally hosted client, or by the socket
// listener when the update arrived over the wire.
class ServerUpdateSink
{
  public:
    virtual ~ServerUpdateSink() = default;
    virtual void ReceiveLetter(NetworkUpdate* _update, char* _fromIp) = 0;
};

// Implemented by ClientToServer. Fed by a locally hosted server, or by the
// socket listener.
class ClientLetterSink
{
  public:
    virtual ~ClientLetterSink() = default;
    virtual void ReceiveLetter(ServerToClientLetter* _letter) = 0;
};
