#include "system.h"

void
diefn(int seppuku, char *file, int line, char *fmt, ...)
{
	va_list	ap;
	int	len;
	char	*format;

	len = strlen(fmt);
	if (len && (fmt[len-1] == '\n')) {
		format = aprintf("%s", fmt);
	} else {
		format = aprintf("%s at %s line %d.\n", fmt, file, line);
	}
	va_start(ap, fmt);
	vfprintf(stderr, format, ap);
	va_end(ap);
	free(format);
	if (seppuku) exit(1);
}
