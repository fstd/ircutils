/* netinfo.c - (C) 2013, Timo Buhrmester
 * ircutils - irc utils
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#define WITH_SSL
#include <libsrsirc/irc_ext.h>
#include <libsrsirc/util.h>
#undef WITH_SSL

#include "../debug.h"
#include "../common.h"

#define CONTO_SOFT 120000000 //unit: us
#define CONTO_HARD 300000000 //unit: us
#define ALL_TO 1800 //unit: s
#define DEF_PORT 6667

static char *s_srvhost = NULL;
static char *s_srvpw = NULL;

#include "names.inc.c" // defines `nicks`, `unames` and `fnames`

static void process_args(int *argc, char ***argv);
static void init(int *argc, char ***argv);
static void usage(FILE *str, const char *a0, int ec);
static void randstr(char *dst, size_t dst_sz);


static void
process_args(int *argc, char ***argv)
{
	char *a0 = (*argv)[0];

	const char *opts = "vqcCh";
	for(int ch; (ch = getopt(*argc, *argv, opts)) != -1;) {
		switch (ch) {
		case 'v':
			setloglvl(getloglvl()+1);
			break;
		case 'q':
			setloglvl(getloglvl()-1);
			break;
		case 'c':
		case 'C':
			logfancy(ch == 'c');
			break;
		case 'h':
			usage(stdout, a0, EXIT_SUCCESS);
			break;
		case '?':
		default:
			usage(stderr, a0, EXIT_FAILURE);
		}
	}

	/* after the switches, we take one mandatory server host specifier
	 * and one optional server password.  Nothing more, nothing less */
	*argc -= optind; *argv += optind;

	if (*argc < 1)
		E("no server specified");

	s_srvhost = strdup((*argv)[0]);

	--*argc; ++*argv;

	if (*argc > 0) {
		s_srvpw = strdup((*argv)[0]);
		--*argc; ++*argv;
	}

	if (*argc != 0)
		E("too many arguments given");
}


static void
init(int *argc, char ***argv)
{
	/* auto-color by default, overriden by arguments */
	logfancy(isatty(fileno(stderr)));

	process_args(argc, argv);
	D("initialized, server '%s'%s", s_srvhost, s_srvpw?" (w/ pw)":"");
}


static void
usage(FILE *str, const char *a0, int ec)
{
	#define I(STR) fputs(STR "\n", str)
	I("==================================================");
	I("== netinfo - collect information on irc network ==");
	I("==================================================");
	fprintf(str, "usage: %s [-h]\n", a0);
	I("");
	I("\t-h: Display brief usage statement and terminate");
	I("");
	I("(C) 2013, Timo Buhrmester (contact: #fstd on irc.freenode.org)");
	#undef I
	exit(ec);
}


bool conread_cb(tokarr *msg, void *tag);


