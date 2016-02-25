/*
 * Copyright 1997-2010,2013-2016 BitMover, Inc
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

#include "sccs.h"

/*
 * sfiles.c - s.file file name processing.
 *
 * File name expansion.
 *	<dir> means <dir>/SCCS/s.* if <dir>/SCCS exists
 *		XXX - shouldn't it really mean `bk gfiles dir`???
 *	<dir> means <dir>/s.* if <dir>/SCCS doesn't exist
 *	NULL means SCCS/s.* if SCCS exists
 *	NULL means ./s.* if SCCS doesn't exist
 *	<files> means <files>
 *	- means read files from stdin until EOF.
 *
 * char	*sfileFirst(av, flags);
 * char	*sfileNext();
 * char *sfileRev();
 * void sfileDone();
 *
 * It is a fatal error to call sfileFirst() without calling sfileDone() or
 * before sfileNext() returned NULL.
 */
struct opts {
	int	flags;		/* saved flags */
	char	**av;		/* saved copy of argv */
	int	ac;		/* where we are - starts at 0 */
	FILE	*flist;		/* set if getting files from stdin */
	char	**d;		/* getdir() of directory we are reading */
	int	di;		/* index into d */
	char	*prefix;	/* path/to/dir/SCCS/ */
	char	sfile[MAXPATH];	/* pathname we actually pass back */
	char	*lprog;		/* av[0], sort of */
	char	*glob;		/* if set, filter through this */
	DATA	rev;		/* 1.1.1.1, keys, tags, etc - see HASREVS */
} *opts;
private	pid_t spid;		/* pid of sfiles for -r */

private	int oksccs(char *s, int flags, int complain);

/*
 * Get the next file and munge it into an s.file name.
 */
char *
sfileNext(void)
{
	char	*name, *r;
	char	*line;
	size_t	len;
	char	buf[MAXPATH];

	assert(opts);
	if (opts->flist) {
		unless(line = fgetln(opts->flist, &len)) {
			if (opts->flist != stdin) fclose(opts->flist);
			opts->flist = 0;
			return (0);
		}
		/* Inline chomp() since fgetln() doesn't return a NULL
		 * terminated string. */
		if (len && (line[len-1] == '\n')) {
			line[--len] = 0;
		} else {
			/* handle truncated line from sfiles being
			 * killed */
			if (feof(opts->flist) && sfilesDied(0, 0)) return (0);
		}
		while (len && (line[len-1] == '\r')) line[--len] = 0;
		debug((stderr, "sfiles::FILE got %s\n", line));
	} else if (opts->d) {
		while (opts->di <= nLines(opts->d)) {
			/*
			 * readdir returns the base name only, must re-construct
			 * the relative path. Otherwise oksccs will be checking
			 * the wrong file.
			 * See test case "A/" in "basic test" of regression test
			 */
			concat_path(buf, opts->prefix, opts->d[opts->di]);
			opts->di++;
			/* I thought I didn't need this but I was wrong. */
			unless (oksccs(buf, opts->flags, 0)) continue;
			if ((opts->flags & SF_NOCSET) && isCsetFile(buf)) {
				continue;
			}
			debug((stderr, "sfiles::DIR got %s\n", buf));
			line = buf;
			goto norev;
		}
		return (0);
	} else if (opts->av) {
		unless (line = opts->av[opts->ac++]) {
			opts->av = 0;
			return (0);
		}
		debug((stderr, "sfiles::AV got %s\n", line));
	} else {
		/* none of the above */
		return (0);
	}

	if (!(opts->flags & SF_NOHASREVS) && (r = strchr(line, BK_FS))) {
		*r++ = 0;
		data_resize(&opts->rev, strlen(r)+1);
		strcpy(opts->rev.buf, r);
		debug((stderr, "sfiles::REV got %s\n", opts->rev));
		/*
		 * XXX - this works for diffs but may or may not be the right
		 * long term answer.
		 */
		opts->flags &= ~SF_GFILE;
	} else {
norev:		if (opts->rev.buf) opts->rev.buf[0] = 0;
	}
	assert(strlen(line) < MAXPATH);
	localName2bkName(line, line);
	cleanPath(line, buf);
	unless (sccs_filetype(buf) == 's') {
		name = name2sccs(buf);
		strcpy(buf, name);
		free(name);
	}
	if (opts->glob) {
		char	*f = strrchr(buf, '/');

		if (f) {
			f += 3;
		} else {
			f = &buf[2];
		}
		unless (match_one(f, opts->glob, 0)) return (sfileNext());
	}
	if (oksccs(buf, opts->flags, !(opts->flags & SF_SILENT))) {
		strcpy(opts->sfile, buf);
		return (opts->sfile);
	}
	return (sfileNext());
}

char *
sfileRev(void)
{
	return ((opts && opts->rev.buf && opts->rev.buf[0])
	    ? opts->rev.buf : 0);
}

int
sfiles_glob(char *glob)
{
	if (getenv("BK_NO_FILE_GLOB")) return (0);
	return (!strchr(glob, '/') && is_glob(glob));
}

/*
 * Initialization - figure out where we are getting the names from and
 * set it up.  This routine doesn't actually get any names, it calls
 * sfileNext() to do that.
 */
