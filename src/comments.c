/* Copyright (c) 1998-2000 L.W.McVoy */
#include "system.h"
#include "sccs.h"

private	char	**saved;	/* saved copy of comments across files */
private	char	*comment;	/* a placed to parse the comment from */
private	int	gotComment;	/* seems redundant but it isn't, this is
				 * is how we know we have a null comment.
				 */

void
comments_save(char *s)
{
	comment = s;
	gotComment = 1;
}

int
comments_got()
{
	return (gotComment);
}

void
comments_done()
{
	int	i;

	if (!saved) return;
	EACH(saved) free(saved[i]);
	free(saved);
	saved = 0;
	gotComment = 0;
	comment = 0;
	/* XXX - NULL comment as well? */
}

delta *
comments_get(delta *d)
{
	int	i;

	unless (d) d = calloc(1, sizeof(*d));
	if (!comment && gotComment) return (d);
	if (!comment) {
		if (saved) {
			EACH(saved) {
				d->comments =
				    addLine(d->comments, strdup(saved[i]));
			}
			return (d);
		}
		if (sccs_getComments("Group comments", 0, d)) {
			return (0);
		}
		EACH(d->comments) {
			saved = addLine(saved, strdup(d->comments[i]));
		}
	} else {
		d = sccs_parseArg(d, 'C', comment, 0);
	}
	if (d && (d->flags & D_ERROR)) {
		sccs_freetree(d);
		return (0);
	}
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
	extern	char *editor;

	unless (editor || (editor = getenv("EDITOR"))) editor = EDITOR;
	while (1) {
		printf("\n-------------------------------------------------\n");
		fflush(stdout);
		if (cat(file)) return (-1);

		flush_fd0(); /* for Win98 and Win/ME */
		printf("-------------------------------------------------\n");
		printf("Use these comments: (e)dit, (a)bort, (u)se? ");
		fflush(stdout);
		unless (getline(0, buf, sizeof(buf)) > 0) return (-1);
		switch (buf[0]) {
		    case 'y': 
		    case 'u':
			return (0);
		    case 'e':
			cmd = aprintf("%s %s", editor, file);
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
	char	*p;
	MMAP	*m;

	unless (access(cfile, R_OK) == 0) return (-1);
	if (prompt && comments_prompt(cfile)) return (-2);
	unless (m = mopen(cfile, "r")) return (-1);
	while (p = mnext(m)) {
		d->comments = addLine(d->comments, strnonldup(p));
	}
	mclose(m);
	return (0);
}

void
comments_cleancfile(char *file)
{
	char	*cfile = name2sccs(file);
	char	*p;

	p = strrchr(cfile, '/');
	if (p) {
		p[1] = 'c';
		unlink(cfile);
	}
	free(cfile);
}
