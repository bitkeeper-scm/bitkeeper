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
 * Code layout:
 * main() calls gitImport()
 *
 * gitImport()
 *    - reads fast-export stream from stdin and processes using
 *      helper functions like getCommit() and getBlob()
 *      - this builds the os->clist data structure with the graph of
 *	  all commits.
 *
 *    - calls importFile() for each unique pathname in os->paths
 *      - Extracts graph for each file and builds sfiles
 *      - returns rootkey/deltakey pairs for toplevel csets
 *
 *    - writes toplevel ChangeSet file and then uses 'bk checksum' for
 *      fixup cset keys
 *
 *
 * Threading possibilities:
 *   The parsing code builds the commit structure in the same order
 *   that importFile() consumes them.
 *
 *   So one thread can be consuming stdin and building the toplevel
 *   data structure.  As it encounters each new pathname it can start
 *   a thread to build each new sfile.  Those threads can write the
 *   sfiles, but can't finish until the parsing thread finishes.
 *
 *   The last step writes the final ChangeSet file, it shouldn't take
 *   that long.
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
	int	refcnt;		/* number of file ops pointing at this blob */
	char	*file;		/* file where data is */
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
	int	ser;		/* commit order */
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
};

typedef struct {
	/* state */
	hash	*blobs;		/* mark -> blob struct */
	hash	*commits;	/* mark -> commit struct */
	hash	*paths;		/* uniq list of pathnames */
	commit	**clist;	/* array of commit*'s, oldest first */
	int	filen;
	sccs	*cset;
} opts;

/* the sfile/delta combo that records a certain git blob */
typedef struct {
	sccs	*s;
	ser_t	d;
	blob	*m;
	u32	rk;		/* rkoff in cset heap */
} sdelta;

private int	gitImport(opts *op);
private void	setup(opts *op);
private	char	*getBlob(opts *op, char *line);
private	char	*reset(opts *op, char *line);
private	char	*getCommit(opts *op, char *line);
private	blob	*data(opts *op, char *line, FILE *f, int want_sha1);

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
private	sdelta	newFile(opts *op, char *file, commit *cmt, gop *g);
private	sdelta	newDelta(opts *op, sdelta td, commit *cmt, gop *g);
private	sdelta	newMerge(opts *op, sdelta p[2], commit *cmt, gop *g);
private	int	mkChangeSet(opts *op);

/* bk fast-import (aliased as _fastimport) */
int
fastimport_main(int ac, char **av)
{
	int	c;
	opts	opts = {0};
	longopt	lopts[] = {
		{ 0, 0 }
	};
	int	rc = 1;

	while ((c = getopt(ac, av, "", lopts)) != -1) {
		switch (c) {
		    default: bk_badArg(c, av);
		}
	}
	if (av[optind]) usage();
	/*
	 * XXX: if we _only_ provided incremental it would have more
	 * symmetrical use cases:
	 *
	 *   $ bk init bk-repo
	 *   $ (cd git-repo ; git fast-export) | (cd bk-repo; bk fast-import)
	 *
	 */
	if (isdir(".bk") || isdir("BitKeeper")) {
		fprintf(stderr, "%s: Incremental imports not done yet\n",
			av[0]);
		rc = 1;
		goto out;
	} else {
		setup(&opts);
	}
	putenv("_BK_NO_UNIQ=1");
	putenv("_BK_MV_OK=1");
	cmdlog_lock(CMD_WRLOCK);
	rc = gitImport(&opts);
out:	return (rc);
}

#define	MATCH(s1)	(strneq(s1, line, strlen(s1)))

