#include "pch.h"

#include <string.h>

#include "Debug.h"

#include "ByteStream.h"
#include "NetworkUpdate.h"
#include "ProtocolLimits.h"


NetworkUpdate::NetworkUpdate()
  : m_type(UpdateType::Invalid),
    m_lastSequenceId(-1),
    m_radius(0.0f),
    m_teamId(255),
    m_entityType(EntityTypeInvalid),
    m_numTroops(0),
    m_unitId(0),
    m_buildingId(-1),
    m_sync(0)
{
}

NetworkUpdate::NetworkUpdate(char const* _datagram, int _len)
  : NetworkUpdate()
{
  // Delegating rather than repeating the member initialisers, which the old
  // stream constructor did not run at all: an update parsed off the wire left
  // every field its type does not carry — m_radius, m_yaw, m_program — reading
  // whatever the stack held.
  ByteReader reader(_datagram, _len > 0 ? static_cast<size_t>(_len) : 0);

  // The frame first, and nothing else is read if it is not ours. A datagram
  // carrying a SERVER letter is refused here too: the kinds are separate so
  // that a letter's type field can never be read as an update's.
  if (ReadDatagramHeader(reader) != DatagramKind::ClientUpdate)
  {
    m_type = UpdateType::Invalid;
    return;
  }

  ReadByteStream(reader);
}

int NetworkUpdate::SerialiseDatagram(char* _buffer, int _capacity)
{
  ByteWriter writer(_buffer, _capacity > 0 ? static_cast<size_t>(_capacity) : 0);
  WriteDatagramHeader(writer, DatagramKind::ClientUpdate);

  int payloadSize = 0;
  char const* payload = GetByteStream(&payloadSize);
  writer.WriteBytes(payload, static_cast<size_t>(payloadSize));

  if (!writer.Ok())
    return 0;

  return static_cast<int>(writer.BytesWritten());
}

bool NetworkUpdate::ReadByteStream(ByteReader& _reader)
{
  const int rawType = _reader.Read<int>();
  m_lastSequenceId = _reader.Read<int>();

  // Switched on the value off the wire rather than on m_type, so that m_type is
  // only ever set to a type this build recognises. It used to be assigned first
  // and the switch run against it, which meant a datagram carrying any integer
  // at all left an update claiming to be that type.
  switch (static_cast<UpdateType>(rawType))
  {
  case UpdateType::ClientJoin:
  case UpdateType::ClientLeave:
  case UpdateType::Pause:
    // Header only. Pause is written the same way and has to be listed here
    // explicitly now that an unlisted type means "not this protocol" — it used
    // to fall out of the switch and work by accident.
    break;

  case UpdateType::RequestTeam:
    m_teamType = _reader.Read<unsigned char>();
    m_entityType = _reader.Read<unsigned char>();
    m_desiredTeamId = _reader.Read<signed char>();
    break;

  case UpdateType::Alive:
    m_teamId = _reader.Read<unsigned char>();
    GetWorldPos().x = _reader.Read<float>();
    GetWorldPos().y = _reader.Read<float>();
    GetWorldPos().z = _reader.Read<float>();
    m_teamControls.SetFlags(_reader.Read<unsigned short>());
    m_sync = _reader.Read<unsigned char>();
    break;

  case UpdateType::Syncronise:
    m_lastProcessedSeqId = _reader.Read<int>();
    m_sync = _reader.Read<unsigned char>();
    break;

  case UpdateType::SelectUnit:
    m_teamId = _reader.Read<unsigned char>();
    m_unitId = _reader.Read<int>();
    m_entityId = _reader.Read<int>();
    m_buildingId = _reader.Read<int>();
    break;

  case UpdateType::CreateUnit:
    m_teamId = _reader.Read<unsigned char>();
    m_entityType = _reader.Read<unsigned char>();
    m_numTroops = _reader.Read<int>();
    m_buildingId = _reader.Read<int>();
    GetWorldPos().x = _reader.Read<float>();
    GetWorldPos().y = _reader.Read<float>();
    GetWorldPos().z = _reader.Read<float>();
    break;

  case UpdateType::AimBuilding:
    m_teamId = _reader.Read<unsigned char>();
    m_buildingId = _reader.Read<int>();
    GetWorldPos().x = _reader.Read<float>();
    GetWorldPos().y = _reader.Read<float>();
    GetWorldPos().z = _reader.Read<float>();
    break;

  case UpdateType::ToggleLaserFence:
    m_buildingId = _reader.Read<int>();
    break;

  case UpdateType::RunProgram:
    m_teamId = _reader.Read<unsigned char>();
    m_program = _reader.Read<unsigned char>();
    break;

  case UpdateType::TargetProgram:
    m_teamId = _reader.Read<unsigned char>();
    m_program = _reader.Read<unsigned char>();
    GetWorldPos().x = _reader.Read<float>();
    GetWorldPos().y = _reader.Read<float>();
    GetWorldPos().z = _reader.Read<float>();
    break;

  case UpdateType::Invalid:
  default:
    // Deliberately NOT a DEBUG_ASSERT, which is what stood here. Every byte
    // reaching this switch came off a socket, so asserting on a type this build
    // does not know hands anyone who can send this port a datagram a way to
    // stop a Debug build.
    m_type = UpdateType::Invalid;
    return false;
  }

  if (!_reader.Ok())
  {
    // Truncated: the fields above read past the end of the datagram and came
    // back zeroed. Half an update is not an update.
    m_type = UpdateType::Invalid;
    return false;
  }

  m_type = static_cast<UpdateType>(rawType);
  return true;
}

