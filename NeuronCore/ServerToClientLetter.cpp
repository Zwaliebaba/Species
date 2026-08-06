#include "pch.h"

#include <string.h>

#include "Debug.h"

#include "ByteStream.h"
#include "ServerToClientLetter.h"


static char s_byteStream[SERVERTOCLIENTLETTER_BYTESTREAMSIZE];


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
ServerToClientLetter::ServerToClientLetter(ServerToClientLetter& copyMe)
  : m_clientId(copyMe.m_clientId),
    m_type(copyMe.m_type),
    m_sequenceId(copyMe.m_sequenceId),
    m_teamId(copyMe.m_teamId),
    m_teamType(copyMe.m_teamType),
    m_ip(copyMe.m_ip)
{
  // memcpy( m_updates, copyMe.m_updates, MAX_SERVER_TO_CLIENT_UPDATES*sizeof(NetworkUpdate) );

  for (int i = 0; i < static_cast<int>(copyMe.m_updates.size()); ++i)
  {
    NetworkUpdate* copyUpdate = copyMe.m_updates[i];
    NetworkUpdate* newUpdate = new NetworkUpdate();
    memcpy(newUpdate, copyUpdate, sizeof(NetworkUpdate));
    m_updates.push_back(newUpdate);
  }
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
      // Built on the stack and copied in by AddUpdate, so an update that fails
      // to read is never owned by anything and cannot leak on the way out.
      NetworkUpdate update;
      if (!update.ReadByteStream(reader))
        return; // fewer updates than the header claimed: the letter is a lie

      AddUpdate(&update);
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
void ServerToClientLetter::AddUpdate(NetworkUpdate* _update)
{
  // Make sure we COPY the update
  NetworkUpdate* update = new NetworkUpdate();
  memcpy(update, _update, sizeof(NetworkUpdate));
  m_updates.push_back(update);
}

// *** GetByteStream
char* ServerToClientLetter::GetByteStream(int* _linearSize)
{
  ByteWriter writer(s_byteStream, sizeof(s_byteStream));

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
      int updateSize = 0;
      char const* updateBytes = m_updates[i]->GetByteStream(&updateSize);
      writer.WriteBytes(updateBytes, static_cast<size_t>(updateSize));
    }
    break;
  }

  case LetterType::Invalid:
    break;
  }

  *_linearSize = static_cast<int>(writer.BytesWritten());

  // Still only a Debug check, and it is now a check that the buffer was big
  // enough rather than a report that it was not: an over-full letter loses its
  // tail instead of overrunning s_byteStream. Losing the tail is still wrong —
  // the count in the header would then exceed what follows, so the receiver
  // drops the whole letter and the server retransmits it unchanged, forever.
  // T3 is what makes the case unreachable, by capping the letter as it is
  // built.
  DEBUG_ASSERT(writer.Ok());

  return s_byteStream;
}
