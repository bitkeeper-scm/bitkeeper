/* Copyright (c) 2002 L.W.McVoy */
#include "system.h"
#include "sccs.h"

private	char	**readFile(char *file);
private char	**getfiles(char *csetrev);
private void	write_editfile(FILE *f, char **files, int to_stdout);
private void	read_editfile(FILE *f);

extern	char	*editor;


private void
usage(void)
{
	system("bk help -s comment");
	exit(1);
}

/*
 * comment - all the updating of checkin comments
 *
 * bk comments [-y<new>] [-r<rev>] files
 *
 * If you don't pass it a comment, it pops you into the editor on the old one.
 *
 * TODO - do not allow them to change comments in pulled changesets.
 */
int
comments_main(int ac, char **av)
{
	char	*name, *file = 0, *comment = 0, *rev = "+";
	int	c;
	char	**files = 0;
	char	**lines = 0;
	char	*csetrev = 0;
	int	to_stdout = 0;
	char	tmp[MAXPATH];
	FILE	*tf;
	
	while ((c = getopt(ac, av, "C|pr|y|Y|")) != -1) {
		switch (c) {
		    case 'C': csetrev = optarg; break;
		    case 'p': to_stdout = 1; break;
		    case 'y': comment = optarg; break;
		    case 'Y': file = optarg; break;
		    case 'r': rev = optarg; break;
		    default: usage(); break;
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
		unless (lines = readFile(file)) return (1);
	}
	unless (editor = getenv("EDITOR")) editor = EDITOR;

	if (av[optind] && streq(av[optind], "-")) {
		/*
		 * Is it OK that we broke reading files to edit from
		 * stdin?
		 */
		read_editfile(stdin);
		return (0);
	}

	/*
	 * Get list of files/revs to edit.  It is either the set if files
	 * in a changeset or a list of files on the command line.
	 */
	if (csetrev) {
 cset:		if (proj_cd2root()) {
			fprintf(stderr,
			    "comments: can't find repository root\n");
			exit(1);
		}
		files = getfiles(csetrev);
	} else {
		for (name = sfileFirst("comment", &av[optind], SF_NODIREXPAND);
		     name; name = sfileNext()) {
			files = addLine(files, 
			    aprintf("%s%c%s", name, BK_FS, rev));
		}
		unless (files) {
			csetrev = "+";
			goto cset;
		}
	}
	bktmp(tmp, "cmt");
	unless (tf = fopen(tmp, "w")) {
		perror(tmp);
		return (1);
	}
	if (lines) {
		int	i;

		/* set all files to same comment */
		EACH (files) {
			int	j;
			char	*sfile = strdup(files[i]);
			char	*rev;
			char	*gfile;
			rev = strchr(sfile, BK_FS);
			*rev++ = 0;
			gfile = sccs2name(sfile);
			
			fprintf(tf, "### Change the comments to %s%c%s below\n",
			    gfile, BK_FS, rev);
			free(gfile);
			free(sfile);
			EACH_INDEX(lines, j) {
				fprintf(tf, "%s\n", lines[j]);
			}
			fprintf(tf, "\n");
		}
	} else {
		write_editfile(tf, files, to_stdout);
	}
	fclose(tf);
	if (to_stdout) {
		char	buf[MAXLINE];
		int	cnt;

		unless (tf = fopen(tmp, "r")) {
			perror(tmp);
			return (1);
		}
		while (cnt = fread(buf, 1, sizeof(buf), tf)) {
			fwrite(buf, 1, cnt, stdout);
		}
		fclose(tf);
		unlink(tmp);
		return (0);
	}
	unless (lines) sys(editor, tmp, SYS);
	unless (tf = fopen(tmp, "r")) {
		perror(tmp);
		return (1);
	}
	read_editfile(tf);
	fclose(tf);
	unlink(tmp);
	return (0);
}

private char **
readFile(char *file)
{
        FILE    *f = fopen(file, "r");
        char    buf[1024];
        char    **lines = 0;

        unless (f) {
                perror(file);
                return (0);
        }
        while (fnext(buf, f)) {
                chomp(buf);
                lines = addLine(lines, strdup(buf));
        }
        fclose(f);
        return (lines);
}


private char **
getfiles(char *csetrev)
{
	char	**files = 0;
	char	*av[30];
	int	i;
	char	*rev = aprintf("-r%s", csetrev);
	pid_t	pid;
	int	status;
	int	pfd;
	FILE	*f;
	char	buf[MAXLINE];
	char	*t;

	av[i=0] = "bk";
	av[++i] = "rset";
	av[++i] = rev;
	av[++i] = 0;
	
	pid = spawnvp_rPipe(av, &pfd, 0);
	if (pid == -1) {
		perror("spawnvp_rPipe");
		return (0);
	}
	free(rev);
	unless (f = fdopen(pfd, "r")) {
		perror("fdopen");
		return(0);
	}
	while (fgets(buf, sizeof(buf), f)) {
		char	*sfile;
		chomp(buf);
		t = strchr(buf, BK_FS);
		assert(t);
		*t++ = 0;
		t = strstr(t, "..");
		assert(t);
		t += 2;
		sfile = name2sccs(buf);
		files = addLine(files, aprintf("%s%c%s", sfile, BK_FS, t));
		free(sfile);
	}

	waitpid(pid, &status, 0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		freeLines(files, free);
		return (0);
	}
	return (files);
}

private void
write_editfile(FILE *f, char **files, int to_stdout)
{
	int	i;
	
	EACH(files) {
		sccs	*s;
		delta	*d;
		char	*t;
		char	*name;
		int	j;

		name = strdup(files[i]);
		t = strchr(name, BK_FS);
		*t++ = 0;
		
		unless (s = sccs_init(name, 0)) continue;
		unless (HASGRAPH(s)) goto next;
		unless (d = sccs_getrev(s, t, 0, 0)) {
			fprintf(stderr, "%s|%s not found\n", s->gfile, t);
			goto next;
		}
		if (to_stdout) {
			fprintf(f, "### Comments for %s%c%s\n",
			    s->gfile, BK_FS, d->rev);
		} else {
			fprintf(f, "### Change the comments to %s%c%s below\n",
			    s->gfile, BK_FS, d->rev);
		}
		EACH_INDEX(d->comments, j) {
			fprintf(f, "%s\n", d->comments[j]);
		}
		fprintf(f, "\n");
next:		sccs_free(s);
		free(name);
	}
}

/*
 * Change the comments, automatically trimming trailing blank lines.
 * This will not let them remove comments.
 */
private void
change_comments(char *file, char *rev, char **comments)
{
	sccs	*s = 0;
	delta	*d;
	char	*sfile = 0;
	int	i;
	
	sfile = name2sccs(file);
	unless (s = sccs_init(sfile, 0)) goto err;
	unless (HASGRAPH(s)) goto err;
	unless (d = sccs_getrev(s, rev, 0, 0)) {
		fprintf(stderr, "%s|%s not found\n", s->gfile, rev);
		goto err;
	}
	EACH(comments);
	for (i--; i > 0; i--) {
		unless (streq(comments[i], "")) break;
		free(comments[i]);
		comments[i] = 0;
	}
	unless (comments && comments[1]) goto err;
	freeLines(d->comments, free);
	d->comments = comments;
	if (d->comments) sccs_newchksum(s);
 err:	if (s) sccs_free(s);
	if (sfile) free(sfile);
}

private void
read_editfile(FILE *f)
{
	char	buf[MAXLINE];
	char	**comments = 0;
	char	*last_file = 0;
	char	*last_rev = 0;
	
	while (fgets(buf, sizeof(buf), f)) {
		char	file[MAXPATH];
		char	*rev;

		chomp(buf);
		if (sscanf(buf,
			"### Change the comments to %s below", file) == 1 ||
		    sscanf(buf,
			"### Comments for %s", file) == 1) {
			rev = strchr(file, BK_FS);
			if (rev) {
				*rev++ = 0;
			} else {
				rev = "+";
			}
			if (last_file) {
				change_comments(last_file, last_rev, comments);
				free(last_file);
				free(last_rev);
				comments = 0;
			}
			last_file = strdup(file);
			last_rev = strdup(rev);
		} else {
			comments = addLine(comments, strdup(buf));
		}
	}
	if (last_file) {
		change_comments(last_file, last_rev, comments);
		free(last_file);
		free(last_rev);
		comments = 0;
	}
}
