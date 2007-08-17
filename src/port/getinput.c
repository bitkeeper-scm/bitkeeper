#include "../sccs.h"

/*
 * Copyright (c) 2001 Andrew Chang       All rights reserved.
 */

/*
 * -------------------------------------------------------------
 * 		Unix  version  of sccs_getXXXX
 * -------------------------------------------------------------
 */
private	int	counter;
private	void	intr(int dummy) { counter++; };

private	void
catch(void)
{
	counter = 0;
	sig_catch((handler)intr);
}

private	int
caught(char *what)
{
	sig_restore();
	if (counter) fprintf(stderr, "\n%s aborted due to interrupt.\n", what);
	return (counter);
}

int
sccs_getComments(char *file, char *rev, delta *n)
{
	char	buf2[1024];

	fprintf(stderr, "End comments with \".\" by itself or a blank line.\n");
	assert(file);
	if (rev) {
		fprintf(stderr, "%s %s>>  ", file, rev);
	} else {
		fprintf(stderr, "%s>>  ", file);
	}
	catch();
	while (getline(0, buf2, sizeof(buf2)) > 0) {
		if ((buf2[0] == 0) || streq(buf2, "."))
			break;
		if (comments_checkStr(buf2)) {
			fprintf(stderr, "Skipped.\n");
			continue;
		}
		comments_append(n, strdup(buf2));
		if (rev) {
			fprintf(stderr, "%s@%s>>  ", file, rev);
		} else {
			fprintf(stderr, "%s>>  ", file);
		}
	}
	return (caught("Check in") ? -1 : 0);
}

char **
getParkComment(int *err)
{
        char    buf2[1024];
	char	**comments = NULL;

	fprintf(stderr, "End comments with \".\" by itself or a blank line.\n");
        fprintf(stderr, "parkfile>>  ");
	catch();
        while (getline(0, buf2, sizeof(buf2)) > 0) {
                if ((buf2[0] == 0) || streq(buf2, "."))
                        break;
                comments = addLine(comments, strdup(buf2));
        	fprintf(stderr, "parkfile>>  ");
        }
	if (caught("Park")) {
		*err = 1;
		freeLines(comments, free);
		return (NULL);
	}
        return (comments);
}
