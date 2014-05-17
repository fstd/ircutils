#include "debug.h"

int s_iudbg_lvl = LOGLVL_WRN;
bool s_iudbg_fancy = false;
const char *s_iudbg_ln[] = {NULL, "ERR", "WRN", "DBG"};
const char *s_iudbg_co[] = {COL_RST, COL_RED, COL_YELLOW, COL_GRAY};

void
setloglvl(int nl)
{
	s_iudbg_lvl = nl;
}

int
getloglvl(void)
{
	return s_iudbg_lvl;
}

void
logfancy(bool on)
{
	s_iudbg_fancy = on;
}

