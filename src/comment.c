/*
 * Copyright 2002-2005,2008,2010-2011,2013 BitMover, Inc
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

private	char	**readFile(char *file);
private char	**getfiles(char *csetrev, int standalone);
private void	write_editfile(FILE *f, char **files, int to_stdout);
private void	read_editfile(FILE *f);

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
	char	*name, *file = 0, *comment = 0, *rev = 0;
	int	c;
	char	**files = 0;
	char	**lines = 0;
	char	*csetrev = 0;
	int	to_stdout = 0;
	int	standalone = 0;
	char	tmp[MAXPATH];
	FILE	*tf;
	longopt	lopts[] = {
		{ "standalone", 'S' },
		{ 0, 0 }
	};

	while ((c = getopt(ac, av, "C:pr:Sy:Y:", lopts)) != -1) {
		switch (c) {
		    case 'C': csetrev = optarg; break;
		    case 'p': to_stdout = 1; break;
		    case 'S': standalone = 1; break;
		    case 'y': comment = optarg; break;
		    case 'Y': file = optarg; break;
		    case 'r': rev = optarg; break;
		    default: bk_badArg(c, av);
		}
	}
	if (comment) {
		comments_save(comment);
		lines = comments_return(0);
	} else if (file) {
		unless (lines = readFile(file)) return (1);
	}
	unless (editor = getenv("EDITOR")) editor = EDITOR;

	if (streq(av[ac-1], "-")) {
		unless (ac-1 == optind) usage();
		/*
		 * Is it OK that we broke reading files to edit from
		 * stdin?
		 */
		if (proj_cd2root()) {
			fprintf(stderr,
			    "comments: can't find repository root\n");
			exit(1);
		}
		unless (standalone) proj_cd2product();
		read_editfile(stdin);
		return (0);
	}
	/* disable illegal combinations */
	if ((rev && csetrev) ||
	    (!av[optind] && rev) || (av[optind] && csetrev)) {
		usage();
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
		unless (standalone) proj_cd2product();
		files = getfiles(csetrev, standalone);
	} else {
		unless (rev) rev = "+";
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
	bktmp(tmp);
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
	} else {
		unless (lines) {
			if (proj_cd2root()) {
				fprintf(stderr,
				    "comments: can't find repository root\n");
				exit(1);
			}
			unless (standalone) proj_cd2product();
			sys(editor, tmp, SYS);
		}
		unless (tf = fopen(tmp, "r")) {
			perror(tmp);
			return (1);
		}
		read_editfile(tf);
	}
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
getfiles(char *csetrev, int standalone)
{
	char	**files = 0;
	char	*av[30];
	int	i;
	char	*rev = aprintf("-r%s", csetrev);
	int	status;
	FILE	*f;
	char	buf[MAXLINE];
	char	*t;

	av[i=0] = "bk";
	av[++i] = "rset";
	if (standalone) av[++i] = "-S";
	av[++i] = rev;
	av[++i] = 0;

	unless (f = popenvp(av, "r")) {
		perror("popenvp");
		return(0);
	}
	free(rev);
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
	/* Makes it easier to find stuff if you are wacking a big cset */
	sortLines(files, 0);

	status = pclose(f);
	if (!WIFEXITED(status) || (WEXITSTATUS(status) != 0)) {
		freeLines(files, free);
		return (0);
	}
	return (files);
}

private void
write_editfile(FILE *f, char **files, int to_stdout)
{
	int	i;
	project	*proj = proj_init(".");

	EACH(files) {
		sccs	*s;
		ser_t	d;
		char	*t, *name;

		name = files[i];
		t = strchr(name, BK_FS);
		*t = 0;
		s = sccs_init(name, 0);
		*t++ = BK_FS;
		unless (s && HASGRAPH(s)) goto next;
		unless (d = sccs_findrev(s, t)) {
			fprintf(stderr, "%s|%s not found\n", s->gfile, t);
			goto next;
		}
		name = _relativeName(s->gfile, 0, 0, 0, proj);
		if (to_stdout) {
			fprintf(f, "### Comments for %s%c%s\n",
			    name, BK_FS, REV(s, d));
		} else {
			fprintf(f, "### Change the comments to %s%c%s below\n",
			    name, BK_FS, REV(s, d));
		}
		fputs(COMMENTS(s, d), f);
		fprintf(f, "\n");
next:		if (s) sccs_free(s);
	}
	if (proj) proj_free(proj);
}

/*
 * Change the comments, automatically trimming trailing blank lines.
 * This will not let them remove comments.
 */
private int
change_comments(char *file, char *rev, char **comments)
{
	sccs	*s = 0;
	ser_t	d;
	char	*sfile = 0;
	int	i;
	int	rc = 1;

	cmdlog_lock(CMD_WRLOCK);
	sfile = name2sccs(file);
	s = sccs_init(sfile, 0);
	unless (s && HASGRAPH(s) && (d = sccs_findrev(s, rev))) {
		fprintf(stderr, "%s|%s not found, comments not updated\n",
		    file, rev);
		goto err;
	}
	/* trim trailing blank lines */
	while ((i = nLines(comments)) && streq(comments[i], "")) {
		removeLineN(comments, i, free);
	}
	if (emptyLines(comments)) goto err;
	comments_set(s, d, comments);
	freeLines(comments, free);
	sccs_newchksum(s);
	rc = 0;
 err:	if (s) sccs_free(s);
	if (sfile) free(sfile);
	return (rc);
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
			unless (last_file) {
				notice("comments-badfmt", 0, "-e");
				exit(1);
			}
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
