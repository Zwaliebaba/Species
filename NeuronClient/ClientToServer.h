#pragma once

#include <memory>
#include <vector>

#include "TeamControls.h"
#include "NeuronMath.h"
#include "Transport.h"

// TeamControls.h is included rather than forward-declared: SendIAmAlive takes it
// by const reference, which a declaration would satisfy, but the declaration used
// to arrive transitively through Entity.h. WorldObject.h and Entity.h are gone —
// the game types they carried were only ever used by ProcessServerUpdates, which
// now lives in Species/ServerUpdates.cpp.

class NetLib;
class ServerToClientLetter;
class NetworkUpdate;


namespace Neuron
{
  class ClientToServer
  {
    private:
      std::unique_ptr<NetLib> m_netLib;

      // Where the server is. One socket does both directions now, so there is
      // no second socket to hold the answer.
      Endpoint m_serverEndpoint;

      void AdvanceSender();
      void ReceiveDatagrams();

    public:
      // Where datagrams go and come from, polled from the frame loop. It
      // replaces one socket for sending, a listener on a blocking thread for
      // receiving, and the two mutexes between them. The default
      // constructor builds a UdpTransport on an OS-chosen port; the other one
      // takes whatever it is given, which is how a test drives a client with no
      // socket in it.
      std::unique_ptr<Transport> m_transport;

      std::vector<std::unique_ptr<ServerToClientLetter>> m_inbox;
      std::vector<std::unique_ptr<NetworkUpdate>> m_outbox;

      int m_connectionId;

      // When a letter last arrived. Set at construction rather than left at
      // zero, so a client that never hears from the server at all — which is
      // the case most worth reporting — is measured from when it started.
      double m_lastHeardFromServer;

      // Chosen once, sent with ClientJoin, and echoed back in the HelloClient
      // that belongs to this client. Every client receives every letter, so
      // this is how one is told apart from another's.
      int m_joinToken;

      int m_lastValidSequenceIdFromServer; // eg if we have 11,12,13,15,18 then this is 13
      // When the client believes server sequence 0 happened, derived from the sequence id of every letter
      // that arrives. This was the g_startTime global in Species/Main.h, which only this class ever wrote.
      // A plain member: both the write and the read are the main thread now, so
      // the unsynchronised-access note that used to sit here describes nothing.
      double m_startTime;

    public:
      ClientToServer();
      ClientToServer(std::unique_ptr<Transport> _transport, Endpoint const& _serverEndpoint);
      ~ClientToServer();

      // Assigned by the server and learned from the HelloClient carrying this
      // client's join token. -1 until then.
      //
      // It replaces GetOurIP_Int, which returned a hard-coded 127.0.0.1 with a
      // "we're not doing networking for now" comment above it — and which
      // ProcessServerLetters compared every TeamAssign against, so on a real
      // network every client believed every team assignment was its own.
      [[nodiscard]] int GetConnectionId() const { return m_connectionId; }

      // True when nothing has arrived from the server for ServerSilenceTimeout.
      //
      // The client used to have no opinion about this at all: an empty inbox
      // and a server that had gone away look identical from here, so it sat
      // there with the simulation stalled and nothing to say about why. This is
      // the state to say it with — no caller acts on it yet, which is a gap
      // recorded in the task rather than an oversight.
      [[nodiscard]] bool IsServerSilent() const;
      [[nodiscard]] double TimeSinceServerHeard() const;

      // Releases the head of the inbox only when it is the letter the caller is
      // next expecting. The caller owns that counter — it tracks how far the
      // simulation has advanced, not how far the socket has.
      std::unique_ptr<ServerToClientLetter> GetNextLetter(int _lastProcessedSequenceId);
      int GetNextLetterSeqID();

      void Advance();

      // Both take ownership, which the signature now says rather than a comment.
      void ReceiveLetter(std::unique_ptr<ServerToClientLetter> letter);
      void SendLetter(std::unique_ptr<NetworkUpdate> letter);

      void ClientJoin();
      void ClientLeave();
      void RequestTeam(int _teamType, int _desiredId);

      void RequestSelectUnit(unsigned char _teamId, int _unitId, int _entityId, int _buildingId);
      void RequestCreateUnit(unsigned char _teamId, unsigned char _troopType, int _numToCreate, int _buildingId);
      void RequestCreateUnit(unsigned char _teamId, unsigned char _troopType, int _numToCreate, DirectX::XMFLOAT3 const& _pos);
      void RequestAimBuilding(unsigned char _teamId, int _buildingId, DirectX::XMFLOAT3 const& _pos);
      void RequestToggleFence(int _buildingId);
      void RequestRunProgram(unsigned char _teamId, unsigned char _program);
      void RequestTargetProgram(unsigned char _teamId, unsigned char _program, DirectX::XMFLOAT3 const& _pos);

      void SendSyncronisation(int _lastProcessedId, unsigned char _sync);
      void SendIAmAlive(unsigned char _teamId, TeamControls const& _teamControls);

      void RequestPause();
  };

  // Owned by App, which assigns this during startup. Declared here so the layers
  // below Species can reach the subsystem without including App.h — see
  // tasks/layering-inversion.yaml T8.
  extern ClientToServer* g_clientToServer;
} // namespace Neuron