void NetworkUpdate::SetType(UpdateType _type) { m_type = _type; }

void NetworkUpdate::SetClientIp(std::string_view ip)
{
  // m_clientIp stays a char[16] and this stays a copy into it. The type has to
  // remain trivially copyable: ServerToClientLetter holds a vector of these by
  // value, and network-transport T2 removed the hand-written copy assignment
  // precisely so it would be. Converting the field to std::string is a change
  // to what the letter's storage costs to copy, not a tidy-up.
  //
  // What does change is that the copy is bounded. The old unbounded copy
  // ran off the end of a 16-byte field for anything longer than a dotted
  // quad; today every caller passes an address the UDP layer produced, but
  // nothing in the signature said so.
  // Written out rather than with std::min, which does not compile anywhere
  // MathUtils.h is reachable: it defines function-style min and max macros, so
  // `std::min(` becomes `std::(...)(` and MSVC reports C2589. NOMINMAX
  // suppresses the Windows pair; nothing suppresses ours. See
  // tasks/language-hygiene.yaml T8.
  size_t length = ip.size();
  if (length > sizeof(m_clientIp) - 1)
  {
    length = sizeof(m_clientIp) - 1;
  }
  std::memcpy(m_clientIp, ip.data(), length);
  m_clientIp[length] = '\0';
}

void NetworkUpdate::SetTeamType(unsigned char _teamType) { m_teamType = _teamType; }

void NetworkUpdate::SetDesiredTeamId(signed char _desiredTeamId) { m_desiredTeamId = _desiredTeamId; }

void NetworkUpdate::SetWorldPos(DirectX::XMFLOAT3 const& _pos)
{
  // Shared with m_teamControls
  GetWorldPos() = _pos;
}

void NetworkUpdate::SetTeamControls(TeamControls const& _teamControls) { m_teamControls = _teamControls; }

void NetworkUpdate::SetTeamId(unsigned char _teamId) { m_teamId = _teamId; }

void NetworkUpdate::SetEntityType(unsigned char _type) { m_entityType = _type; }

void NetworkUpdate::SetNumTroops(int _numTroops) { m_numTroops = _numTroops; }

void NetworkUpdate::SetRadius(float _radius) { m_radius = _radius; }


void NetworkUpdate::SetLastSequenceId(int _lastSequenceId) { m_lastSequenceId = _lastSequenceId; }

void NetworkUpdate::SetUnitId(int _unitId) { m_unitId = _unitId; }

void NetworkUpdate::SetEntityId(int _entityId) { m_entityId = _entityId; }

void NetworkUpdate::SetBuildingID(int _buildingId) { m_buildingId = _buildingId; }

void NetworkUpdate::SetYaw(float _yaw) { m_yaw = _yaw; }

