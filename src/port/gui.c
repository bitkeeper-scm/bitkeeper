#include "../system.h"
#include "../sccs.h"

/*
 * Copyright (c) 2001 Andrew Chang       All rights reserved.
 */

int
gui_useDisplay()
{
	if (getenv("BK_NO_GUI_PROMPT")) return (0);

	/* If we're windows and in a non-local bkd then no GUI */
	if (win32() && getenv("_BK_IN_BKD") && !getenv("_BK_BKD_IS_LOCAL")) {
		return (0);
	}

	unless (getenv("BK_GUI")) return (0);

	/* FYI: Cygwin has X11 clients so there may be a display there too */
	if (getenv("DISPLAY")) return (1);

	/* Under Windows we always have a GUI at this point but not on Unix */
	return (win32());
}

char *
gui_displayName()
{
#ifdef WIN32
	return ("monitor");
#else
	return (getenv("DISPLAY"));
#endif

}
