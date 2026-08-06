#include "pch.h"

#include "Debug.h"

#include "ByteStream.h"
#include "ServerToClientLetter.h"


// *** Constructor
ServerToClientLetter::ServerToClientLetter()
  : m_clientId(-1),
    m_type(LetterType::Invalid),
    m_sequenceId(0),
    m_teamId(0),
    m_teamType(0),
    m_connectionId(-1),
    m_joinToken(0),
    m_updateBytes(0)
{
}


// *** Constructor
ServerToClientLetter::ServerToClientLetter(char const* _byteStream, int _len)
  : m_clientId(-1),
    m_type(LetterType::Invalid),
    m_sequenceId(0),
    m_teamId(0),
    m_teamType(0),
    m_connectionId(-1),
    m_joinToken(0),
    m_updateBytes(0)
{
  ByteReader reader(_byteStream, _len > 0 ? static_cast<size_t>(_len) : 0);

  // The frame first. Anything that is not this protocol at this version, or is
  // travelling the wrong way, is dropped without reading a byte of payload.
  if (ReadDatagramHeader(reader) != DatagramKind::ServerLetter)
    return;

  const int rawType = reader.Read<int>();
  m_sequenceId = reader.Read<int>();

  // As in NetworkUpdate: switched on the value off the wire, so m_type only
  // ever holds a type this build knows, and stays Invalid otherwise.
  switch (static_cast<LetterType>(rawType))
  {
  case LetterType::HelloClient:
    m_connectionId = reader.Read<int>();
    m_joinToken = reader.Read<int>();
    break;

  case LetterType::GoodbyeClient:
    m_connectionId = reader.Read<int>();
    break;

  case LetterType::TeamAssign:
    m_teamId = reader.Read<unsigned char>();
    m_teamType = reader.Read<unsigned char>();
    m_connectionId = reader.Read<int>();
    break;

  case LetterType::Update:
  {
    // NOTHING HERE TRUSTS numUpdates. It arrived on a socket, so it can be
    // negative, or larger than any datagram could carry — the DEBUG_ASSERT that
    // used to stand for checking it is not a check, and in Release it was not
    // even that. The count is a loop bound and no more: each update is read
    // through the same bounded reader, and a count the datagram cannot back up
    // leaves the letter Invalid rather than half-built.
    const int numUpdates = reader.Read<int>();
    if (numUpdates < 0)
      return;

    for (int i = 0; i < numUpdates; ++i)
    {
      // Read into a local and added only once it is whole, so a half-read
      // update never reaches the letter.
      NetworkUpdate update;
      if (!update.ReadByteStream(reader))
        return; // fewer updates than the header claimed: the letter is a lie

      // Through AddUpdate rather than push_back, so the running size stays
      // right — and so a datagram longer than one is allowed to be is refused
      // on the way in as well as on the way out. It cannot arrive over a socket
      // sized to MaxDatagramSize; it can arrive from a caller with a longer
      // buffer, which is exactly what a test is.
      if (!AddUpdate(update))
        return;
    }
    break;
  }

  case LetterType::Invalid:
  default:
    return;
  }

  if (!reader.Ok())
  {
    // Truncated. The letter is dropped whole rather than acted on: its sequence
    // id is what the client's inbox orders on and what it acks, so a letter
    // half-read out of a short datagram would advance both on made-up numbers.
    m_type = LetterType::Invalid;
    return;
  }

  m_type = static_cast<LetterType>(rawType);
}


// *** SetSequenceId
void ServerToClientLetter::SetSequenceId(int _id) { m_sequenceId = _id; }


// *** SetTeamType
void ServerToClientLetter::SetTeamType(int teamType) { m_teamType = teamType; }


// *** SetTeamId
void ServerToClientLetter::SetTeamId(int teamId) { m_teamId = teamId; }

// *** SetSequenceId
void ServerToClientLetter::SetClientId(int _id) { m_clientId = _id; }


// *** SetType
void ServerToClientLetter::SetType(LetterType _type) { m_type = _type; }


// *** GetClientId
int ServerToClientLetter::GetClientId() const { return m_clientId; }


// *** GetSequenceId
int ServerToClientLetter::GetSequenceId() const { return m_sequenceId; }

