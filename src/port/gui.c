#include "../system.h"
#include "../sccs.h"

/*
 * Copyright (c) 2001 Andrew Chang       All rights reserved.
 */

int
hasGUIsupport()
{
#ifdef WIN32
	return (1);
#else
	return (getenv("DISPLAY") != NULL);
#endif
}

char *
GUI_display()
{
#ifdef WIN32
	return ("monitor");
#else
	return (getenv("DISPLAY"));
#endif

}
