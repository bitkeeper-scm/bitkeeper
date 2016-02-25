/*
 * Copyright 1999-2002,2005-2007,2011-2012,2015-2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
