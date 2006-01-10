#include "system.h"

#undef	perror

void
my_perror(char *file, int line, char *msg)
{
	fprintf(stderr, "%s:%d: ", file, line);
	perror(msg);
}
