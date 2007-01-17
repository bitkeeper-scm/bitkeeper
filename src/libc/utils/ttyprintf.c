#include "system.h"

/*
 * Print a message on /dev/tty
 */
void
ttyprintf(char *fmt, ...)
{
	FILE	*f;
	va_list	ptr;

	unless ((f = efopen("BK_TTYPRINTF")) ||
	    (f = fopen(DEV_TTY, "w"))) {
		f = stderr;
	}
	va_start(ptr, fmt);
	vfprintf(f, fmt, ptr);
	va_end(ptr);
	if (f != stderr) fclose(f);
}
