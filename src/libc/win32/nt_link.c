#include "system.h"

int
nt_link(const char *file1, const char *file2)
{
	unless (CreateHardLink(file2, file1, 0)) {
		(void)GetLastError(); /* set errno */
		return (-1);
	}
	return (0);
}
