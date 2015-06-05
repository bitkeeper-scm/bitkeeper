#include "sccs.h"
#include "range.h"
#include "nested.h"
#include "logging.h"

typedef struct {
	char	*branch;
	u32	addMD5Keys:1;
	u32	md5KeysAsSubject:1;
	// state that gets passed around
	hash	*rkdk2fi;
	MDBM	*idDB;
	MDBM	*goneDB;
	MDBM	*sDB;
} opts;

private	char	*gitMode(mode_t mode);
private	char	*gitTZ(sccs *s, ser_t d);
private	void	gitLine(opts *op, char ***lines,
    char *comp_rk, char *rk, char *dk1, char *dk2,
    char *prefix1, char *prefix2);
private	int	gitExport(opts *op);

/* bk fast-export (aliased as _fastexport) */
int
fastexport_main(int ac, char **av)
{
	int	c;
	opts	opts = {0};
	longopt	lopts[] = {
		{ "branch;", 310},
		{ "bk-regressions", 320},
		{ "no-bk-keys", 330},
		{ 0, 0 }
	};
	int	rc = 1;

	if (bk_notLicensed(0, LIC_EXPORT, 1)) {
		/* act like it doesn't exist */
		fprintf(stderr, "%s: command not found\n", prog);
		return (1);
	}
	opts.addMD5Keys = 1;
	while ((c = getopt(ac, av, "", lopts)) != -1) {
		switch (c) {
		    case 310:	/* --branch */
			opts.branch = strdup(optarg);
			break;
		    case 320:	/* --bk-regressions */
			opts.md5KeysAsSubject = 1;
			break;
		    case 330:	/* --no-bk-keys */
			opts.addMD5Keys = 0;
			break;
		    default: bk_badArg(c, av);
		}
	}
	if (av[optind]) usage();
	unless (opts.branch) opts.branch = strdup("master");
	bk_nested2root(0);
	if (proj_isProduct(0) && !nested_isPortal(0)) {
		fprintf(stderr, "%s: only allowed in a portal\n", prog);
		goto out;
	}

	rc = gitExport(&opts);
out:	free(opts.branch);
	return (rc);
}

private char *
gitMode(mode_t mode)
{
	char	buf[20];

	/* Git only supports 100644, 100755, and 120000 */
	sprintf(buf, "%o", (int)mode);
	if (buf[1] == '2') {
		return ("120000");
	} else if (buf[3] == '7') {
		return ("100755");
	} else {
		return ("100644");
	}
	return ("100644");
}

/*
 * Git's timezone is rfc2822 so no ':'
 */
private char *
gitTZ(sccs *s, ser_t d)
{
	char	*q, *tz;

	if (HAS_ZONE(s, d)) {
		tz = q = strdup(ZONE(s, d));
		while (*q && (*q != ':')) q++;
		if (*q == ':') for (; *q = *(q+1); q++) ;
	} else {
		tz = strdup("");
	}
	return (tz);
}

typedef struct finfo {
	u64	mark;
	mode_t	mode;
} finfo;


/*
 * Decode an rset line and print what happened in Git's terms.
 *
 * rkdk2fi: hash of "rk dk" to info about the file (cached)
 * rk:  rootkey of the file
 * comp_rk: comonent root key
 * dk1: deltakey of the parent
 * dk2: deltakey of this
 * prefix1: component prefix of dk1
 * prefix2: component prefix of dk2
 */
private	void
gitLine(opts *op, char ***lines, char *comp_rk, char *rk, char *dk1, char *dk2,
    char *prefix1, char *prefix2)
{
	char	*hkey = 0;
	char	*path1, *path2;
	int	del1, del2, rename;
	finfo	*fip;
	char	**ret = *lines;

	path1 = key2path(dk1, 0, 0, 0); /* could be NULL for new files */
	path2 = key2path(dk2, 0, 0, 0);
	assert(path2);
	/*
	 * We assume everyting inside BitKeeper/ is deleted so that if
	 * a user creates a file outside of it and moves it into it,
	 * it'll disappear both in BitKeeper and Git.
	 */
	del1 = path1 && strneq(path1, "BitKeeper/", 10);
	del2 = strneq(path2, "BitKeeper/", 10);
	rename = path1 && !streq(path1, path2);
	if ((del1 && del2) || (!path1 && del2)) {
		goto out;
	}
	hkey = aprintf("%s %s %s", comp_rk, rk, dk2);
	if (prefix1) {
		char	*p;

		p = aprintf("%s/%s", prefix1, path1);
		free(path1);
		path1 = p;
	}
	if (prefix2) {
		char	*p;

		p = aprintf("%s/%s", prefix2, path2);
		free(path2);
		path2 = p;
	}
	unless (fip = hash_fetchStrMem(op->rkdk2fi, hkey)) {
		if (gone(rk, op->goneDB) || gone(dk2, op->goneDB)) {
			goto out;
		}
		fprintf(stderr, "1. Key not found: %s\n", hkey);
		exit(1);
	}
	if (rename && !del1 && del2) {
		ret = addLine(ret, aprintf("D %s\n", path1));
	} else {
		if (rename && !del1 && !del2) {
			ret = addLine(ret,
			    aprintf("D %s\n", path1));
		}
		ret = addLine(ret,
		    aprintf("M %s :%llu %s\n", gitMode(fip->mode),
			fip->mark, path2));
	}
out:	free(hkey);
	free(path1);
	free(path2);
	*lines = ret;
}

