#include "../system.h"
#include "../sccs.h"

/*
 * Copyright (c) 2001 Andrew Chang       All rights reserved.
 */

/*
 * -------------------------------------------------------------
 * 		Unix  version  of sccs_getXXXX
 * -------------------------------------------------------------
 */
private jmp_buf	jmp;
private	handler	old;
private	int	counter;
private	void	intr(int dummy) { counter++; };

private	void
catch()
{
	counter = 0;
	old = sig_catch((handler)intr);
}

private	int
caught(char *what)
{
	sig_catch(old);
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
		n->comments = addLine(n->comments, strdup(buf2));
		if (rev) {
			fprintf(stderr, "%s@%s>>  ", file, rev);
		} else {
			fprintf(stderr, "%s>>  ", file);
		}
	}
	return (caught("Check in") ? -1 : 0);
}

int
sccs_getHostName(delta *n)
{
	char	buf2[1024];

	fprintf(stderr, "Hostname of your machine>>  ");
	catch();
	while (getline(0, buf2, sizeof(buf2)) > 0) {
		if (isValidHost(buf2)) {
			n->hostname = strdup(buf2);
			break;
		}
		fprintf(stderr, "%s is not a valid hostname\n", buf2);
		fprintf(stderr, "Hostname of your machine>>  ");
	}
	return (caught("Check in") ? -1 : 0);
}


int
sccs_getUserName(delta *n)
{
	char	buf2[1024];

	catch();
	fprintf(stderr, "User name>>  ");
	while (getline(0, buf2, sizeof(buf2)) > 0) {
		char	*t;

		/* Null the newline */
		for (t = buf2; *t; t++);
		t[-1] = 0;
		if (isValidUser(buf2)) {
			n->user = strdup(buf2);
			break;
		}
		fprintf(stderr, "%s is not a valid user name\n", buf2);
		fprintf(stderr, "User name>>  ");
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
		freeLines(comments);
		return (NULL);
	}
        return (comments);
}
