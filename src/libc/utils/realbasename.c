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
	int	i, j, len;
	char	buf[MAXPATH];

	len = strlen(path);
	for (i = 0, j = 0; i < len;) {
		if (path[i] == '?' || path[i] == '*') {
			buf[j++] = '\\';
		}
		buf[j++] = path[i++];
	}
	buf[j] = '\0';
	if ((h = FindFirstFile(buf, &data)) == INVALID_HANDLE_VALUE) {
		unless (p = strrchr(buf, '/')) p = strrchr(buf, '\\');
		if (p) {
			strcpy(realname, ++p);
		} else {
			strcpy(realname, buf);
		}
	} else {
		strcpy(realname, data.cFileName);
		FindClose(h);
	}
	return (realname);
}
#endif	/* WIN32 */