private void
gitLineComp(opts *op, char ***lines, char *rk, char *dk1, char *dk2)
{
	int	i;
	sccs	*s;
	ser_t	d1, d2;
	char	*prefix1, *prefix2, *p;
	rset_df *rset;

	s = sccs_keyinitAndCache(0, rk, INIT_MUSTEXIST, op->sDB, op->idDB);
	unless (s) {
		if (gone(rk, op->goneDB)) return;
		fprintf(stderr, "failed to init: %s\n", rk);
		exit(1);
	}
	d1 = sccs_findKey(s, dk1);
	if (!d1 && !streq(dk1, "")) {
		if (gone(dk1, op->goneDB)) return;
		fprintf(stderr, "%s not found\n", dk1);
		exit(1);
	}
	d2 = sccs_findKey(s, dk2);
	unless (d2) {
		if (gone(dk2, op->goneDB)) return;
		fprintf(stderr, "%s not found\n", dk2);
		exit(1);
	}
	rset = rset_diff(s, d1, 0, d2, 1);
	prefix1 = 0;
	if (p = key2path(dk1, 0, 0, 0)) {
		prefix1 = dirname(p);
	}
	p = key2path(dk2, 0, 0, 0);
	prefix2 = dirname(p);
	EACH(rset) {
		gitLine(op, lines,
		    rk,
		    HEAP(s, rset[i].rkoff),
		    HEAP(s, rset[i].dkleft1),
		    HEAP(s, rset[i].dkright),
		    prefix1, prefix2);
	}
	free(prefix1);
	free(prefix2);
}

