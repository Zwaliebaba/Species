#ifndef SERVER_H
#define SERVER_H

#include "NetworkSinks.h"
#include "LList.h"
#include "DArray.h"

class NetLib;
class NetMutex;
class NetSocketListner;
class ServerToClient;
class ServerToClientLetter;
class NetworkUpdate;

class ServerTeam
{
  public:
    int m_clientId;

    ServerTeam(int _clientId);
};

class Server : public ServerUpdateSink
{
  NetLib* m_netLib;

  // A locally hosted client, or null when this server only talks over sockets.
  ClientLetterSink* m_localClient;
  bool m_bypassNetworking;
  class Profiler* m_profiler;

  LList<ServerToClientLetter*> m_history;

  public:
    int m_sequenceId;

    DArray<ServerToClient*> m_clients;
    DArray<ServerTeam*> m_teams;

    NetMutex* m_inboxMutex;
    NetMutex* m_outboxMutex;
    LList<NetworkUpdate*> m_inbox;
    LList<ServerToClientLetter*> m_outbox;

    DArray<unsigned char> m_sync; // Synchronisation values for each sequenceId

    Server();
    ~Server();

    // The server is handed what it needs rather than reading it off a global:
    // whether to skip sockets, where profiling goes, and the local client to
    // deliver to when hosting in-process. _localClient may be null.
    void Initialise(bool _bypassNetworking, class Profiler* _profiler, ClientLetterSink* _localClient);

    NetworkUpdate* GetNextLetter();

    void ReceiveLetter(NetworkUpdate* update, char* fromIP) override;
    void SendLetter(ServerToClientLetter* letter);

    int GetClientId(char* _ip);
    void RegisterNewClient(char* _ip);
    void RemoveClient(char* _ip);
    void RegisterNewTeam(char* _ip, int _teamType, int _desiredTeamId);

    void AdvanceSender();
    void Advance();
};

#endif
