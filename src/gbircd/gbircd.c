/* gbircd.c - (C) 2013, Timo Buhrmester
 * ircutils - irc utils
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

#include <unistd.h>
#include <getopt.h>

#include <sys/socket.h>

#include <libsrsbsns/addr.h>
#include <libsrsbsns/io.h>

#include "../debug.h"
#include "../common.h"

#define MAX_ARGS 20

static char *s_srvhost = NULL;
static char *s_srvpw = NULL;

const char *s_cmds[] = {
	"NICK", "OPER", "MODE", "JOIN", "PART", "MODE", "TOPIC", "INVITE", "KICK", "PRIVMSG", "NOTICE", "PING", "PONG", "001",
	"002", "003", "004", "005", "200", "201", "202", "203", "204", "205", "206", "207", "208", "209", "210", "211", "212",
	"213", "215", "217", "219", "221", "231", "233", "234", "235", "240", "242", "243", "244", "246", "250", "251", "252",
	"253", "254", "255", "256", "257", "258", "259", "261", "262", "263", "300", "301", "302", "303", "305", "306", "311",
	"312", "313", "314", "315", "317", "318", "319", "321", "322", "323", "324", "325", "331", "332", "341", "342", "346",
	"347", "348", "349", "351", "352", "353", "361", "363", "364", "365", "366", "367", "368", "369", "371", "372", "374",
	"375", "376", "381", "382", "383", "384", "391", "392", "393", "394", "395", "401", "402", "403", "404", "405", "406",
	"407", "408", "409", "411", "412", "413", "414", "415", "421", "422", "423", "424", "431", "432", "433", "436", "437",
	"441", "442", "443", "444", "445", "446", "451", "461", "462", "463", "465", "466", "467", "471", "472", "473", "474",
	"475", "476", "477", "478", "481", "482", "483", "484", "485", "491", "492", "501", "502", "004", "004", "004", "004" 
};

static void process_args(int *argc, char ***argv);
static void init(int *argc, char ***argv);
static void usage(FILE *str, const char *a0, int ec);
static char* randstr(char *dst, size_t dst_sz, bool nows);


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
}


static void
init(int *argc, char ***argv)
{
	process_args(argc, argv);
	D("initialized, server '%s'%s", s_srvhost, s_srvpw?" (w/ pw)":"");
}


static void
usage(FILE *str, const char *a0, int ec)
{
	#define I(STR) fputs(STR "\n", str)
	I("============");
	I("== gbircd ==");
	I("============");
	fprintf(str, "usage: %s [-h]\n", a0);
	I("");
	I("\t-h: Display brief usage statement and terminate");
	I("");
	I("(C) 2013, Timo Buhrmester (contact: #fstd on irc.freenode.org)");
	#undef I
	exit(ec);
}

void
mkline(char *dest)
{
	bool pfx = (rand()>>3)%2;
	int cmd = (rand()>>3) % (sizeof s_cmds / sizeof s_cmds[0]);
	int nargs = (rand()>>3) % MAX_ARGS;

	char rndstr[1024];

	dest[0] = '\0';

	if (pfx) {
		strcat(dest, ":");
		int pfxlen = (rand()>>3) % 24;
		if ((rand()>>3) % 10) {
			strcat(dest, randstr(rndstr, pfxlen, true));
		}
		//if ((rand()>>3) % 10 == 0)
			//return;
		strcat(dest, " ");
	}

	if (strcmp(s_cmds[cmd], "004") == 0 && (rand()>>3) % 2) {
		strcat(dest, "005 foo ");
		switch ((rand()>>3)%4) {
		case 0:
			strcat(dest, "CASEMAPPING=");
			break;
		case 1:
			strcat(dest, "CHANMODES=");
			break;
		case 2:
			strcat(dest, "PREFIX=");
			break;
		case 3:
			strcat(dest, randstr(rndstr, (rand()>>3)%20, true));
			strcat(dest, "=");
			break;
		}

		strcat(dest, randstr(rndstr, (rand()>>3)%20, true));
	} else
		strcat(dest, s_cmds[cmd]);
	//if ((rand()>>3) % 10 == 0)
		//return;
	

	for(int i = 0; i < nargs; i++) {
		strcat(dest, " ");
		int arglen = (rand()>>3) % 24;
		if (i + 1 == nargs && (rand()>>3) % 2)
			strcat(dest, ":");
		strcat(dest, randstr(rndstr, arglen, true));
	}
}

void
handle(int sck)
{
	fprintf(stderr, "=== new client ===\n");
	for (;;) {
		char line[1024];
		mkline(line);
		strcat(line, "\r\n");

		if (!io_writeall(sck, line, strlen(line)))
			break;
	}

	close(sck);
}


int
main(int argc, char **argv)
{
	init(&argc, &argv);
	signal(SIGPIPE, SIG_IGN); 

	int sck = addr_bind_socket_p("192.168.1.12", 6667, NULL, NULL, 0, 0);
	if (sck < 0)
		EE("addr_bind_socket_p");

	if (listen(sck, 128) != 0)
		EE("listen");

	int csck;

	while ((csck = accept(sck, NULL, NULL)) >= 0) {
		handle(csck);
	}

	EE("accept");
}


static char*
randstr(char *dst, size_t dst_sz, bool nows)
{
	for (size_t i = 0; i < dst_sz; i++) {
		dst[i] = (rand()>>3) % 256;
		//dst[i] = 'a' + (rand()>>3) % 26;
		if (!dst[i])
			dst[i] = 'z';
		if (dst[i] == '\r' || dst[i] == '\n')
			dst[i] = 'x';
		if (nows && (dst[i] == '\t' || dst[i] == ' '))
			dst[i] = 'y';
	}
	
	dst[dst_sz-1] = '\0';

	return dst;
}
