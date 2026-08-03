#pragma once

#include <memory>
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

  std::vector<std::unique_ptr<ServerToClientLetter>> m_history;

public:
  int m_sequenceId;

  Neuron::SlotMap<std::unique_ptr<ServerToClient>> m_clients;
  Neuron::SlotMap<std::unique_ptr<ServerTeam>> m_teams;

  NetMutex* m_inboxMutex;
  NetMutex* m_outboxMutex;
  std::vector<std::unique_ptr<NetworkUpdate>> m_inbox;
  std::vector<std::unique_ptr<ServerToClientLetter>> m_outbox;

  Neuron::SlotMap<unsigned char> m_sync; // Synchronisation values for each sequenceId

  Server();
  ~Server();

  // Handed its profiler rather than reaching for it through the application
  // object. Networking is always real UDP; there is no in-process shortcut.
  void Initialise(class Profiler* _profiler);

  std::unique_ptr<NetworkUpdate> GetNextLetter();

  void ReceiveLetter(std::unique_ptr<NetworkUpdate> update, char* fromIP);
  void SendLetter(std::unique_ptr<ServerToClientLetter> letter);

  int GetClientId(char* _ip);
  void RegisterNewClient(char* _ip);
  void RemoveClient(char* _ip);
  void RegisterNewTeam(char* _ip, int _teamType, int _desiredTeamId);

  void AdvanceSender();
  void Advance();
};

