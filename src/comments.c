/* Copyright (c) 1998-2000 L.W.McVoy */
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
	char	*p, **split;
	int	i, len;

	unless (s) goto out;	/* null comment */

	if (saved) freeLines(saved, free);
	saved = 0;
	split = splitLineToLines(s, 0);
	EACH(split) {
		if (comments_checkStr(split[i])) {
			freeLines(split, free);
			freeLines(saved, free);
			saved = 0;
			return (-1);
		}
		len = strlen(split[i]);
		if (len <= MAXCMT) {
			saved = addLine(saved, strdup(split[i]));
		} else {
			for (p = split[i]; len > MAXCMT; len -= MAXCMT) {
				saved = addLine(saved, strndup(p, MAXCMT));
				p += MAXCMT;
				fprintf(stderr,
				    "Splitting comment line \"%.50s\"...\n", p);
			}
			saved = addLine(saved, strdup(p));
		}
	}
	freeLines(split, free);
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
delta *
comments_get(char *file, char *rev, sccs *s, delta *d)
{
	int	i;
	char	**cmts;
	char	*prompt = 0;

	if (file) {
		prompt = aprintf("%s%s%s",
		    file,
		    rev ? "@" : ":",
		    rev ? rev : "");
	}
	if (cmts = comments_return(prompt)) {
		unless (d) d = new(delta);
		/* sort of a catLines() plus setting comments metadata */
		EACH(cmts) comments_append(d, cmts[i]);
		freeLines(cmts, 0);
	} else {
		if (d) sccs_freetree(d);
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
		uniq_close();	/* don't hold uniqdb lock */
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
comments_readcfile(sccs *s, int prompt, delta *d)
{
	char	*cfile = sccs_Xfile(s, 'c');
	FILE	*f;
	char	*p;
	char	tmp[MAXPATH];

	unless (access(cfile, W_OK) == 0) return (-1);
	bktmp(tmp, "cfile");
	if (fileCopy(cfile, tmp)) perror(tmp);
	if (prompt && comments_prompt(tmp)) return (-2);
	if (fileMove(tmp, cfile)) perror(tmp);
	unless (f = fopen(cfile, "r")) return (-1);
	s->used_cfile = 1;
	while (p = fgetline(f)) {
		comments_append(d, strdup(p));
	}
	fclose(f);
	return (0);
}

void
comments_cleancfile(sccs *s)
{
	unless (s->used_cfile) return;
	unlink(sccs_Xfile(s, 'c'));
}

void
comments_writefile(char *file)
{
	lines2File(saved, file);
}

int
comments_checkStr(u8 *s)
{
	assert(s);
	for (; *s; s++) {
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
 * Append 1 line to the comments for a delta.
 * A newline is added to the line.
 */
void
comments_append(delta *d, char *line)
{
	assert(d->localcomment || (d->cmnts == 0));

	d->localcomment = 1;
	d->cmnts = addLine(d->cmnts, line);
}

/* force comments to be allocated locally */
char **
comments_load(sccs *s, delta *d)
{
	char	**lines = 0;
	char	buf[MAXCMT+6];	/* "^Ac " (3) + "\n\0" (2) + 1 slop = 6 */

	if (d->localcomment) goto out;
	d->localcomment = 1;
	unless (d->cmnts) goto out;
	assert(s && (s->state & S_SOPEN));
	fseek(s->fh, p2int(d->cmnts), SEEK_SET);
	while (fnext(buf, s->fh)) {
		chomp(buf);
		unless (strneq(buf, "\001c ", 3)) break;
		lines = addLine(lines, strdup(buf+3));
	}
	d->cmnts = lines;
out:	return (d->cmnts);
}

void
comments_free(delta *d)
{
	if (d->localcomment) freeLines(d->cmnts, free);
	d->cmnts = 0;
	d->localcomment = 0;
}
