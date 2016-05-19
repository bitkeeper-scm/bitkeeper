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
	GCOPY,			/* copy file (not supproted) */
	GDELETE,		/* delete path */
	GDELETE_ALL,		/* delete all files (not supported) */
	GMODIFY,		/* new contents */
	GNOTE,			/* nodemodify: ?? (not supported) */
	GRENAME,		/* rename file (not supported) */
};

typedef struct {
	enum op	op;		/* file operation */
	char	*path1;		/* path */
	char	*path2;		/* second path if rename or copy */
	blob	*m;		/* 'mark' struct where contents are */
	char	*mode;		/* mode of file */
} gop;				/* git operation */

typedef struct commit commit;
struct commit {
	char	*mark;
	time_t	when;		/* commit time */
	char	*tz;		/* timezone  */
	who	*author;	/* git's author */
	who	*committer;	/* git's committer */
	char	*comments;	/* commit's comments */
	commit	**parents;	/* list of parents */
	gop	**fops;		/* file operations for this commit  */
};

typedef struct {
	/* state */
	hash	*blobs;		/* mark -> blob struct */
	hash	*commits;	/* mark -> commit struct */
	commit	**clist;	/* array of commit*'s, oldest first */
} opts;

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
private	char	*parseMode(opts *op, char **s);
private char	*parsePath(opts *op, char **s);

private	blob	*saveInline(opts *op);


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
	if (isdir(".bk") || isdir("BitKeeper")) {
		fprintf(stderr, "%s: Incremental imports not done yet\n",
			av[0]);
		rc = 1;
		goto out;
	} else {
		setup(&opts);
	}
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
	int	rc = 1;
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

	printf("commits\n");
	EACH(op->clist) {
		commit	*c = op->clist[i];
		printf(":%s -", c->mark);
		EACH_INDEX(c->parents, j) {
			printf(" :%s", c->parents[j]->mark);
		}
		printf("\n%s", c->comments);
		EACH_INDEX(c->fops, j) {
			if (c->fops[j]->path1) {
				printf("\t%s\n", c->fops[j]->path1);
			}
		}
	}

	/*
	 * Teardown
	 */
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
	printf("%d marks in hash (%d blobs, %d commits)\n",
	    blobCount + commitCount, blobCount, commitCount);
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
		    case GDELETE: case GMODIFY:
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
	free(op->path1);
	free(op->path2);
	free(op->mode);
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
 * consumed portion. Returns an allocated string with the parsed part.
 */
private	char *
parseMode(opts *op, char **s)
{
	char	*p, *ret;
	int	i;
	int	valid = 0;
	char	*validModes[] = {
		"100644", "644",    /* normal file */
		"100755", "755",    /* executable */
		"120000",	    /* symlink */
		"160000",	    /* gitlink (submodule) */
		"040000",	    /* subdirectory */
		0
	};

	assert(s);
	p = strchr(*s, ' ');
	unless (p) {
		fprintf(stderr, "Parse Failed: '%s'\n", *s);
		exit(1);
	}
	*p = 0;
	valid = 0;
	for (i = 0; validModes[i]; i++) {
		if (streq(*s, validModes[i])) {
			valid = 1;
			break;
		}
	}
	unless (valid) {
		fprintf(stderr, "invalid mode found: %s\n", *s);
		exit(1);
	}
	ret = strdup(*s);
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
	char	*ret;

	assert(s);
	start = *s;
	if (start[0] != '"') {
		/*
		 * Simple case, nothing funny
		 */
		if (end = strchr(start, ' ')) {
			/* space delimiter */
			*end = 0;
			ret = strdup(start);
			*end = ' ';
			*s = end + 1;
			return (ret);
		}
		/* rest of string is file name */
		ret = strdup(start);
		*s = start + strlen(start);
		return (ret);
	}
	/*
	 * Complicated case, do the parsing
	 */
	p = ret = malloc(strlen(start) * 4); // 2 should suffice but whatever
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
	EACH(m->fops) freeOp(m->fops[i]);
	free(m->fops);
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
	putenv("BK_DATE_TIME_ZONE=1970-01-01 01:00:00-0");
	putenv("BK_NO_TRIGGERS=1");
	/* safe_putenv("BK_USER=%s", op->user); */
	/* safe_putenv("BK_HOST=%s", op->host); */
	if (systemf("bk init")) {
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
	size_t	len, l, l2, blocks, last;
	char	*p, *sha1, *file = 0;
	int	i, idx, binary;
	int	closeit = 0;
	hash_state	md;
	unsigned char	buf[1<<12];

	assert(MATCH("data "));

	/*
	 * Get a temp file to put the data.
	 */
	unless (f) {
		file = bktmp(0);
		f = fopen(file, "w");
		assert(f);
		closeit = 1;
	}

	/*
	 * Initialize vars and get length
	 */
	len = strtoul(line + strlen("data "), &p, 10);
	blocks = len / (1<<12);
	last = len % (1<<12);
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
	while (blocks-- && ((l = fread(buf, 1, 1<<12, stdin)) > 0)) {
		unless (binary) {
			for (i = 0; i < l; i++) {
				unless (buf[i]) (binary) = 1;
			}
		}
		l2 = fwrite(buf, 1, l, f);
		assert(l == l2);
		if (want_sha1) hash_descriptor[idx].process(&md, buf, l2);
	}
	if (blocks != -1) {
		perror("reading blob");
		exit(1);
	}
	if ((l = fread(buf, 1, last, stdin)) != last) {
		perror("fread on blob");
		exit(1);
	}
	assert(l == last);
	l2 = fwrite(buf, 1, l, f);
	assert(l == l2);
	if (closeit) fclose(f);

	/* Now insert the mark */
	m = new(blob);
	if (want_sha1) {
		hash_descriptor[idx].process(&md, buf, l2);
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
