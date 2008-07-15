#include <stdio.h>

int
Fprintf(char *file, char *format, ...)
{
	va_list	ap;
	int	rc;
	FILE	*f = fopen(file, "w");

	if (!f) return (0);
	va_start(ap, format);
	rc = vfprintf(f, format, ap);
	va_end(ap);
	fclose(f);
	return (rc);
}
