/*
 * Copyright 2015-2016 BitMover, Inc
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
#include "range.h"
#include "nested.h"

typedef struct {
	char	*branch;
	char	*baserepo;
	u32	addMD5Keys:1;
	u32	md5KeysAsSubject:1;
	u32	nested:1;
	u32	quiet:1;
	// state that gets passed around
	hash	*rkdk2fi;	// map "crk rk dk" to 'struct finfo'
				// crk == component rootkey
	hash	*bk2git;	// map bkkeys(seen in git) to git-revs
	MDBM	*idDB;
	MDBM	*goneDB;
	MDBM	*sDB;
	hash	*compGone;
	hash	*authors;
} opts;


/*
 * How is data passed? Inline or with a SHA1 or :mark reference?
 */
enum {INLINE, EXTERNAL};

/*
 * A line like:
 *
 *   'M' SP <mode> SP <dataref> SP <path> LF
 *
 * and an optional inline FILE* that will be dumped after the line if
 * it isn't NULL.  If the latter, <dataref> must be the word 'inline'
 * (without the quotes). Note the print function checks none of this.
 *
 */
typedef struct gitOp {
	char	*op;
	FILE	*data;
} gitOp;

private	char	*gitMode(mode_t mode);
private	char	*gitTZ(sccs *s, ser_t d);
private	void	gitLine(opts *op, gitOp **oplist,
    char *comp_rk, char *rk, char *dk1, char *dk2,
    char *prefix1, char *prefix2);
private	int	gitExport(opts *op);
private	hash	*loadAuthors(char *file);
private	void	authorInfo(opts *op, sccs *s, ser_t d);
private	void	printUserDate(opts *op, sccs *s, ser_t d);
private void	printOp(struct gitOp op, FILE *f);
private void	data(sccs *s, ser_t d, FILE *f);
private void	gitProgress(opts *op, char *format, ...)
#ifdef __GNUC__
     __attribute__((format (__printf__, 2, 3)))
#endif
;
private int	uncolorAlreadyImported(opts *op, sccs *cset);

