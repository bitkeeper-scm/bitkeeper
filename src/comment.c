/* Copyright (c) 2002 L.W.McVoy */
#include "system.h"
#include "sccs.h"

private	char	**readFile(char *file, char *delete);
private	char	**editComments(sccs *s, delta *d);
extern	char	*editor;

/*
 * comment - all the updating of checkin comments
 *
 * bk comment [-y<new>] [-r<rev>] files
 *
 * If you don't pass it a comment, it pops you into the editor on the old one.
 *
 * TODO - do not allow them to change comments in pulled changesets.
 */
int
comment_main(int ac, char **av)
{
	sccs	*s = 0;
	delta	*d;
	char	*name, *file = 0, *comment = 0, *rev = "+";
	int	c;
	char	**lines = 0;

	while ((c = getopt(ac, av, "r|y|Y|")) != -1) {
		switch (c) {
		    case 'y': comment = optarg; break;
		    case 'Y': file = optarg; break;
		    case 'r': rev = optarg; break;
		}
	}
	if (comment) {
		char	*p = comment;

		while (p = strrchr(comment, '\n')) {
			lines = addLine(lines, strdup(p+1));
			*p = 0;
		}
		lines = addLine(lines, strdup(comment));
	} else if (file) {
		unless (lines = readFile(file, 0)) return (1);
	}
	unless (editor = getenv("EDITOR")) editor = "vi";

	for (name = sfileFirst("comment", &av[optind], SF_NODIREXPAND);
	    name; name = sfileNext()) {
		unless (s = sccs_init(name, 0, 0)) continue;
		unless (HASGRAPH(s)) goto next;
		unless (d = sccs_getrev(s, rev, 0, 0)) {
			fprintf(stderr, "%s|%s not found\n", s->gfile, rev);
			goto next;
		}
		unless (lines) {
			lines = editComments(s, d);
			freeLines(d->comments);
			d->comments = lines;
			lines = 0;
		} else {
			freeLines(d->comments);
			d->comments = lines;
		}
		if (d->comments) sccs_newchksum(s);
next:		sccs_free(s);
	}
	return (0);
}

private char **
editComments(sccs *s, delta *d)
{
	char	tmp[MAXPATH];
	FILE	*f;
	char	**lines;
	int	i;
	char	buf[200];

	gettemp(tmp, "cmt");
	unless (f = fopen(tmp, "w")) {
		perror(tmp);
		return (0);
	}
	sprintf(buf, "# Change the comment to %s@%s below and exit.\n",
	    s->gfile, d->rev);
	fputs(buf, f);
	EACH(d->comments) {
		fprintf(f, "%s\n", d->comments[i]);
	}
	fclose(f);
	sys(editor, tmp, SYS);
	lines = readFile(tmp, buf);
	unlink(tmp);
	return (lines);
}

private char **
readFile(char *file, char *delete)
{
	FILE	*f = fopen(file, "r");
	char	buf[1024];
	char	**lines = 0;

	unless (f) {
		perror(file);
		return (0);
	}
	while (fnext(buf, f)) {
		if (delete && streq(delete, buf)) {
			delete = 0;
			continue;
		}
		delete = 0;
		chomp(buf);
		lines = addLine(lines, strdup(buf));
	}
	fclose(f);
	return (lines);
}
