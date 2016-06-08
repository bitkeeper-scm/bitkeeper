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

/*
 * TODO
 *
 * correctness problems
 *   - the --no-ff merge in t.fast-import occasinally leaves an
 *     unmerged tip
 *   - verify that the incoming fast-export graph has a single tip
 *     before starting import
 *   - delete of a BAM file saves empty file for tip
 *   - octo-merge that doesn't touch any one file with more than one
 *     tip
 *   - octo-merge in files themselves
 *
 * performance
 *   - need to use pthreads to import files in parallel
 *
 * features
 *   - incremental import
 *   - support for submodules -> nested or large repo
 *   - -iGLOB & -xGLOB to import a subset
 *   - add first line from each commit comment to files
 *   - support reading a fast-import -M file (still no rename)
 *
 *  future
 *   - add rename support
 */

/*
 * Code layout:
 * main() calls gitImport()
 *
 * gitImport()
 *    - reads fast-export stream from stdin and processes using
 *      helper functions like getCommit() and getBlob()
 *
 *    - do a breadth first graph traversal to order the topological
 *      graph in time order matching bk's layout.
 *
 *    - open a new top-level ChangeSet file
 *
 *    - calls importFile() for each unique pathname in os->paths
 *      - Extracts graph for each file and builds sfiles
 *      - save rootkey/deltakey pairs in cset heap
 *      - save cset weave fragments in cmt->weave
 *
 *    - writes toplevel ChangeSet file and then uses renumber/resum to
 *      fixup issues
 *
 * Threading possibilities:
 *   The first phase of parsing the incoming csets has to happen in
 *   order because of the time reordering step before we process
 *   files, but this is pretty fast.
 *
 *   The importFile() calls could all be done in parallel since they
 *   do everything locally.  The only outputs from this phase are a
 *   new sfile on the disk, adding rk/dk pairs to the cset heap, and
 *   writing cmt->weave. We can keep the current code and have a
 *   global mutex for updates weave entries, or save the cset heap
 *   part for the end and use a mutex per cset.  Another option is a
 *   channel to send weave updates back to main thread which will add
 *   the updates.
 *
 *   The last step writes the final ChangeSet file and moves the
 *   sfiles to their final locations. This is pretty fast and has to
 *   be in order.
 */
#include "sccs.h"
#include "range.h"
#include "nested.h"
#include "tomcrypt.h"

typedef struct {
	/* git's fields */
	char	*name;		/* "Oscar Bonilla" */
	char	*email;		/* "ob@bitkeeper.com" */
} who;

typedef	struct {
	char	*sha1;
	off_t	off;		/* offset where data starts */
	size_t	len;		/* length of data */
	u8	binary:1;	/* did we see a NULL in the data? */
} blob;

/* types of git operations */
enum op {
	GCOPY = 1,		/* copy file (not supproted) */
	GDELETE,		/* delete path */
	GDELETE_ALL,		/* delete all files (not supported) */
	GMODIFY,		/* new contents */
	GNOTE,			/* notemodify: ?? (not supported) */
	GRENAME,		/* rename file (not supported) */
};

enum m {
	MODE_FILE = 1,
	MODE_EXE,
	MODE_SYMLINK,
	MODE_GITLINK,
	MODE_SUBDIR,
};

typedef struct {
	enum op	op;		/* file operation */
	char	*path1;		/* path (ptr is unique) */
	char	*path2;		/* second path if rename or copy */
	blob	*m;		/* 'mark' struct where contents are */
	enum m	mode;		/* mode of file */
} gop;				/* git operation */

typedef struct commit commit;
struct commit {
	u32	ser;		/* commit order */
	char	*mark;
	time_t	when;		/* commit time */
	u32	fudge;		/* time fudge */
	char	*tz;		/* timezone  */
	who	*author;	/* git's author */
	who	*committer;	/* git's committer */
	char	*comments;	/* commit's comments */
	commit	**parents;	/* list of parents */
	gop	**fops;		/* file operations for this commit  */
	u32	*weave;		/* rk/dk pairs */
	u32	inarray:1;	/* in op->clist */
};

typedef struct {
	u32	quiet:1;	/* -q */
	u32	verbose:1;	/* -v */

	/* state */
	hash	*blobs;		/* mark -> blob struct */
	hash	*commits;	/* mark -> commit struct */
	hash	*paths;		/* uniq list of pathnames */
	commit	**clist;	/* array of commit*'s, oldest first */
	commit	*lastcset;	/* last cset from git, assume tip */
	int	filen;
	int	ncsets;
	sccs	*cset;

	char	*btmp;		/* tmpfile with blob data */
	FILE	*btmpf;		/* write handle to 'btmp' */
	MMAP	*bmap;		/* mmap for reading 'btmp' */
} opts;

typedef struct {
	sccs	*s;
	ser_t	*dlist;		/* tip of this sfile for each cset */
	u32	rk;		/* offset to rk in op->cset heap */
	u32	binary:1;
} finfo;

private int	gitImport(opts *op);
private void	setup(opts *op);
private	char	*getBlob(opts *op, char *line);
private	char	*reset(opts *op, char *line);
private	char	*getCommit(opts *op, char *line);
private	blob	*data(opts *op, char *line, FILE *f, int want_sha1);
private	char	*progressCmd(opts *op, char *line);

private	who	*parseWho(char *line, time_t *when, char **tz);
private	void	freeWho(who *w);
private	void	freeBlob(blob *m);
private	void	freeCommit(commit *m);

private gop	*parseOp(opts *op, char *line);
private void	freeOp(gop *op);

private	blob	*parseDataref(opts *op, char **s);
private	enum m	parseMode(opts *op, char **s);
private char	*parsePath(opts *op, char **s);

private	blob	*saveInline(opts *op);

private	int	importFile(opts *op, char *file);
private	finfo	newFile(opts *op, char *file, commit *cmt, gop *g);
private	void	newDelta(opts *op, finfo *, ser_t p,
    commit *cmt, gop *gp, gop *g);
private	void	newMerge(opts *op, finfo *, commit *cmt, gop **ghist, gop *g);
private	int	mkChangeSet(opts *op);

/* bk fast-import (aliased as _fastimport) */
int
fastimport_main(int ac, char **av)
{
	int	c;
	char	*dir;
	opts	opts = {0};
	longopt	lopts[] = {
		{ 0, 0 }
	};
	int	rc = 1;

	while ((c = getopt(ac, av, "qv", lopts)) != -1) {
		switch (c) {
		    case 'q': opts.quiet = 1; break;
		    case 'v': opts.verbose = 1; break;
		    default: bk_badArg(c, av);
		}
	}
	if (opts.quiet && opts.verbose) usage();
	if (dir = av[optind]) {
		if (streq(dir, "-")) usage();
		if (av[optind+1]) usage();

		if (!exists(dir) && mkdir(dir, 0777)) {
			fprintf(stderr, "%s: failed to create '%s'\n",
			    prog, dir);
			goto out;
		}
		if (chdir(dir)) {
			fprintf(stderr, "%s: cannot chdir to '%s'\n",
			    prog, dir);
			goto out;
		}
	}
	if (isdir(BKROOT)) {
		fprintf(stderr, "%s: Incremental imports not done yet\n",
			prog);
		rc = 1;
		goto out;
	} else if (emptyDir(".")) {
		/* we need a place to save tmp files before we get started */
		sccs_mkroot(".");
	} else {
		fprintf(stderr,
		    "%s: needs to be run in an empty directory "
		         "or existing repository\n",
		    prog);
		goto out;
	}
	rc = gitImport(&opts);
out:	return (rc);
}

