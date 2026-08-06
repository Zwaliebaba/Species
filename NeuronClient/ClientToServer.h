#pragma once

#include <memory>
#include <vector>

#include "TeamControls.h"
#include "NeuronMath.h"
#include "UdpSocket.h"

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
      // One socket, bound and polled from the frame loop. It replaces a
      // NetSocket for sending, a NetSocketListener on a blocking thread for
      // receiving, and the two mutexes between them.
      UdpSocket m_socket;

      std::vector<std::unique_ptr<ServerToClientLetter>> m_inbox;
      std::vector<std::unique_ptr<NetworkUpdate>> m_outbox;

      int m_lastValidSequenceIdFromServer; // eg if we have 11,12,13,15,18 then this is 13
      // When the client believes server sequence 0 happened, derived from the sequence id of every letter
      // that arrives. This was the g_startTime global in Species/Main.h, which only this class ever wrote.
      // A plain member: both the write and the read are the main thread now, so
      // the unsynchronised-access note that used to sit here describes nothing.
      double m_startTime;

    public:
      ClientToServer();
      ~ClientToServer();

      int GetOurIP_Int();

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
