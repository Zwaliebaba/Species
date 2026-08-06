#pragma once

#include <vector>
#include "NeuronMath.h"
#include "NetworkUpdate.h"
#include "ProtocolLimits.h"


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

    LetterType m_type;
    unsigned char m_teamId;
    unsigned char m_teamType;
    int m_ip; // This tells you specifically which client gets the HelloClient or TeamAssign

    // By value. It held raw owning NetworkUpdate* and the class had no
    // destructor, so every Update letter the client received and every history
    // copy the server made leaked all of its updates — and the header used to
    // carry a "remember to update the copy constructor" warning for a copy
    // constructor that existed only to deep-copy these pointers. A vector of
    // values needs neither: the compiler's copy is the deep copy, and the
    // vector is the owner.
    std::vector<NetworkUpdate> m_updates;

  private:
    int m_clientId; // An index into the server's slot map of ServerToClient objects
    int m_sequenceId;

    // Running total of what m_updates costs on the wire, so the cap can be
    // checked before an update is committed rather than after the buffer has
    // already been written past.
    int m_updateBytes;

    // The bytes this letter's type costs before any updates. Kept in step with
    // Serialise by TheSerialisedSizeMatchesWhatSerialiseWrites, which is the
    // only thing stopping the two drifting apart.
    [[nodiscard]] int HeaderSize() const;

  public:
    ServerToClientLetter();
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

    // Returns false if the update did not fit, having added nothing. A letter
    // has to fit in one datagram — see MaxDatagramSize — and the caller keeps
    // what did not fit for the next one rather than dropping it.
    //
    // This is a runtime refusal, in Release as well as Debug. What it replaces
    // was a DEBUG_ASSERT after the fact, which in Release let the letter
    // overrun its buffer and in Debug only said so once the damage was done.
    [[nodiscard]] bool AddUpdate(NetworkUpdate const& _update);

    // How many bytes this letter would occupy on the wire right now.
    [[nodiscard]] int SerialisedSize() const;

    // False for anything that did not parse: a truncated datagram, a letter type
    // this build does not know, or an update count the datagram cannot back up.
    // The receiving end drops those rather than acting on a half-read letter's
    // sequence id.
    [[nodiscard]] bool IsValid() const { return m_type != LetterType::Invalid; }

    // Writes the letter into _buffer as one datagram and returns how many bytes
    // it used, or 0 if it did not fit.
    //
    // The buffer belongs to the caller. It used to be one file-scope
    // char[1024] shared by every letter in the process, so the bytes a letter
    // "returned" stayed valid only until the next letter was serialised — with
    // the sending loop being the only reason that ever held.
    int Serialise(char* _buffer, int _capacity);
};