private int
cmtTimeSort(void *a, void *b)
{
	commit	*c1 = a;
	commit	*c2 = b;
	long	diff;

	diff = (long)c1->when - (long)c2->when;
	if (diff) return (diff);    /* time order */
	return (c1->ser - c2->ser); /* original order */
}

#define	MATCH(s1)	(strneq(s1, line, strlen(s1)))

private int
gitImport(opts *op)
{
	char	*line;
	int	i, n;
	int	rc;
	char	**list = 0;
	PQ	*pq;
	commit	*cmt;
	ticker	*tick = 0;
	struct {
		char	*s;			/* cmd */
		char	*(*fn)(opts *, char *);	/* handler */
	} cmds[] = {
		{"blob", getBlob},
		{"commit", getCommit},
		{"progress", progressCmd},
		{"reset", reset},
		{0, 0}
	};

	/*
	 * Setup
	 */
	op->commits = hash_new(HASH_MEMHASH);
	op->blobs = hash_new(HASH_MEMHASH);
	op->paths = hash_new(HASH_MEMHASH);
	op->btmp = bktmp_local(0);
	op->btmpf = fopen(op->btmp, "wb");
	assert(op->btmpf);

	/*
	 * Processing:
	 *
	 * All functions may 'peek' at the next line by reading it and
	 * returning it. If a function returns NULL, we'll read a line
	 * from stdin. I.e. if a function consumes a line it has to
	 * either read another line or return NULL.
	 */
	rc = 0;
	line = fgetline(stdin);
	while (line || (line = fgetline(stdin))) {
		/*
		 * Git has a bunch of optional LF, just skip them. If
		 * we don't screw up the state machine this is safe to
		 * do.
		 */
		unless (*line) {
			line = 0;
			continue;
		}

		/*
		 * Call the appropriate handler for a git command.
		 */
		for (i = 0; cmds[i].s; i++) {
			if (MATCH(cmds[i].s)) {
				line = cmds[i].fn(op, line);
				break;
			}
		}
		unless (cmds[i].s) {
			fprintf(stderr, "%s: Unknown command: %s\n", prog, line);
			rc = 1;
			break;
		}
	}
	fclose(op->btmpf);
	op->bmap = mopen(op->btmp, "r");
	assert(op->bmap);
	if (rc || !op->ncsets) {
		fprintf(stderr, "%s: no csets to import on stdin\n", prog);
		rc = 1;
		goto out;
	}

	/* sort csets in time order */
	pq = pq_new(cmtTimeSort);
	assert(op->ncsets == hash_count(op->commits));
	growArray(&op->clist, op->ncsets);
	pq_insert(pq, op->lastcset);
	n = op->ncsets;
	while (pq_peek(pq)) {
		cmt = pq_pop(pq);
		cmt->ser = n;
		op->clist[n--] = cmt;

		EACH(cmt->parents) {
			commit	*p = cmt->parents[i];

			if (p->inarray) continue;
			p->inarray = 1;
			pq_insert(pq, p);
		}
	}
	pq_free(pq);
	assert(n == 0);

	setup(op);

	putenv("_BK_NO_UNIQ=1");
	putenv("_BK_MV_OK=1");
	cmdlog_lock(CMD_WRLOCK);

	/* get sorted unique list of pathnames */
	EACH_HASH(op->paths) list = addLine(list, (char *)op->paths->kptr);
	sortLines(list, 0);

	unless (op->quiet || op->verbose) {
		tick = progress_start(PROGRESS_BAR, nLines(list));
	}
	op->cset = sccs_init(CHANGESET, 0);
	EACH(list) {
		if (op->verbose) fprintf(stderr, "import %s\n", list[i]);
		importFile(op, list[i]);
		if (tick) progress(tick, i);
	}
	freeLines(list, 0);
	if (tick) {
		progress_done(tick, "OK");
		tick = 0;
	}
	mclose(op->bmap);
	op->bmap = 0;
	unlink(op->btmp);
	FREE(op->btmp);

	mkChangeSet(op);
	sccs_newchksum(op->cset); /* write new heap */
	sccs_free(op->cset);

	unless (op->quiet) fprintf(stderr, "Rename sfiles...\n");
	if (rc = system("bk -r names")) {
		fprintf(stderr, "%s: 'bk -r names' failed\n", prog);
	}

out:
	/*
	 * Teardown
	 */
	free(op->clist);
	EACH_HASH(op->commits) {
		commit	*m = op->commits->vptr;
		freeCommit(m);
	}
	hash_free(op->commits);
	EACH_HASH(op->blobs) {
		blob	*m = op->blobs->vptr;

		freeBlob(m);
	}
	hash_free(op->blobs);
	hash_free(op->paths);
	return (rc);
}

/*
 *   blob
 *       Requests writing one file revision to the packfile. The
 *       revision is not connected to any commit; this connection must
 *       be formed in a subsequent commit command by referencing the
 *       blob through an assigned mark.
 *
 *                   'blob' LF
 *                   mark?
 *                   data
 *
 *       The mark command is optional here as some frontends have
 *       chosen to generate the Git SHA-1 for the blob on their own,
 *       and feed that directly to commit. This is typically more work
 *       than it's worth however, as marks are inexpensive to store
 *       and easy to use.
 *
 *   -----------------------------------------------------------------
 *
 *   What we do for blobs is to stash them in a temporary file and add
 *   them to the marks hash so that we can 'mv' the temp file to
 *   where it goes once we reach the actual commit. While we're doing
 *   that, we check to see their type so we can detect type changes
 *   (from text to binary) which git doesn't tell us about here (it
 *   does in the diff-tree command).
 */
private char *
getBlob(opts *op, char *line)
{
	blob	*m;
	char	*mark = 0;
	char	*key;

	assert(MATCH("blob"));
	line = fgetline(stdin);
	if (MATCH("mark :")) {
		/* we got ourselves a mark, let's store it */
		mark = line + strlen("mark :");
		mark = strdup(mark);
		line = fgetline(stdin);
	}
	m = data(op, line, 0, !mark);
	key = mark ? mark : m->sha1;
	/* save the mark */
	unless (hash_insertStrMem(op->blobs, key, m, sizeof(*m))) {
		fprintf(stderr, "%s: ERROR: Duplicate key: %s\n", prog, key);
		exit(1);
	}
	free(m);		/* memory is copied into hash */
	free(mark);
	return (0);		/* we used up all the lines */
}

/*
 *   commit
 *       Create or update a branch with a new commit, recording one logical
 *       change to the project.
 *
 *                   'commit' SP <ref> LF
 *                   mark?
 *                   ('author' (SP <name>)? SP LT <email> GT SP <when> LF)?
 *                   'committer' (SP <name>)? SP LT <email> GT SP <when> LF
 *                   data
 *                   ('from' SP <commit-ish> LF)?
 *                   ('merge' SP <commit-ish> LF)?
 *                   (filemodify | filedelete | filecopy | filerename |
 *                    filedeleteall | notemodify)*
 *                   LF?
 *
 */
