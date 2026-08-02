#ifndef NETWORK_GENERIC
#define NETWORK_GENERIC

void IpToString(struct in_addr in, char *newip);

// Dotted-quad <-> packed int. Lives here rather than on Server: these are string
// conversions with nothing to do with running a server, and the client needs
// them too (to name its own loopback address) without knowing Server exists.
int ConvertIPToInt(const char* _ip);
char* ConvertIntToIP(int _ip);

#endif // NETWORK_GENERIC
