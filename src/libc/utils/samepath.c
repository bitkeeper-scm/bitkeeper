#include "system.h"

int
samepath(char *a, char *b)
{
	char a1[MAXPATH],b1[MAXPATH];

	fullname(a, a1);
	fullname(b, b1);
	return (patheq(a1, b1));
}


/*
 * Check whether two paths "overlap".
 */
int
paths_overlap(char *a, char *b)
{
	while (*a && (*a == *b)) ++a, ++b;

	return (((*a == 0) || (*a == '/')) &&
	    ((*b == 0) || (*b == '/')));
}

#ifdef WIN32

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
patheq(char *a, char *b)
{
	return(streq(a, b));
}
#endif
