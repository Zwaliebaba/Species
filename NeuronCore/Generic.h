#pragma once

#include <string>

// Formats a Winsock address as a dotted quad.
//
// It used to write into a caller-supplied char*, with no length in the
// signature and a 16-byte buffer at every call site by convention. Returning
// the string removes the convention.
std::string IpToString(struct in_addr in);

// Dotted-quad <-> packed int. Lives here rather than on Server: these are string
// conversions with nothing to do with running a server, and the client needs
// them too (to name its own loopback address) without knowing Server exists.
int ConvertIPToInt(const char* _ip);

// Returns by value. This used to hand back a pointer to a function-local
// `static char[16]`, which is neither reentrant nor safe to hold across a
// second call — two calls in one expression silently returned the same
// address twice. Nothing in the tree did that, but nothing stopped it either.
std::string ConvertIntToIP(int _ip);