int
main(int argc, char **argv)
{
	srand(time(NULL));
	init(&argc, &argv);

	irc *ictx = irc_init();
	irc_regcb_conread(ictx, conread_cb, NULL);

	char host[256];
	unsigned short port;
	char portstr[6];
	bool ssl = false;

	lsi_ut_parse_hostspec(host, sizeof host, &port, &ssl, s_srvhost);
	if (!port)
		port = DEF_PORT;
	
	snprintf(portstr, sizeof portstr, "%hu", port);

	struct addrinfo hints, *res, *res0;
	int error;
	bool success = false;
	//const char *cause = NULL;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	error = getaddrinfo(host, portstr, &hints, &res0);
	printf("GIVENHOST %s:%hu%s\n", host, port, ssl?"/ssl":"");
	if (error) {
		printf("CANTRESOLVE\n");
		E("%s: %s:%s", gai_strerror(error), host, portstr);
	}

	char paddr[64] = "(non-INET/INET6)";
	unsigned short peerport = 0;
	int acount = 0;
	for (res = res0; res; res = res->ai_next) {
		acount++;
		if (peeraddr(paddr, sizeof paddr, &peerport, res))
			printf("ADDR %s:%hu\n", paddr, peerport);
	}

	printf("NUMADDRS %d\n", acount);

	size_t nameid = rand() % COUNTOF(nicks);
	printf("MYNICK %s\n", nicks[nameid]);
	printf("MYUNAME %s\n", unames[nameid]);
	printf("MYFNAME %s\n", fnames[nameid]);
	for (res = res0; res; res = res->ai_next) {

		if (!peeraddr(paddr, sizeof paddr, &peerport, res)) {
			W("couldn't obtain peer addr from addrinfo!");
			continue;
		}

		irc_set_server(ictx, paddr, peerport);
		irc_set_pass(ictx, s_srvpw); //NULL == no password
		irc_set_ssl(ictx, ssl);
		irc_set_nick(ictx, nicks[nameid]);
		irc_set_uname(ictx, unames[nameid]);
		irc_set_fname(ictx, fnames[nameid]);
		irc_set_connect_timeout(ictx, CONTO_SOFT, CONTO_HARD);

		if (!irc_connect(ictx))
			W("couldn't connect/logon to '%s:%hu'", paddr, peerport);
		else {
			printf("ACTUALHOST %s:%hu\n", paddr, peerport);
			success = true;
			break;
		}

	}
	freeaddrinfo(res0);

	if (!success) {
		printf("CONFAIL\n");
		E("failed to connect/logon to irc, giving up");
	}

	int failed = 0;
	int seenmotd = 0;

	time_t ton = time(NULL);
	for (;;) {
		if (ton + ALL_TO < time(NULL)) {
			W("aborting due to timeout (server doesn't ping?)");
			printf("PINGDELAY %d\n", (int)(time(NULL) - ton));//least
			break;
		}

		if (!irc_online(ictx)) {
			failed=1;
			W("disconnected unexpectedly");
			break;
		}

		tokarr tok;
		int r = irc_read(ictx, &tok, 5000000);

		if (r < 0) {
			failed=2 + irc_eof(ictx);
			W("irc_read failed");
			break;
		}

		if (r == 0)
			continue;

		char line[1024];
		lsi_ut_sndumpmsg(line, sizeof line, 0, &tok);
		D("LO: %s", line);

		if (strcmp(tok[1], "PING") == 0) {
			irc_printf(ictx, "PONG :%s\r\n", tok[2]);
			printf("PINGDELAY %d\n", (int)(time(NULL) - ton));
			break;

		} else if (strcmp(tok[1], "005") == 0) {
			size_t i = 3;
			while(i+1 < COUNTOF(tok) && tok[i+1]) //ignore last
				printf("005 %s\n", tok[i++]);

		} else if (strcmp(tok[1], "372") == 0) { //MOTD data
			printf("MOTD %s\n", tok[3]);
			seenmotd = 1;
		} else if (strcmp(tok[1], "375") == 0) { //MOTD end
			irc_dump(ictx);
		} else if (strcmp(tok[1], "376") == 0) { //MOTD start
		} else if (strcmp(tok[1], "422") == 0) { //MOTD missing
			irc_dump(ictx);
		} else {
			W("unhandled command: '%s'", tok[1]);
			fputs("BOGUS", stdout);
			size_t i = 1;
			while(i < COUNTOF(tok) && tok[i])
				printf(" '%s'", tok[i++]);
			puts("");
		}
	}

	if (!failed) {
		if (!seenmotd)
			printf("NOMOTD\n");
		printf("OKAY\n");
	} else if (failed == 1)
		printf("FAILED UNEXPECTED_DISCONNECT\n");
	else if (failed == 2)
		printf("FAILED IRC_READ_FAILED\n");
	else
		printf("FAILED UNEXPECTED_EOF\n");

	irc_dump(ictx);

	irc_reset(ictx);

	return EXIT_SUCCESS;
}

bool
conread_cb(tokarr *tok, void *tag)
{
	char line[1024];
	lsi_ut_sndumpmsg(line, sizeof line, tag, tok);
	D("CR: %s", line);
	if (strcmp((*tok)[1], "001") == 0) {
		printf("001 %s\n", (*tok)[3]);
	} else if (strcmp((*tok)[1], "002") == 0) {
		printf("002 %s\n", (*tok)[3]);
	} else if (strcmp((*tok)[1], "003") == 0) {
		printf("003 %s\n", (*tok)[3]);
	} else if (strcmp((*tok)[1], "004") == 0) {
		size_t i = 3;
		while(i < COUNTOF(*tok) && (*tok)[i])
			i++;

		printf("004 HOST %s\n", i > 3 ? (*tok)[3] : "(none)");
		printf("004 VER %s\n", i > 4 ? (*tok)[4] : "(none)");
		printf("004 UMODES %s\n", i > 5 ? (*tok)[5] : "(none)");
		printf("004 CMODES %s\n", i > 6 ? (*tok)[6] : "(none)");

		if (i > 7) {
			i = 7;
			fputs("004 XTOK", stdout);
			while(i < COUNTOF(*tok) && (*tok)[i])
				printf(" '%s'", (*tok)[i++]);

			puts("");
		}
	} else {
		W("unhandled command: '%s'", (*tok)[1]);
		fputs("BOGUS_CR", stdout);
		size_t i = 1;
		while (i < COUNTOF(*tok) && (*tok)[i])
			printf(" '%s'", (*tok)[i++]);
		puts("");
	}
	return true;
}

static void
randstr(char *dst, size_t dst_sz)
{
	for (size_t i = 0; i < dst_sz; i++)
		dst[i] = (((rand()>>3)&1) ? 'A' : 'a') + ((rand()>>3)%26);
	
	dst[dst_sz-1] = '\0';
}
