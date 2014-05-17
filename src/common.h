/* common.h - (C) 2013, Timo Buhrmester
 * ircutils - irc utils
 * See README for contact-, COPYING for license information. */

#ifndef IRCUTILS_COMMON_H
#define IRCUTILS_COMMON_H 1

#include <stdarg.h>
#include <netdb.h>

#include <libsrsirc/irc_basic.h>

#define COUNTOF(ARR) (sizeof (ARR) / sizeof *(ARR))

bool iprintf(ibhnd_t irc, const char *fmt, ...);
bool peeraddr(char *addr, size_t addr_sz, unsigned short *port, struct addrinfo *res);

struct ircsrv {
	

};

#endif /* IRCUTILS_COMMON_H */