private char *
getCommit(opts *op, char *line)
{
	blob	*mrk;
	commit	*cmt, *parent;
	char	*key;
	char	*p;
	gop	*gop;
	FILE	*f;
	long	tdiff;

	assert(MATCH("commit "));
	/* XXX: Need to get ref from line before reading another one. */
	line = fgetline(stdin);
	unless (MATCH("mark :")) {
		fprintf(stderr, "%s: unmarked commits not supported\n", prog);
		exit(1);
	}
	key = line + strlen("mark :");
	unless (cmt = hash_insertStrMem(op->commits, key, 0, sizeof(commit))) {
		fprintf(stderr, "%s: ERROR: Duplicate mark: %s\n", prog, key);
		exit(1);
	}
	cmt->mark = op->commits->kptr;
	line = fgetline(stdin);

	if (MATCH("author")) {
		/* Ignore author? */
		cmt->author = parseWho(line, 0, 0);
		line = fgetline(stdin);
	}

	unless (MATCH("committer")) {
		fprintf(stderr, "%s: expected 'committer' line\n", prog);
		exit(1);
	}
	cmt->committer = parseWho(line, &cmt->when, &cmt->tz);
	line = fgetline(stdin);

	/*
	 * Get comments from data command
	 */
	f = fmem();
	mrk = data(op, line, f, 0);
	freeBlob(mrk);
	free(mrk);
	cmt->comments = fmem_close(f, 0);
	line = fgetline(stdin);

	while (MATCH("from ") || MATCH("merge")) {
		if (line[0] == 'f') {
			p = line + strlen("from ");
		} else {
			p = line + strlen("merge ");
		}
		if (p[0] == ':') {
			parent = (commit *)hash_fetchStr(op->commits, p+1);
			unless (parent) {
				fprintf(stderr, "%s: parent '%s' not found\n",
				    prog, p);
				exit(1);
			}
			addArrayV(&cmt->parents, parent);
			tdiff = (long)parent->when - (long)cmt->when;
			if (tdiff >= 0) {
				/* parent is younger than me */

				++tdiff;
				cmt->when += tdiff;
				cmt->fudge += tdiff;
			}
		} else {
			fprintf(stderr, "%s: line '%s' not handled\n",
			    prog, line);
			exit(1);
		}
		line = fgetline(stdin);
	}

	while (gop = parseOp(op, line)) {
		switch (gop->op) {
		    case GMODIFY:
		    case GDELETE:
			/* supported */
			break;
		    case GCOPY:
		    case GDELETE_ALL:
		    case GRENAME:
		    case GNOTE:
		    default:
			fprintf(stderr, "%s: line '%s' not supported\n",
			    prog, line);
			exit(1);
		}
		addArrayV(&cmt->fops, gop);
		line = fgetline(stdin);
	}
	op->lastcset = cmt;
	cmt->ser = ++op->ncsets;
	return (line);
}

/*
 * Parse a file operation.
 *
 * filemodify:
 *    'M' SP <mode> SP <dataref> SP <path> LF
 *    'M' SP <mode> SP 'inline' SP <path> LF
 *    data
 *
 * filedelete:
 *   'D' SP <path> LF
 *
 * filecopy:
 *   'C' SP <path> SP <path> LF
 *
 * filerename:
 *   'R' SP <path> SP <path> LF
 *
 * filedeleteall:
 *   'deleteall' LF
 *
 * notemodify:
 *   'N' SP <dataref> SP <commit-ish> LF
 *   'N' SP 'inline' SP <commit-ish> LF
 *   data
 */
private gop *
parseOp(opts *op, char *line)
{
	gop	*g;
	char	*p;

	g = new(gop);
	if (streq(line, "deleteall")) {
		g->op = GDELETE_ALL;
		return g;
	}

	switch (line[0]) {
	    case 'C': g->op = GCOPY;	break;
	    case 'D': g->op = GDELETE;	break;
	    case 'M': g->op = GMODIFY;	break;
	    case 'N': g->op = GNOTE;	break;
	    case 'R': g->op = GRENAME;	break;
	    default:
		free(g);
		return (0);
		break;
	}
	assert(line[1] == ' ');
	p = &line[2];
	if (g->op == GMODIFY) {
		/* parse mode */
		g->mode = parseMode(op, &p);
		switch(g->mode) {
		    case MODE_GITLINK:
			fprintf(stderr,
			    "%s: importing submodules not supported\n",
			    prog);
			exit(1);
		    case MODE_SUBDIR:
			assert(0);
			break;
		    default:
			break;
		}
	}
	if ((g->op == GMODIFY) || (g->op == GNOTE)) {
		/* parse dataref/inline */
		g->m = parseDataref(op, &p);
	}
	if (g->op == GNOTE) {
		/*
		 * Notes are not supported anyway so skip parsing
		 * commit-ish.
		 */
		return (g);
	}
	g->path1 = parsePath(op, &p);
	if ((g->op == GDELETE) || (g->op == GMODIFY)) return (g);
	g->path2 = parsePath(op, &p);
	return (g);
}

private void
freeOp(gop *op)
{
	unless (op) return;
	free(op);
}

/*
 * Try to parse a dataref out of 's'. Leave 's' pointing after the
 * consumed portion. Returns a 'mark' with all the info from the file.
 *
 * Special case: if the word 'inline' is encountered, it is consumed
 * from the string and stdin is read looking for a 'data' command. If
 * there is none, the program exits 1 (parse error). If 'data' is
 * found, the actual data is read and saved in 'marks'. The SHA1 of
 * the temporary file is saved in the marks hash and the newly created
 * 'mark' is what is returned.
 */
private blob *
parseDataref(opts *op, char **s)
{
	char	*key;
	blob	*m;

	assert(s && *s);
	if (strneq(*s, "inline", strlen("inline"))) {
		(*s) += strlen("inline");
		(*s)++;	/* skip space */
		return (saveInline(op));
	}
	key = (*s);
	if (key[0] == ':') {
		char	*p;

		/* we have a mark, get rid of the ':' */
		key++;
		p = strchr(key, ' ');
		assert(p);
		*p = 0;
		*s = p+1;
	} else {
		*s = strchr(*s, ' ');
	}
	unless (m = hash_fetchStrMem(op->blobs, key)) {
		fprintf(stderr, "%s: ERROR: failed to find mark: %s\n",
		    prog, key);
		exit(1);
	}
	return (m);
}

private blob *
saveInline(opts *op)
{
	fprintf(stderr, "%s: 'inline' not implemented\n", prog);
	exit(1);
	return (0);
}

/*
 * Try to parse a mode out of 's'. Leave 's' pointing after the
 * consumed portion. Returns the mode as an enum m.
 */
