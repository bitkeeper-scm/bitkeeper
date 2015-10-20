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

char **
sccs_getComments(char *prompt)
{
	char	**ret = allocLines(4);
	char	buf2[1024];

	fprintf(stderr, "End comments with \".\" by itself or a blank line.\n");
	assert(prompt);
	fprintf(stderr, "%s>>  ", prompt);
	catch();
	while (getline(0, buf2, sizeof(buf2)) > 0) {
		if ((buf2[0] == 0) || streq(buf2, "."))
			break;
		if (comments_checkStr(buf2, strlen(buf2))) {
			fprintf(stderr, "Skipped.\n");
			continue;
		}
		ret = addLine(ret, strdup(buf2));
		fprintf(stderr, "%s>>  ", prompt);
	}
	if (caught("Check in")) {
		freeLines(ret, free);
		ret = 0;
	}
	return (ret);
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