// *** SetConnectionId
void ServerToClientLetter::SetConnectionId(int _connectionId) { m_connectionId = _connectionId; }

void ServerToClientLetter::SetJoinToken(int _joinToken) { m_joinToken = _joinToken; }

// *** HeaderSize
int ServerToClientLetter::HeaderSize() const
{
  // The datagram frame is part of what a letter costs on the wire, so it is
  // part of what the cap is checked against. Leaving it out here would let a
  // letter be built four bytes too long to send.
  switch (m_type)
  {
  case LetterType::HelloClient:
    return DatagramHeaderSize + 4 * static_cast<int>(sizeof(int)); // type, sequence id, connection id, join token

  case LetterType::GoodbyeClient:
    return DatagramHeaderSize + 3 * static_cast<int>(sizeof(int)); // type, sequence id, connection id

  case LetterType::TeamAssign:
    return DatagramHeaderSize + 3 * static_cast<int>(sizeof(int)) + 2 * static_cast<int>(sizeof(unsigned char)); // ...plus team id and type

  case LetterType::Update:
    return DatagramHeaderSize + 3 * static_cast<int>(sizeof(int)); // type, sequence id, update count

  case LetterType::Invalid:
    break;
  }

  return DatagramHeaderSize + 2 * static_cast<int>(sizeof(int)); // type and sequence id, which every letter carries
}

// *** SerialisedSize
int ServerToClientLetter::SerialisedSize() const { return HeaderSize() + m_updateBytes; }

// *** AddUpdate
// The letter takes a copy, as it always did. What is gone is the copy being made
// by hand out of a raw new and a memcpy — and what is new is that it can refuse.
bool ServerToClientLetter::AddUpdate(NetworkUpdate const& _update)
{
  // Pushed first and popped back off if it does not fit, because measuring an
  // update means serialising it and NetworkUpdate serialises into a buffer of
  // its own — which the caller's const reference does not give access to.
  m_updates.push_back(_update);

  int updateSize = 0;
  m_updates.back().GetByteStream(&updateSize);

  if (SerialisedSize() + updateSize > MaxDatagramSize)
  {
    m_updates.pop_back();
    return false;
  }

  m_updateBytes += updateSize;
  return true;
}

// *** Serialise
int ServerToClientLetter::Serialise(char* _buffer, int _capacity)
{
  ByteWriter writer(_buffer, _capacity > 0 ? static_cast<size_t>(_capacity) : 0);
  WriteDatagramHeader(writer, DatagramKind::ServerLetter);

  writer.Write<int>(static_cast<int>(m_type));
  writer.Write<int>(m_sequenceId);

  switch (m_type)
  {
  case LetterType::HelloClient:
    writer.Write<int>(m_connectionId);
    writer.Write<int>(m_joinToken);
    break;

  case LetterType::GoodbyeClient:
    writer.Write<int>(m_connectionId);
    break;

  case LetterType::TeamAssign:
    writer.Write<unsigned char>(m_teamId);
    writer.Write<unsigned char>(m_teamType);
    writer.Write<int>(m_connectionId);
    break;

  case LetterType::Update:
  {
    const int numUpdates = static_cast<int>(m_updates.size());
    writer.Write<int>(numUpdates);

    for (int i = 0; i < numUpdates; ++i)
    {
      // Each update is flattened into its own buffer first and copied in as a
      // block, rather than written straight through this writer. That costs a
      // copy of at most 42 bytes and buys the thing T3 needs: whether the next
      // update fits is answerable BEFORE any of it is committed, so a full
      // letter can stop cleanly instead of leaving half an update behind it.
      int updateSize = 0;
      char const* updateBytes = m_updates[i].GetByteStream(&updateSize);
      writer.WriteBytes(updateBytes, static_cast<size_t>(updateSize));
    }
    break;
  }

  case LetterType::Invalid:
    break;
  }

  // A letter that did not fit reports nothing written rather than a truncated
  // datagram: the count in its header would otherwise promise more updates than
  // followed it, the receiver would drop the whole thing, and the server would
  // retransmit the same over-long letter forever. T3 is what stops the case
  // arising, by capping the letter as it is built.
  DEBUG_ASSERT(writer.Ok());
  if (!writer.Ok())
    return 0;

  return static_cast<int>(writer.BytesWritten());
}
