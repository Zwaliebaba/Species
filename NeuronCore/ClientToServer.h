#ifndef _CLIENTTOSERVER_H
#define _CLIENTTOSERVER_H

#include "NetworkSinks.h"
#include "LList.h"
#include "Vector3.h"

#include "WorldObject.h"
#include "Entity.h"


class NetLib;
class NetSocket;
class NetMutex;
class NetSocketListener;
class ServerToClientLetter;
class NetworkUpdate;


class ClientToServer : public ClientLetterSink
{
private:
	NetLib				*m_netLib;

  // A server hosted in this process, or null when talking over sockets.
  ServerUpdateSink* m_localServer;
  bool m_bypassNetworking;

  void AdvanceSender();

public:
    NetSocket           *m_sendSocket;
    NetSocketListener   *m_receiveSocket;

    NetMutex            *m_inboxMutex;
    NetMutex            *m_outboxMutex;
    LList               <ServerToClientLetter *> m_inbox;
    LList               <NetworkUpdate *> m_outbox;

    int                 m_lastValidSequenceIdFromServer;    // eg if we have 11,12,13,15,18 then this is 13

public:
  // Given its configuration rather than reading it off the application object.
  explicit ClientToServer(bool _bypassNetworking);
  void SetLocalServer(ServerUpdateSink* _server);
  ~ClientToServer();

  int GetOurIP_Int();
  char* GetOurIP_String();

  ServerToClientLetter* GetNextLetter();
  int GetNextLetterSeqID();

  void Advance();

  void ReceiveLetter(ServerToClientLetter* letter) override;
  void SendLetter(NetworkUpdate* letter);
  void ProcessServerUpdates(ServerToClientLetter* letter);

  void ClientJoin();
  void ClientLeave();
  void RequestTeam(int _teamType, int _desiredId);

  void RequestSelectUnit(unsigned char _teamId, int _unitId, int _entityId, int _buildingId);
  void RequestCreateUnit(unsigned char _teamId, unsigned char _troopType, int _numToCreate, int _buildingId);
  void RequestCreateUnit(unsigned char _teamId, unsigned char _troopType, int _numToCreate, Vector3 const& _pos);
  void RequestAimBuilding(unsigned char _teamId, int _buildingId, Vector3 const& _pos);
  void RequestToggleFence(int _buildingId);
  void RequestRunProgram(unsigned char _teamId, unsigned char _program);
  void RequestTargetProgram(unsigned char _teamId, unsigned char _program, Vector3 const& _pos);

  void SendSyncronisation(int _lastProcessedId, unsigned char _sync);
  void SendIAmAlive(unsigned char _teamId, TeamControls const& _teamControls);

  void RequestPause();
};


#endif