private	enum m
parseMode(opts *op, char **s)
{
	char	*p;
	enum m	ret;
	int	i;
	struct {
		char	*str;
		enum m	mode;
	} validModes[] = {
		{ "100644", MODE_FILE},	{ "644", MODE_FILE},
		{ "100755", MODE_EXE},	{ "755", MODE_EXE},
		{ "120000", MODE_SYMLINK},
		{ "160000", MODE_GITLINK},
		{ "040000", MODE_SUBDIR},
		{ 0, 0 }
	};

	assert(s);
	p = strchr(*s, ' ');
	unless (p) {
		fprintf(stderr, "%s: Parse Failed: '%s'\n", prog, *s);
		exit(1);
	}
	*p = 0;
	ret = 0;
	for (i = 0; validModes[i].str; i++) {
		if (streq(*s, validModes[i].str)) {
			ret = validModes[i].mode;
			break;
		}
	}
	unless (ret) {
		fprintf(stderr, "%s: invalid mode found: %s\n", prog, *s);
		exit(1);
	}
	*s = p+1;
	return (ret);
}

/*
 * Try to parse a path out of 's'. Leave 's' pointing after the
 * consumed portion. Returns an allocated string with the parsed part.
 */
private char *
parsePath(opts *op, char **s)
{
	char	*start, *end;
	char	*p, *q;
	u8	n;
	int	c;
	char	*ret;

	assert(s);
	start = *s;
	if (start[0] != '"') {
		/*
		 * Simple case, nothing funny
		 */
		n = (end = strchr(start, ' ')) ? start - end : strlen(start);
		*s = start + n;
		c = start[n];
		start[n] = 0;
		hash_insertStrSet(op->paths, start);
		start[n] = c;
		ret = op->paths->kptr;
		return (ret);
	}
	/*
	 * Complicated case, do the parsing
	 */
	ret = p = malloc(strlen(start));
	for (q = start + 1; *q && (*q != '"'); p++, q++) {
		if (*q == '\\') {
			q++;
			switch (*q) {
			    case 'a': *p = '\a'; break;
			    case 'b': *p = '\b'; break;
			    case 'f': *p = '\f'; break;
			    case 'n': *p = '\n'; break;
			    case 'r': *p = '\r'; break;
			    case 't': *p = '\t'; break;
			    case 'v': *p = '\v'; break;
			    case '\\': *p = '\\'; break;
			    case '"':  *p = '"'; break;
			    case '0'...'7':
				/* octal */
				n = ((*q++ - '0') << 6);
				assert((*q >= '0') && (*q <= '7'));
				n |= ((*q++ - '0') << 3);
				assert((*q >= '0') && (*q <= '7'));
				n |= (*q - '0');
				*p = n;
				break;
			    default:
				assert(!"unknown escape");
				break;
			}
		} else {
			*p = *q;
		}
	}
	*p = 0;
	*s = q;
	hash_insertStrSet(op->paths, ret);
	free(ret);
	ret = op->paths->kptr;
	return (ret);
}

/*
 * Parse an authorship line like this one:
 *
 *     'committer' (SP <name>)? SP LT <email> GT SP <when> LF
 *
 */
private	who *
parseWho(char *line, time_t *when, char **tz)
{
	char	*p, *q;
	who	*w;
	int	zone;

	assert(line);
	w = new(who);
	/* skip whatever command was first (e.g. 'author ') */
	p = strchr(line, ' ');
	assert(p);
	p++;			/* skip space */
	if (*p != '<') {
		/* Get name */
		w->name = p;
		p = strchr(p, '<');
		*(p-1) = 0;
		w->name = strdup(w->name);
	}
	/* Get email */
	while (p[1] == ' ') ++p; /* skip space */
	w->email = ++p;
	p += strcspn(p, "<>");
	while ((p > w->email) && (p[-1] == ' ')) --p; /* trim space */
	*p++ = 0;
	w->email = strdup(w->email);

	/*
	 * XXX Larry wants to ignore email and cons up something else when
	 * the email is stupid like none@none
	 * I think I disagree, if git history is stupid, we should reproduce
	 * that.
	 */

	/*
	 * skip any extra cruft:
	 *   Guoli Shu<Kerry.Shu@Sun.COM> <none@none> 1219106876 -0700
	 * (git has the same code)
	 */
	if (q = strrchr(p, '>')) p = q+1;

	/*
	 * other bad lines:
	 * roland.mainz <roland.mainz@nexenta.com <none@none> 1297982978 -0800
	 */
	assert(*p == ' ');

	/*
	 * Get when. Git supports multiple formats for the 'when'
	 * field. This only handles the default.
	 *
	 * XXX: fix me.
	 */
	if (when) {
		++p;			/* skip space */
		*when = strtol(p, &q, 10);
		assert(*when);
		assert(*q == ' ');
		p = ++q;
		zone = strtol(p, &q, 10);
		assert(!*q);
		q = *tz = malloc(7); /* -HH:MM */
		if (zone < 0) {
			*q++ = '-';
			zone = -zone;
		} else {
			*q++ = '+';
		}
		assert(zone < 10000);
		sprintf(q, "%02d:%02d", (zone / 100), (zone % 100));
	}
	return (w);
}

private	void
freeWho(who *w)
{
	unless (w) return;
	free(w->name);
	free(w->email);
	free(w);
}

private void
freeBlob(blob *m)
{
	free(m->sha1);
}

private void
freeCommit(commit *m)
{
	int	i;

	unless (m) return;
	free(m->tz);
	freeWho(m->author);
	freeWho(m->committer);
	free(m->comments);
	free(m->parents);
	EACH(m->fops) freeOp(m->fops[i]);
	free(m->fops);
	free(m->weave);
}

/*
 *   reset
 *       Creates (or recreates) the named branch, optionally starting from a
 *       specific revision. The reset command allows a frontend to issue a new
 *       from command for an existing branch, or to create a new branch from an
 *       existing commit without creating a new commit.
 *
 *                   'reset' SP <ref> LF
 *                   ('from' SP <commit-ish> LF)?
 *                   LF?
 *
 *       For a detailed description of <ref> and <commit-ish> see above under
 *       commit and from.
 *
 *       The LF after the command is optional (it used to be required).
 *
 *       The reset command can also be used to create lightweight
 *       (non-annotated) tags. For example:
 *
 *           reset refs/tags/938
 *           from :938
 *
 *       would create the lightweight tag refs/tags/938 referring to whatever
 *       commit mark :938 references.
 */
private char *
reset(opts *op, char *line)
{
	assert(MATCH("reset "));
	/* ignored for now since we don't really have branches */
	line = fgetline(stdin);
	if (MATCH("from ")) {
		/* handle from */
		return (0);
	}
	return (line);
}

/*  progress
 *      Causes fast-import to print the entire progress line unmodified to its
 *      standard output channel (file descriptor 1) when the command is
 *      processed from the input stream. The command otherwise has no impact on
 *      the current import, or on any of fast-import's internal state.
 *
 *                  'progress' SP <any> LF
 *                  LF?
 *
 *      The <any> part of the command may contain any sequence of bytes that
 *      does not contain LF. The LF after the command is optional. Callers may
 *      wish to process the output through a tool such as sed to remove the
 *      leading part of the line, for example:
 *
 *          frontend | git fast-import | sed 's/^progress //'
 *
 *      Placing a progress command immediately after a checkpoint will inform
 *      the reader when the checkpoint has been completed and it can safely
 *      access the refs that fast-import updated.
 */
