#include <string.h>
#include <stddef.h>

int
strcnt(char *p, char ch)
{
	int	n = 0;

	while (*p) if (*p++ == ch) n++;
	return (n);
}
