#pragma once

#include "ByteStream.h"

// Constants both sides of the wire must agree on.
//
// These lived in Species/Globals.h beside the renderer's frame budget and the
// gravity constant, which meant the server could not see them without depending
// on the game. They are protocol facts: change one and old and new builds stop
// agreeing about how many teams exist or how often the server ticks.
//
// Species/Globals.h includes this, so nothing that used those names had to change.

#define NUM_TEAMS 4
#define SERVER_ADVANCE_PERIOD 0.1f // Server ticks at this rate
#define SERVER_ADVANCE_FREQ 10.0f  // Server ticks at this rate
#define IAMALIVE_PERIOD 0.1f       // Clients must send IAmAlive this often

// THE SIZE OF A DATAGRAM. One number, used by the send path, the receive
// buffer, and the cap that stops a letter growing past it.
//
// It used to be two numbers that disagreed, and the disagreement was a
// permanent stall rather than a dropped packet. Letters were serialised into a
// 1024-byte buffer and received into a 512-byte one, so a busy tick produced a
// letter the receiver could not take. The client could then never acknowledge
// that sequence id, the server retransmits unacknowledged letters, and it
// retransmitted the same over-long letter forever — while every build stayed
// green and the game simply stopped advancing.
//
// 1024 rather than 512 because a full tick of updates already reaches roughly
// 1000 bytes, and rather than anything larger because this has to survive one
// IP datagram on an ordinary path: an Ethernet MTU is 1500, less 20 bytes of
// IP header and 8 of UDP. Growing it past that trades a bounded letter for
// fragmentation, where losing any fragment loses the whole letter.
inline constexpr int MaxDatagramSize = 1024;

// THE PORT. One, now: the server's.
//
// There was a second, 4001, which the client bound so the server had somewhere
// fixed to reply to. That single fact limited a host to one client and could
// not work through NAT. The server replies to the address a datagram came FROM,
// so a client binds port 0 and lets the OS choose.
inline constexpr unsigned short ServerPort = 4000;

// LIVENESS. A client that stops sending is disconnected after this many server
// ticks — ten, which at SERVER_ADVANCE_PERIOD is ten times IAMALIVE_PERIOD, the
// rate a live client sends at.
//
// Counted in TICKS rather than seconds, deliberately: the server has no clock
// in Advance, a tick count is exactly what "ten IAmAlive periods" means when
// the two periods are equal, and it makes the timeout reachable by a test
// without one. There was NO timeout at all before — RemoveClient fired only on
// a graceful ClientLeave, so a client that crashed was retransmitted to forever
// while its history grew without bound.
inline constexpr int ClientTimeoutTicks = 10;

// The other direction, and this one IS in seconds because the client has a
// clock and no tick of its own. A client that has heard nothing for this long
// has lost the server; it used to advance its simulation forward on an empty
// inbox indefinitely and never say so.
inline constexpr double ServerSilenceTimeout = 5.0;

// ****************************************************************************
//  THE DATAGRAM FRAME. Four bytes in front of everything that goes out.
// ****************************************************************************
//
// Without it, the first four bytes of ANY datagram arriving on the port are
// read as a message type. A stray packet, a port scan, a client from a
// different build or a reply that outlived the session it belonged to all get
// interpreted rather than dropped, and the bounded reader added in T1 is what
// stops that being a crash rather than what stops it happening.
//
// PROTOCOL 2 DOES NOT INTEROPERATE WITH PROTOCOL 1. That is the point of doing
// it now: there are no deployed builds to protect, and there will never be a
// cheaper moment to spend the compatibility break.
//
// The kind byte says which direction the datagram is going. It costs nothing
// and it means a server handed a server-to-client letter — which is what
// happens the moment somebody points two servers at each other, or a client at
// itself — drops it instead of parsing a letter type as an update type.

inline constexpr unsigned char ProtocolMagic0 = 'S';
inline constexpr unsigned char ProtocolMagic1 = 'P';
inline constexpr unsigned char ProtocolVersion = 2;

enum class DatagramKind : unsigned char
{
  Invalid = 0,
  ClientUpdate = 1, // one NetworkUpdate, client to server
  ServerLetter = 2  // one ServerToClientLetter, server to client
};

inline constexpr int DatagramHeaderSize = 4;

inline void WriteDatagramHeader(Neuron::ByteWriter& _writer, DatagramKind _kind)
{
  _writer.Write<unsigned char>(ProtocolMagic0);
  _writer.Write<unsigned char>(ProtocolMagic1);
  _writer.Write<unsigned char>(ProtocolVersion);
  _writer.Write<unsigned char>(static_cast<unsigned char>(_kind));
}

// Returns the kind the datagram claims to be, or Invalid for anything that is
// not this protocol at this version. Silent by design: a receiver that logged
// every foreign datagram would hand anyone who can reach the port a way to
// fill a disk.
[[nodiscard]] inline DatagramKind ReadDatagramHeader(Neuron::ByteReader& _reader)
{
  const unsigned char magic0 = _reader.Read<unsigned char>();
  const unsigned char magic1 = _reader.Read<unsigned char>();
  const unsigned char version = _reader.Read<unsigned char>();
  const unsigned char kind = _reader.Read<unsigned char>();

  if (!_reader.Ok() || magic0 != ProtocolMagic0 || magic1 != ProtocolMagic1 || version != ProtocolVersion)
    return DatagramKind::Invalid;

  if (kind != static_cast<unsigned char>(DatagramKind::ClientUpdate) && kind != static_cast<unsigned char>(DatagramKind::ServerLetter))
    return DatagramKind::Invalid;

  return static_cast<DatagramKind>(kind);
}
