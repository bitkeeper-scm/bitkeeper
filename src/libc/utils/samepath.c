#include "system.h"

#ifdef WIN32
int
samepath(char *a, char *b)
{
	char a1[MAXPATH],b1[MAXPATH];

	strcpy(a1, fullname(a));
	strcpy(b1, fullname(b));
	return (patheq(a1, b1));
}

/* make comparisons case insensitive and forward slash vs backslash agnostic */
int
patheq(char *a, char *b)
{
	char	*ad = strdup(a);
	char	*bd = strdup(b);
	int	rc;

	assert(ad && bd);
	localName2bkName(ad, ad);
	localName2bkName(bd, bd);
	rc = !strcasecmp(ad, bd);
	free(ad);
	free(bd);
	return (rc);
}

#else	/* UNIX */

int
samepath(char *a, char *b)
{
	struct  stat sa, sb;

	if (lstat(a, &sa) == -1) return 0;
	if (lstat(b, &sb) == -1) return 0;
	return ((sa.st_dev == sb.st_dev) && (sa.st_ino == sb.st_ino));
}

int
patheq(char *a, char *b)
{
	return(streq(a, b));
}
#endif