private char *
progressCmd(opts *op, char *line)
{
	assert(MATCH("progress "));
	unless (op->quiet) {
		printf("%s\n", line);
		fflush(stdout);
	}
	return (0);
}

/*
 */
private void
setup(opts *op)
{
	int	zone = 0;
	int	neg = 1;
	char	*p;
	time_t	tt;
	commit	*cmt = op->clist[1];
	char	buf[MAXLINE];

	tt = cmt->when - 5*MINUTE;
	p = cmt->tz;
	if (*p == '-') {
		neg = -1;
		++p;
	} else if (*p == '+') {
		++p;
	}
	zone = HOUR * atoi_p(&p);
	if (*p++ == ':') zone += MINUTE * atoi_p(&p);
	tt += neg * zone;
	strftime(buf, sizeof(buf), "%y/%m/%d %H:%M:%S", gmtime(&tt));

	if (systemf("bk '-?BK_DATE_TIME_ZONE=%s' init", buf)) {
		perror("bk init");
		exit(1);
	}
}

/*
 *   data
 *       Supplies raw data (for use as blob/file content, commit
 *       messages, or annotated tag messages) to fast-import. Data can
 *       be supplied using an exact byte count or delimited with a
 *       terminating line. Real frontends intended for
 *       production-quality conversions should always use the exact
 *       byte count format, as it is more robust and performs
 *       better. The delimited format is intended primarily for
 *       testing fast-import.
 *
 *       Comment lines appearing within the <raw> part of data
 *       commands are always taken to be part of the body of the data
 *       and are therefore never ignored by fast-import. This makes it
 *       safe to import any file/message content whose lines might
 *       start with #.
 *
 *       Exact byte count format
 *           The frontend must specify the number of bytes of data.
 *
 *                       'data' SP <count> LF
 *                       <raw> LF?
 *
 *           where <count> is the exact number of bytes appearing
 *           within <raw>.  The value of <count> is expressed as an
 *           ASCII decimal integer. The LF on either side of <raw> is
 *           not included in <count> and will not be included in the
 *           imported data.
 *
 *           The LF after <raw> is optional (it used to be required)
 *           but recommended. Always including it makes debugging a
 *           fast-import stream easier as the next command always
 *           starts in column 0 of the next line, even if <raw> did
 *           not end with an LF.
 *
 *       Delimited format
 *           A delimiter string is used to mark the end of the
 *           data. fast-import will compute the length by searching
 *           for the delimiter. This format is primarily useful for
 *           testing and is not recommended for real data.
 *
 *                       'data' SP '<<' <delim> LF
 *                       <raw> LF
 *                       <delim> LF
 *                       LF?
 *
 *           where <delim> is the chosen delimiter string. The string
 *           <delim> must not appear on a line by itself within <raw>,
 *           as otherwise fast-import will think the data ends earlier
 *           than it really does.  The LF immediately trailing <raw>
 *           is part of <raw>. This is one of the limitations of the
 *           delimited format, it is impossible to supply a data chunk
 *           which does not have an LF as its last byte.
 *
 *           The LF after <delim> LF is optional (it used to be
 *           required).
 * -------------------------------------------------------------------
 *
 * Read a 'data' command from git and put it in 'marks'. Also check if the
 * stream has nulls in it and if so, tag it as binary.
 *
 * BUGS: It only supports git's exact byte count format. Needs support
 * for the delimited format.
 *
 */
private blob *
data(opts *op, char *line, FILE *f, int want_sha1)
{
	blob	*m;
	size_t	len, c1, c2;
	char	*sha1;
	int	i, idx, binary;
	hash_state	md;
	u8	buf[1<<12];

	assert(MATCH("data "));

	/*
	 * Get a temp file to put the data.
	 */
	unless (f) f = op->btmpf;
	m = new(blob);
	m->off = ftell(f);

	/*
	 * Initialize vars and get length
	 */
	m->len = len = strtoul(line + strlen("data "), 0, 10);
	binary = 0;
	if (want_sha1) {
		idx = register_hash(&sha1_desc);
		hash_descriptor[idx].init(&md);
		sprintf(buf, "blob %ld", len);
		/* we _want_ the trailing NULL */
		hash_descriptor[idx].process(&md, buf, strlen(buf)+1);
	}
	/*
	 * Read data and compute sha1
	 */
	while ((c1 = fread(buf, 1, min(sizeof(buf), len), stdin)) > 0) {
		unless (binary) binary = (memchr(buf, 0, c1) != 0);
		c2 = fwrite(buf, 1, c1, f);
		assert(c1 == c2);
		len -= c2;
		if (want_sha1) hash_descriptor[idx].process(&md, buf, c2);
	}

	/* Now insert the mark */
	if (want_sha1) {
		hash_descriptor[idx].done(&md, buf);

		/*
		 * Store the mark
		 */
		sha1 = malloc(2 * hash_descriptor[idx].hashsize + 2);
		for (i = 0; i < (int)hash_descriptor[idx].hashsize; i++) {
			sprintf(sha1 + i*2, "%02x", buf[i]);
		}
		*(sha1 + i * 2 + 1) = 0;

		m->sha1 = sha1;		/* free'd by freeBlob() */
	}
	m->binary = binary;
	return (m);
}

/* Everything above was parsing (analysis). This next part is synthesis. */


/*
 * Look at the list of sfiles for the current pathname at pick the
 * best one.  We can have mutiple for a couple reasons:
 *  - switching from ascii to BAM
 *  - files created in parallel (create/create conflict)
 */
private int
findMatch(finfo *sfiles, gop *g, u32 dp)
{
	sccs	*s, *sb = 0;
	int	best = 0;
	int	i;
	int	a, b;
	ser_t	d, db;
	char	key1[MAXKEY], key2[MAXKEY];

	/*
	 * Of the files that are active for this serial that have the right
	 * binary state, pick the oldest one and favor non-delete files
	 */
	EACH(sfiles) {
		if (g && (g->op != GDELETE) &&
		    (g->m->binary != sfiles[i].binary)) {
			/* BAM or not? */
			continue;
		}
		/* can't use a file that didn't exist at parent rev */
		unless (d = sfiles[i].dlist[dp]) continue;

		s = sfiles[i].s;
		unless (best) {	/* anything is better than nothing */
new:			best = i;
			sb = s;
			db = d;
			continue;
		}
		/* not deleted is better than deleted */
		a = begins_with(PATHNAME(s, d), "BitKeeper/deleted/");
		b = begins_with(PATHNAME(sb, db), "BitKeeper/deleted/");
		if (!a && b) goto new;
		if (a && !b) continue;

		/* older is better than newer */
		if (DATE(s, 1) < DATE(sb, 1)) goto new;
		if (DATE(s, 1) > DATE(sb, 1)) continue;

		/* if all else fails, sort by rootkey so we are stable */
		sccs_sdelta(s, 1, key1);
		sccs_sdelta(sb, 1, key2);
		if (strcmp(key1, key2) < 0) goto new;
	}
	return (best);
}


