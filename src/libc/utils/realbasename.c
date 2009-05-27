#define	FSLAYER_NODEFINES
#include "system.h"

#ifndef WIN32

char *
realBasename(const char *path, char *realname)
{
	/* implement me */
	assert(0);
	return(0);
}

#else  /* WIN32 */

char *
realBasename(const char *path, char *realname)
{
	HANDLE	h;
	WIN32_FIND_DATA	data;
	char	*p;

	if ((h = FindFirstFile(path, &data)) == INVALID_HANDLE_VALUE) {
		assert(strchr(path, '\\') == 0);
		if (p = strrchr(path, '/')) {
			strcpy(realname, ++p);
		} else {
			strcpy(realname, path);
		}
	} else {
		strcpy(realname, data.cFileName);
		FindClose(h);
	}
	return (realname);
}
#endif	/* WIN32 */
