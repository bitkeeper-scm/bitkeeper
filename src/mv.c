/*
 * Copyright 1999-2011,2013-2016 BitMover, Inc
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
#include "nested.h"

private int checkdirs(char **rootkeys);

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
	int	rc = 0;
	int	dofree = 0;
	int	force = 0;
	int	i;
	int	c;
	project	*proj;
	char	*root;
	MDBM	*idDB;
	char	idcache[MAXPATH];

	has_proj("mv");
	while ((c = getopt(ac, av, "fu", 0)) != -1) {
		switch (c) {
		    case 'f':	force = 1; break;
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

	/*
	 * Five cases
	 * 1) File -> File
	 * 2) File -> Existing Dir
	 * 3) File -> Non-Existing Dir (error case)
	 * 4) Dir -> non-Existing Dir
	 * 5) Dir -> Existing Dir
	 */
	putenv("_BK_MV_OK=1");
	strcpy(idcache, dest);
	root = isDir ? dest : dirname(idcache);
	proj = proj_init(root);
	concat_path(idcache, proj_root(proj), getIDCACHE(proj));
	idDB = 0;
	for (i = optind; i < (ac - 1); i++) {
		localName2bkName(av[i], av[i]);
		if (isdir(av[i])) {
			if (idDB) {
				idcache_write(proj, idDB);
				mdbm_close(idDB);
				idDB = 0;
			}
			rc |= sys("bk", "-?BK_NO_REPO_LOCK=YES",
			    "mvdir", av[i], dest, SYS) ? 1 : 0;
		} else {
			unless (idDB) idDB = loadDB(idcache, 0, DB_IDCACHE);
			assert(idDB);
			rc |= sccs_mv(av[i], dest, isDir, 0, isUnDelete,
			    force, idDB) ? 1 : 0;
		}
	}
	if (idDB) {
		idcache_write(proj, idDB);
		mdbm_close(idDB);
	}
	if (proj) proj_free(proj);
	if (dofree) free(dest);
	return (rc);
}

int
mvdir_main(int ac, char **av)
{
	int	c, fix_pfile;
	char	*freeme = NULL;
	char	*cmd, *p, *rev, *from, *to;
	MDBM	*idDB;
	FILE	*f;
	sccs	*s = NULL;
	pfile   pf;
	project	*proj;
	char	**rootkeys = 0;
	int	rc;
	char	*t;
	char	rkey[MAXKEY];
	char	buf[MAXPATH];

	while ((c = getopt(ac, av, "", 0)) != -1) {
		switch (c) {
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
	cmd = aprintf("bk gfiles '%s'", from);
	f = popen(cmd, "r");
	free(cmd);
	assert(f);
	rc = 0;
	while (t = fgetline(f)) {
		s = sccs_init(t, INIT_NOCKSUM|INIT_MUSTEXIST);
		assert(s);
		if (CSET(s) ||
		    begins_with(PATHNAME(s, sccs_ino(s)), "BitKeeper/")) {
			fprintf(stderr,
			    "Moving directories with BitKeeper files "
			    "not allowed\n");
			sccs_free(s);
			rc = 1;
			break;
		}
		sccs_sdelta(s, sccs_ino(s), rkey);
		rootkeys = addLine(rootkeys, strdup(rkey));
		sccs_free(s);
	}
	if (pclose(f) || rc) return (1);

	if (checkdirs(rootkeys)) return (1);

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
		return (1);
	}

	proj = proj_init(to);
	concat_path(buf, proj_root(proj), getIDCACHE(proj));
	idDB = loadDB(buf, 0, DB_IDCACHE);
	proj_free(proj);
	cmd = aprintf("bk gfiles \"%s\"", to);
	f = popen(cmd, "r");
	free(cmd);
	while (fnext(buf, f)) {
		chomp(buf);
		unless (s = sccs_init(buf, INIT_NOCKSUM|INIT_MUSTEXIST)) {
			fprintf(stderr, "mvdir: cannot sccs_init(%s)\n", buf);
			return (1);
		}

		if (!HAS_PFILE(s) && S_ISREG(s->mode) && WRITABLE(s)) {
			 // LMXXX - can this happen?  We did a check above
			 fprintf(stderr, 
				"mvdir: %s is writable but not edited\n",
				s->gfile);
		}

		if (fix_pfile = HAS_PFILE(s)) {
			if (sccs_read_pfile(s, &pf)) {
				fprintf(stderr, "%s: bad pfile\n", s->gfile);
				sccs_free(s);
				return (1);
			}
		}

                if (sccs_adminFlag(s, ADMIN_NEWPATH)) {
                        sccs_whynot("mvdir", s);
			sccs_free(s);
			return (1);
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
		sccs_sdelta(s, sccs_ino(s), rkey);
		idcache_item(idDB, rkey, PATHNAME(s, sccs_top(s)));
		sccs_free(s);
	}
	pclose(f);
	idcache_write(0, idDB);
	mdbm_close(idDB);

	if (freeme) free(freeme);
	if (checkdirs(rootkeys)) return (1);
	freeLines(rootkeys, free);
	return (0);
}

private int
checkdirs(char **rootkeys)
{
	FILE	*f;
	int	i;

	f = popen("bk -R _key2path | bk -?BK_NO_REPO_LOCK=YES check -c -", "w");
	EACH(rootkeys) fprintf(f, "%s\n", rootkeys[i]);
	return (pclose(f));
}
