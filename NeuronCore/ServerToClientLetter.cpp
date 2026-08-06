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
    m_ip(0)
{
}


// *** Constructor
ServerToClientLetter::ServerToClientLetter(char const* _byteStream, int _len)
  : m_clientId(-1),
    m_type(LetterType::Invalid),
    m_sequenceId(0),
    m_teamId(0),
    m_teamType(0),
    m_ip(0)
{
  ByteReader reader(_byteStream, _len > 0 ? static_cast<size_t>(_len) : 0);

  const int rawType = reader.Read<int>();
  m_sequenceId = reader.Read<int>();

  // As in NetworkUpdate: switched on the value off the wire, so m_type only
  // ever holds a type this build knows, and stays Invalid otherwise.
  switch (static_cast<LetterType>(rawType))
  {
  case LetterType::HelloClient:
  case LetterType::GoodbyeClient:
    m_ip = reader.Read<int>();
    break;

  case LetterType::TeamAssign:
    m_teamId = reader.Read<unsigned char>();
    m_teamType = reader.Read<unsigned char>();
    m_ip = reader.Read<int>();
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
      // Read into a local and pushed only once it is whole, so a half-read
      // update never reaches the letter.
      NetworkUpdate update;
      if (!update.ReadByteStream(reader))
        return; // fewer updates than the header claimed: the letter is a lie

      m_updates.push_back(update);
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

// *** SetIp
void ServerToClientLetter::SetIp(int ip) { m_ip = ip; }

// *** AddUpdate
// The letter takes a copy, as it always did. What is gone is the copy being
// made by hand out of a raw new and a memcpy.
void ServerToClientLetter::AddUpdate(NetworkUpdate const& _update) { m_updates.push_back(_update); }

// *** Serialise
int ServerToClientLetter::Serialise(char* _buffer, int _capacity)
{
  ByteWriter writer(_buffer, _capacity > 0 ? static_cast<size_t>(_capacity) : 0);

  writer.Write<int>(static_cast<int>(m_type));
  writer.Write<int>(m_sequenceId);

  switch (m_type)
  {
  case LetterType::HelloClient:
  case LetterType::GoodbyeClient:
    writer.Write<int>(m_ip);
    break;

  case LetterType::TeamAssign:
    writer.Write<unsigned char>(m_teamId);
    writer.Write<unsigned char>(m_teamType);
    writer.Write<int>(m_ip);
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
