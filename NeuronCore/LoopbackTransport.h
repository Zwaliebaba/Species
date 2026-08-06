// ****************************************************************************
//  A wire made of memory, so the protocol can be tested without a socket.
// ****************************************************************************

#pragma once

#include <vector>

#include "Transport.h"

namespace Neuron
{
  // The switchboard. Datagrams are addressed to an endpoint and queued there
  // until whoever is bound to it takes them.
  //
  // NOT A RUNTIME SHORTCUT. Single player stays real UDP over loopback — that
  // is a recorded stance, and it is what makes the one-player case exercise the
  // same code path as the many-player case. This exists so a test can drive a
  // scripted client through join, hello, team assignment, sequenced updates and
  // retransmission, which no test in this tree has ever been able to do.
  //
  // Delivery is immediate, ordered and lossless, which real UDP is none of. It
  // is a fake wire for exercising the conversation, not for characterising the
  // network — Drop below is the one concession, because "an unacked letter is
  // retransmitted" cannot be tested on a wire that never loses anything.
  class LoopbackNetwork
  {
    public:
      class Datagram
      {
        public:
          Endpoint m_from;
          std::vector<char> m_bytes;
      };

      // Queues a datagram at _to, unless a drop has been armed for it.
      void Deliver(Endpoint const& _to, Endpoint const& _from, char const* _data, int _length);

      // Takes the oldest datagram queued at _at, or returns false.
      [[nodiscard]] bool Take(Endpoint const& _at, Datagram& _datagram);

      [[nodiscard]] int Queued(Endpoint const& _at) const;

      // Drops the next _count datagrams addressed to _to, then stops dropping.
      // This is what makes the retransmission window testable: the letter is
      // sent, never arrives, is never acknowledged, and has to come again.
      void DropNext(Endpoint const& _to, int _count);

      [[nodiscard]] int Dropped() const { return m_dropped; }

    private:
      class Mailbox
      {
        public:
          Endpoint m_address;
          std::vector<Datagram> m_queue;
          int m_dropsRemaining = 0;
      };

      // A vector with a linear search rather than a map, because Endpoint has no
      // ordering and this holds two or three entries. Iteration order is
      // insertion order, which keeps a failing test reproducible.
      Mailbox& MailboxFor(Endpoint const& _address);

      std::vector<Mailbox> m_mailboxes;
      int m_dropped = 0;
  };


  // One end of that wire, bound to an address.
  class LoopbackTransport : public Transport
  {
    public:
      LoopbackTransport(LoopbackNetwork& _network, Endpoint const& _address)
        : m_network(&_network),
          m_address(_address)
      {
      }

      [[nodiscard]] Endpoint const& Address() const { return m_address; }
      [[nodiscard]] int Sent() const { return m_sent; }

      [[nodiscard]] std::error_code Send(Endpoint const& _to, char const* _data, int _length) override;
      [[nodiscard]] bool TryReceive(char* _buffer, int _capacity, int& _bytesReceived, Endpoint& _from) override;

    private:
      LoopbackNetwork* m_network;
      Endpoint m_address;
      int m_sent = 0;
  };
} // namespace Neuron
