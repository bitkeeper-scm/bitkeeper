/* Copyright (c) 1997 L.W.McVoy */
#include "system.h"
#include "sccs.h"
#include "nested.h"

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
	int	rc = 0, skip_lock = 0;
	int	dofree = 0;
	int	force = 0;
	int	i;
	int	c;

	has_proj("mv");
	while ((c = getopt(ac, av, "ful", 0)) != -1) {
		switch (c) {
		    case 'f':	force = 1; break;
		    case 'l':	skip_lock = 1; break;
		    case 'u':	isUnDelete = 1; break;
		    default: bk_badArg(c, av);
		}
	}

	if (ac < 3) usage();
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

	if ((isDir = isdir(dest)) && streq(basenm(dest), "SCCS")) {
		fprintf(stderr, "mv: %s is not a legal destination\n", dest);
		if (dofree) free(dest);
		exit(1);
	}
	if (ac - (optind - 1) > 3 && !isDir) {
		fprintf(stderr,
		    "Multiple files must be moved to a directory!\n");
		return (1);
	}
	av[ac-1] = 0;

	if (!skip_lock && repository_wrlock(0)) {
                fprintf(stderr, "mv: unable to write-lock repository\n");
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
	putenv("_BK_MV_OK=1");
	for (i = optind; i < (ac - 1); i++) {
		localName2bkName(av[i], av[i]);
		if (isdir(av[i])) {
			rc |= sys("bk", "mvdir", "-l", av[i], dest,
			    SYS) ? 1 : 0;
		} else {
			rc |= sccs_mv(av[i], dest, isDir, 0, isUnDelete,
			    force) ? 1 : 0;
		}
	}
	if (dofree) free(dest);
	unless (skip_lock) repository_wrunlock(0, 0);
	return (rc);
}

int
mvdir_main(int ac, char **av)
{
	int	skip_lock = 0;
	int	c, fix_pfile;
	char	*freeme = NULL;
	char	*cmd, *p, *rev, *from, *to;
	char	tempfile[MAXPATH], buf[MAXPATH];
	FILE	*f;
	sccs	*s = NULL;
	pfile   pf;

	while ((c = getopt(ac, av, "l", 0)) != -1) {
		switch (c) {
		    case 'l':	skip_lock = 1; break; /* internal interface */
		    default: bk_badArg(c, av);
		}
	}

	if ((ac - optind + 1) != 3) usage();
	from = av[optind];
	to = av[optind + 1];

	localName2bkName(from, from);
	localName2bkName(to, to);
	cleanPath(from, from);
	cleanPath(to, to);

	unless (isdir(from)) {
		fprintf(stderr, "%s is not a directory\n", from);
		return (1);
	}

	if (isComponent(from)) {
		fprintf(stderr, "mvdir: %s is a component\n"
		    "Component renaming is unsupported in this release\n",
		    from);
		return (1);
	}

	if (exists(to) && !isdir(to)) {
		fprintf(stderr, "%s exists and is not a directory\n", to);
		return (1);
	}

	if (!skip_lock && repository_wrlock(0)) {
                fprintf(stderr, "mv: unable to write lock repository\n");
                return (1);
        }

	if (streq(basenm(from), "SCCS")) {
		fprintf(stderr, "mvdir: %s is not a movable directory\n", from);
		return (1);
	}
	if (streq(basenm(to), "SCCS")) {
		fprintf(stderr, "mvdir: %s is not a legal destination\n", to);
		return (1);
	}

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
			"bk prs -hr1.1 -nd:DPN: - | grep BitKeeper/ > '%s'",
			from, tempfile);
	system(cmd);
	free(cmd);

	if (size(tempfile) != 0) {
		fprintf(stderr,
		    "Moving directories with BitKeeper files not allowed\n");
		unlink(tempfile);
err:		unless (skip_lock) repository_wrunlock(0, 0);
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

	if (rename(from, to)) {
		fprintf(stderr, "mv %s %s failed\n", from, to);
		goto err;
	}

	cmd = aprintf("bk sfiles \"%s\"", to);
	f = popen(cmd, "r");
	free(cmd);
	while (fnext(buf, f)) {
		chomp(buf);
		unless (s = sccs_init(buf, INIT_NOCKSUM)) {
			fprintf(stderr, "mvdir: cannot sccs_init(%s)\n", buf);
			goto err;
		}
		unless (HAS_SFILE(s)) {
			fprintf(stderr, "mvdir: not an SCCS file: %s\n", buf);
		}

		if (!HAS_PFILE(s) && S_ISREG(s->mode) && WRITABLE(s)) {
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

                if (sccs_adminFlag(s, ADMIN_NEWPATH)) {
                        sccs_whynot("mvdir", s);
			goto err;
                }

		if (fix_pfile) {
			/*
			 * Update the p.file, make sure we preserve -i -x
			 */
			free(pf.oldrev);
			pf.oldrev = strdup(pf.newrev);
			rev = NULL; /* Next rev will be relative to TOT */
			sccs_getedit(s, &rev);
			free(pf.newrev);
			pf.newrev = strdup(rev);
			sccs_rewrite_pfile(s, &pf);
			free_pfile(&pf);
		}
		sccs_free(s);
	}
	pclose(f);

	if (freeme) free(freeme);
	if (sccs_reCache(1))  {
		fprintf(stderr, "mvdir: cannot update idcache\n");
	}
	if (sys("bk", "-r", "check", "-a", SYS)) return (1);
	unless (skip_lock) repository_wrunlock(0, 0);
	return (0);
}
