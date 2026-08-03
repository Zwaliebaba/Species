#include "pch.h"

#include "NetLib.h"

#include "Generic.h"


std::string IpToString(struct in_addr in)
{
  return std::format("{}.{}.{}.{}", in.S_un.S_un_b.s_b1, in.S_un.S_un_b.s_b2, in.S_un.S_un_b.s_b3, in.S_un.S_un_b.s_b4);
}

int ConvertIPToInt(const char* _ip)
{
  ASSERT_TEXT(strlen(_ip) < 17, "IP address too long");

  // The octet separators become newlines so one sscanf reads all four. The
  // copy exists because the input is const; it was a fixed char[17] guarded by
  // the assert above, which is the same bound the assert already states.
  std::string ipCopy(_ip);
  std::ranges::replace(ipCopy, '.', '\n');

  int part1, part2, part3, part4;
  sscanf(ipCopy.c_str(), "%d %d %d %d", &part1, &part2, &part3, &part4);

  int result = ((part4 & 0xff) << 24) + ((part3 & 0xff) << 16) + ((part2 & 0xff) << 8) + (part1 & 0xff);
  return result;
}

std::string ConvertIntToIP(const int _ip)
{
  // The masks are reproduced exactly. 0xff000000 does not fit in an int, so it
  // is an unsigned literal, which makes `_ip & 0xff000000` unsigned and its
  // shift logical rather than arithmetic — that is what makes the top octet of
  // a negative packed address come out as 255 instead of a sign-extended mess.
  // Pinned by ConvertIpToIntHandlesTheAllOnesBroadcast.
  return std::format("{}.{}.{}.{}", (_ip & 0x000000ff), (_ip & 0x0000ff00) >> 8, (_ip & 0x00ff0000) >> 16, (_ip & 0xff000000) >> 24);
}