private int
gitImport(opts *op)
{
	char	*line;
	int	i, j;
	int	blobCount, commitCount;
	int	rc;
	char	**list = 0;
	commit	*cmt;
	struct {
		char	*s;			/* cmd */
		char	*(*fn)(opts *, char *);	/* handler */
	} cmds[] = {
		{"blob", getBlob},
		{"commit", getCommit},
		{"reset", reset},
		{0, 0}
	};

	/*
	 * Setup
	 */
	op->commits = hash_new(HASH_MEMHASH);
	op->blobs = hash_new(HASH_MEMHASH);
	op->paths = hash_new(HASH_MEMHASH);

	/*
	 * Processing:
	 *
	 * All functions may 'peek' at the next line by reading it and
	 * returning it. If a function returns NULL, we'll read a line
	 * from stdin. I.e. if a function consumes a line it has to
	 * either read another line or return NULL.
	 */
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
			fprintf(stderr, "Unknown command: %s\n", line);
			rc = 1;
			break;
		}
	}
	// XXX should time sort clist constrained by parents here

	// set time fudge
	EACH(op->clist) {
		commit	*p;
		long	tdiff;

		cmt = op->clist[i];
		EACH_INDEX(cmt->parents, j) {
			p = cmt->parents[j];
			if ((tdiff = ((long)p->when - (long)cmt->when)) >= 0) {
				/* parent is younger than me */

				++tdiff;
				cmt->when += tdiff;
				cmt->fudge += tdiff;
			}
		}
	}

	/* get sorted unique list of pathnames */
	EACH_HASH(op->paths) list = addLine(list, (char *)op->paths->kptr);
	sortLines(list, 0);

	op->cset = sccs_init(CHANGESET, 0);
	EACH(list) {
		printf("import %s\n", list[i]);
		importFile(op, list[i]);
	}
	freeLines(list, 0);

	mkChangeSet(op);
	sccs_newchksum(op->cset); /* write new heap */
	sccs_free(op->cset);

	printf("Rename sfiles...\n");
	fflush(stdout);
	rc = system("bk -r names");
	assert(!rc);

	/*
	 * Teardown
	 */
	free(op->clist);
	commitCount = 0;
	EACH_HASH(op->commits) {
		commit	*m = op->commits->vptr;
		freeCommit(m);
		commitCount++;
	}
	hash_free(op->commits);
	blobCount = 0;
	EACH_HASH(op->blobs) {
		blob	*m = op->blobs->vptr;

		assert(m->refcnt == 0);
		freeBlob(m);
		blobCount++;
	}
	hash_free(op->blobs);
	hash_free(op->paths);
	printf("%d marks in hash (%d blobs, %d commits)\n",
	    blobCount + commitCount, blobCount, commitCount);
	return (0);
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
		fprintf(stderr, "ERROR: Duplicate key: %s\n", key);
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

	assert(MATCH("commit "));
	/* XXX: Need to get ref from line before reading another one. */
	line = fgetline(stdin);
	unless (MATCH("mark :")) {
		fprintf(stderr, "unmarked commits not supported\n");
		exit(1);
	}
	key = line + strlen("mark :");
	unless (cmt = hash_insertStrMem(op->commits, key, 0, sizeof(commit))) {
		fprintf(stderr, "ERROR: Duplicate mark: %s\n", key);
		exit(1);
	}
	cmt->mark = op->commits->kptr;
	addArray(&op->clist, &cmt);
	cmt->ser = nLines(op->clist);
	line = fgetline(stdin);

	if (MATCH("author")) {
		/* Ignore author? */
		cmt->author = parseWho(line, 0, 0);
		line = fgetline(stdin);
	}

	unless (MATCH("committer")) {
		fprintf(stderr, "expected 'committer' line\n");
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
				fprintf(stderr, "parent '%s' not found\n",
				    p);
				exit(1);
			}
			addArray(&cmt->parents, &parent);
		} else {
			fprintf(stderr, "line '%s' not handled\n",
			    line);
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
			fprintf(stderr, "line '%s' not supported\n", line);
			exit(1);
		}
		addArray(&cmt->fops, &gop);
		line = fgetline(stdin);
	}
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
	    case 0:		/* empty line, break */
		free(g);
		return (0);
		break;
	    default:
		fprintf(stderr, "Error parsing %s\n", line);
		exit(1);
		break;
	}
	assert(line[1] == ' ');
	p = &line[2];
	if (g->op == GMODIFY) {
		/* parse mode */
		g->mode = parseMode(op, &p);
		switch(g->mode) {
		    case MODE_SUBDIR:
		    case MODE_GITLINK:
			assert(0);
			break;
		    default:
			break;
		}
	}
	if ((g->op == GMODIFY) || (g->op == GNOTE)) {
		/* parse dataref/inline */
		g->m = parseDataref(op, &p);
		g->m->refcnt++;
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
	if (op->m) op->m->refcnt--;
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
		fprintf(stderr, "ERROR: failed to find mark: %s\n", key);
		exit(1);
	}
	return (m);
}

