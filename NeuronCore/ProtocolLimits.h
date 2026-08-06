#pragma once

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

// The ports. Named here rather than spelled at each socket, which is where they
// were: 4000 in the server's listen thread and 4001 in three separate places.
//
// ClientPort is on its way out. A client binds it today so the server has
// somewhere fixed to reply to, which is exactly what limits a host to one
// client and breaks through NAT — T9 replaces it with replying to the address
// the datagram actually came from.
inline constexpr unsigned short ServerPort = 4000;
inline constexpr unsigned short ClientPort = 4001;
