#include "pch.h"
#include "NetLibWin32.h"

#include <ws2tcpip.h>

// Wrapper that provides the older hostent style interface while using
// getaddrinfo() internally to avoid deprecated API usage.
NetHostDetails* NetGetHostByName(const char *hostname)
{
    if (!hostname || !*hostname)
        return nullptr;

    static HOSTENT host;
    static char* addr_list[2];
    static unsigned long addr; // store IPv4 address in network byte order

    ZeroMemory(&host, sizeof(host));
    addr_list[0] = nullptr;
    addr_list[1] = nullptr;

    struct addrinfo hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET; // IPv4 only (matches original code expectations)

    struct addrinfo* res = nullptr;
    int rc = getaddrinfo(hostname, NULL, &hints, &res);
    if (rc != 0 || !res)
    {
        if (res)
            freeaddrinfo(res);
        return nullptr;
    }

    // Extract the first IPv4 address
    struct sockaddr_in* sa = (struct sockaddr_in*)res->ai_addr;
    addr = sa->sin_addr.s_addr;

    addr_list[0] = (char*)&addr;
    addr_list[1] = nullptr;

    host.h_addr_list = addr_list;
    host.h_addrtype = AF_INET;
    host.h_length = sizeof(addr);

    freeaddrinfo(res);
    return &host;
}
