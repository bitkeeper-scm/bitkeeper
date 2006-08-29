#include "system.h"
#include "sccs.h"

/*
 * sfiles.c - s.file file name processing.
 *
 * File name expansion.
 *	<dir> means <dir>/SCCS/s.* if <dir>/SCCS exists
 *		XXX - shouldn't it really mean `bk sfiles dir`???
 *	<dir> means <dir>/s.* if <dir>/SCCS doesn't exist
 *	NULL means SCCS/s.* if SCCS exists
 *	NULL means ./s.* if SCCS doesn't exist
 *	<files> means <files>
 *	- means read files from stdin until EOF.
 *
 * XXX - if you have a file s.foo, "foo" doesn't work.  OK?
 * XXX - if SCCS/SCCS exists, that bites.
 *
 * char	*sfileFirst(av, flags);
 * char	*sfileNext();
 * void sfileDone();
 *
 * It is a fatal error to call sfileFirst() without calling sfileDone() or
 * before sfileNext() returned NULL.
 */
private	int flags;		/* saved flags */
private	char **av;		/* saved copy of argv */
private	int ac;			/* where we are - starts at 0 */
private	FILE *flist;		/* set if getting files from stdin */
private	char **d;		/* getdir() of directory we are reading */
private	int di;			/* index into d */
private	char prefix[MAXPATH];	/* path/to/dir/SCCS/ */
private	char buf[MAXPATH];	/* pathname we actually pass back */
private	int unget;		/* if set, return path again */
private	char *prog;		/* av[0], sort of */
private	char *glob;		/* if set, filter through this */
private	char rev[MAXREV+1];	/* 1.1.1.1 - see HASREVS */
private	pid_t spid;		/* pid of sfiles for -r */

private	int oksccs(char *s, int flags, int complain);
private	int sfilesDied(int killit);

/*
 * Get the next file and munge it into an s.file name.
 */
char *
sfileNext(void)
{
	char	*name;

	if (unget) {
		unget = 0;
		return (buf);
	}
again:
	if (!flist && !d && !av) {
		return (0);
	}
	if (flist) {
		unless (fgets(buf, sizeof(buf), flist)) {
			if (flist != stdin) fclose(flist);
			flist = 0;
			return (0);
		}
		unless (chomp(buf)) {
			/* handle truncated buffer from sfiles being killed */
			if (feof(flist) && sfilesDied(0)) return (0);
		}
		debug((stderr, "sfiles::FILE got %s\n", buf));
	} else if (d) {
		while (di <= nLines(d)) {
			/*
			 * readdir returns the base name only, must re-construct
			 * the relative path. Otherwise oksccs will be checking
			 * the wrong file.
			 * See test case "A/" in "basic test" of regression test
			 */
			concat_path(buf, prefix, d[di]);
			di++;
			/* I thought I didn't need this but I was wrong. */
			unless (oksccs(buf, flags, 0)) continue;
			if ((flags & SF_NOCSET) && isCsetFile(buf)) {
				continue;
			}
			debug((stderr, "sfiles::DIR got %s\n", buf));
			goto norev;
		}
		freeLines(d, free);
		d = 0;
		return (0);
	} else if (av) {
		unless (name = av[ac++]) {
			av = 0;
			return (0);
		}
		strcpy(buf, name);
		localName2bkName(buf, buf);
		cleanPath(buf, buf); /* for win98 */
		debug((stderr, "sfiles::AV got %s\n", buf));
	}

	unless (flags & SF_NOHASREVS)  {
		char	*r = strchr(buf, BK_FS);

		rev[0] = 0;	/* paranoia is your friend */
		if (!r) goto norev;
		*r++ = 0;
		strcpy(rev, r);
		debug((stderr, "sfiles::REV got %s\n", rev));
		/*
		 * XXX - this works for diffs but may or may not be the right
		 * long term answer.
		 */
		flags &= ~SF_GFILE;
	}
norev:
	unless (sccs_filetype(buf) == 's') {
#ifdef	ATT_SCCS
		fprintf(stderr, "Not an SCCS file: %s\n", buf);
		goto again;
#endif
		name = name2sccs(buf);
		unless (name) goto again;
		strcpy(buf, name);
		free(name);
	}
	if (glob) {
		char	*f = strrchr(buf, '/');

		if (f) {
			f += 3;
		} else {
			f = &buf[2];
		}
		unless (match_one(f, glob, 0)) goto again;
	}
	if (oksccs(buf, flags, !(flags & SF_SILENT))) {
		return (buf);
	}
	goto again;
}

