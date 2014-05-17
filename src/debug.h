/* debug.h - (C) 2013, Timo Buhrmester
 * ircutils - irc utils
 * See README for contact-, COPYING for license information. */

/* This header defines a variable, which is obviously a bad thing,
 * however, I see no proper way of getting the loglevel here without
 * requiring more arguments to [EWD]X?(). The log level also should be
 * runtime-adjustable. */

#ifndef IRCUTILS_DEBUG_H
#define IRCUTILS_DEBUG_H 1

#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <err.h>

#define LOGLVL_OFF 0
#define LOGLVL_ERR 1
#define LOGLVL_WRN 2
#define LOGLVL_DBG 3

#define COL_RED "\033[31;01m"
#define COL_YELLOW "\033[33;01m"
#define COL_GRAY "\033[30;01m"
#define COL_RST "\033[0m"

/* see top comment */
extern int s_iudbg_lvl;
extern bool s_iudbg_fancy;
extern const char *s_iudbg_ln[];
extern const char *s_iudbg_co[];
extern void setloglvl(int nl);
extern int getloglvl(void);
extern void logfancy(bool on);


#define E_(FNC, EC, THR, FMT, A...) do {                                  \
		if (s_iudbg_lvl < THR) break;                             \
		FNC(EC, "%s[%s] %s:%d:%s(): " FMT "%s",                   \
				s_iudbg_fancy ? s_iudbg_co[THR] : "",     \
				s_iudbg_ln[THR],                          \
				__FILE__, __LINE__, __func__, ##A,        \
				s_iudbg_fancy ? s_iudbg_co[0] : "");      \
		} while(0)

#define W_(FNC, THR, FMT, A...) do {                                      \
		if (s_iudbg_lvl < THR) break;                             \
		FNC("%s[%s] %s:%d:%s(): " FMT "%s",                       \
				s_iudbg_fancy ? s_iudbg_co[THR] : "",     \
				s_iudbg_ln[THR],                          \
				__FILE__, __LINE__, __func__, ##A,        \
				s_iudbg_fancy ? s_iudbg_co[0] : "");      \
		} while(0)

/* user interface */

#define EE(FMT, A...) E_(err, EXIT_FAILURE, LOGLVL_ERR, FMT, ##A)
#define E(FMT, A...) E_(errx, EXIT_FAILURE, LOGLVL_ERR, FMT, ##A)

#define WE(FMT, A...) W_(warn, LOGLVL_WRN, FMT, ##A)
#define W(FMT, A...) W_(warnx, LOGLVL_WRN, FMT, ##A)

#define DE(FMT, A...) W_(warn, LOGLVL_DBG, FMT, ##A)
#define D(FMT, A...) W_(warnx, LOGLVL_DBG, FMT, ##A)

/* end of user interface */

#endif /* IRCUTILS_DEBUG_H */
