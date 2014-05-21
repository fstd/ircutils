#include <stdio.h>

#include <arpa/inet.h>

#include "debug.h"
#include "common.h"

bool
iprintf(irc irc, const char *fmt, ...)
{
	char buf[1024];
	va_list l;
	va_start(l, fmt);
	vsnprintf(buf, sizeof buf, fmt, l);
	va_end(l);
	return irc_write(irc, buf);
}

bool
peeraddr(char *addr, size_t addr_sz, unsigned short *port, struct addrinfo *res)
{
	if (res->ai_family == AF_INET) {
		struct sockaddr_in *sin = (struct sockaddr_in*)res->ai_addr;

		inet_ntop(AF_INET, &sin->sin_addr, addr, addr_sz);

		*port = ntohs(sin->sin_port);
	} else if (res->ai_family == AF_INET6) {
		struct sockaddr_in6 *sin = (struct sockaddr_in6*)res->ai_addr;

		inet_ntop(AF_INET6, &sin->sin6_addr, addr, addr_sz);

		*port = ntohs(sin->sin6_port);
	} else {
		W("non-inet[6] address family: %d", res->ai_family);
		return false;
	}

	return true;
}