private int
importFile(opts *op, char *file)
{
	int	i, j, k;
	int	npar_git, npar_file;
	u32	d, dp;
	commit	*cmt = 0;
	u32	n, ud, uniq_n[2], uniq_d[2];
	gop	*g, **ghist = 0;

	/* all the sfiles that have been created for this pathname */
	finfo	*sfiles = 0;

	growArray(&ghist, op->ncsets);

	EACH_INDEX(op->clist, j) {
		cmt = op->clist[j];
		d = cmt->ser;

		/* did this cset change pathname 'file' ? */
		g = 0;
		EACH(cmt->fops) {
			if (cmt->fops[i]->path1 == file) {
				g = cmt->fops[i];
				break;
			}
		}
		npar_git = nLines(cmt->parents);
		dp = npar_git ? cmt->parents[1]->ser : d;
		ghist[d] = g ? g : ghist[dp];
		if (!g && (!sfiles || (npar_git == 1))) {
			/* nothing interesting happened */
			/* copy state from parent */
			EACH(sfiles) sfiles[i].dlist[d] = sfiles[i].dlist[dp];
			continue;
		}
		/* how many unique parents in file? */
		npar_file = 0;
		EACH_INDEX(cmt->parents, k) {
			dp = cmt->parents[k]->ser;
			unless (n = findMatch(sfiles, g, dp)) continue;
			unless (ud = sfiles[n].dlist[dp]) continue;
			for (i = 0; i < npar_file; i++) {
				if ((uniq_n[i] == n) && (uniq_d[i] == ud)) {
					break;
				}
			}
			assert(i < 2); /* no octopus on file */
			uniq_n[i] = n;
			uniq_d[i] = ud;
			if (i == npar_file) npar_file++;
		}
		if ((npar_file == 2) && (uniq_n[0] != uniq_n[1])) {
			sccs	*s0 = sfiles[uniq_n[0]].s;
			sccs	*s1 = sfiles[uniq_n[1]].s;
			int	del0 = begins_with(PATHNAME(s0, uniq_d[0]),
						   "BitKeeper/deleted/");
			int	del1 = begins_with(PATHNAME(s1, uniq_d[1]),
						   "BitKeeper/deleted/");

			/* 'i' is the side to keep */
			if (del0 && !del1) {
				i = 1;
			} else if (!del0 && del1) {
				i = 0;
			} else if (DATE(s0, 1) > DATE(s1, 1)) {
				/* keep older file */
				i = 1;
			} else if (DATE(s0, 1) < DATE(s1, 1)) {
				i = 0;
			} else {
				char	key0[MAXKEY];
				char	key1[MAXKEY];

				sccs_sdelta(s0, sccs_ino(s0), key0);
				sccs_sdelta(s1, sccs_ino(s1), key1);
				i = (strcmp(key0, key1) > 0);
			}
			uniq_n[0] = uniq_n[i];
			uniq_d[0] = uniq_d[i];
			npar_file = 1;
			unless (g) g = ghist[cmt->parents[1]->ser];
		}
		n = uniq_n[0];
		ud = uniq_d[0];
		if (npar_file == 0) {
			if (g) {
				// new file
				addArrayV(&sfiles, newFile(op, file, cmt, g));
				n = nLines(sfiles);
			} else {
				n = 0;
			}
		} else if ((npar_file == 1) && g) {
			gop	*gp = 0;

			if (npar_git == 1) gp = ghist[cmt->parents[1]->ser];
			// new delta
			newDelta(op, &sfiles[n], ud, cmt, gp, g);
		} else {
			// merge in file with new contents (g may be zero)
			assert(npar_file > 0);
			newMerge(op, &sfiles[n], cmt, ghist, g);
		}
		/* now update the other files */
		EACH(sfiles) {
			gop	gtmp = { GDELETE };

			if (i == n) continue;
			if (npar_git == 0) {
			} else if (npar_git == 1) {
				/* nothing interesting happened */
				dp = sfiles[i].dlist[cmt->parents[1]->ser];
				if (dp) {
					newDelta(op, &sfiles[i], dp,
					    cmt, 0, &gtmp);
				}
			} else {
				assert(npar_git > 1);
				// handle merging existing sfiles
				newMerge(op, &sfiles[i], cmt, ghist, &gtmp);
			}
		}
	}
	EACH(sfiles) {
		sccs_free(sfiles[i].s);
		free(sfiles[i].dlist);
	}
	free(sfiles);
	free(ghist);
	return (0);
}

private void
loadMetaData(sccs *s, ser_t d, commit *cmt)
{
	ser_t	prev;

	USERHOST_SET(s, d, cmt->committer->email);
	DATE_SET(s, d, cmt->when);
	DATE_FUDGE_SET(s, d, cmt->fudge);
	ZONE_SET(s, d, cmt->tz);

	// fudge
	if ((prev = sccs_prev(s, d)) &&
	    (DATE(s, d) <= DATE(s, prev)))  {
		time_t	tdiff;
		tdiff = DATE(s, prev) - DATE(s, d) + 1;
		DATE_SET(s, d, (DATE(s, d) + tdiff));
		DATE_FUDGE_SET(s, d, (DATE_FUDGE(s, d) + tdiff));
	}
}

private void
mkFile(opts *op, gop *g, char *gfile)
{
	int	fd;
	char	*p;
	int	rc;

	unlink(gfile);
	switch(g->mode) {
	    case MODE_SYMLINK:
		p = strndup(op->bmap->mmap + g->m->off, g->m->len);
		rc = symlink(p, gfile);
		assert(!rc);
		free(p);
		break;
	    case MODE_FILE:
	    case MODE_EXE:
		fd = open(gfile, O_WRONLY|O_CREAT,
		    (g->mode == MODE_EXE) ? 0755 : 0644);
		assert(fd >= 0);
		rc = write(fd, op->bmap->mmap + g->m->off, g->m->len);
		assert(rc == g->m->len);
		rc = close(fd);
		assert(!rc);
		break;
	    default:
		assert(0);
		break;
	}
}

private finfo
newFile(opts *op, char *file, commit *cmt, gop *g)
{
	sccs	*s;
	finfo	ret = {0};
	ser_t	d, d0;
	char	buf[MAXLINE];

	assert(g->op == GMODIFY);

	growArray(&ret.dlist, op->ncsets);
	ret.binary = g->m->binary;

	sprintf(buf, "%d", ++op->filen);
	ret.s = s = sccs_init(buf, 0);
	assert(s);

	mkFile(op, g, buf);
	check_gfile(s, 0);

	d0 = sccs_newdelta(s);
	loadMetaData(s, d0, cmt);
	sccs_parseArg(s, d0, 'P', file, 0);
	sccs_parseArg(s, d0, 'R', "1.0", 0);
	randomBits(buf+2);
	safe_putenv("BK_RANDOM=%s", buf+2);
	s->xflags = X_DEFAULT;
	XFLAGS(s, d0) = X_DEFAULT;
	SUM_SET(s, d0, almostUnique()); /* reads BK_RANDOM */
	SORTSUM_SET(s, d0, SUM(s, d0));
	if (g->m->binary) {
		buf[0] = 'B';
		buf[1] = ':';
		RANDOM_SET(s, d0, buf);
		s->encoding_in = sccs_encoding(s, 0, "BAM");
	} else {
		RANDOM_SET(s, d0, buf+2);
	}
	FLAGS(s, d0) |= D_INARRAY;

	ret.dlist[cmt->ser] = d = sccs_newdelta(s);
	loadMetaData(s, d, cmt);
	PARENT_SET(s, d, d0);
	sccs_parseArg(s, d, 'P', file, 0);
	sccs_parseArg(s, d, 'R', "1.1", 0);
	XFLAGS(s, d) = X_DEFAULT;

	if (BAM(s)) bp_delta(s, d);

	if (sccs_delta(s, SILENT|DELTA_NEWFILE|DELTA_PATCH|DELTA_CSETMARK,
	    d, 0, 0, 0)) {
		perror(s->gfile);
		exit(1);
	}
	s->state |= S_SFILE;

	/* record the cset weave */
	sccs_sdelta(s, sccs_ino(s), buf);
	ret.rk = sccs_addUniqRootkey(op->cset, buf);
	assert(ret.rk);
	addArrayV(&cmt->weave, ret.rk);
	sccs_sdelta(s, d, buf);
	addArrayV(&cmt->weave, sccs_addStr(op->cset, buf));
	/* and oldest delta marker */
	addArrayV(&cmt->weave, ret.rk);
	addArrayV(&cmt->weave, 0);

	return (ret);
}