char *
sfileFirst(char *cmd, char **Av, int Flags)
{
	int	i;
	char	*dir;

	assert(!opts);
	opts = new(struct opts);
	if (sfilesDied(0, 0)) return (0);
	opts->lprog = cmd;
	opts->flags = Flags;
	if (getenv("BK_NODIREXPAND")) opts->flags |= SF_NODIREXPAND;
	if (Av[0]) {
		/*
		 * if the last arg looks like a glob, then save it and
		 * remove it from the command line.
		 */
		for (i = 0; Av[i+1]; ++i);
		if (streq("-", Av[i])) {
			if ((i > 0) && sfiles_glob(Av[i-1])) {
				opts->glob = strdup(Av[i-1]);
				Av[i-1] = "-";
				Av[i] = 0;
			}
		} else if (sfiles_glob(Av[i])) {
			opts->glob = strdup(Av[i]);
			Av[i] = 0;
		}
	}
	if (Av[0]) localName2bkName(Av[0], Av[0]);
	if (Av[0] && streq("-", Av[0])) {
		// read from stdin
		if (Av[1]) {
			fprintf(stderr,
			    "%s: - option must be alone.\n",
			    opts->lprog);
			return (0);
		}
		opts->flist = stdin;
	} else if (!Av[0] || isdir(Av[0])) {
		// single directory (or default to ".")
		if (opts->flags & SF_NODIREXPAND) return (0);
		if (Av[0] && Av[1]) {
			fprintf(stderr,
			    "%s: directory must be alone.\n",
			    opts->lprog);
			return (0);
		}
		opts->prefix = Av[0]
		    ? aprintf("%s/SCCS", Av[0]) : strdup("SCCS");
		opts->di = 1;
		unless (opts->d = getdir(opts->prefix)) {
			/* trim off the "SCCS" part and try again */
			if (Av[0]) {
				opts->prefix[strlen(opts->prefix) - 4] = 0;
			} else {
				opts->prefix[0] = 0;
			}
			dir = opts->prefix[0] ? opts->prefix : ".";
			unless (opts->d = getdir(dir)) {
				perror(dir);
			}
		}
	} else {
		// files on command line
		opts->av = Av;
		opts->ac = 0;
	}
	return (sfileNext());
}

int
sfileDone(void)
{
	assert(opts);
	if (opts->rev.buf) free(opts->rev.buf);
	if (opts->flist && (opts->flist != stdin)) fclose(opts->flist);
	if (opts->d) freeLines(opts->d, free);
	if (opts->glob) free(opts->glob);
	if (opts->prefix) free(opts->prefix);
	free(opts);
	opts = 0;

	/* wait for sfiles to exit, kill() if needed */
	return (sfilesDied(1, 1));
}

int
sfiles(char **av)
{
	int	pfd;
	char	ignore[] = "BitKeeper/etc/ignore";

	/*
	 * Match sfiles.c - try to get only if writable.
	 * Avoids ugly error message.
	 */
	if (!exists(ignore) && writable("BitKeeper/etc")) {
		get(ignore, SILENT);
	}
	if ((spid = spawnvpio(0, &pfd, 0, av)) == -1) {
		fprintf(stderr, "cannot spawn bk gfiles\n");
		return (1);
	}
	dup2(pfd, 0);
	close(pfd);
	return (0);
}

int
sfilesDied(int wait, int killit)
{
	int	ret, opt;
	static	int sfilesRet;

	if (spid > 0) {
		opt = (wait || killit) ? 0 : WNOHANG;
		if (wait && (spid == waitpid(spid, &ret, 0))) goto err;
		if (killit) {
			if (kill(spid, SIGTERM) && (errno != ESRCH)) {
				kill(spid, SIGKILL);
			}
		}
		if (spid == waitpid(spid, &ret, opt)) {
err:			if (WIFEXITED(ret)) {
				sfilesRet = WEXITSTATUS(ret);
			} else if (WIFSIGNALED(ret)) {
				sfilesRet = WTERMSIG(ret);
			} else {
				sfilesRet = 1;
			}
			spid = 0;
		}
	}
	return (sfilesRet);
}

/*
 * return true if the file is an sfile, plus a couple more optional
 * checks
 */
private int
oksccs(char *sfile, int flags, int complain)
{
	char	*g;
	int	ok;
	int	rc = 0;		/* default == no OK */
	struct	stat sbuf;

	unless (sccs_filetype(sfile) == 's') {
		if (complain)
			fprintf(stderr, "%s: not an s.file: %s\n",
			    opts->lprog, sfile);
		return (0);
	}
	g = sccs2name(sfile);
	if (flags & (SF_GFILE|SF_WRITE_OK)) {
		ok = (lstat(g, &sbuf) == 0);
	} else {
		ok = 1;
	}
	if ((flags&SF_GFILE) && !ok) {
		if (complain) {
			unless (exists(sfile)) {
				fprintf(stderr,
				    "%s: neither '%s' nor '%s' exists.\n",
				    opts->lprog, g, sfile);
			} else {
				fprintf(stderr,
				    "%s: no such file: %s\n", opts->lprog, g);
			}
		}
	} else if ((flags&SF_WRITE_OK) && (!ok || !(sbuf.st_mode & 0200))) {
		if (complain) {
			fprintf(stderr,
			    "%s: %s: no write permission\n", opts->lprog, g);
		}
	} else {
		rc = 1;
	}
	free(g);
	return (rc);
}

#ifdef        MAIN
main(int ac, char **av)
{
	char	*name;
	sccs	*s;

	for (name = sfileFirst(&av[1], 0); name; name = sfileNext()) {
		printf("NAME %s\n", name);
		if (s = sccs_init(name, 0)) {
			printf("\t%s\n\t%s\n",
			    s->sfile, s->gfile);
			sccs_free(s);
		}
		rev = sfileRev();
	}
	sfileDone();
}
#endif
