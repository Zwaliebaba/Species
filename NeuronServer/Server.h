#pragma once

#include <memory>
#include <vector>

#include "NetworkUpdate.h"
#include "SlotMap.h"
#include "UdpSocket.h"

// NeuronCore types. They are still at global scope — namespace-migration works
// bottom-up and T2 has not reached NeuronClient or the rest of the core yet — so
// they are declared OUT here rather than inside the namespace, where they would
// name different types entirely.
//
// NetworkUpdate is INCLUDED rather than forward-declared: Server holds a
// std::vector of them by value now, so every translation unit that destroys a
// Server needs the complete type.
class NetLib;
class ServerToClientLetter;
class Profiler;

namespace Neuron
{
  class ServerToClient;

  // Moves as many updates as fit from the front of _pending into _letter,
  // leaving the rest where they are, in order.
  //
  // A free function rather than a method on either, because it is the only part
  // of Server::Advance that can be exercised without a socket, a mutex or a
  // preferences file — the same lift the input plan made for DeriveFrameState.
  // What it has to get right is that a full letter STOPS: an update that does
  // not fit is carried to the next tick rather than dropped, and nothing behind
  // it may overtake it, because the order updates are broadcast in is the order
  // every client applies them in.
  void FillLetter(ServerToClientLetter& _letter, std::vector<NetworkUpdate>& _pending);

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

      // No mutexes. Both of these are filled and drained by Advance on one
      // thread now — the listen thread that used to fill the inbox is gone,
      // and with it the only reason two threads ever touched either.
      std::vector<std::unique_ptr<NetworkUpdate>> m_inbox;
      std::vector<std::unique_ptr<ServerToClientLetter>> m_outbox;

      Neuron::SlotMap<unsigned char> m_sync; // Synchronisation values for each sequenceId

      // Accepted for broadcast and not yet sent. A tick that produces more
      // updates than one datagram can carry leaves the remainder here and sends
      // them at the front of the next tick's letter.
      std::vector<NetworkUpdate> m_pendingUpdates;

      // The one socket, bound to ServerPort and polled from Advance.
      UdpSocket m_socket;

      Server();
      ~Server();

      // Handed its profiler rather than reaching for it through the application
      // object. Networking is always real UDP; there is no in-process shortcut.
      void Initialise(Profiler* _profiler);

      // Drains everything the socket is holding into the inbox. Called at the
      // top of Advance; separate so that T7's transport seam has one place to
      // go in.
      void ReceiveDatagrams();

      std::unique_ptr<NetworkUpdate> GetNextLetter();

      void ReceiveLetter(std::unique_ptr<NetworkUpdate> update, std::string_view fromIP);
      void SendLetter(std::unique_ptr<ServerToClientLetter> letter);

      int GetClientId(char* _ip);
      void RegisterNewClient(char* _ip);
      void RemoveClient(char* _ip);
      void RegisterNewTeam(char* _ip, int _teamType, int _desiredTeamId);

      void AdvanceSender();
      void Advance();
  };
} // namespace Neuron
