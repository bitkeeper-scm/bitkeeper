#include "system.h"

/*
 * concatenate two paths "first" and "second", and put the result in "buf"
 * TODO: This function should be grouped with cleanPath() and put in
 *	 the same file.
 */
void
concat_path(char *buf, char *first, char *second)
{
	int	len;

	if (buf != first) strcpy(buf, first);
	len = strlen(buf);
	if (len >= 2 &&
	    (buf[len -2] == '/') && (buf[len -1] == '.') && second[0]) {
		buf[len - 1] = 0; len--;
	}
	/*
	 * if "first" and "second" already have a seperator between them,
	 * don't add another one.
	 * Another special case is also checked here:
	 * 	first or "second" is a null string.
	 */
	if ((buf[0] != '\0') && (second[0] != '\0') &&
	    (buf[len -1] != '/') && (second[0] != '/'))
		strcat(buf, "/");
	strcat(buf, second);
}
