#include "pch.h"

#include "NetLib.h"

#include "Generic.h"


void IpToString(struct in_addr in, char *newip)
{
        sprintf ( newip, "%u.%u.%u.%u", in.S_un.S_un_b.s_b1,
                                        in.S_un.S_un_b.s_b2,
                                        in.S_un.S_un_b.s_b3,
                                        in.S_un.S_un_b.s_b4 );
}

int ConvertIPToInt(const char* _ip)
{
  ASSERT_TEXT(strlen(_ip) < 17, "IP address too long");
  char ipCopy[17];
  strcpy(ipCopy, _ip);
  int ipLen = strlen(ipCopy);

  for (int i = 0; i < ipLen; ++i)
  {
    if (ipCopy[i] == '.')
      ipCopy[i] = '\n';
  }

  int part1, part2, part3, part4;
  sscanf(ipCopy, "%d %d %d %d", &part1, &part2, &part3, &part4);

  int result = ((part4 & 0xff) << 24) + ((part3 & 0xff) << 16) + ((part2 & 0xff) << 8) + (part1 & 0xff);
  return result;
}

char* ConvertIntToIP(const int _ip)
{
  static char result[16];
  sprintf(result, "%d.%d.%d.%d", (_ip & 0x000000ff), (_ip & 0x0000ff00) >> 8, (_ip & 0x00ff0000) >> 16, (_ip & 0xff000000) >> 24);

  return result;
}