void NetworkUpdate::SetDive(float _dive) { m_dive = _dive; }

void NetworkUpdate::SetPower(float _power) { m_power = _power; }

void NetworkUpdate::SetProgram(unsigned char _prog) { m_program = _prog; }

void NetworkUpdate::SetLastProcessedId(int _lastProcessedId) { m_lastProcessedSeqId = _lastProcessedId; }

void NetworkUpdate::SetSync(unsigned char _sync) { m_sync = _sync; }

char* NetworkUpdate::GetByteStream(int* _linearSize)
{
  ByteWriter writer(m_byteStream, sizeof(m_byteStream));

  writer.Write<int>(static_cast<int>(m_type));
  writer.Write<int>(m_lastSequenceId);

  switch (m_type)
  {
  case UpdateType::ClientJoin:
  case UpdateType::ClientLeave:
  case UpdateType::Pause:
    break;

  case UpdateType::RequestTeam:
    writer.Write<unsigned char>(m_teamType);
    writer.Write<unsigned char>(m_entityType);
    writer.Write<signed char>(m_desiredTeamId);
    break;

  case UpdateType::Alive:
    writer.Write<unsigned char>(m_teamId);
    writer.Write<float>(GetWorldPos().x);
    writer.Write<float>(GetWorldPos().y);
    writer.Write<float>(GetWorldPos().z);
    writer.Write<unsigned short>(m_teamControls.GetFlags());
    writer.Write<unsigned char>(m_sync);
    break;

  case UpdateType::Syncronise:
    writer.Write<int>(m_lastProcessedSeqId);
    writer.Write<unsigned char>(m_sync);
    break;

  case UpdateType::SelectUnit:
    writer.Write<unsigned char>(m_teamId);
    writer.Write<int>(m_unitId);
    writer.Write<int>(m_entityId);
    writer.Write<int>(m_buildingId);
    break;

  case UpdateType::CreateUnit:
    writer.Write<unsigned char>(m_teamId);
    writer.Write<unsigned char>(m_entityType);
    writer.Write<int>(m_numTroops);
    writer.Write<int>(m_buildingId);
    writer.Write<float>(GetWorldPos().x);
    writer.Write<float>(GetWorldPos().y);
    writer.Write<float>(GetWorldPos().z);
    break;

  case UpdateType::AimBuilding:
    writer.Write<unsigned char>(m_teamId);
    writer.Write<int>(m_buildingId);
    writer.Write<float>(GetWorldPos().x);
    writer.Write<float>(GetWorldPos().y);
    writer.Write<float>(GetWorldPos().z);
    break;

  case UpdateType::ToggleLaserFence:
    writer.Write<int>(m_buildingId);
    break;

  case UpdateType::RunProgram:
    writer.Write<unsigned char>(m_teamId);
    writer.Write<unsigned char>(m_program);
    break;

  case UpdateType::TargetProgram:
    writer.Write<unsigned char>(m_teamId);
    writer.Write<unsigned char>(m_program);
    writer.Write<float>(GetWorldPos().x);
    writer.Write<float>(GetWorldPos().y);
    writer.Write<float>(GetWorldPos().z);
    break;

  case UpdateType::Invalid:
    // Still an assert on this side, unlike the reading one: nothing hostile can
    // reach here. An Invalid update being serialised is this build asking to
    // send a packet it never filled in.
    DEBUG_ASSERT(false);
  }

  *_linearSize = static_cast<int>(writer.BytesWritten());
  DEBUG_ASSERT(writer.Ok());
  return m_byteStream;
}


// void NetworkUpdate::SendToDebugStream(FILE *_out, int _seqNum)
//{
//     _ostr << _seqNum << ": ";
//
//     switch (m_type)
//     {
//     case Invalid:
//         _ostr << "Invalid: ";
//         break;
//     case ClientJoin:
//         _ostr << "Client Join: ";
//         break;
//     case RequestTeam:
//         _ostr << "Request Team: ";
//         break;
//     case Alive:
//         _ostr << "Alive: ";
//         break;
//     case SelectUnit:
//         _ostr << "SelectUnit:" ;
//         break;
//     }
//     _ostr << "\n";
// }
