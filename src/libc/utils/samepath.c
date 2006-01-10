#include "system.h"

int
samepath(char *a, char *b)
{
#ifdef WIN32
	char a1[MAXPATH],b1[MAXPATH];

	strcpy(a1, fullname(a));
	strcpy(b1, fullname(b));
	return (patheq(a1, b1));
#else
        struct  stat sa, sb;

        if (lstat(a, &sa) == -1) return 0;
        if (lstat(b, &sb) == -1) return 0;
        return ((sa.st_dev == sb.st_dev) && (sa.st_ino == sb.st_ino));
#endif
}