private blob *
saveInline(opts *op)
{
	fprintf(stderr, "'inline' not implemented\n");
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
		fprintf(stderr, "Parse Failed: '%s'\n", *s);
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
		fprintf(stderr, "invalid mode found: %s\n", *s);
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
	w->email = ++p;
	p = strchr(p, '>');
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
		*tz = strdup(p);
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
	if (m->file) {
		unlink(m->file);
		free(m->file);
	}
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

/*
 */
private void
setup(opts *op)
{
	/* XXX seems like this should be tied to the oldest cset. */

	if (systemf("bk '-?BK_DATE_TIME_ZONE=1970-01-01 01:00:00-0' init")) {
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
	char	*sha1, *file = 0;
	int	i, idx, binary;
	int	closeit = 0;
	hash_state	md;
	u8	buf[1<<12];

	assert(MATCH("data "));

	/*
	 * Get a temp file to put the data.
	 */
	unless (f) {
		file = bktmp_local(0);
		f = fopen(file, "w");
		assert(f);
		closeit = 1;
	}

	/*
	 * Initialize vars and get length
	 */
	len = strtoul(line + strlen("data "), 0, 10);
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
	if (closeit) fclose(f);

	/* Now insert the mark */
	m = new(blob);
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
	m->file = file;		/* free'd by freeBlob() */
	m->binary = binary;
	return (m);
}

/* Everything above was parsing (analysis). This next part is synthesis. */


private int
importFile(opts *op, char *file)
{
	int	ncsets = hash_count(op->commits);
	sdelta	lastmod[ncsets+1];
	int	i, j;
	int	nparents;
	commit	*cmt = 0, **p;
	sdelta	dp[2], td;
	gop	*g;

	EACH_INDEX(op->clist, j) {
		cmt = op->clist[j];

		/* did this cset change 'file' ? */
		g = 0;
		EACH(cmt->fops) {
			if (cmt->fops[i]->path1 == file) {
				g = cmt->fops[i];
				break;
			}
		}

		/* how many unique parents? */
		nparents = 0;
		EACHP(cmt->parents, p) {
			td = lastmod[(*p)->ser];
			unless (td.s) continue;
			for (i = 0; i < nparents; i++) {
				if ((dp[i].s == td.s) && (dp[i].d == td.d)) {
					break;
				}
			}
			assert(i < 2); /* octopus on file */
			dp[i] = td;
			if (i == nparents) nparents++;
		}
		/* skip cases where this file doesn't matter */
		if (!g && (nparents < 2)) {
			if (nparents == 1) {
				lastmod[cmt->ser] = dp[0];
			} else {
				memset(&lastmod[cmt->ser], 0, sizeof(sdelta));
			}
			continue;
		}

		if (nparents == 0) {
			// new file
			td = newFile(op, file, cmt, g);
		} else if (nparents == 1) {
			// new delta
			td = newDelta(op, dp[0], cmt, g);
		} else {
			assert(nparents == 2);

			// merge in file with new contents (g might be 0)
			td = newMerge(op, dp, cmt, g);
		}
		lastmod[cmt->ser] = td;
	}
	if (cmt && lastmod[cmt->ser].s) sccs_free(lastmod[cmt->ser].s);
	return (0);
}

private void
loadMetaData(sccs *s, ser_t d, commit *cmt)
{
	ser_t	prev;

	USERHOST_SET(s, d, cmt->committer->email);
	DATE_SET(s, d, cmt->when);
	DATE_FUDGE_SET(s, d, cmt->fudge);
	ZONE_SET(s, d, cmt->tz); //XXX git -0400 bk -04:00 (but both work)

	// fudge
	if ((prev = sccs_prev(s, d)) &&
	    (DATE(s, d) <= DATE(s, prev)))  {
		time_t	tdiff;
		tdiff = DATE(s, prev) - DATE(s, d) + 1;
		DATE_SET(s, d, (DATE(s, d) + tdiff));
		DATE_FUDGE_SET(s, d, (DATE_FUDGE(s, d) + tdiff));
	}
}

private sdelta
newFile(opts *op, char *file, commit *cmt, gop *g)
{
	sccs	*s;
	sdelta	ret = {0};
	char	*p;
	ser_t	d, d0;
	int	rc;
	char	buf[MAXLINE];

	assert(g->op == GMODIFY);
	ret.m = g->m;

	sprintf(buf, "%d", ++op->filen);
	ret.s = s = sccs_init(buf, 0);
	assert(s);

	switch(g->mode) {
	    case MODE_SYMLINK:
		p = loadfile(g->m->file, 0);
		rc = symlink(p, s->gfile);
		assert(!rc);
		free(p);
		break;
	    case MODE_FILE:
	    case MODE_EXE:
		fileLink(g->m->file, s->gfile);
		if (g->mode == MODE_EXE) chmod(s->gfile, 0700);
		break;
	    default:
		assert(0);
		break;
	}
	check_gfile(s, 0);

	d0 = sccs_newdelta(s);
	loadMetaData(s, d0, cmt);
	sccs_parseArg(s, d0, 'P', file, 0);
	sccs_parseArg(s, d0, 'R', "1.0", 0);
	// XXX want randomCons(buf+2, s, d0);
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

	ret.d = d = sccs_newdelta(s);
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

private sdelta
newDelta(opts *op, sdelta td, commit *cmt, gop *g)
{
	sdelta	ret = td;
	sccs	*s = td.s;
	ser_t	p = td.d;
	ser_t	d;
	char	*t, *rel;
	int	rc;
	FILE	*diffs;
	char	buf[MAXLINE];

	if ((g->op == GMODIFY) && (BAM(s) || g->m->binary)) {
		assert(g->mode == MODE_EXE || g->mode == MODE_FILE);
		unless (BAM(s)) {
			gop	gtmp;

			strcpy(buf, PATHNAME(td.s, td.d));

			gtmp.op = GDELETE;
			(void)newDelta(op, td, cmt, &gtmp);
			sccs_free(td.s); // XXX parallel deletes

			return (newFile(op, buf, cmt, g));
		}
		rc = sccs_get(s, REV(s, p), 0, 0, 0,
		    SILENT|GET_SKIPGET|GET_EDIT, 0, 0);
		assert(!rc);
		d = sccs_newdelta(s);
		loadMetaData(s, d, cmt);
		PATHNAME_INDEX(s, d) = PATHNAME_INDEX(s, 1);
		SORTPATH_INDEX(s, d) = SORTPATH_INDEX(s, 1);
		fileLink(g->m->file, s->gfile);
		if (g->mode == MODE_EXE) chmod(s->gfile, 0755);
		check_gfile(s, 0);
		ret.m = g->m;

		rc = sccs_delta(s, SILENT|DELTA_DONTASK|DELTA_CSETMARK,
		    d, 0, 0, 0);
		assert(!rc);
	} else if ((g->op == GMODIFY) && (g->mode == MODE_SYMLINK)) {
		rc = sccs_get(s, REV(s, p), 0, 0, 0,
		    SILENT|GET_EDIT, s->gfile, 0);
		assert(!rc);

		d = sccs_newdelta(s);
		loadMetaData(s, d, cmt);
		PATHNAME_INDEX(s, d) = PATHNAME_INDEX(s, 1);
		SORTPATH_INDEX(s, d) = SORTPATH_INDEX(s, 1);

		assert(td.m);
		t = loadfile(g->m->file, 0);
		unlink(s->gfile);
		rc = symlink(t, s->gfile);
		assert(!rc);
		free(t);

		rc = sccs_delta(s, SILENT|DELTA_DONTASK|DELTA_CSETMARK,
		    d, 0, 0, 0);
		assert(!rc);
		ret.m = g->m;
	} else if (g->op == GMODIFY) {
		if (td.m) {
			rc = sccs_get(s, REV(s, p), 0, 0, 0,
			    SILENT|GET_SKIPGET|GET_EDIT, 0, 0);
			assert(!rc);

			sprintf(buf, "bk ndiff -n '%s' '%s'",
			    td.m->file,
			    g->m->file);
			diffs = popen(buf, "r");
			assert(diffs);
		} else {
			rc = sccs_get(s, REV(s, p), 0, 0, 0,
			    SILENT|GET_EDIT, s->gfile, 0);
			assert(!rc);
			diffs = 0;
		}
		d = sccs_newdelta(s);
		loadMetaData(s, d, cmt);
		PATHNAME_INDEX(s, d) = PATHNAME_INDEX(s, 1);
		SORTPATH_INDEX(s, d) = SORTPATH_INDEX(s, 1);

		rc = sccs_delta(s,
		    SILENT|DELTA_DONTASK|DELTA_CSETMARK|DELTA_FORCE,
		    d, 0, diffs, 0);
		assert(!rc);
		if (diffs) pclose(diffs);
		ret.m = g->m;
	} else {
		assert(g->op = GDELETE);

		rc = sccs_get(s, REV(s, p), 0, 0, 0,
		    SILENT|GET_SKIPGET|GET_EDIT, 0, 0);
		assert(!rc);

		d = sccs_newdelta(s);
		loadMetaData(s, d, cmt);
		t = sccs_rmName(s);
		rel = proj_relpath(s->proj, t);
		free(t);
		PATHNAME_SET(s, d, rel);
		free(rel);
		t = aprintf("Delete: %s\n", PATHNAME(s, p));
		COMMENTS_SET(s, d, t);
		free(t);
		if (BAM(s)) {
			// XXX gfile doesn't matter here, we are deleting
			// but better is probably to copy my parent's file
			touch(s->gfile, 0666);
		}
		diffs = fmem();
		rc = sccs_delta(s,
		    SILENT|DELTA_DONTASK|DELTA_CSETMARK|DELTA_FORCE,
		    d, 0, diffs, 0);
		assert(!rc);
		fclose(diffs);

		ret.m = 0;
	}
	s->state &= ~S_PFILE;

	/* record the cset weave */
	assert(ret.rk);
	addArrayV(&cmt->weave, ret.rk);
	sccs_sdelta(s, d, buf);
	addArrayV(&cmt->weave, sccs_addStr(op->cset, buf));

	ret.d = d;
	return (ret);
}

private sdelta
newMerge(opts *op, sdelta m[2], commit *cmt, gop *g)
{
	sdelta	ret;
	sccs	*s;
	char	*t, *rel;
	ser_t	d;
	int	i, rc;
	char	buf[MAXPATH];

	if (m[0].s != m[1].s) {
		gop	gtmp;

		/* delete newer file */
		i =  (DATE(m[0].s, 1) < DATE(m[1].s, 1));

		gtmp.op = GDELETE;
		newDelta(op, m[i], cmt, &gtmp);
		sccs_free(m[i].s); // XXX parallel deletes

		if (g) {
			ret = newDelta(op, m[!i], cmt, g);
		} else {
			ret = m[!i];	/* older sfile */
			ret.m = 0;
		}
		return (ret);
	}
	ret = m[0];
	s = ret.s;
	ret.m = 0;

	if (!g || (g->op == GMODIFY)) {
		ser_t	dp = m[1].d;
		ser_t	dm = m[0].d;

		if (sccs_needSwap(s, dp, dm, 0)) {
			dp = m[0].d;
			dm = m[1].d;
		}
		rc = sccs_get(s, REV(s, dp), REV(s, dm), 0, 0,
		    SILENT|GET_EDIT, s->gfile, 0);
		assert(!rc);

		d = sccs_newdelta(s);
		loadMetaData(s, d, cmt);

		if (g) {
			/*
			 * if we have data, then it must be the original
			 * pathname.
			 */
			PATHNAME_INDEX(s, d) = PATHNAME_INDEX(s, 1);
			SORTPATH_INDEX(s, d) = SORTPATH_INDEX(s, 1);

			fileLink(g->m->file, s->gfile);
			if (g->mode == MODE_EXE) chmod(s->gfile, 0755);
			check_gfile(s, 0);
			ret.m = g->m;
		} else {
			/* merge with g==0, must be merging deletes */
			PATHNAME_INDEX(s, d) = PATHNAME_INDEX(s, m[0].d);
			SORTPATH_INDEX(s, d) = SORTPATH_INDEX(s, m[0].d);
		}
		rc = sccs_delta(s, SILENT|DELTA_DONTASK|DELTA_CSETMARK,
		    d, 0, 0, 0);
		assert(!rc);
		s->state &= ~S_PFILE;
	} else if (g->op == GDELETE) {
		ser_t	dp = m[1].d;
		ser_t	dm = m[0].d;

		if (sccs_needSwap(s, dp, dm, 0)) {
			dp = m[0].d;
			dm = m[1].d;
		}
		rc = sccs_get(s, REV(s, dp), REV(s, dm), 0, 0,
		    SILENT|GET_EDIT, s->gfile, 0);
		assert(!rc);
		d = sccs_newdelta(s);
		loadMetaData(s, d, cmt);
		t = sccs_rmName(s);
		rel = proj_relpath(s->proj, t);
		free(t);
		PATHNAME_SET(s, d, rel);
		free(rel);
		rc = sccs_delta(s,
		    SILENT|DELTA_DONTASK|DELTA_CSETMARK|DELTA_FORCE,
		    d, 0, 0, 0);
		assert(!rc);
		s->state &= ~S_PFILE;

		ret.m = 0;
	} else {
		assert(0);
	}
	ret.d = d;

	/* record the cset weave */
	assert(ret.rk);
	addArrayV(&cmt->weave, ret.rk);
	sccs_sdelta(s, d, buf);
	addArrayV(&cmt->weave, sccs_addStr(op->cset, buf));
	return (ret);
}

private int
mkChangeSet(opts *op)
{
	sccs	*s = op->cset;
	ser_t	d, p, m;
	int	i;
	int	nparents;
	commit	*cmt = 0;

	printf("Writing new ChangeSet file...\n");
	fflush(stdout);
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
			if (sccs_needSwap(s, p, m, 0)) {
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
	printf("Renumber...\n");
	fflush(stdout);
	sccs_renumber(s, 0);

	printf("Generate checksums...\n");
	fflush(stdout);
	cset_resum(s, 0, 1, 0, 0);

	return (0);
}