private int
gitExport(opts *op)
{
	FILE	*f1, *f2;
	char	*file, *sfile, *p;
	char	*tz;
	sccs	*s;
	ser_t	d, md;
	u64	mark;
	size_t	len;
	u64	progress = 0;
	finfo	fi;
	kvpair	kv;
	rset_df	*rset;
	int	i;
	char	**gitOps;
	symbol	*sym;
	hash	*tags;
	char	rk[MAXKEY];
	char	dk[MAXKEY];

	/* Give git all the files */
	op->rkdk2fi = hash_new(HASH_MEMHASH);
	mark = 1;

	printf("progress Processing files\n");
	f1 = popen("bk -A", "r");
	f2 = fmem();
	progress = 0;
	while (file = fgetline(f1)) {
		if (streq(basenm(file), GCHANGESET)) continue;
		sfile = name2sccs(file);
		s = sccs_init(sfile, INIT_MUSTEXIST);
		assert(s);
		sccs_sdelta(s, sccs_ino(s), rk);
		for (d = TREE(s); d <= TABLE(s); d++) {
			/* We only want cset marked deltas */
			unless (FLAGS(s, d) & D_CSET) continue;
			sccs_sdelta(s, d, dk);
			p = aprintf("%s %s %s", proj_rootkey(s->proj), rk, dk);
			fi.mark = mark;
			fi.mode = MODE(s, d);
			unless (hash_insertStrMem(op->rkdk2fi, p,
			    &fi, sizeof(fi))) {
				fprintf(stderr, "Duplicate rk/dk: %s\n", p);
				exit(1);
			}
			free(p);
			sccs_get(s, REV(s, d), 0, 0, 0, SILENT, 0, f2);
			printf("blob\nmark :%lld\n", mark++);
			p = fmem_peek(f2, &len);
			if (S_ISLNK(MODE(s, d))) {
				p  += strlen("SYMLINK -> ");
				len -= strlen("SYMLINK -> ") + 1 /* \n */;
				printf("data %u\n%s", (u32)len, p);
			} else {
				printf("data %u\n", (u32)len);
				fwrite(p, 1, len, stdout);
			}
			ftrunc(f2, 0); /* reuse memory */
		}
		sccs_free(s);
		free(sfile);
		if ((++progress % 1000) == 0) {
			printf("progress %llu files done\n", progress);
		}
	}
	pclose(f1);
	fclose(f2);

	op->idDB = loadDB(IDCACHE, 0, DB_IDCACHE);
	op->goneDB = loadDB(GONE, 0, DB_GONE);
	op->sDB = mdbm_mem();

	/* Now walk the ChangeSet file and do all the ChangeSets */

	printf("progress Processing changes\n");
	s = sccs_csetInit(0);
	assert(s);
	progress = 0;
	f1 = fmem();
	for (d = TREE(s); d <= TABLE(s); d++) {
		char	md5key[MD5LEN];

		if (TAG(s, d)) continue;
		printf("commit refs/heads/%s\n", op->branch);
		printf("mark : %llu\n", mark + d);
		tz = gitTZ(s, d);
		printf("committer <%s@%s> %d %s\n", USER(s, d),
		    HOSTNAME(s, d), (int)DATE(s, d), tz);
		free(tz);

		if (op->addMD5Keys || op->md5KeysAsSubject) {
			sccs_md5delta(s, d, md5key);
		}
		if (op->md5KeysAsSubject) fprintf(f1, "%s\n\n", md5key);
		if (HAS_COMMENTS(s, d)) {
			fprintf(f1, "%s", COMMENTS(s, d));
		}
		if (op->addMD5Keys) {
			fprintf(f1, "\nbk: %s", md5key);
		}
		p = fmem_peek(f1, &len);
		printf("data %lu\n", len);
		fwrite(p, 1, len, stdout);
		ftrunc(f1, 0);	/* reuse memory */
		if (PARENT(s, d)) {
			printf("from :%llu\n", mark + PARENT(s, d));
		}
		if (MERGE(s, d)) {
			printf("merge :%llu\n", mark + MERGE(s, d));
		}

		rset = rset_diff(s, PARENT(s, d), 0, d, 1);
		gitOps = 0;
		EACH(rset) {
			if (componentKey(HEAP(s, rset[i].dkright))) {
				gitLineComp(op, &gitOps,
				    HEAP(s, rset[i].rkoff),
				    HEAP(s, rset[i].dkleft1),
				    HEAP(s, rset[i].dkright));
			} else {
				gitLine(op, &gitOps,
				    proj_rootkey(s->proj),
				    HEAP(s, rset[i].rkoff),
				    HEAP(s, rset[i].dkleft1),
				    HEAP(s, rset[i].dkright), 0, 0);
			}
		}
		free(rset);
		/*
		 * Sort the git operations on files so that
		 * the deletes (D) happen before the new files
		 * (M). Also, don't do renames, git doesn't
		 * care and 'R' sorts after 'M' :)
		 */
		sortLines(gitOps, 0);
		EACH(gitOps) fputs(gitOps[i], stdout);
		freeLines(gitOps, free);

		if ((++progress % 1000) == 0) {
			printf("progress %llu csets done\n", progress);
		}
	}
	fclose(f1);
	printf("progress Processing Tags\n");

	tags = hash_new(HASH_MEMHASH);
	EACHP_REVERSE(s->symlist, sym) {
		unless (hash_insertStrSet(tags, SYMNAME(s, sym))) continue;
		printf("tag %s\n", SYMNAME(s, sym));
		printf("from :%llu\n", mark + sym->ser);
		md = sym->meta_ser;
		tz = gitTZ(s, md);
		printf("tagger <%s@%s> %d %s\n", USER(s, md),
		    HOSTNAME(s, md), (int)DATE(s, md), tz);
		free(tz);
		printf("data 0\n");
	}
	hash_free(tags);
	sccs_free(s);
	mdbm_close(op->idDB);
	mdbm_close(op->goneDB);
	EACH_KV(op->sDB) {
		memcpy(&s, kv.val.dptr, sizeof (sccs *));
		if (s) sccs_free(s);
	}
	mdbm_close(op->sDB);
	hash_free(op->rkdk2fi);
	printf("progress done\n");
	return (0);
}
