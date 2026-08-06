
/*
 * NETWORK UPDATE
 *
 * Represents a single event that occured and must be transmitted
 * Could be Client->Server or Server->Client
 *
 */


#pragma once

#include "ByteStream.h"
#include "TeamControls.h"
#include "NeuronMath.h"
#include "UdpSocket.h"

#define NETWORKUPDATE_BYTESTREAMSIZE 42

// The protocol carries the entity-type tag as an opaque byte. It deliberately
// does not know GameLogic's Entity::Type enum — only that zero means "none".
// GameLogic maps its enum onto this at the boundary, and the enum's TypeInvalid
// is 0 to match.
const unsigned char EntityTypeInvalid = 0;


class NetworkUpdate
{
  public:
    // enum class with an explicit int: these values go out as a four-byte write
    // and come back as a four-byte read, so the underlying type is the wire's
    // and the numbers are pinned by
    // NetworkUpdateTests::TheUpdateTypeValuesAreTheProtocol.
    enum class UpdateType : int
    {
      Invalid,          // Unused
      ClientJoin,       // New Client connects
      ClientLeave,      // Client disconnects
      RequestTeam,      // Client requests a new team, either virtual player or real
      Alive,            // Updates Avatar position
      SelectUnit,       // A player selected a unit
      CreateUnit,       // A player issued instructions to a factory
      AimBuilding,      // A player is aiming something like a radar dish
      ToggleLaserFence, // A laser fence is toggled
      RunProgram,       // A player wishes to run a program
      TargetProgram,    // A player targets a running program
      Pause,
      Syncronise // Performs a check to make sure we're in sync
    };

    UpdateType m_type;

    // Where this update came from. NOT on the wire — the server stamps it from
    // the datagram's source address on receive, and it is how a client is
    // identified from that point on. It replaces a char[16] holding a dotted
    // quad, which could name only one client per host and could not survive
    // NAT: two players behind one router are one address and two ports.
    Neuron::Endpoint m_source;

    // Chosen by the client, echoed by the server in HelloClient, and how a
    // client picks its own welcome out of a stream every client receives. See
    // the note in ServerToClientLetter.h.
    int m_joinToken;
    unsigned char m_teamType;
    signed char m_desiredTeamId; // Used by the server host to force AI players to have a specific ID. -1 if we don't care
    int m_unitId;
    int m_entityId;
    int m_buildingId;
    unsigned char m_entityType;
    int m_numTroops;
    unsigned char m_teamId;
    float m_radius;
    float m_yaw, m_dive;
    float m_power;
    unsigned char m_program;
    TeamControls m_teamControls;

    int m_lastProcessedSeqId; // Used for sync checks
    unsigned char m_sync;

    int m_lastSequenceId;

    char m_byteStream[NETWORKUPDATE_BYTESTREAMSIZE];

  public:
    NetworkUpdate();
    // From a whole DATAGRAM: the four-byte frame, then the payload. _len is the
    // number of bytes the datagram actually carries, and an update that does
    // not fit inside it — or that is not this protocol at this version, or is
    // going the wrong way — comes back Invalid.
    NetworkUpdate(char const* _datagram, int _len);

    void SetType(UpdateType _type);
    void SetJoinToken(int _token);
    void SetTeamType(unsigned char _teamType);
    void SetDesiredTeamId(signed char _desiredTeamId);
    void SetWorldPos(DirectX::XMFLOAT3 const& _pos);
    void SetTeamControls(TeamControls const& _teamControls);
    void SetTeamId(unsigned char _teamId);
    void SetEntityType(unsigned char _type);
    void SetNumTroops(int _numTroops);
    void SetRadius(float _radius);
    void SetUnitId(int _unitId);
    void SetEntityId(int _entityId);
    void SetBuildingID(int _buildingId);
    void SetYaw(float _yaw);
    void SetDive(float _dive);
    void SetPower(float _power);
    void SetProgram(unsigned char _prog);
    void SetLastProcessedId(int _lastProcessedId);
    void SetSync(unsigned char _sync);

    void SetLastSequenceId(int _lastSequenceId);

    const DirectX::XMFLOAT3& GetWorldPos() const;
    DirectX::XMFLOAT3& GetWorldPos();

    // Reads one update out of a reader shared with whatever contains it, so a
    // letter carrying several of them stays bounded by the one datagram length.
    // Returns false — and leaves m_type Invalid — for a truncated or
    // unrecognised update; the reader's cursor is then not to be trusted, so a
    // caller reading a sequence of them stops at the first false.
    bool ReadByteStream(Neuron::ByteReader& _reader);

    // The PAYLOAD, unframed. This is what a letter carries several of, so the
    // frame is deliberately not here: an update nested inside a letter is not a
    // datagram and does not get a datagram's header.
    char* GetByteStream(int* _linearSize);

    // The whole datagram — frame and payload — into the caller's buffer.
    // Returns the bytes written, or 0 if it did not fit. This is what the
    // client actually sends.
    [[nodiscard]] int SerialiseDatagram(char* _buffer, int _capacity);

    [[nodiscard]] bool IsValid() const { return m_type != UpdateType::Invalid; }

    // No hand-written copy assignment. It was memcpy(this, &n, sizeof(*this)),
    // which is what the implicit one does for a class of scalars anyway — and
    // declaring it stopped the type being trivially copyable, which is the
    // property ServerToClientLetter's vector of these now relies on.

    //    void SendToDebugStream(FILE *_out, int _seqNum);
};

// Inlines
#include "NetworkUpdate.inc"
