#pragma once

#include <vector>

#include "SlotMap.h"

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

  std::vector<ServerToClientLetter*> m_history;

public:
  int m_sequenceId;

  Neuron::SlotMap<ServerToClient*> m_clients;
  Neuron::SlotMap<ServerTeam*> m_teams;

  NetMutex* m_inboxMutex;
  NetMutex* m_outboxMutex;
  std::vector<NetworkUpdate*> m_inbox;
  std::vector<ServerToClientLetter*> m_outbox;

  Neuron::SlotMap<unsigned char> m_sync; // Synchronisation values for each sequenceId

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

