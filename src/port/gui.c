#include "../system.h"
#include "../sccs.h"

/*
 * Copyright (c) 2001 Andrew Chang       All rights reserved.
 */

int
gui_useDisplay(void)
{
	if (getenv("BK_NO_GUI_PROMPT")) return (0);
	if (win32() || macosx()) return (getenv("BK_GUI") != (char*)0);
	return (getenv("DISPLAY") && getenv("BK_GUI"));
}

char *
gui_displayName(void)
{
#ifdef WIN32
	return ("monitor");
#else
	if (gui_useAqua()) {return ("monitor");}
	return (getenv("DISPLAY"));
#endif

}

int
gui_useAqua(void)
{
#ifdef	__APPLE__
	unless (getenv("DISPLAY")) {return (1);}
#endif
	return 0;
}