private void
newDelta(opts *op, finfo *fi, ser_t p, commit *cmt, gop *gp, gop *g)
{
	sccs	*s = fi->s;
	ser_t	d;
	char	*t, *rel;
	int	rc;
	FILE	*diffs;
	char	buf[MAXLINE];

	if ((g->op == GMODIFY) && g->m->binary) {
		assert(BAM(s));
		assert(g->mode == MODE_EXE || g->mode == MODE_FILE);
		rc = sccs_get(s, REV(s, p), 0, 0, 0,
		    SILENT|GET_SKIPGET|GET_EDIT, 0, 0);
		assert(!rc);
		d = sccs_newdelta(s);
		loadMetaData(s, d, cmt);
		PATHNAME_INDEX(s, d) = PATHNAME_INDEX(s, 1);
		SORTPATH_INDEX(s, d) = SORTPATH_INDEX(s, 1);
		mkFile(op, g, s->gfile);
		check_gfile(s, 0);
		switch(g->mode) {
		    case MODE_FILE:
			MODE_SET(s, d, 0100664);
			break;
		    case MODE_EXE:
			MODE_SET(s, d, 0100775);
			break;
		    default:
			assert(0);
		}

		rc = sccs_delta(s, SILENT|DELTA_DONTASK|DELTA_CSETMARK,
		    d, 0, 0, 0);
		assert(!rc);
	} else if ((g->op == GMODIFY) && (g->mode == MODE_SYMLINK)) {
		rc = sccs_get(s, REV(s, p), 0, 0, 0,
		    SILENT|GET_SKIPGET|GET_EDIT, s->gfile, 0);
		assert(!rc);

		d = sccs_newdelta(s);
		loadMetaData(s, d, cmt);
		PATHNAME_INDEX(s, d) = PATHNAME_INDEX(s, 1);
		SORTPATH_INDEX(s, d) = SORTPATH_INDEX(s, 1);
		MODE_SET(s, d, 0120777);

		mkFile(op, g, s->gfile);
		t = strndup(op->bmap->mmap + g->m->off, g->m->len);
		SYMLINK_SET(s, d, t);
		free(t);
		check_gfile(s, 0);

		rc = sccs_delta(s, SILENT|DELTA_DONTASK|DELTA_CSETMARK,
		    d, 0, 0, 0);
		assert(!rc);
		assert(!exists(s->gfile));
	} else if (g->op == GMODIFY) {
		if (!(gp && gp->m) || HAS_SYMLINK(s, p)) {
			rc = sccs_get(s, REV(s, p), 0, 0, 0,
			    SILENT|GET_EDIT, s->gfile, 0);
			assert(!rc);
			diffs = 0;
			mkFile(op, g, s->gfile);
			check_gfile(s, 0);
		} else  {
			df_opt	dop = {0};
			char	*data[2];
			size_t	len[2];

			rc = sccs_get(s, REV(s, p), 0, 0, 0,
			    SILENT|GET_SKIPGET|GET_EDIT, 0, 0);
			assert(!rc);

			//dop.ignore_trailing_cr = 1;
			dop.out_rcs = 1;
			data[0] = op->bmap->mmap + gp->m->off;
			len[0] = gp->m->len;
			data[1] = op->bmap->mmap + g->m->off;
			len[1] = g->m->len;
			diffs = fmem();
			rc = diff_mem(data, len, &dop, diffs);
			assert((rc == 0) || (rc == 1));
			rewind(diffs);
		}
		d = sccs_newdelta(s);
		loadMetaData(s, d, cmt);
		PATHNAME_INDEX(s, d) = PATHNAME_INDEX(s, 1);
		SORTPATH_INDEX(s, d) = SORTPATH_INDEX(s, 1);
		switch(g->mode) {
		    case MODE_FILE:
			MODE_SET(s, d, 0100664);
			break;
		    case MODE_EXE:
			MODE_SET(s, d, 0100775);
			break;
		    default:
			assert(0);
		}

		rc = sccs_delta(s,
		    SILENT|DELTA_DONTASK|DELTA_CSETMARK|DELTA_FORCE,
		    d, 0, diffs, 0);
		assert(!rc);
		if (diffs) fclose(diffs);
	} else {
		assert(g->op = GDELETE);

		if (begins_with(PATHNAME(s, p), "BitKeeper/deleted/")) {
			/* already deleted */
			fi->dlist[cmt->ser] = p;
			return;
		}
		rc = sccs_get(s, REV(s, p), 0, 0, 0,
		    SILENT|GET_SKIPGET|GET_EDIT, 0, 0);
		assert(!rc);

		d = sccs_newdelta(s);
		loadMetaData(s, d, cmt);
		rel = sccs_rmName(s);
		t = sccs2name(rel);
		free(rel);
		rel = proj_relpath(s->proj, t);
		free(t);
		PATHNAME_SET(s, d, rel);
		SORTPATH_SET(s, d, rel);
		free(rel);
		t = aprintf("Delete: %s\n", PATHNAME(s, p));
		COMMENTS_SET(s, d, t);
		free(t);
		if (BAM(s)) {
			// XXX gfile doesn't matter here, we are deleting
			// but better is probably to copy my parent's file
			//fileLink(td.m->file, s->gfile);
			touch(s->gfile, 0666);
		}
		diffs = fmem();
		rc = sccs_delta(s,
		    SILENT|DELTA_DONTASK|DELTA_CSETMARK|DELTA_FORCE,
		    d, 0, diffs, 0);
		assert(!rc);
		fclose(diffs);
	}
	s->state &= ~S_PFILE;

	/* record the cset weave */
	assert(fi->rk);
	addArrayV(&cmt->weave, fi->rk);
	sccs_sdelta(s, d, buf);
	addArrayV(&cmt->weave, sccs_addStr(op->cset, buf));

	fi->dlist[cmt->ser] = d;
}