/* bk fast-export (aliased as _fastexport) */
int
fastexport_main(int ac, char **av)
{
	int	c;
	int	standalone = 0;
	opts	opts = {0};
	longopt	lopts[] = {
		{ "authors-file;", 'A'},   // opt name from git cvs/svn import
		{ "branch;", 310},
		{ "bk-regressions", 320},
		{ "incremental;", 325},
		{ "no-bk-keys", 330},
		{ "standalone", 'S' },
		{ "quiet", 'q'},

	// future options:
	//   --flatten  export ALL component csets, not only ones in product
		{ 0, 0 }
	};
	int	rc = 1;

	opts.addMD5Keys = 1;
	while ((c = getopt(ac, av, "A;Sq", lopts)) != -1) {
		switch (c) {
		    case 'q':
			opts.quiet = 1;
			break;
		    case 'A':
			unless (opts.authors = loadAuthors(optarg)) return (0);
			break;
		    //case 's':  // -sALIAS export a subset of tree
		    case 'S':
			standalone = 1;
			break;
		    case 310:	/* --branch */
			opts.branch = strdup(optarg);
			break;
		    case 320:	/* --bk-regressions */
			opts.md5KeysAsSubject = 1;
			break;
		    case 325:	/* --incremental */
			opts.baserepo = strdup(optarg);
			break;
		    case 330:	/* --no-bk-keys */
			opts.addMD5Keys = 0;
			break;
		    default: bk_badArg(c, av);
		}
	}
	if (av[optind]) usage();
	unless (opts.branch) opts.branch = strdup("master");
	opts.nested = bk_nested2root(standalone);
	if (opts.nested && !nested_isPortal(0)) {
		fprintf(stderr, "%s: only allowed in a portal\n", prog);
		goto out;
	}
	rc = gitExport(&opts);
out:	free(opts.branch);
	free(opts.baserepo);
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
 * Decode an rset line and create a gitOp suitable for printing.
 *
 * Note that renames are not handled so we just delete the old path
 * and add the new path.
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
gitLine(opts *op, gitOp **oplist, char *comp_rk, char *rk,
    char *dk1, char *dk2, char *prefix1, char *prefix2)
{
	char	*hkey = 0;
	char	*path1 = 0, *path2 = 0;
	int	del1, del2, rename;
	finfo	*fip;
	sccs	*s;
	ser_t	d;
	gitOp	*gop;

	/* punt early if gone */
	if (gone(rk, op->goneDB) || gone(dk2, op->goneDB)) {
		return;
	}

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
	if (rename && !del1 && del2) {
		gop = addArray(oplist, 0);
		gop->op = aprintf("D %s\n", path1);
	} else {
		if (rename && !del1 && !del2) {
			gop = addArray(oplist, 0);
			gop->op = aprintf("D %s\n", path1);
		}
		gop = addArray(oplist, 0);
		if (fip = hash_fetchStrMem(op->rkdk2fi, hkey)) {
			gop->op = aprintf("M %s :%llu %s\n",
			    gitMode(fip->mode), fip->mark, path2);
		} else {
			unless (s = sccs_keyinitAndCache(0, rk, INIT_MUSTEXIST,
			    op->sDB, op->idDB)) {
				fprintf(stderr, "failed to find %s\n", rk);
				exit(1);
			}
			unless (d = sccs_findKey(s, dk2)) {
				fprintf(stderr, "failed to find delta %s "
				    "for rootkey %s\n", dk2, rk);
				exit(1);
			}
			gop->op = aprintf("M %s inline %s\n",
			    gitMode(MODE(s, d)), path2);
			gop->data = fmem();
			data(s, d, gop->data);
			fseek(gop->data, 0, SEEK_SET);
		}
	}
out:	free(hkey);
	free(path1);
	free(path2);
}

private void
gitLineComp(opts *op, gitOp **oplist, char *rk, char *dk1, char *dk2)
{
	int	i;
	sccs	*s;
	ser_t	d1, d2;
	char	*prefix1, *prefix2, *p;
	rset_df *rset;
	MDBM	*goneSave;

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

	/*
	 * swap op->goneDB for the gone file in this component.
	 * we will restore it before returning
	 */
	unless (op->compGone) op->compGone = hash_new(HASH_MEMHASH);
	goneSave = op->goneDB;
	unless (op->goneDB = hash_fetchStrPtr(op->compGone, rk)) {
		op->goneDB = loadDB(proj_fullpath(s->proj, GONE), 0, DB_GONE);
		hash_storeStrPtr(op->compGone, rk, op->goneDB);
	}

	rset = rset_diff(s, d1, 0, d2, 1);
	prefix1 = 0;
	if (p = key2path(dk1, 0, 0, 0)) {
		prefix1 = dirname(p);
	}
	p = key2path(dk2, 0, 0, 0);
	prefix2 = dirname(p);
	EACH(rset) {
		gitLine(op, oplist,
		    rk,
		    HEAP(s, rset[i].rkoff),
		    HEAP(s, rset[i].dkleft1),
		    HEAP(s, rset[i].dkright),
		    prefix1, prefix2);
	}
	op->goneDB = goneSave;
	free(rset);
	free(prefix1);
	free(prefix2);
}

/*
 * Print a git 'data' command (see git help fast-import)
 */
private void
data(sccs *s, ser_t d, FILE *f)
{
	char	*p;
	size_t	len;
	FILE	*f2;

	assert(s);
	assert(d);
	f2 = fmem();
	sccs_get(s, REV(s, d), 0, 0, 0, SILENT, 0, f2);
	p = fmem_peek(f2, &len);
	if (S_ISLNK(MODE(s, d))) {
		p  += strlen("SYMLINK -> ");
		len -= strlen("SYMLINK -> ") + 1 /* \n */;
		fprintf(f, "data %u\n%s", (u32)len, p);
	} else {
		fprintf(f, "data %u\n", (u32)len);
		fwrite(p, 1, len, f);
	}
	fclose(f2);
}

private void
printOp(struct gitOp op, FILE *f)
{
	int	len;
	char	buf[8<<10];

	fputs(op.op, f);
	if (op.data) {
		while ((len = fread(buf, 1, sizeof(buf), op.data)) > 0) {
			if (fwrite(buf, 1, len, f) != len) {
				fprintf(stderr,
				    "%s: failed writing data\n", prog);
				exit(1);
			}
		}
		if (ferror(op.data)) {
			fprintf(stderr, "%s: failed reading data\n", prog);
			exit(1);
		}
	}
}

private int
gitOpCmp(const void *a, const void *b)
{
	gitOp	op1 = *(gitOp *)a;
	gitOp	op2 = *(gitOp *)b;

	/*
	 * This relies on the fact that 'D' < 'M', all we want are
	 * deletes before adds.
	 */
	return (op1.op - op2.op);
}

private int
gitExport(opts *op)
{
	FILE	*f1 = 0;
	char	*file, *sfile, *p;
	char	*cmd;
	sccs	*s, *cset;
	ser_t	d, md, left, right, dp;
	u64	mark;
	size_t	len;
	u64	progress = 0;
	finfo	fi;
	kvpair	kv;
	rset_df	*rset;
	int	i;
	gitOp	*gitOps;
	symbol	*sym;
	hash	*tags;
	char	*sha1;
	char	md5key[MD5LEN];
	char	rk[MAXKEY];
	char	dk[MAXKEY];

	/* Give git all the files */
	op->rkdk2fi = hash_new(HASH_MEMHASH);
	op->bk2git = hash_new(HASH_MEMHASH);
	mark = 1;

	cset = sccs_csetInit(0);
	assert(cset);

	if (op->baserepo) {
		char	*line;
		char	*sha1, *md5;
		int	numcsets = 0;

		gitProgress(op, "Analyzing baseline repo %s\n",
		    op->baserepo);

		/*
		 * We're going to use range_unrange to find the
		 * endpoints of the incremental, that means we want to
		 * tag everything we want with D_SET. However, we're
		 * reading the MD5 keys that git already has, so we
		 * tag *everything* with D_SET and untag each key git
		 * has. */
		for (d = TREE(cset); d <= TABLE(cset); d++) {
			if (TAG(cset, d)) continue;
			FLAGS(cset, d) |= D_SET;
		}

		cmd = aprintf("git --git-dir='%s/.git' "
		    "log --pretty='%%w(0,1,1)%%B%%n%%w(0,0,0)%%H' --all",
		    op->baserepo);
		f1 = popen(cmd, "r");
		free(cmd);
		while (!feof(f1)) {
			md5 = 0;
			while ((line = fgetline(f1)) && line[0] == ' ') {
				if (strneq(line, " bk: ", 5) &&
				    isKey(line + 5)) {
					md5 = strdup(line + 5);
				}
			}
			unless(line) break;
			unless (md5) continue;
			sha1 = line;
			unless (hash_insertStrStr(op->bk2git, md5, sha1)) {
				fprintf(stderr,
				    "Duplicated md5: |%s| -> |%s|\n",
				    md5, sha1);
				exit(1);
			}
			/*
			 * Untag the delta since it's already in git.
			 */
			if (d = sccs_findMD5(cset, md5)) {
				FLAGS(cset, d) &= ~D_SET;
				numcsets++;
			}
			free(md5);
		}
		if (pclose(f1)) {
			fprintf(stderr, "%s failed\n", cmd);
			exit(1);
		}

		f1 = 0;

		numcsets += uncolorAlreadyImported(op, cset);
		gitProgress(op, "%d csets already imported\n", numcsets);

		/* Try to find the endpoints of the colored range */
		range_unrange(cset, &left, &right, 0);

		if (cset->rstart != cset->rstop) {
			cmd = aprintf("bk rset %s -ahH -r%s..%s",
			    op->nested ? "" : "-S",
			    REV(cset, cset->rstart),
			    REV(cset, cset->rstop));
			f1 = popen(cmd, "r");
			free(cmd);
		}
	} else {
		/* Non-incremental, do all files */
		cset->rstart = TREE(cset);
		cset->rstop = TABLE(cset);
		cmd = strdup(op->nested ? "bk -A" : "bk -r");
		f1 = popen(cmd, "r");
		free(cmd);
	}
	gitProgress(op, "Processing files\n");
	progress = 0;
	while (f1 && (file = fgetline(f1))) {
		char	*revs = 0;
		RANGE	rargs = {0};

		if (op->baserepo) {
			char	**lines;

			lines= splitLine(file, "|", 0);
			sfile = strdup(lines[1]);
			revs = aprintf("%s..%s", lines[3], lines[5]);
			freeLines(lines, free);
			if (range_addArg(&rargs, revs, 0)) {
				fprintf(stderr, "%s: range failure\n", prog);
				exit(1);
			}
		} else {
			if (streq(basenm(file), GCHANGESET)) continue;
			sfile = name2sccs(file);
		}
		s = sccs_init(sfile, INIT_MUSTEXIST);
		assert(s);
		sccs_sdelta(s, sccs_ino(s), rk);
		range_process("fastexport", s, RANGE_SET, &rargs);
		for (d = s->rstart; d <= s->rstop; d++) {
			unless (FLAGS(s, d) & D_SET) continue;
			/*
			 * We only want cset marked deltas
			 * But this can still send too much data in components
			 * when only some of the component csets are in
			 * the product.  (see --flatten)
			 * Harmless, but bloats git's object store.
			 */
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
			printf("blob\nmark :%lld\n", mark++);
			data(s, d, stdout);
		}
		sccs_free(s);
		free(sfile);
		if ((++progress % 1000) == 0) {
			gitProgress(op, "%llu files done\n", progress);
		}
		free(revs);
	}
	if (f1 && pclose(f1)) {
		fprintf(stderr, "%s failed\n", cmd);
		exit(1);
	}

	op->idDB = loadDB(IDCACHE, 0, DB_IDCACHE);
	op->goneDB = loadDB(GONE, 0, DB_GONE);
	op->sDB = mdbm_mem();

	/* Now walk the ChangeSet file and do all the ChangeSets */

	gitProgress(op, "Processing changes\n");
	progress = 0;
	f1 = fmem();
	mark--;			/* we were one over */
	for (d = cset->rstart; d <= cset->rstop; d++) {
		if (TAG(cset, d)) continue;
		if (op->baserepo && !(FLAGS(cset, d) & D_RED)) continue;
		printf("commit refs/heads/%s\n", op->branch);
		printf("mark :%llu\n", mark + d);
		authorInfo(op, cset, d);

		if (op->addMD5Keys || op->md5KeysAsSubject) {
			sccs_md5delta(cset, d, md5key);
		}
		if (op->md5KeysAsSubject) fprintf(f1, "%s\n\n", md5key);
		if (HAS_COMMENTS(cset, d)) {
			fprintf(f1, "%s", COMMENTS(cset, d));
		}
		if (op->addMD5Keys) {
			fprintf(f1, "\nbk: %s", md5key);
		}
		p = fmem_peek(f1, &len);
		printf("data %lu\n", len);
		fwrite(p, 1, len, stdout);
		ftrunc(f1, 0);	/* reuse memory */
		EACH_PARENT(cset, d, dp, i) {
			printf("%s ", i ? "merge" : "from");
			if (op->baserepo &&
			    !(FLAGS(cset, dp) & D_RED)) {
				sccs_md5delta(cset, dp, md5key);
				sha1 = hash_fetchStrStr(op->bk2git, md5key);
				assert(sha1);
				printf("%s\n", sha1);
			} else {
				printf(":%llu\n", mark + dp);
			}
		}
		rset = rset_diff(cset, PARENT(cset, d), 0, d, 1);
		gitOps = 0;
		EACH(rset) {
			if (componentKey(HEAP(cset, rset[i].dkright))) {
				unless (op->nested) continue;
				gitLineComp(op, &gitOps,
				    HEAP(cset, rset[i].rkoff),
				    HEAP(cset, rset[i].dkleft1),
				    HEAP(cset, rset[i].dkright));
			} else {
				gitLine(op, &gitOps,
				    proj_rootkey(cset->proj),
				    HEAP(cset, rset[i].rkoff),
				    HEAP(cset, rset[i].dkleft1),
				    HEAP(cset, rset[i].dkright), 0, 0);
			}
		}
		free(rset);
		/*
		 * Sort the git operations on files so that
		 * the deletes (D) happen before the new files
		 * (M). Also, don't do renames, git doesn't
		 * care and 'R' sorts after 'M' :)
		 */
		sortArray(gitOps, gitOpCmp);
		EACH(gitOps) {
			printOp(gitOps[i], stdout);
			free(gitOps[i].op);
			if (gitOps[i].data) fclose(gitOps[i].data);
		}
		free(gitOps);
		if ((++progress % 1000) == 0) {
			gitProgress(op, "%llu csets done\n", progress);
		}
	}
	fclose(f1);
	gitProgress(op, "Processing Tags\n");

	tags = hash_new(HASH_U32HASH, sizeof(u32), sizeof(u32));
	EACHP_REVERSE(cset->symlist, sym) {
		unless (hash_insertU32U32(tags, sym->symname, 1)) continue;
		if (op->baserepo && !(FLAGS(cset, sym->ser) & D_RED)) {
			continue;
		}
		printf("tag %s\n", SYMNAME(cset, sym));
		if (op->baserepo &&
		    !(FLAGS(cset, sym->ser) & D_RED)) {
			sccs_md5delta(cset, sym->ser, md5key);
			sha1 = hash_fetchStrStr(op->bk2git, md5key);
			assert(sha1);
			printf("from %s\n", sha1);
		} else {
			printf("from :%llu\n", mark + sym->ser);
		}
		md = sym->meta_ser;
		printf("tagger ");
		printUserDate(op, cset, md);
		printf("data 0\n");
	}
	hash_free(tags);
	sccs_free(cset);
	mdbm_close(op->idDB);
	mdbm_close(op->goneDB);
	EACH_KV(op->sDB) {
		memcpy(&s, kv.val.dptr, sizeof (sccs *));
		if (s) sccs_free(s);
	}
	mdbm_close(op->sDB);
	hash_free(op->bk2git);
	hash_free(op->rkdk2fi);
	EACH_HASH(op->compGone) mdbm_close(*(MDBM **)op->compGone->vptr);
	hash_free(op->compGone);
	gitProgress(op, "done\n");
	return (0);
}

/*
 * Load an authors file file mapping bk usernames to names and email
 * addresses. The file is a list of lines like this:
 *   wscott=Wayne Scott <wscott@bitkeeper.com>
 * one line per user.  The format is reused from git's svn and cvs
 * import tools.
 *
 * This will be matched on user/realuser or usr of a cset.  If no match
 * is found then:
 *     :USER: <:USER:@:HOST:>
 * is used.
 */
private hash *
loadAuthors(char *file)
{
	FILE	*f;
	char	*t;
	hash	*ret;
	int	line = 0;
	const char	*error;
	int	off;
	pcre	*re;
	int	vec[9];

	re = pcre_compile(
		"^\\s*(\\S+)\\s*=\\s*"
		"(.*<[\\w0-9._%+-]+@[\\w0-9.-]+\\.\\w{2,}>)\\s*$",
		0, &error, &off, 0);
	if (error) fprintf(stderr, "%s: %s at %d\n", prog, error, off);
	assert(re);

	unless (f = fopen(file, "r")) {
		perror(file);
		return (0);
	}

	ret = hash_new(HASH_MEMHASH);
	while (t = fgetline(f)) {
		++line;
		if (!*t || (*t == '#')) continue;
		if (pcre_exec(re, 0, t, strlen(t), 0, 0,
		    vec, sizeof(vec)/sizeof(*vec)) < 0) {
			fprintf(stderr, "%s: %s:%d: bad line: %s\n",
			    prog, file, line, t);
			hash_free(ret);
			ret = 0;
			break;
		}
		hash_insert(ret,
		    t+vec[2], vec[3]-vec[2],
		    t+vec[4], vec[5]-vec[4]);
	}
	fclose(f);
	pcre_free(re);
	return (ret);
}

private void
printUserDate(opts *op, sccs *s, ser_t d)
{
	char	*tz;
	char	*userhost = USERHOST(s, d);
	char	*user, *host;
	int	ulen, hlen;

	tz = gitTZ(s, d);
	user = userhost;
	ulen = strcspn(user, "/@");
	if (hash_fetch(op->authors, user, ulen)) {
		printf("%.*s", op->authors->vlen, (char *)op->authors->vptr);
	} else {
		/* :USER: <:USER:@:HOST:> */
		if (host = strchr(userhost, '@')) {
			++host;
		} else {
			host = "NOHOST.com";
		}
		hlen = strcspn(host, "/[");
		printf("%.*s <%.*s@%.*s>",
		    ulen, user, ulen, user, hlen, host);
	}
	printf(" %d %s\n", (int)DATE(s, d), tz);
	free(tz);
}

private void
authorInfo(opts *op, sccs *s, ser_t d)
{
	char	*tz;
	char	*userhost = USERHOST(s, d);
	char	*import;

	/* userhost = user/realuser@host/realhost[importer] */

	/* [importer] only works with author map */
	if ((import = strchr(userhost, '[')) &&
	    hash_fetch(op->authors, import+1, strlen(import)-2)) {
		/* need separate author line */
		tz = gitTZ(s, d);
		printf("author %.*s %d %s\n",
		    op->authors->vlen, (char *)op->authors->vptr,
		    (int)DATE(s, d), tz);
		free(tz);
	}
	printf("committer ");

	printUserDate(op, s, d);
}

private void
gitProgress(opts *op, char *format, ...)
{
	char	*fmt;
	va_list	ap;

	if (op->quiet) return;
	fmt = aprintf("progress %s\n", format);
	va_start(ap, format);
	vfprintf(stdout, fmt, ap);
}

/*
 * Walk the graph looking for csets tagged as 'GIT:' which we assume
 * are already imported in GIT.
 */
private int
uncolorAlreadyImported(opts *op, sccs *cset)
{
	ser_t	d;
	char	*t, *p;
	int	i, n = 0;

	for (d = TABLE(cset); d >= TREE(cset); d--) {
		if (TAG(cset, d)) continue;
		t = COMMENTS(cset, d);
		while (p = eachline(&t, &i)) {
			char old = p[i];
			p[i] = 0;
			if (strneq(p, "GIT: ", 5)) {
				char	md5[MD5KEYLEN];
				char	*sha1 = p + 5;

				sccs_md5delta(cset, d, md5);
				hash_storeStrStr(op->bk2git, md5, sha1);
				FLAGS(cset, d) &= ~D_SET;
				n++;
			}
			p[i] = old;
		}
	}
	return (n);
}
