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
	/* state */
	hash	*marks;		/* Marks hash (also sha1's) */
} opts;

typedef struct {
	/* git's fields */
	char	*name;		/* "Oscar Bonilla" */
	char	*email;		/* "ob@bitkeeper.com" */
	time_t	when;		/* secs since epoch */
	char	*tz;		/* timezone  */
	/* bk's fields */
	char	*user;		/* ob */
	char	*host;		/* dirac.bitkeeper.com */
	char	*date;		/* BK's weird _almost_ ISO date */
} who;

/* Types of marks */
enum {
	BLOB,
	COMMIT,
};

typedef struct mark {
	char	*sha1;		/* sha1 of the data (or commit) */
	u32	type;		/* type of mark (see above) */
	struct {
		char	*file;		/* file where data is */
		int	binary;		/* did we see a NULL in the data? */
	} blob;
	struct {
		char	*deltakey;	/* deltakey of this commit */
		who	*author;	/* git's author */
		who	*committer;	/* git's committer */
	} commit;
} mark;

/* types of git operations */
enum {
	GCOPY,
	GDELETE,
	GDELETE_ALL,
	GMODIFY,
	GNOTE,
	GRENAME,
};

typedef struct {
	int	op;		/* file operation */
	char	*path1;		/* path */
	char	*path2;		/* second path if rename or copy */
	mark	*m;		/* 'mark' struct where contents are */
	char	*mode;		/* mode of file */
} gop;				/* git operation */


private int	gitImport(opts *op);
private void	setup(opts *op);
private char	*blob(opts *op, char *line);
private	char	*reset(opts *op, char *line);
private	char	*commit(opts *op, char *line);
private	mark	*data(opts *op, char *line, FILE *f);

private	who	*parseWho(char *line);
private	void	freeWho(who *w);
private void	freeMark(mark *m);

private gop	*parseOp(opts *op, char *line);
private void	freeOp(gop *op);

private mark	*parseDataref(opts *op, char **s);
private	char	*parseMode(opts *op, char **s);
private char	*parsePath(opts *op, char **s);

private	mark	*saveInline(opts *op);


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
	int	i;
	int	blobCount, commitCount;
	int	rc = 1;
	struct {
		char	*s;			/* cmd */
		char	*(*fn)(opts *, char *);	/* handler */
	} cmds[] = {
		{"blob", blob},
		{"commit", commit},
		{"reset", reset},
		{0, 0}
	};

	/*
	 * Setup
	 */
	op->marks = hash_new(HASH_MEMHASH);

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

	/*
	 * Teardown
	 */
	blobCount = commitCount = 0;
	EACH_HASH(op->marks) {
		mark	*m = op->marks->vptr;
		freeMark(m);
		if (m->type == BLOB) {
			blobCount++;
		} else {
			commitCount++;
		}
	}
	fprintf(stderr, "%d marks in hash (%d blobs, %d commits)\n",
	    blobCount + commitCount, blobCount, commitCount);
	hash_free(op->marks);
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
blob(opts *op, char *line)
{
	mark	*m;
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
	m = data(op, line, 0);
	key = mark ? mark : m->sha1;
	/* save the mark */
	unless (hash_insertStrMem(op->marks, key, m, sizeof(*m))) {
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
commit(opts *op, char *line)
{
	mark	*mrk, *cmt;
	char	*key;
	int	root;
	char	*comments;
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
	unless (cmt = hash_insertStrMem(op->marks, key, 0, sizeof(mark))) {
		fprintf(stderr, "ERROR: Duplicate mark: %s\n", key);
		exit(1);
	}
	line = fgetline(stdin);

	if (MATCH("author")) {
		/* Ignore author? */
		cmt->commit.author = parseWho(line);
		line = fgetline(stdin);
	}

	unless (MATCH("committer")) {
		fprintf(stderr, "expected 'committer' line\n");
		exit(1);
	}

	cmt->commit.committer = parseWho(line);
	line = fgetline(stdin);

	/*
	 * Get comments from data command
	 */
	f = fmem();
	mrk = data(op, line, f);
	freeMark(mrk);
	free(mrk);
	comments = fmem_close(f, 0);

	line = fgetline(stdin);

	root = 1;
	if (MATCH("from ")) {
		/* parent */
		root = 0;
		line = fgetline(stdin);
	}

	while (MATCH("merge")) {
		/* merge */
		line = fgetline(stdin);
	}

	while (gop = parseOp(op, line)) {
		switch (gop->op) {
		    case GNOTE: break;
		    case 'M':
			break;
		    default:
			/* fprintf(stderr, "unknown op: %d\n", gop->op); */
			/* exit(1); */
			break;
		}
		freeOp(gop);
		line = fgetline(stdin);
	}

	/* XXX: do commit */

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
private mark *
parseDataref(opts *op, char **s)
{
	char	*key;
	mark	*m;

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
	unless (m = hash_fetchStrMem(op->marks, key)) {
		fprintf(stderr, "ERROR: failed to find mark: %s\n", key);
		exit(1);
	}
	return (m);
}

private	mark *
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
parseWho(char *line)
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
	++p;			/* skip space */
	w->when = strtol(p, &q, 10);
	assert(w->when);
	assert(*q == ' ');
	p = ++q;
	w->tz = strdup(p);
	return (w);
}

private	void
freeWho(who *w)
{
	unless (w) return;
	free(w->name);
	free(w->email);
	free(w->tz);
	free(w);
}

private void
freeMark(mark *m)
{
	unless (m) return;
	free(m->sha1);
	if (m->type == BLOB) {
		free(m->blob.file);
	}
	if (m->type == COMMIT) {
		freeWho(m->commit.author);
		freeWho(m->commit.committer);
		free(m->commit.deltakey);
	}
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
 * This function is a bit of cargo cult science. I'm not sure we need
 * all these tweaks to a standard setup.
 */
private void
setup(opts *op)
{
	FILE	*f;

	putenv("BK_DATE_TIME_ZONE=1970-01-01 01:00:00-0");
	putenv("BK_NO_TRIGGERS=1");
	/* safe_putenv("BK_USER=%s", op->user); */
	/* safe_putenv("BK_HOST=%s", op->host); */
	f = fopen("/tmp/bk_config", "w");
	fprintf(f, "checkout:edit\n");
	fclose(f);
	if (systemf("bk setup -fc/tmp/bk_config")) {
		perror("bk setup");
		exit(1);
	}
	unlink("/tmp/bk_config");
	system("bk root >/dev/null");
	system("bk sane");
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
private mark *
data(opts *op, char *line, FILE *f)
{
	mark	*m;
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
	idx = register_hash(&sha1_desc);
	hash_descriptor[idx].init(&md);
	sprintf(buf, "blob %ld", len);
	/* we _want_ the trailing NULL */
	hash_descriptor[idx].process(&md, buf, strlen(buf)+1);

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
		hash_descriptor[idx].process(&md, buf, l2);
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

	/* Now insert the mark */
	m = new(mark);
	m->type = BLOB;
	m->sha1 = sha1;		/* free'd by freeMark() */
	m->blob.file = file;	/* free'd by freeMark() */
	m->blob.binary = binary;
	return (m);
}

/* Everything above was parsing (analysis). This next part is synthesis. */
