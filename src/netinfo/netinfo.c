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
#include <libsrsirc/irc_basic.h>
#include <libsrsirc/irc_util.h>
#undef WITH_SSL

#include "../debug.h"
#include "../common.h"

#define CONTO_SOFT 120000000 //unit: us
#define CONTO_HARD 300000000 //unit: us
#define ALL_TO 600 //unit: s
#define DEF_PORT 6667

static char *s_srvhost = NULL;
static char *s_srvpw = NULL;


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


bool conread_cb(char **msg, size_t msg_len, void *tag);


int
main(int argc, char **argv)
{
	init(&argc, &argv);

	ibhnd_t irc = ircbas_init();
	ircbas_regcb_conread(irc, conread_cb, NULL);

	char host[256];
	unsigned short port;
	char portstr[6];
	bool ssl = false;

	parse_hostspec(host, sizeof host, &port, &ssl, s_srvhost);
	if (!port)
		port = DEF_PORT;
	
	snprintf(portstr, sizeof portstr, "%hu", port);

	struct addrinfo hints, *res, *res0;
	int error;
	bool success = false;
	const char *cause = NULL;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	error = getaddrinfo(host, portstr, &hints, &res0);
	if (error)
		E("%s", gai_strerror(error));

	printf("GIVENHOST %s:%hu%s\n", host, port, ssl?"/ssl":"");
	char paddr[64] = "(non-INET/INET6)";
	unsigned short peerport = 0;
	int acount = 0;
	for (res = res0; res; res = res->ai_next) {
		acount++;
		if (peeraddr(paddr, sizeof paddr, &peerport, res))
			printf("ADDR %s:%hu\n", paddr, peerport);
	}

	printf("NUMADDRS %d\n", acount);

	for (res = res0; res; res = res->ai_next) {

		if (!peeraddr(paddr, sizeof paddr, &peerport, res)) {
			W("couldn't obtain peer addr from addrinfo!");
			continue;
		}

		ircbas_set_server(irc, paddr, peerport);
		ircbas_set_pass(irc, s_srvpw); //NULL == no password
		ircbas_set_ssl(irc, ssl);
		char str[10];
		randstr(str, sizeof str);
		ircbas_set_nick(irc, str);
		randstr(str, sizeof str);
		ircbas_set_uname(irc, str);
		randstr(str, sizeof str);
		ircbas_set_fname(irc, str);
		ircbas_set_connect_timeout(irc, CONTO_SOFT, CONTO_HARD);

		if (!ircbas_connect(irc))
			W("couldn't connect/logon to '%s:%hu'", paddr, peerport);
		else {
			printf("ACTUALHOST %s:%hu\n", paddr, peerport);
			success = true;
			break;
		}

	}
	freeaddrinfo(res0);

	if (!success)
		E("failed to connect/logon to irc, giving up");


	time_t ton = time(NULL);
	for (;;) {
		if (ton + ALL_TO < time(NULL)) {
			W("aborting due to timeout (server doesn't ping?)");
			printf("PINGDELAY: %d\n", time(NULL) - ton);//least
			break;
		}

		if (!ircbas_online(irc))
			E("disconnected unexpectedly");

		char *tok[16];
		int r = ircbas_read(irc, tok, COUNTOF(tok), 5000000);

		if (r < 0)
			E("ircbas_read failed");

		if (r == 0)
			continue;

		char line[1024];
		sndumpmsg(line, sizeof line, 0, tok, COUNTOF(tok));
		D("LO: %s", line);

		if (strcmp(tok[1], "PING") == 0) {
			iprintf(irc, "PONG :%s\r\n", tok[2]);
			printf("PINGDELAY: %d\n", time(NULL) - ton);
			break;

		} else if (strcmp(tok[1], "005") == 0) {
			int i = 3;
			while(i+1 < COUNTOF(tok) && tok[i+1]) //ignore last
				printf("005 %s\n", tok[i++]);

		} else if (strcmp(tok[1], "372") == 0) { //MOTD data
			printf("MOTD %s\n", tok[3]);

		} else if (strcmp(tok[1], "375") == 0) { //MOTD end
		} else if (strcmp(tok[1], "376") == 0) { //MOTD start
		} else if (strcmp(tok[1], "422") == 0) { //MOTD missing
		} else {
			W("unhandled command: '%s'", tok[1]);
			fputs("BOGUS", stdout);
			int i = 1;
			while(i < COUNTOF(tok) && tok[i])
				printf(" '%s'", tok[i++]);
			puts("");
		}
	}

	ircbas_reset(irc);

	return EXIT_SUCCESS;
}

bool
conread_cb(char **tok, size_t tok_len, void *tag)
{
	char line[1024];
	sndumpmsg(line, sizeof line, tag, tok, tok_len);
	D("CR: %s", line);
	if (strcmp(tok[1], "001") == 0) {
		printf("001 %s\n", tok[3]);
	} else if (strcmp(tok[1], "002") == 0) {
		printf("002 %s\n", tok[3]);
	} else if (strcmp(tok[1], "003") == 0) {
		printf("003 %s\n", tok[3]);
	} else if (strcmp(tok[1], "004") == 0) {
		int i = 3;
		while(i < tok_len && tok[i])
			i++;

		printf("004 HOST %s\n", i > 3 ? tok[3] : "(none)");
		printf("004 VER %s\n", i > 4 ? tok[4] : "(none)");
		printf("004 UMODES %s\n", i > 5 ? tok[5] : "(none)");
		printf("004 CMODES %s\n", i > 6 ? tok[6] : "(none)");

		if (i > 7) {
			i = 7;
			fputs("004 XTOK", stdout);
			while(i < tok_len && tok[i])
				printf(" '%s'", tok[i++]);

			puts("");
		}
	} else {
		W("unhandled command: '%s'", tok[1]);
		fputs("BOGUS_CR", stdout);
		int i = 1;
		while(i < tok_len && tok[i])
			printf(" '%s'", tok[i++]);
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
