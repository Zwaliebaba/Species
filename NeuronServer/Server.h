#pragma once

#include <memory>
#include <vector>

#include "SlotMap.h"

// NeuronCore types. They are still at global scope — namespace-migration works
// bottom-up and T2 has not reached NeuronClient or the rest of the core yet — so
// they are declared OUT here rather than inside the namespace, where they would
// name different types entirely.
class NetLib;
class NetMutex;
class NetSocketListner;
class ServerToClientLetter;
class NetworkUpdate;
class Profiler;

namespace Neuron
{
  class ServerToClient;

  class ServerTeam
  {
    public:
      int m_clientId;

      ServerTeam(int _clientId);
  };

  class Server
  {
      NetLib* m_netLib;
      Profiler* m_profiler;

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
      void Initialise(Profiler* _profiler);

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
} // namespace Neuron