char *
sfileRev(void)
{
	return (rev[0] ? rev : 0);
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
	if (sfilesDied(0)) return (0);
	rev[0] = 0;
	prog = cmd;
	flags = Flags;
	if (Av[0]) {
		int	i;

		for (i = 0; Av[i+1]; ++i);
		if (streq("-", Av[i])) {
			if ((i > 0) && sfiles_glob(Av[i-1])) {
				glob = strdup(Av[i-1]);
				Av[i-1] = "-";
				Av[i] = 0;
			}
		} else if (sfiles_glob(Av[i])) {
			glob = strdup(Av[i]);
			Av[i] = 0;
		}
	}
	if (Av[0]) {
		if (streq("-", Av[0])) {
			if (Av[1]) {
				fprintf(stderr,
				    "%s: - option must be alone.\n", prog);
				sfileDone();
				return (0);
			}
			flist = stdin;
			flags |= SF_SILENT;
			return (sfileNext());
		}
		localName2bkName(Av[0], Av[0]);
		if (isdir(Av[0])) {
			if (flags & SF_NODIREXPAND) return (0);
			if (Av[1]) {
				fprintf(stderr,
				    "%s: directory must be alone.\n", prog);
				sfileDone();
				return (0);
			}
			concat_path(prefix, Av[0], "SCCS");
			di = 1;
			unless (d = getdir(prefix)) {
				/*
				 * trim off the "SCCS" part
				 * and try again
				 */
				prefix[strlen(prefix) - 4] = 0;
				unless (d = getdir(prefix)) {
					perror(prefix);
				}
			}
			return (sfileNext());
		}
		av = Av;
		ac = 0;
		return (sfileNext());
	}
	if (flags & SF_NODIREXPAND) return (0);
	unless (d) {
		di = 1;
		strcpy(prefix, "SCCS");
		unless (d = getdir("SCCS")) {
			/*
			 * trim off the "SCCS" part
			 * and try again
			 */
			prefix[0] = 0;
			d = getdir(".");
		}
	}
	return (sfileNext());
}

int
sfileDone(void)
{
	int	ret;

	if (av) {
		av = 0;
		ac = 0;
	} else if (d) {
		freeLines(d, free);
		d = 0;
	} else if (flist) {
		if (flist != stdin) fclose(flist);
		flist = 0;
	}
	if (glob) free(glob);
	glob = 0;
	prog = "";
	if (ret = sfilesDied(0)) {
		return (ret);
	}
	sfilesDied(1); /* kill sfiles */
	return (0);
}

int
sfiles(char *opts)
{
	int	pfd;
	char	*sav[4] = {"bk", "sfiles", 0, 0};
	char	ignore[] = "BitKeeper/etc/ignore";

	unless (exists(ignore)) get(ignore, SILENT, "-");
	sav[2] = opts;
	if ((spid = spawnvp_rPipe(sav, &pfd, 0)) == -1) {
		fprintf(stderr, "cannot spawn bk sfiles\n");
		return (1);
	}
	dup2(pfd, 0);
	close(pfd);
	return (0);
}

int
sfilesDied(int killit)
{
	int	ret, opt;
	static	int sfilesRet = 0;

	if (spid) {
		opt = WNOHANG;
		if (killit) {
			kill(spid, SIGTERM);
			opt = 0;
		}
		if (spid == waitpid(spid, &ret, opt)) {
			if (WIFEXITED(ret)) {
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

private int
oksccs(char *sfile, int flags, int complain)
{
	char	*g;
	int	ok;
	struct	stat sbuf;

	unless (sccs_filetype(buf) == 's') {
		if (complain)
			fprintf(stderr, "%s: not an s.file: %s\n", prog, sfile);
		return (0);
	}
	g = sccs2name(sfile);
	ok = lstat(g, &sbuf) == 0;
	if ((flags&SF_GFILE) && !ok) {
		if (complain) {
			unless (exists(sfile)) {
				fprintf(stderr,
				    "%s: neither '%s' nor '%s' exists.\n",
				    prog, g, sfile);
			} else {
				fprintf(stderr,
				    "%s: no such file: %s\n", prog, g);
			}
		}
		free(g);
		return (0);
	}
	if ((flags&SF_WRITE_OK) && (!ok || !(sbuf.st_mode & 0200))) {
		if (complain)
			fprintf(stderr,
			    "%s: %s: no write permission\n", prog, g);
		free(g);
		return (0);
	}
	free(g);
	return (1);
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
	}
}
#endif

