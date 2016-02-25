/*
 * Copyright 1998-2000,2002-2016 BitMover, Inc
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

#include "system.h"
#include "sccs.h"

#define	MAXCMT	1020

private	char	**saved;	/* saved copy of comments across files */
private	int	gotComment;	/* seems redundant but it isn't, this is
				 * is how we know we have a null comment.
				 */

int
comments_save(char *s)
{
	char	*p;
	int	len;

	unless (s) goto out;	/* null comment */

	if (saved) freeLines(saved, free);
	saved = 0;
	while (p = eachline(&s, &len)) {
		if (comments_checkStr(p, len)) {
			freeLines(saved, free);
			saved = 0;
			return (-1);
		}
		if (len <= MAXCMT) {
			saved = addLine(saved, strndup(p, len));
		} else {
			for (/* p */; len > MAXCMT; len -= MAXCMT) {
				saved = addLine(saved, strndup(p, MAXCMT));
				p += MAXCMT;
				fprintf(stderr,
				    "Splitting comment line \"%.50s\"...\n", p);
			}
			saved = addLine(saved, strndup(p, len));
		}
	}
out:	gotComment = 1;
	return (0);
}

int
comments_savefile(char *s)
{
	char	*str;
	int	rc;

	unless (str = loadfile(s, 0)) return (-1);

	rc = comments_save(str);
	free(str);
	return (rc);
}

int
comments_got(void)
{
	return (gotComment);
}

void
comments_done(void)
{
	freeLines(saved, free);
	saved = 0;
	gotComment = 0;
}

/*
 * Return the collected comments from comments_save() or comments_savefile().
 * If nothing saved and prompt!=0 then prompt for commands and return that.
 *
 * return null if prompt aborted
 */
char **
comments_return(char *prompt)
{
	int	i;
	char	**ret = 0;

	if (!saved && !gotComment && prompt) {
		unless (saved = sccs_getComments(prompt)) return (0);
	}
	EACH(saved) ret = addLine(ret, strdup(saved[i]));
	unless (ret) ret = allocLines(2);
	return (ret);
}

/*
 * Attach saved comments to delta, prompted if needed
 */
ser_t
comments_get(char *file, char *rev, sccs *s, ser_t d)
{
	char	**cmts;
	char	*prompt = 0;

	if (file) {
		prompt = aprintf("%s%s%s",
		    file,
		    rev ? "@" : ":",
		    rev ? rev : "");
	}
	if (cmts = comments_return(prompt)) {
		unless (d) d = sccs_newdelta(s);
		comments_set(s, d, cmts);
		freeLines(cmts, free);
	} else {
		if (d) sccs_freedelta(s, d);
		d = 0;		/* prompt aborted */
	}
	if (prompt) free(prompt);
	return (d);
}

/*
 * Prompt the user with a set of comments, returning
 * 0 if they want to use them,
 * -1 for an error or an abort.
 */
int
comments_prompt(char *file)
{
	char	*cmd, buf[10];

	unless (editor || (editor = getenv("EDITOR"))) editor = EDITOR;
	while (1) {
		printf("\n-------------------------------------------------\n");
		fflush(stdout);
		if (cat(file)) return (-1);

		printf("-------------------------------------------------\n");
		printf("Use these comments: (e)dit, (a)bort, (u)se? ");
		fflush(stdout);
		unless (getline(0, buf, sizeof(buf)) > 0) return (-1);
		switch (buf[0]) {
		    case 'y': 
		    case 'u':
			return (0);
		    case 'e':
			cmd = aprintf("%s '%s'", editor, file);
			system(cmd);
			free(cmd);
			break;
		    case 'a':
		    case 'q':
			return (-1);
		}
	}
}

int
comments_readcfile(sccs *s, int prompt, ser_t d)
{
	FILE	*f;
	char	*t;
	char	**comments = 0;
	char	tmp[MAXPATH];

	unless (t = xfile_fetch(s->gfile, 'c')) return (-1);
	bktmp(tmp);
	unless (f = fopen(tmp, "w")) {
		free(t);
		perror(tmp);
		unlink(tmp);
		return (-1);
	}
	fputs(t, f);
	free(t);
	fclose(f);
	if (prompt && comments_prompt(tmp)) {
		unlink(tmp);
		return (-2);
	}
	comments = file2Lines(0, tmp);
	comments = addLine(comments, ""); /* add blank for trailing NL */
	t = joinLines("\n", comments);
	popLine(comments); /* remove blank we just added */
	xfile_store(s->gfile, 'c', t);
	free(t);
	s->used_cfile = 1;
	comments_set(s, d, comments);
	freeLines(comments, free);
	unlink(tmp);
	return (0);
}

void
comments_cleancfile(sccs *s)
{
	unless (s->used_cfile) return;
	xfile_delete(s->gfile, 'c');
}

void
comments_writefile(char *file)
{
	lines2File(saved, file);
}

int
comments_checkStr(u8 *s, int len)
{
	assert(s);
	for (; *s && (len > 0); s++, len--) {
		/* disallow control characters, but still allow UTF-8 */
		if ((*s < 0x20) && (*s != '\t')) {
			fprintf(stderr,
			    "Non-printable character (0x%x) is illegal in "
			    "comments string.\n", *s);
			return (1);
		}
	}
	return (0);
}

/*
 * Replace the comments on a delta with a new set
 */
void
comments_set(sccs *s, ser_t d, char **comments)
{
	int	i;

	COMMENTS_SET(s, d, "");
	EACH(comments) {
		sccs_appendStr(s, comments[i]);
		sccs_appendStr(s, "\n");
	}
}
