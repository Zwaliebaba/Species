#pragma once

#include <vector>
#include "NeuronMath.h"
#include "NetworkUpdate.h"


#define SERVERTOCLIENTLETTER_BYTESTREAMSIZE 1024


// ****************************************************************************
//  Class ServerToClientLetter
// ****************************************************************************

class ServerToClientLetter
{
  public:
    // Scoped and int-backed for the same reason as NetworkUpdate::UpdateType;
    // the values are pinned by TheLetterTypeValuesAreTheProtocol.
    enum class LetterType : int
    {
      Invalid,
      HelloClient,
      GoodbyeClient,
      TeamAssign,
      Update
    };

    LetterType m_type; // If you add any new data here, remember to update the copy constructor
    unsigned char m_teamId;
    unsigned char m_teamType;
    int m_ip; // This tells you specifically which client gets the HelloClient or TeamAssign

    std::vector<NetworkUpdate*> m_updates;

  private:
    int m_clientId; // An index into the server's slot map of ServerToClient objects
    int m_sequenceId;

  public:
    ServerToClientLetter();
    ServerToClientLetter(ServerToClientLetter& copyMe);
    // _len is the datagram's real length, and it is now consulted: a letter that
    // claims more than _len holds comes back Invalid rather than reading past
    // the receive buffer.
    ServerToClientLetter(char const* _byteStream, int _len);

    void SetClientId(int _id);
    void SetType(LetterType _type);
    void SetSequenceId(int seqId);
    void SetTeamId(int teamId);
    void SetTeamType(int teamType);
    void SetIp(int ip);

    int GetClientId() const;
    int GetSequenceId() const;

    void AddUpdate(NetworkUpdate* _update);

    // False for anything that did not parse: a truncated datagram, a letter type
    // this build does not know, or an update count the datagram cannot back up.
    // The receiving end drops those rather than acting on a half-read letter's
    // sequence id.
    [[nodiscard]] bool IsValid() const { return m_type != LetterType::Invalid; }

    // Writes all the current data into a sequential byte stream suitable to
    // be stuffed into a UDP packet. Sets linearSize to be the stream length.
    // Do NOT DELETE the returned pointer - it is part of this object.
    char* GetByteStream(int* _linearSize);
};
