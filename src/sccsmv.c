/* Copyright (c) 1997 L.W.McVoy */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

/*
 * Emulate mv(1)
 *
 * usage: mv a b
 * usage: mv a [b c d ...] dir
 */
int
mv_main(int ac, char **av)
{
	char	*dest;
	int	isDir;
	int	isUnDelete = 0;
	int	errors = 0, skip_lock = 0;
	int	dofree = 0;
	int	force = 0;
	int	i;
	int	c;

	has_proj("mv");
	if (ac == 2 && streq("--help", av[1])) {
		system("bk help mv");
		return (0);
	}

	while ((c = getopt(ac, av, "ful")) != -1) {
		switch (c) {
		    case 'f':	force = 1; break;
		    case 'l':	skip_lock = 1; break;
		    case 'u':	isUnDelete = 1; break;
		    default:	system("bk help -s mv");
				return (1);
		}
	}

	debug_main(av);
	if (ac < 3) {
		system("bk help -s mv");
		return (1);
	}
	dest = av[ac-1];
	localName2bkName(dest, dest);
	cleanPath(dest, dest);
	if (sccs_filetype(dest) == 's') {
		dest = sccs2name(dest);
		dofree++;
	}
	if (strchr(dest, BK_FS)) {
		fprintf(stderr, "%c is not allowed in pathname\n", BK_FS);
		return (1);
	}

	isDir = isdir(dest);
	if (ac - (optind - 1) > 3 && !isDir) {
		fprintf(stderr,
		    "Multiple files must be moved to a directory!\n");
		return (1);
	}
	av[ac-1] = 0;

        if (!skip_lock && repository_wrlock()) {
                fprintf(stderr, "mv: unable to write lock repository\n");
                return (1);
        }

	/*
	 * Five cases
	 * 1) File -> File
	 * 2) File -> Existing Dir
	 * 3) File -> Non-Existing Dir (error case)
	 * 4) Dir -> non-Existing Dir
	 * 5) Dir -> Existing Dir
	 */
	for (i = optind; i < (ac - 1); i++) {
		localName2bkName(av[i], av[i]);
		if (isdir(av[i])) {
			errors |= sys("bk", "mvdir", "-l", av[i], dest, SYS);
		} else {
			errors |= sccs_mv(av[i], dest,
						isDir, 0, isUnDelete, force);
		}
	}
	if (dofree) free(dest);
	unless (skip_lock) repository_wrunlock(0);
	return (errors);
}

int
mvdir_main(int ac, char **av)
{
	int	rc = 0, skip_lock = 0;
	int	c, fix_pfile;
	char	*freeme = NULL;
	char	*cmd, *p, *rev, *from, *to;
	char	tempfile[MAXPATH], buf[MAXPATH];
	FILE	*f;
	sccs	*s = NULL;
	pfile   pf;

	if (ac == 2 && streq("--help", av[1])) {
usage:		system("bk help mv"); /* bk mv is prefered interface	*/
				      /* bk mvdir is just a supporting	*/
				      /* command used by bk mv		*/
		return (rc);
	}

	while ((c = getopt(ac, av, "l")) != -1) {
		switch (c) {
		    case 'l':	skip_lock = 1; break; /* internal interface */
		    default:	goto usage;
		}
	}

	if ((ac - optind + 1) != 3) {
		rc = 1;
		goto usage;
	}

	from = av[optind];
	to = av[optind + 1];

	unless (isdir(from)) {
		fprintf(stderr, "%s is not a directory\n", from);
		return (1);
	}

	if (exists(to) && !isdir(to)) {
		fprintf(stderr, "%s exists and is not a directory\n", to);
		return (1);
	}

        if (!skip_lock && repository_wrlock()) {
                fprintf(stderr, "mv: unable to write lock repository\n");
                return (1);
        }

	localName2bkName(from, from);
	localName2bkName(to, to);
	if (isdir(to)) {
		freeme = to = aprintf("%s/%s", to, basenm(from));
	}

	/*
	 * Do not allow moving BitKeeper files, this test also
	 * protect the ChangeSet file; this is because ChangeSet
	 * is located at the root of the tree, 
	 * "bk mvdir project-root" => moving the BitKeeper sub-tree.
	 */
	bktmp(tempfile, "bk_mvdir");
	cmd = aprintf("bk sfiles \"%s\" | "
			"bk prs -hr1.1 -nd:DPN: - | grep BitKeeper/ > %s",
			from, tempfile);
	system(cmd);
	free(cmd);

	if (size(tempfile) != 0) {
		fprintf(stderr,
		    "Moving directories with BitKeeper files not allowed\n");
		unlink(tempfile);
err:		unless (skip_lock) repository_wrunlock(0);
		if (s) sccs_free(s);
		return (1);
	}
	unlink(tempfile);
	if (sys("bk", "-r", "check", "-a", SYS)) return (1);

	p = strrchr(to, '/');
	if (p && (p != to)) {
		*p = 0;
		if (mkdirp(to)) {
			perror(to);
			return (1);
		}
		*p = '/';
	}

	if (sys("mv", from, to, SYS)) {
		fprintf(stderr, "mv %s %s failed\n", from, to);
		goto err;
	}

	cmd = aprintf("bk sfiles \"%s\"", to);
	f = popen(cmd, "r");
	free(cmd);
	while (fnext(buf, f)) {
		chomp(buf);
		unless (s = sccs_init(buf, INIT_NOCKSUM|INIT_FIXSTIME)) {
			fprintf(stderr, "mvdir: cannot sccs_init(%s)\n", buf);
			goto err;
		}
		unless (HAS_SFILE(s)) {
			fprintf(stderr, "mvdir: not an SCCS file: %s\n", buf);
		}

		if (!HAS_PFILE(s) && S_ISREG(s->mode) && IS_WRITABLE(s)) {
			 fprintf(stderr, 
				"mvdir: %s is writable but not edited\n",
				s->gfile);
		}

		if (fix_pfile = HAS_PFILE(s)) {
			if (sccs_read_pfile("mvdir", s, &pf)) {
				fprintf(stderr, "%s: bad pfile\n", s->gfile);
				goto err;
			}
		}

                if (sccs_admin(s, 0, ADMIN_NEWPATH, 0, 0, 0, 0, 0, 0, 0, 0)) {
                        sccs_whynot("mvdir", s);
			goto err;
                }

		if (fix_pfile) {
			/*
			 * Update the p.file, make sure we preserve -i -x
			 */
			strcpy(pf.oldrev, pf.newrev);
			rev = NULL; /* Next rev will be relative to TOT */
			getedit(s, &rev);
			strcpy(pf.newrev, rev);
			sccs_rewrite_pfile(s, &pf);
			free_pfile(&pf);
		}
		sccs_free(s);
	}
	pclose(f);

	if (freeme) free(freeme);
	if (sys("bk", "idcache", "-q", SYS))  {
		fprintf(stderr, "mvdir: cannot update idcache\n");
	}
	if (sys("bk", "-r", "check", "-a", SYS)) return (1);
	unless (skip_lock) repository_wrunlock(0);
	return (0);
}
