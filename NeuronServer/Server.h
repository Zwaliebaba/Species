#pragma once

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

class Server
{
  NetLib* m_netLib;
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

    // Handed its profiler rather than reaching for it through the application
    // object. Networking is always real UDP; there is no in-process shortcut.
    void Initialise(class Profiler* _profiler);

    NetworkUpdate* GetNextLetter();

    void ReceiveLetter(NetworkUpdate* update, char* fromIP);
    void SendLetter(ServerToClientLetter* letter);

    int GetClientId(char* _ip);
    void RegisterNewClient(char* _ip);
    void RemoveClient(char* _ip);
    void RegisterNewTeam(char* _ip, int _teamType, int _desiredTeamId);

    void AdvanceSender();
    void Advance();
};

