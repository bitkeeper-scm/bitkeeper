#include "../system.h"
#include "../sccs.h"

/*
 * Copyright (c) 2001 Andrew Chang       All rights reserved.
 */

int
gui_useDisplay()
{
	if (getenv("BK_NO_GUI_PROMPT")) return (0);
	return (getenv("BK_GUI") != (char*)0);
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