private void
newMerge(opts *op, finfo *fi, commit *cmt, gop **ghist, gop *g)
{
	sccs	*s = fi->s;
	char	*t, *rel;
	ser_t	d;		/* new merge delta being created */
	ser_t	dp, dm;		/* bk's parent/merge deltas */
	ser_t	dp_git;		/* bk's delta matching git's parent */
	int	rc;
	FILE	*f;
	gop	*gp;
	char	buf[MAXPATH];

	assert(nLines(cmt->parents) == 2);
	dp = dp_git = fi->dlist[cmt->parents[1]->ser];
	dm =          fi->dlist[cmt->parents[2]->ser];
	if (!(dp && dm) || (dp == dm) || isReachable(s, dp, dm)) {
		d = max(dp, dm);
		if (!d) {
			/* no history for this file */
			assert(!g || (g->op == GDELETE));
			fi->dlist[cmt->ser] = 0;
		} else if (g) {
			newDelta(op, fi, d, cmt, 0, g);
		} else if (!dp && (d != dp_git)) {
			/* if a merge has g==0 and the file only exists on
			 * merge parent, then the merge is ignored and
			 * the file is deleted
			 */
			gop	gtmp = { GDELETE };
			newDelta(op, fi, d, cmt, 0, &gtmp);
		} else if (d == dp_git) {
			/* add nothing, just keep parent */
			fi->dlist[cmt->ser] = d;
		} else {
			/*
			 * a merge with g==0 where we are reverting
			 * changes we must be reverting the changes on the
			 * merge side
			 */
			assert(dp && dm && (d == dm));
			rc = sccs_get(s, REV(s, d), 0, 0, 0,
			    SILENT|GET_SKIPGET|GET_EDIT, s->gfile, 0);
			assert(!rc);

			d = sccs_newdelta(s);
			loadMetaData(s, d, cmt);

			PATHNAME_INDEX(s, d) = PATHNAME_INDEX(s, dp_git);
			SORTPATH_INDEX(s, d) = SORTPATH_INDEX(s, dp_git);
			unlink(s->gfile);
			f = fopen(s->gfile, "w");
			rc = sccs_get(s, REV(s, dp_git), 0, 0, 0,
			    SILENT|PRINT, 0, f);
			assert(!rc);
			fclose(f);
			check_gfile(s, 0);
			rc = sccs_delta(s, SILENT|DELTA_DONTASK|DELTA_CSETMARK,
			    d, 0, 0, 0);
			assert(!rc);
			s->state &= ~S_PFILE;
			goto out;
		}
		return;
	}  else if (sccs_needSwap(s, dp, dm, 0)) {
		d = dp;
		dp = dm;
		dm = d;
		/* dp_git doesn't move */
	}
	if (!g || (g->op == GMODIFY)) {
		rc = sccs_get(s, REV(s, dp), REV(s, dm), 0, 0,
		    SILENT|GET_EDIT, s->gfile, 0);
		assert(!rc);

		d = sccs_newdelta(s);
		loadMetaData(s, d, cmt);

		gp = ghist[cmt->parents[1]->ser];
		if (g) {
			/*
			 * if we have data, then it must be the original
			 * pathname.
			 */
			PATHNAME_INDEX(s, d) = PATHNAME_INDEX(s, 1);
			SORTPATH_INDEX(s, d) = SORTPATH_INDEX(s, 1);

			mkFile(op, g, s->gfile);
		} else {
			/* merge with g==0, we match parent */
			PATHNAME_INDEX(s, d) = PATHNAME_INDEX(s, dp_git);
			SORTPATH_INDEX(s, d) = SORTPATH_INDEX(s, dp_git);

			if (gp->op != GDELETE)	mkFile(op, gp, s->gfile);
		}
		check_gfile(s, 0);
		rc = sccs_delta(s, SILENT|DELTA_DONTASK|DELTA_CSETMARK,
		    d, 0, 0, 0);
		assert(!rc);
		s->state &= ~S_PFILE;
	} else if (g->op == GDELETE) {
		/* gfile will contain "sccs-merge" */
		rc = sccs_get(s, REV(s, dp), REV(s, dm), 0, 0,
		    SILENT|GET_EDIT, s->gfile, 0);
		assert(!rc);
		d = sccs_newdelta(s);
		loadMetaData(s, d, cmt);
		rel = sccs_rmName(s);
		t = sccs2name(rel);
		free(rel);
		rel = proj_relpath(s->proj, t);
		free(t);
		PATHNAME_SET(s, d, rel);
		SORTPATH_SET(s, d, rel);
		free(rel);
		rc = sccs_delta(s,
		    SILENT|DELTA_DONTASK|DELTA_CSETMARK|DELTA_FORCE,
		    d, 0, 0, 0);
		assert(!rc);
		s->state &= ~S_PFILE;
	} else {
		assert(0);
	}
out:	/* record the cset weave */
	assert(fi->rk);
	addArrayV(&cmt->weave, fi->rk);
	sccs_sdelta(s, d, buf);
	addArrayV(&cmt->weave, sccs_addStr(op->cset, buf));

	fi->dlist[cmt->ser] = d;
}

private int
mkChangeSet(opts *op)
{
	sccs	*s = op->cset;
	ser_t	d, p, m;
	int	i;
	int	nparents;
	commit	*cmt = 0;

	unless (op->quiet) fprintf(stderr, "Writing new ChangeSet file...\n");
	EACH(op->clist) {
		cmt = op->clist[i];

		d = sccs_newdelta(s);
		FLAGS(s, d) |= D_INARRAY|D_CSET;
		loadMetaData(s, d, cmt);
		XFLAGS(s, d) = XFLAGS(s, 2);
		CSETFILE_SET(s, d, CSETFILE(s, 2));
		PATHNAME_INDEX(s, d) = PATHNAME_INDEX(s, 2);
		SORTPATH_INDEX(s, d) = SORTPATH_INDEX(s, 2);
		MODE_SET(s, d, MODE(s, 2));

		ADDED_SET(s, d, nLines(cmt->weave)/2);
		SAME_SET(s, d, 1);
		R0_SET(s, d, 1);
		R1_SET(s, d, cmt->ser + 1);
		COMMENTS_SET(s, d, cmt->comments);
		WEAVE_SET(s, d, s->heap.len);
		addArrayV(&cmt->weave, 0);
		data_append(&s->heap,
		    cmt->weave+1, sizeof(u32)*nLines(cmt->weave));

		nparents = nLines(cmt->parents);
		if (nparents == 0) {
			PARENT_SET(s, d, 2);
		} else if (nparents == 1) {
			PARENT_SET(s, d, cmt->parents[1]->ser+2);
		} else if (nparents == 2) {
			p = cmt->parents[1]->ser+2;
			m = cmt->parents[2]->ser+2;
			if (isReachable(s, p, m)) {
				/* no need for merge */
				if (p < m) p = m;
				m = 0;
			} else if (sccs_needSwap(s, p, m, 0)) {
				ser_t	tmp = p;
				p = m;
				m = tmp;
			}
			PARENT_SET(s, d, p);
			MERGE_SET(s, d, m);
		} else {
			assert(0);
		}
		TABLE_SET(s, d);
	}
	unless (op->quiet) fprintf(stderr, "Renumber...\n");
	sccs_renumber(s, 0);

	unless (op->quiet) fprintf(stderr, "Generate checksums...\n");
	cset_resum(s, 0, 1, 0, 0);

	return (0);
}
