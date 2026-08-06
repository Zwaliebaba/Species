#include "pch.h"

#include "LoopbackTransport.h"
#include "ProtocolLimits.h"

namespace Neuron
{
  LoopbackNetwork::Mailbox& LoopbackNetwork::MailboxFor(Endpoint const& _address)
  {
    for (Mailbox& mailbox : m_mailboxes)
    {
      if (mailbox.m_address == _address)
        return mailbox;
    }

    Mailbox created;
    created.m_address = _address;
    m_mailboxes.push_back(created);
    return m_mailboxes.back();
  }

  void LoopbackNetwork::Deliver(Endpoint const& _to, Endpoint const& _from, char const* _data, int _length)
  {
    if (!_data || _length <= 0)
      return;

    Mailbox& mailbox = MailboxFor(_to);

    if (mailbox.m_dropsRemaining > 0)
    {
      --mailbox.m_dropsRemaining;
      ++m_dropped;
      return;
    }

    // Truncated at the cap rather than delivered whole, because that is what a
    // real socket does with an oversized datagram — and a fake wire that
    // delivers what the real one would not is a test that passes for the wrong
    // reason. Nothing should ever produce one; T3's cap is what makes sure.
    const int carried = _length > MaxDatagramSize ? MaxDatagramSize : _length;

    Datagram datagram;
    datagram.m_from = _from;
    datagram.m_bytes.assign(_data, _data + carried);
    mailbox.m_queue.push_back(std::move(datagram));
  }

  bool LoopbackNetwork::Take(Endpoint const& _at, Datagram& _datagram)
  {
    Mailbox& mailbox = MailboxFor(_at);
    if (mailbox.m_queue.empty())
      return false;

    _datagram = std::move(mailbox.m_queue.front());
    mailbox.m_queue.erase(mailbox.m_queue.begin());
    return true;
  }

  int LoopbackNetwork::Queued(Endpoint const& _at) const
  {
    for (Mailbox const& mailbox : m_mailboxes)
    {
      if (mailbox.m_address == _at)
        return static_cast<int>(mailbox.m_queue.size());
    }

    return 0;
  }

  void LoopbackNetwork::DropNext(Endpoint const& _to, int _count) { MailboxFor(_to).m_dropsRemaining = _count; }


  std::error_code LoopbackTransport::Send(Endpoint const& _to, char const* _data, int _length)
  {
    if (!_data || _length <= 0)
      return std::make_error_code(std::errc::invalid_argument);

    m_network->Deliver(_to, m_address, _data, _length);
    ++m_sent;
    return std::error_code();
  }

  bool LoopbackTransport::TryReceive(char* _buffer, int _capacity, int& _bytesReceived, Endpoint& _from)
  {
    _bytesReceived = 0;
    _from = Endpoint();

    if (!_buffer || _capacity <= 0)
      return false;

    LoopbackNetwork::Datagram datagram;
    if (!m_network->Take(m_address, datagram))
      return false;

    // A datagram longer than the buffer offered is dropped, not truncated into
    // it — Winsock reports WSAEMSGSIZE and the payload is gone, and UdpSocket
    // skips it and carries on. Delivering a partial one here would let a test
    // pass on bytes no real receiver would ever see.
    if (static_cast<int>(datagram.m_bytes.size()) > _capacity)
      return false;

    _bytesReceived = static_cast<int>(datagram.m_bytes.size());
    std::memcpy(_buffer, datagram.m_bytes.data(), datagram.m_bytes.size());
    _from = datagram.m_from;
    return true;
  }
} // namespace Neuron
