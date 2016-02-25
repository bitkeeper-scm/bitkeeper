/*
 * Copyright 2001-2006,2008-2016 BitMover, Inc
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

typedef struct conflct	conflct;
typedef struct ld	ld_t;
typedef struct diffln	diffln;
typedef struct file	file_t;

private char	*find_gca(sccs *s, char *left, char *right);
private int	do_weave_merge(u32 start, u32 end);
private conflct	*find_conflicts(void);
private int	resolve_conflict(conflct *curr);
private diffln	*unidiff(conflct *curr, int left, int right);
private diffln	*unidiff_ndiff(conflct *curr, int left, int right);
private int	sameline(ld_t *left, ld_t *right);
private	int	file_init(sccs *s, int side, char *anno, file_t *f);
private	void	file_free(file_t *f);
private	int	do_diff_merge(void);

private void	highlight_diff(diffln *diff);

private	void	user_conflict(conflct *curr);
private	void	user_conflict_fdiff(conflct *curr);
private	void	conflct_free(conflct *c);

/* automerge functions */
private	void	enable_mergefcns(char *list, int enable);
private	void	mergefcns_help(void);

private int	merge_same_changes(conflct *r);
private int	merge_only_one(conflct *r);
private int	merge_content(conflct *r);
private int	merge_common_header(conflct *r);
private int	merge_common_footer(conflct *r);
private	int	merge_common_deletes(conflct *r);
private int	merge_added_oneside(conflct *r);
private	int	merge_with_following_gca(conflct *r);
private	int	merge_conflicts(conflct *head);

private	char	*smergeData(int idx, int side, void *extra, int *len);

enum {
	MODE_GCA,
	MODE_2WAY,
	MODE_3WAY,
	MODE_NEWONLY
};

enum {
	LEFT,			/* Match DF_LEFT */
	RIGHT,			/* Match DF_RIGHT */
	GCA,			/* GCA with whodel relative to LEFT rev */
	GCAR,			/* GCA with whodel relative to RIGHT rev */
	NFILES
};

/* ld = line data  (Information to save about each line in the file) */
struct ld {
	u32	seq;		/* seq number for line */
	char	*line;		/* Pointer to data in line */
	int	len;		/* length of line */
	char	*anno;		/* Pointer to annotations */
};

struct diffln {
	ld_t	*ld;
	int	*highlight;
	char	c;		/* character at start of line */
};

struct file {
	FILE	*f;		/* fmem of annotated output */
	ld_t	*lines;
	int	n;
};


private	char	*revs[NFILES];
private	file_t	body[NFILES];
private	char	*file;
private	int	mode;
private	int	fdiff;
private	int	show_mergefn;
private	char	*anno = 0;
private	int	parse_range(char *range, u32 *start, u32 *end);
private	int	show_seq;

int
smerge_main(int ac, char **av)
{
	int	c;
	int	i;
	u32	start = 0, end = ~0;
	int	ret = 2;
	int	do_diff3 = 0;
	int	identical = 0;
	char	*p, *sfile;
	sccs	*s;
	longopt	lopts[] = {
		{ "show-merge-fn", 310 },
		{ 0, 0 }
	};

	/* A "just in case" hook to disable one or more of the merge
	 * heuristics.  We can add a BK_MERGE_ENABLE in the future if
	 * needed.
	 */
	if (p = getenv("BK_MERGE_DISABLE")) {
		enable_mergefcns(p, 0);
	}
	if (getenv("BK_MERGE_DIFF3")) do_diff3 = 1;

	mode = MODE_3WAY;
	while ((c = getopt(ac, av, "234A;a;defghI;l;npr;R;s", lopts)) != -1) {
		switch (c) {
		    case '2': /* 2 way format (like diff3) */
			mode = MODE_2WAY;
			break;
		    case '3': /* 3 way format (default) */
			mode = MODE_3WAY;
			break;
		    case 'g': /* gca format */
			mode = MODE_GCA;
			break;
		    case 'n': /* newonly (like -2 except marks added lines) */
			mode = MODE_NEWONLY;
			break;
		    case 'I': /* Add annotations (Info) */
		    	anno = optarg;
			break;
		    case 'A':
		    case 'a':
			enable_mergefcns(optarg, c == 'a');
			break;
		    case 'd':	do_diff3 = 1; break;
		    case 'f': /* fdiff output mode */
			fdiff = 1;
			break;
		    case 'e': /* show examples */
			getMsg("smerge_examples", 0, 0, stdout);
			return(2);
		    case 'l':
			revs[LEFT] = strdup(optarg);
			break;
		    case 'r':
			revs[RIGHT] = strdup(optarg);
			break;
		    case 'R': /* show output in the range <r> */
			if (parse_range(optarg, &start, &end)) {
				usage();
				return (2);
			}
			break;
		    case 's': /* show sequence numbers */
			show_seq = 1;
			break;
		    case 'h': /* help */
		    default: goto help;
		    case 310: /* --show-merge-fn */
			show_mergefn = 1;
			break;
		}
	}
	unless (av[optind] && !av[optind+1] && revs[LEFT] && revs[RIGHT]) {
help:		system("bk help -s smerge");
		mergefcns_help();
		return (2);
	}
	if (fdiff && mode == MODE_3WAY) mode = MODE_GCA;
	file = av[optind];
	sfile = name2sccs(file);
	unless (s = sccs_init(sfile, INIT_MUSTEXIST)) goto err;
	free(sfile);
	revs[GCA] = revs[GCAR] = find_gca(s, revs[LEFT], revs[RIGHT]);
	unless(revs[GCA]) goto err;

	/*
	 * Disable the merge_content heuristic if the merge GCA is a
	 * set node and not a single rev.  We have had cases of
	 * merging two merges that resolve a conflict differently just
	 * deleting everything as a result of this code.  See testcase
	 * in same cset.
	 */
	if (strchr(revs[GCA], '+') || strchr(revs[GCA], '-')) {
		enable_mergefcns("3", 0);
	}

	for (i = 0; i < NFILES; i++) {
		if (file_init(s, i, anno, &body[i])) {
			goto err;
		}
	}
	if (body[LEFT].n == body[RIGHT].n) {
		for (i = 0; i < body[LEFT].n; i++) {
			if (!sameline(&body[LEFT].lines[i],
				&body[RIGHT].lines[i])) break;
		}
		if (i == body[LEFT].n) {
			/* The left and right files are identical! */
			free(revs[RIGHT]);
			revs[RIGHT] = strdup(revs[LEFT]);
			file_free(&body[RIGHT]);
			body[RIGHT] = body[LEFT];
			identical = 1;
		}
	}

	if (do_diff3) {
		ret = do_diff_merge();
	} else {
		ret = do_weave_merge(start, end);
	}
 err:
	if (s) sccs_free(s);
	for (i = 0; i < NFILES; i++) {
		unless (i == RIGHT && identical) {
			file_free(&body[i]);
		}
		unless (i == GCAR) free(revs[i]);
	}
	return (ret);
}

/*
 * Open a file and populate the file_t structure.
 */
private	int
file_init(sccs *s, int side, char *anno, file_t *f)
{
	char	*p;
	char	*end;
	size_t	len;
	int	l;
	int	flags = GET_ALIGN|GET_SEQ|SILENT;
	char	*inc, *exc;
	char	*rev;

	if (anno) flags = annotate_args(flags, anno);

	unless (ASCII(s)) {
		fprintf(stderr, "%s: cannot merge binary file.\n", s->gfile);
		return (-1);
	}
	if (side == GCA) {
		s->whodel = sccs_findrev(s, revs[LEFT]);
	} else if (side == GCAR){
		s->whodel = sccs_findrev(s, revs[RIGHT]);
	} else {
		/*
		 * Enable a nop whodel so that the annotation alignment is the
		 * same.
		 */
		s->whodel = sccs_findrev(s, revs[side]);
	}
	rev = strdup(revs[side]);
	if (inc = strchr(rev, '+')) *inc++ = 0;
	if (exc = strchr(inc ? inc : rev, '-')) *exc++ = 0;

	f->f = fmem();
	if (sccs_get(s, rev, 0, inc, exc, flags, 0, f->f)) {
		fprintf(stderr, "Fetch of revision %s failed!\n", rev);
		free(rev);
		return (-1);
	}
	free(rev);
	rev = 0;

	f->n = s->lines;
	f->lines = calloc(f->n+1, sizeof(ld_t));

	l = 0;
	p = fmem_peek(f->f, &len);
	end = p + len;
	while (p < end) {
		char	*start;

		assert(l < f->n);
		/* Seq #: " *\d+ " */
		while (p < end && *p++ == ' ');
		--p;
		f->lines[l].seq = atoi_p(&p);
		++p;	/* skip space after \d+ */
		f->lines[l].anno = anno ? p : 0;
		while (p < end && *p++ != '|');
		++p;	/* skip space after | */
		f->lines[l].line = p;
		start = p;
		while (p < end && *p++ != '\n');
		assert(p > start); /* no empty lines */
		f->lines[l].len = p - start;
		++l;
	}
	f->lines[l].seq = ~0;
	f->lines[l].line = end;
	f->lines[l].anno = 0;
	f->lines[l].len = 0;
	assert(f->n == l);

	return (0);
}

private	void
file_free(file_t *f)
{
	if (f->lines) {
		free(f->lines);
		f->lines = 0;
	}
	if (f->f) {
		fclose(f->f);
		f->f = 0;
	}
}

private char *
find_gca(sccs *s, char *left, char *right)
{
	ser_t	dl, dr, dg;
	char	*inc = 0, *exc = 0;
	FILE	*revlist = 0;

	dl = sccs_findrev(s, left);
	unless (dl) {
		fprintf(stderr, "ERROR: couldn't find %s in %s\n",
		    left, s->gfile);
		return (0);
	}
	dr = sccs_findrev(s, right);
	unless (dr) {
		fprintf(stderr, "ERROR: couldn't find %s in %s\n",
		    right, s->gfile);
		return (0);
	}
	dg = sccs_gca(s, dl, dr, &inc, &exc);
	revlist = fmem();
	fputs(REV(s, dg), revlist);
	if (inc) {
		fprintf(revlist, "+%s", inc);
		free(inc);
	}
	if (exc) {
		fprintf(revlist, "-%s", exc);
		free(exc);
	}
	return (fmem_close(revlist, 0));
}

/*
 * print a line from a ld_t structure and optionally put the
 * first_char argument at the beginning.
 * Handles the -s (show_seq) knob.
 */
private void
printline(ld_t *ld, char first_char, int forcenl)
{
	char	*a = ld->anno;
	char	*p = ld->line;
	char	*end = ld->line + ld->len;

	if (fdiff && !first_char) first_char = ' ';
	if (first_char) putchar(first_char);
	if (show_seq) printf("%6d\t", ld->seq);

	/* Print annotation before line, if present */
	if (a) while (a < p) putchar(*a++);
	/* Print line */
	while (p < end) putchar(*p++);
	if (forcenl && (end[-1] != '\n')) putchar('\n');
}

private void
printhighlight(int *highlight)
{
	int	s, e;

	putchar('h');
	while (1) {
		s = highlight[0];
		e = highlight[1];
		if (s == 0 && e == 0) break;
		printf(" %d-%d", s, e);
		highlight += 2;
	}
	putchar('\n');
}

struct conflct {
	int	start[NFILES];	/* lines at start of conflict */
	int	end[NFILES];	/* lines after end of conflict */
	int	start_seq;	/* seq of lines before 'start' */
	int	end_seq;	/* seq of lines at 'end' */
	ld_t	*merged;	/* result of an automerge */
	u32	split:1;	/* created by split */
	conflct	*prev, *next;
	char	**algos;	/* merge algos that touched this block */
};

/*
 * Print a series of lines on one side of a conflict
 */
private void
printlines(conflct *curr, int side, int forcenl)
{
	int	len = curr->end[side] - curr->start[side];
	int	i;
	int	idx = curr->start[side];

	for (i = 0; i < len; i++) {
		printline(&body[side].lines[idx++], 0, forcenl);
	}
}

private int
do_weave_merge(u32 start, u32 end)
{
	conflct	*clist;
	conflct	*curr;
	int	mk[NFILES] = {0};
	int	i;
	int	len;
	int	ret = 0;

	/* create a linked list of all conflict regions */
	clist = find_conflicts();

	curr = clist;
	while (curr) {
		conflct	*tmp;

		/* print lines up to the next conflict */
		len = curr->start[GCA] - mk[GCA];

		/* should be the same number of lines */
		for (i = 0; i < NFILES; i++) {
			assert(len == curr->start[i] - mk[i]);
		}

		for (i = 0; i < len; i++) {
			ld_t	*gcaline = &body[GCA].lines[mk[GCA] + i];

			/* All common lines should match exactly */
			assert(gcaline->seq ==
			    body[LEFT].lines[mk[LEFT]+i].seq &&
			    gcaline->seq ==
			    body[RIGHT].lines[mk[RIGHT]+i].seq);

			if (gcaline->seq <= start) continue;
			if (gcaline->seq >= end) {
				while (curr) {
					tmp = curr;
					curr = curr->next;
					conflct_free(tmp);
				}
				return (ret);
			}
			printline(gcaline, 0, 0);
		}
		if (curr->start_seq >= start) {
			/* handle conflict */
			if (resolve_conflict(curr)) ret = 1;
		}
		for (i = 0; i < NFILES; i++) mk[i] = curr->end[i];

		tmp = curr;
		curr = curr->next;
		conflct_free(tmp);
	}
	/* Handle any lines after the last conflict */

	len = body[GCA].n - mk[GCA];
	for (i = 0; i < NFILES; i++) assert(len == body[i].n - mk[i]);

	for (i = 0; i < len; i++) {
		ld_t	*gcaline = &body[GCA].lines[mk[GCA] + i];

		assert(gcaline->seq ==
		    body[LEFT].lines[mk[LEFT]+i].seq &&
		    gcaline->seq ==
		    body[RIGHT].lines[mk[RIGHT]+i].seq);
		if (gcaline->seq <= start) continue;
		if (gcaline->seq >= end) return (ret);
		printline(gcaline, 0, 0);
	}
	return (ret);
}

/*
 * Find all conflict sections in the file based only on sequence numbers.
 * Return a linked list of conflict regions.
 */
private conflct *
find_conflicts(void)
{
	int	i, j;
	int	mk[NFILES] = {0};
	ld_t	*lines[NFILES];
	conflct	*list = 0;
	conflct	*end = 0;
	conflct	*p;

	while (1) {
		int	minlen;
		int	at_end;
		u32	seq[NFILES];

		/* Assume everything matches here */
		minlen = INT_MAX;
		for (i = 0; i < NFILES; i++) {
			lines[i] = &body[i].lines[mk[i]];
			if (minlen > body[i].n - mk[i]) {
				minlen = body[i].n - mk[i];
			}
		}
		j = 0;
		while (j < minlen &&
		    lines[LEFT][j].seq == lines[RIGHT][j].seq &&
		    lines[LEFT][j].seq == lines[GCA][j].seq &&
		    lines[LEFT][j].seq == lines[GCAR][j].seq) {
			j++;
		}
		/* at start of conflict or end of file */
		at_end = 1;
		for (i = 0; i < NFILES; i++) {
			mk[i] += j;
			if (mk[i] < body[i].n) at_end = 0;
		}
		if (at_end) break;

		p = new(conflct);

		/* link into end of chain */
		if (list) {
			end->next = p;
			p->prev = end;
		} else {
			list = p;
		}
		end = p;

		if (mk[0] > 0) {
			p->start_seq = body[0].lines[mk[0]-1].seq;
		} else {
			p->start_seq = 0;
		}
		for (i = 0; i < NFILES; i++) {
			p->start[i] = mk[i];
			lines[i] = body[i].lines;
		}

		/* find end */
		for (i = 0; i < NFILES; i++) {
			seq[i] = lines[i][mk[i]].seq;
		}
		do {
			u32	maxseq = 0;
			at_end = 1;
			for (i = 0; i < NFILES; i++) {
				if (seq[i] > maxseq) maxseq = seq[i];
			}
			for (i = 0; i < NFILES; i++) {
				while (seq[i] < maxseq) {
					++mk[i];
					seq[i] = lines[i][mk[i]].seq;
					at_end = 0;
				}
				if (seq[i] > maxseq) maxseq = seq[i];
			}
		} while (!at_end);

		assert(seq[0]);
		for (i = 0; i < NFILES; i++) {
			assert(seq[0] == seq[i]);
			p->end[i] = mk[i];
		}
		p->end_seq = seq[0];
	}
	return (list);
}

private void
conflct_free(conflct *c)
{
	unless (c) return;
	if (c->algos) freeLines(c->algos, 0);
	free(c);
}

/*
 * Return true if a line appears anywhere inside a conflict.
 */
private int
contains_line(conflct *c, ld_t *line)
{
	int	i, j;

	for (i = 0; i < NFILES; i++) {
		for (j = c->start[i]; j < c->end[i]; j++) {
			if (sameline(&body[i].lines[j], line)) return (1);
		}
	}
	return (0);
}

/*
 * Merge a conflict into the following conflict if they are adjacent
 * or if they are separated by one line that exists in one of the
 * conflict regions.
 * XXX should handle N
 */
private int
merge_conflicts(conflct *head)
{
	ld_t	*line;
	conflct	*tmp;
	int	i;

	unless (head->next) return (0);
	if (head->split) return (0);
	line = &body[0].lines[head->end[0]];
	if ((((head->next->start[0] - head->end[0]) == 1) &&
		(contains_line(head, line) ||
		 contains_line(head->next, line))) ||
	    (head->next->start[0] == head->end[0])) {

		/* merge the two regions */
		T_DEBUG("merge %d-%d %d-%d",
		    head->start[LEFT], head->end[LEFT],
		    head->next->start[LEFT], head->next->end[LEFT]);
		tmp = head->next;
		for (i = 0; i < NFILES; i++) {
			head->end[i] = tmp->end[i];
		}
		assert(tmp->end_seq);
		head->end_seq = tmp->end_seq;
		head->next = tmp->next;
		if (tmp->next) tmp->next->prev = head;
		conflct_free(tmp);
		return (1);
	}
	return (0);
}

/*
 * The structure for a class that creates and then
 * walks the diffs of two files and returns the
 * lines for the right file as requested.  All struct members
 * should be considered private and all operations are done
 * with the member functions below.
 */
typedef struct difwalk difwalk;
struct difwalk {
	FILE	*diff;
	char	*file[2];	/* tmpfiles being diff'ed */
	int	offset;		/* difference between gca lineno and right */
	int	start, end;
	int	lines;
	struct {
		int	gcastart, gcaend;
		int	lines;
	} cmd;
};

/*
 * Read a line from the diff and return it.  The arguments are like fgets().
 * We need to just skip over lines missing newlines; we don't actually
 * read the lines so we don't need to know about the missing newline.
 * We are just looking for line counts.
 */
private char *
readdiff(char *buf, int size, FILE *f)
{
	char	*ret;
	int	c;

	do {
		if ((ret = fgets(buf, size, f)) && !strchr(buf, '\n')) {
			/* skip to next newline */
			while (((c = getc(f)) != EOF) && (c != '\n'));
		}
	} while (ret && strneq(buf, "\\ No newline", 12));
	return (ret);
}

/*
 * Internal function.
 * Read next command from diff stream
 */
private void
diffwalk_readcmd(difwalk *dw)
{
	char	*p;
	int	start, end;
	int	cnt;
	char	cmd;
	char	buf[64];

	unless (readdiff(buf, sizeof(buf), dw->diff)) {
		dw->cmd.gcastart = dw->cmd.gcaend = INT_MAX;
		return;
	}

	dw->cmd.gcastart = strtol(buf, &p, 10);
	if (*p == ',') {
		dw->cmd.gcaend = strtol(p+1, &p, 10) + 1;
	} else {
		dw->cmd.gcaend = dw->cmd.gcastart;
	}
	cmd = *p++;
	start = strtol(p, &p, 10);
	if (*p == ',') {
		end = strtol(p+1, &p, 10);
	} else {
		end = start;
	}
	dw->cmd.lines = (cmd == 'd') ? 0 : (end - start + 1);

	if (cmd != 'a') {
		char	buf[MAXLINE];
		int	cnt;

		if (dw->cmd.gcaend == dw->cmd.gcastart) {
			++dw->cmd.gcaend;
		}
		cnt = dw->cmd.gcaend - dw->cmd.gcastart;
		while (cnt--) {
			readdiff(buf, sizeof(buf), dw->diff);
			assert(buf[0] == '<');
			assert(buf[1] == ' ');
		}
		if (cmd == 'c') {
			readdiff(buf, sizeof(buf), dw->diff);
			assert(buf[0] == '-');
		}
	} else {
		assert(dw->cmd.gcaend == dw->cmd.gcastart);
		dw->cmd.gcaend = ++dw->cmd.gcastart;
	}
	cnt = dw->cmd.lines;
	while (cnt--) {
		readdiff(buf, sizeof(buf), dw->diff);
		assert(buf[0] == '>');
		assert(buf[1] == ' ');
	}
}

/*
 * create a new diffwalk struct from two filenames
 */
private difwalk *
diffwalk_new(file_t *left, file_t *right)
{
	difwalk	*dw;
	char	*cmd;
	FILE	*f;
	char	*d;
	size_t	len;
	int	i;

	dw = new(difwalk);
	for (i = 0; i < 2; i++) {
		dw->file[i] = bktmp(0);
		f = fopen(dw->file[i], "w");
		d = fmem_peek(((i == 0) ? left : right)->f, &len);
		fwrite(d, 1, len, f);
		fclose(f);
	}
	cmd = aprintf("bk diff %s '%s' '%s'",
	    (anno ? "--ignore-to-str='\\| '" : ""),
	    dw->file[0], dw->file[1]);
	dw->diff = popen(cmd, "r");
	free(cmd);

	diffwalk_readcmd(dw);
	return (dw);
}

/*
 * return the gca linenumber of the start of the next diff in the
 * file.
 */
private int
diffwalk_nextdiff(difwalk *dw)
{
	return (dw->cmd.gcastart);
}

/*
 * Extend the current diff region to at least contain the given
 * GCA line number.
 */
private void
diffwalk_extend(difwalk *dw, int gcalineno)
{
	if (gcalineno < dw->end) return;

	while (gcalineno > dw->end && dw->end < dw->cmd.gcastart) {
		++dw->end;
	}
	if (gcalineno >= dw->cmd.gcastart) {
		int	extra;

		extra = dw->cmd.lines - (dw->cmd.gcaend - dw->cmd.gcastart);
		dw->lines += extra;
		dw->offset += extra;
		dw->end = dw->cmd.gcaend;
		diffwalk_readcmd(dw);
		diffwalk_extend(dw, gcalineno);
	}
}

/*
 * Start a new diff at the given line number in the GCA file.
 */
private void
diffwalk_start(difwalk *dw, int gcalineno)
{
	assert(gcalineno <= dw->cmd.gcastart);
	dw->start = dw->end = gcalineno;
	dw->lines = 0;
	if (gcalineno == dw->cmd.gcastart) diffwalk_extend(dw, dw->cmd.gcaend);
}

/*
 * Return the ending GCA line number for the current diff region
 */
private int
diffwalk_end(difwalk *dw)
{
	return (dw->end);
}

private void
diffwalk_range(difwalk *dw, int side, conflct *conf)
{
	conf->start[side] = dw->start + dw->offset - 1 - dw->lines;
	conf->end[side] = dw->end + dw->offset - 1;
}

/*
 * free a diffwalk struct
 */
private void
diffwalk_free(difwalk *dw)
{
	int	i;

	pclose(dw->diff);
	for (i = 0; i < 2; i++) {
		unlink(dw->file[i]);
		free(dw->file[i]);
	}
	free(dw);
}

private int
do_diff_merge(void)
{
	difwalk	*ldiff;
	difwalk	*rdiff;
	int	gcaline;
	int	ret = 0;
	conflct	*c;
	conflct	conf;

	ldiff = diffwalk_new(&body[GCA], &body[LEFT]);
	rdiff = diffwalk_new(&body[GCAR], &body[RIGHT]);

	gcaline = 1;
	while (1) {
		int	start, end;
		int	tmp;

		start = diffwalk_nextdiff(ldiff);
		tmp = diffwalk_nextdiff(rdiff);
		if (tmp < start) start = tmp;

		while (gcaline < start) {
			if (gcaline > body[GCA].n) break;
			printline(&body[GCA].lines[gcaline-1], 0, 0);
			++gcaline;
		}
		if (gcaline < start) break;
		diffwalk_start(ldiff, start);
		diffwalk_start(rdiff, start);
		while (1) {
			int	lend, rend;

			lend = diffwalk_end(ldiff);
			rend = diffwalk_end(rdiff);

			if (lend < rend) {
				diffwalk_extend(ldiff, rend);
			} else if (rend < lend) {
				diffwalk_extend(rdiff, lend);
			} else {
				/* They agree */
				end = lend;
				break;
			}
		}
		memset(&conf, 0, sizeof(conf));
		conf.start[GCA] = conf.start[GCAR] = start - 1;
		conf.end[GCA] = conf.end[GCAR] = end - 1;
		if (start - 1 > 0) {
			conf.start_seq = body[GCA].lines[start - 2].seq;
		}
		conf.end_seq = body[GCA].lines[end - 1].seq;

		diffwalk_range(ldiff, LEFT, &conf);
		diffwalk_range(rdiff, RIGHT, &conf);

		c = &conf;
		while (c) {
			if (resolve_conflict(c)) ret = 1;
			c = c->next;
		}
		gcaline = end;
	}

	diffwalk_free(ldiff);
	diffwalk_free(rdiff);
	return (ret);
}

/*
 * Define of set of autoresolve functions for processing a
 * conflict. Each function takes a conflict region and looks at it to
 * determine if a merge is possible.  If a merge is possible then the
 * ->merged array is initialized with the lines in the merged output
 * and 1 is returned.  The conflict can be split into to adjacent
 * conflicts that exactly cover the original conflict region.  (use
 * split_conflict() to make this split) If a split occurs then a 1 is
 * returns.  If the function changes nothing then 0 is returned.
 *
 * The ->merged array is a malloced array of ld_t's that have line data. 
 * Most merge functions will just fill this with copies of ld_t's from
 * one of the two sides.  When a function is written that starts to
 * generate merged lines that didn't exist on either side (like
 * character merging) then the lines themselves will need to be
 * included in same memory block.  Also don't forget that annotations
 * need to be faked.  Perhaps this will be ugly enough to warrant
 * rewriting this interface...
 *
 * The numbers used below, once shipped, must always mean the same thing.
 * If you evolve this code, use new numbers.
 *
 * Currently all functions are enabled by default, we use -1 to mean that
 * it is enabled, but hasn't been overridden by the command line.  It gets
 * set to 1 if a function is explicitly enabled.
 *
 * The order in the struct is the order the enabled functions are run.
 */
struct mergefcns {
	char	*name;		/* "name" of function for commandline */
	int	enable;		/* is this function enabled */
	int	(*fcn)(conflct *r);
	char	*fname;		/* function name */
	char	*help;
} mergefcns[] = {
	{"9",   -1, merge_conflicts, "merge_conflicts",
	 "Merge adjacent conflict regions"},
	{"1",	-1, merge_same_changes, "merge_same_changes",
	"Merge identical changes made by both sides"},
	{"2",	-1, merge_only_one, "merge_only_one",
	"Merge when only one side changes"},
	{"3",	-1, merge_content, "merge_content",
	"Merge adjacent non-overlapping modifications on both sides"},
	{"4",	-1, merge_common_header, "merge_common_header",
	"Merge identical changes at the start of a conflict"},
	{"7",	-1, merge_added_oneside, "merge_added_oneside",
	"Split one side add from conflict"},
	{"5",	-1, merge_common_footer, "merge_common_footer",
	"Merge identical changes at the end of a conflict"},
	{"6",	-1, merge_common_deletes, "merge_common_deletes",
	"Merge identical deletions made by both sides"},
	{"8",   -1, merge_with_following_gca, "merge_with_following_gca",
	 "Match single side with GCA"},
};
#define	N_MERGEFCNS (sizeof(mergefcns)/sizeof(struct mergefcns))

/*
 * Called by the command line parser to enable/disable different functions.
 * This function takes a comma or space separated list of function names
 * and enables/disables those functions.  "all" is an alias for all functions.
 */
private void
enable_mergefcns(char *list, int enable)
{
	int	i;
	int	len;

	while (*list) {
		list += strspn(list, ", ");
		len = strcspn(list, ", ");
		unless (len) break;

		if (len == 3 && strneq(list, "all", 3)) {
			for (i = 0; i < N_MERGEFCNS; i++) {
				mergefcns[i].enable = enable;
			}
		} else {
			for (i = 0; i < N_MERGEFCNS; i++) {
				if (strlen(mergefcns[i].name) == len &&
				    strneq(list, mergefcns[i].name, len)) {
					mergefcns[i].enable = enable;
					break;
				}
			}
			if (i == N_MERGEFCNS) {
				fprintf(stderr,
					"ERROR: unknown merge function '%*s'\n",
					len, list);
				exit(2);
			}
		}
		list += len;
	}
}

private void
mergefcns_help(void)
{
	int	i;

	fprintf(stderr,
"The following is a list of merge algorithms which may be enabled or disabled\n\
to change the way smerge will automerge.  Starred entries are on by default.\n");
	for (i = 0; i < N_MERGEFCNS; i++) {
		fprintf(stderr, "%2s%c %s\n",
		    mergefcns[i].name,
		    mergefcns[i].enable ? '*' : ' ',
		    mergefcns[i].help);
	}
}

/*
 * Resolve a conflict region.
 * Try automerging and then print the merge, or display a conflict.
 */
private int
resolve_conflict(conflct *curr)
{
	int	i;
	int	ret = 0;
	int	changed;
	char	*t;

	/*
	 * Try every enabled automerge function on the current conflict
	 * until an automerge occurs or no more changes are possible.
	 */
	do {
		changed = 0;
		for (i = 0; i < N_MERGEFCNS; i++) {
			unless (mergefcns[i].enable) continue;
			if (mergefcns[i].fcn(curr)) {
				curr->algos =
				    addLine(curr->algos, mergefcns[i].fname);
				changed |= 1;
				T_DEBUG("%s did change: %d", mergefcns[i].fname,
				    nLines(curr->merged));
			}
			if (curr->merged) break;
		}
	} while (changed && !curr->merged);
	if (curr->merged) {
		ld_t	*p;
		/* This region was automerged */
		if (fdiff) {
			putchar('M');
			printf(" %d", curr->start_seq);
			if (show_mergefn && curr->algos) {
				t = joinLines(", ", curr->algos);
				printf(" %s", t);
				free(t);
			}
			putchar('\n');
		}
		EACHP(curr->merged, p) printline(p, 0, (fdiff != 0));
		free(curr->merged);
		if (fdiff) user_conflict_fdiff(curr);
	} else {
		/* found a conflict that needs to be resolved
		 * by the user
		 */
		ret = 1;
		if (fdiff) {
			user_conflict_fdiff(curr);
		} else {
			user_conflict(curr);
		}
	}
	if (fdiff) {
		putchar('E');
		assert(curr->end_seq);
		printf(" %d", curr->end_seq);
		putchar('\n');
	}
	return (ret);
}

/*
 * Compare two lines and return true if they are the same.
 * The function does not compare the sequence numbers.
 */
private int
sameline(ld_t *left, ld_t *right)
{
	if (left->len != right->len) return (0);
	return (memcmp(left->line, right->line, left->len) == 0);
}

/*
 * Return true if the lines on side 'l' of a conflict are the same as the
 * lines on side 'r'
 * Sequence numbers are ignored in the comparison.
 */
private int
samedata(conflct *c, int l, int r)
{
	int	i;
	int	len;
	int	sl, sr;

	len = c->end[l] - c->start[l];
	if (len != c->end[r] - c->start[r]) return (0);

	sl = c->start[l];
	sr = c->start[r];
	for (i = 0; i < len; i++) {
		unless (sameline(&body[l].lines[sl + i],
			    &body[r].lines[sr + i])) {
			return (0);
		}
	}
	return (1);
}

/*
 * Print a conflict region that must be resolved by the user.
 */
private void
user_conflict(conflct *curr)
{
	int	i;
	diffln	*diffs;
	char	*t, *fcns = "";
	int	sameleft;
	int	sameright;

	if (show_mergefn && curr->algos) {
		t = joinLines(", ", curr->algos);
		fcns = aprintf(" %s", t);
		free(t);
	}
	switch (mode) {
	    case MODE_GCA:
		sameleft = samedata(curr, GCA, LEFT);
		sameright = samedata(curr, GCAR, RIGHT);
		unless (sameleft && !sameright) {
			printf("<<<<<<< local %s %s vs %s%s\n",
			    file, revs[GCA], revs[LEFT], fcns);
			diffs = unidiff_ndiff(curr, GCA, LEFT);
			for (i = 0; diffs[i].ld; i++) {
				printline(diffs[i].ld, diffs[i].c, 1);
			}
			free(diffs);
		}
		unless (sameright && !sameleft) {
			printf("<<<<<<< remote %s %s vs %s%s\n",
			    file, revs[GCAR], revs[RIGHT], fcns);
			diffs = unidiff_ndiff(curr, GCAR, RIGHT);
			for (i = 0; diffs[i].ld; i++) {
				printline(diffs[i].ld, diffs[i].c, 1);
			}
			free(diffs);
		}
		printf(">>>>>>>\n");
		break;
	    case MODE_2WAY:
/* #define MATCH_DIFF3 */
#ifdef MATCH_DIFF3
		printf("<<<<<<< L\n");
		printlines(curr, LEFT, 1);
		printf("=======\n");
		printlines(curr, RIGHT, 1);
		printf(">>>>>>> R\n");
#else
		printf("<<<<<<< local %s %s\n", file, revs[LEFT]);
		printlines(curr, LEFT, 1);
		printf("<<<<<<< remote %s %s\n", file, revs[RIGHT]);
		printlines(curr, RIGHT, 1);
		printf(">>>>>>>\n");
#endif
		break;
	    case MODE_3WAY:
		printf("<<<<<<< gca %s %s\n", file, revs[GCA]);
		printlines(curr, GCA, 1);
		printf("<<<<<<< local %s %s\n", file, revs[LEFT]);
		printlines(curr, LEFT, 1);
		printf("<<<<<<< remote %s %s\n", file, revs[RIGHT]);
		printlines(curr, RIGHT, 1);
		printf(">>>>>>>\n");
		break;
	    case MODE_NEWONLY:
		sameleft = samedata(curr, GCA, LEFT);
		sameright = samedata(curr, GCAR, RIGHT);
		unless (sameleft && !sameright) {
			printf("<<<<<<< local %s %s vs %s%s\n",
			    file, revs[GCA], revs[LEFT], fcns);
			diffs = unidiff_ndiff(curr, GCA, LEFT);
			for (i = 0; diffs[i].ld; i++) {
				if (diffs[i].c != '-') {
					printline(diffs[i].ld, diffs[i].c, 1);
				}
			}
			free(diffs);
		}
		unless (sameright && !sameleft) {
			printf("<<<<<<< remote %s %s vs %s%s\n",
			    file, revs[GCAR], revs[RIGHT], fcns);
			diffs = unidiff_ndiff(curr, GCAR, RIGHT);
			for (i = 0; diffs[i].ld; i++) {
				if (diffs[i].c != '-') {
					printline(diffs[i].ld, diffs[i].c, 1);
				}
			}
			free(diffs);
		}
		printf(">>>>>>>\n");
		break;
	}
	if (*fcns) free(fcns);
}

/*
 * Print a conflict in fdiff format
 */
private void
user_conflict_fdiff(conflct *c)
{
	int	i, j;
	diffln	*left = 0, *right = 0;
	diffln	*diffs;
	diffln	*rightbuf;
	diffln	*lp, *rp;
	ld_t	*p, *end;
	ld_t	blankline;

	switch (mode) {
	    case MODE_GCA:
		left = unidiff(c, GCA, LEFT);
		right = unidiff(c, GCAR, RIGHT);
		break;
	    case MODE_2WAY:
		/* fake a diff output */
		left = calloc(c->end[LEFT] - c->start[LEFT] + 1,
		    sizeof(diffln));
		p = &body[LEFT].lines[c->start[LEFT]];
		end = &body[LEFT].lines[c->end[LEFT]];
		i = 0;
		while (p < end) {
			left[i].ld = p++;
			left[i].c = ' ';
			i++;
		}
		left[i].ld = 0;
		right = calloc(c->end[RIGHT] - c->start[RIGHT] + 1,
		    sizeof(diffln));
		p = &body[RIGHT].lines[c->start[RIGHT]];
		end = &body[RIGHT].lines[c->end[RIGHT]];
		i = 0;
		while (p < end) {
			right[i].ld = p++;
			right[i].c = ' ';
			i++;
		}
		right[i].ld = 0;
		break;
	    case MODE_NEWONLY:
		diffs = unidiff(c, GCA, LEFT);
		for (i = 0; diffs[i].ld; i++);
		left = calloc(i+1, sizeof(diffln));
		j = 0;
		for (i = 0; diffs[i].ld; i++) {
			if (diffs[i].c != '-') {
				left[j++] = diffs[i];
			}
		}
		left[j].ld = 0;
		free(diffs);
		diffs = unidiff(c, GCAR, RIGHT);
		for (i = 0; diffs[i].ld; i++);
		right = calloc(i+1, sizeof(diffln));
		j = 0;
		for (i = 0; diffs[i].ld; i++) {
			if (diffs[i].c != '-') {
				right[j++] = diffs[i];
			}
		}
		right[j].ld = 0;
		free(diffs);
		break;
	    case MODE_3WAY:
		fprintf(stderr, "Can't use 3-way diff with fdiff output!\n");
		exit(2);
		break;
	}
	
	/*
	 * generate highlighting, don't forget to free below
	 */
	highlight_diff(left);
	highlight_diff(right);

	/*
	 * Need to allocate rightbuf to hold data that will be printed
	 * on the right.
	 * The length can never be longer that right + left + 1
	 */
	i = 0;
	for (diffs = left; diffs->ld; diffs++) i++;
	for (diffs = right; diffs->ld; diffs++) i++;
	rightbuf = calloc(i+1, sizeof(diffln));

	blankline.line = "\n";
	blankline.len = 1;
	blankline.seq = 0;
	blankline.anno = 0;

	putchar('L');
	unless (c->merged) printf(" %d", c->start_seq);
	putchar('\n');
	lp = left;
	rp = right;
	i = 0;
	while (lp->ld || rp->ld) {
		if (!rp->ld || lp->ld && lp->ld->seq < rp->ld->seq) {
			/* line on left */
			printline(lp->ld, lp->c, 1);
			if (lp->highlight) printhighlight(lp->highlight);
			rightbuf[i].ld = &blankline;
			rightbuf[i].c = 's';
			rightbuf[i].highlight = 0;
			++lp;
		} else if (!lp->ld || rp->ld->seq < lp->ld->seq) {
			/* line on right */
			printline(&blankline, 's', 1);
			rightbuf[i].ld = rp->ld;
			rightbuf[i].c = rp->c;
			rightbuf[i].highlight = rp->highlight;
			++rp;
		} else {
			/* matching line */
			printline(lp->ld, lp->c, 1);
			if (lp->highlight) printhighlight(lp->highlight);
			rightbuf[i].ld = rp->ld;
			rightbuf[i].c = rp->c;
			rightbuf[i].highlight = rp->highlight;
			++lp;
			++rp;
		}
		++i;
	}

	fputs("R\n", stdout);
	for (j = 0; j < i; j++) {
		printline(rightbuf[j].ld, rightbuf[j].c, 1);
		if (rightbuf[j].highlight) {
			printhighlight(rightbuf[j].highlight);
		}
	}

	/* 
	 * free highlight,  C++ makes this much nicer
	 */
	for (i = 0; left[i].ld; i++) {
		if (left[i].highlight) free(left[i].highlight);
	}
	for (i = 0; right[i].ld; i++) {
		if (right[i].highlight) free(right[i].highlight);
	}

	free(left);
	free(right);
	free(rightbuf);
}

/*
 * do a diff of two sides of a conflict region and return
 * an array of diffln structures.  The diff is purely based on
 * sequence numbers and is very quick.
 * The returned array is allocated with malloc and needs to be freed by
 * the user and the end of the array is marked by a ld pointer set to null.
 * Typical usage:
 *
 *    diffs = unidiff(curr, GCA, LEFT);
 *    for (i = 0; diffs[i].ld; i++) {
 *	     printline(diffs[i].ld, diffs[i].c, 0);
 *    }
 *    free(diffs);
 */
private diffln *
unidiff(conflct *curr, int left, int right)
{
	ld_t	*llines, *rlines;
	ld_t	*el, *er;
	diffln	*out;
	diffln	*p;
	diffln	*del, *delp;
	diffln	*ins, *insp;

	llines = &body[left].lines[curr->start[left]];
	rlines = &body[right].lines[curr->start[right]];
	el = &body[left].lines[curr->end[left]];
	er = &body[right].lines[curr->end[right]];

	/* allocate the maximum space that might be needed. */
	p = out = calloc(el - llines + er - rlines + 1, sizeof(diffln));
	delp = del = calloc(el - llines, sizeof(diffln));
	insp = ins = calloc(er - rlines, sizeof(diffln));

	while ((llines < el) || (rlines < er)) {
		if ((rlines == er) ||
		    ((llines < el) && (llines->seq < rlines->seq))) {
			delp->ld = llines++;
			delp->c = '-';
			++delp;
		} else if ((llines == el) || (rlines->seq < llines->seq)) {
			insp->ld = rlines++;
			insp->c = '+';
			++insp;
		} else {
			assert(llines->seq == rlines->seq);
			if (delp > del) {
				int	cnt = delp - del;
				memcpy(p, del, cnt * sizeof(diffln));
				p += cnt;
				delp = del;
			}
			if (insp > ins) {
				int	cnt = insp - ins;
				memcpy(p, ins, cnt * sizeof(diffln));
				p += cnt;
				insp = ins;
			}
			p->ld = llines++;
			p->c = ' ';
			++p;
			++rlines;
		}

	}
	if (delp > del) {
		int	cnt = delp - del;
		memcpy(p, del, cnt * sizeof(diffln));
		p += cnt;
		delp = del;
	}
	free(del);
	if (insp > ins) {
		int	cnt = insp - ins;
		memcpy(p, ins, cnt * sizeof(diffln));
		p += cnt;
		insp = ins;
	}
	free(ins);
	p->ld = 0;
	p->c = 0;

	return (out);
}

/*
 * do a diff of two sides of a conflict region and return
 * an array of diffln structures.
 * The returned array is allocated with malloc and needs to be freed by
 * the user and the end of the array is marked by a ld pointer set to null.
 * Typical usage:
 *
 *    diffs = unidiff_ndiff(curr, GCA, LEFT);
 *    for (i = 0; diffs[i].ld; i++) {
 *	     printline(diffs[i].ld, diffs[i].c, 0);
 *    }
 *    free(diffs);
 */

typedef	struct {
	ld_t	*lines[2];
} smdiff;

private diffln *
unidiff_ndiff(conflct *curr, int left, int right)
{
	hunk	range, *hunks;
	hunk	*h, *from, common;
	diffln	*ret, *dl;	
	int	i, n;
	int	j, side;
	smdiff	data;

	/* range being diffed */
	range.start[DF_LEFT] = curr->start[left];
	range.len[DF_LEFT] = curr->end[left] - curr->start[left];
	range.start[DF_RIGHT] = curr->start[right];
	range.len[DF_RIGHT] = curr->end[right] - curr->start[right];

	data.lines[DF_LEFT] = body[left].lines;
	data.lines[DF_RIGHT] = body[right].lines;

	/* XXX: this disables some alignment by starting diff here */
	hunks = diff_items(&range, 1,
	    smergeData, diff_cmpLine, diff_hashLine, diff_cost, &data);

	/* Length of output is all of one side plus hunks on other */
	n = range.len[DF_LEFT];
	EACH(hunks) n += hunks[i].len[DF_RIGHT];
	/* result is a null entry terminated list */
	ret = dl = calloc(n + 1, sizeof(diffln));
	from = 0;
	EACHP(hunks, h) {
		diff_mkCommon(&common, &range, from, h);
		DFOREACH(&common, DF_LEFT, j) {
			dl->c = ' ';
			dl->ld = &data.lines[DF_LEFT][j];
			dl++;
		}
		from = h;
		for (side = 0; side < 2; side++) {
			DFOREACH(h, side, j) {
				dl->c = (side == DF_LEFT) ? '-' : '+';
				dl->ld = &data.lines[side][j];
				dl++;
			}
		}
	}
	diff_mkCommon(&common, &range, from, 0);
	DFOREACH(&common, DF_LEFT, j) {
		dl->c = ' ';
		dl->ld = &data.lines[DF_LEFT][j];
		dl++;
	}

	assert(ret + n == dl);
	free(hunks);
	return (ret);
}

/*
 * Look at a pair of matching lines and attempt to highlight them.
 */
private void
highlight_line(diffln *del, diffln *add)
{
	int	s, e;
	char	*dline = del->ld->line;
	int	dlen = del->ld->len;
	char	*aline = add->ld->line;
	int	alen = add->ld->len;
	int	len;

	s = 0;
	while (s < dlen && s < alen && dline[s] == aline[s]) s++;
	
	e = 0;
	while (e < dlen - s && e < alen - s && 
	    dline[dlen - e - 1] == aline[alen - e - 1]) e++;
	
	len = max(alen, dlen);

	if (s + e < len / 3) return; /* not enough matched */
	
	if (s + e < dlen) {
		del->highlight = calloc(4, sizeof(int));
		del->highlight[0] = s;
		del->highlight[1] = dlen - e;
	}
	if (s + e < alen) {
		add->highlight = calloc(4, sizeof(int));
		add->highlight[0] = s;
		add->highlight[1] = alen - e;
	}
}

/*
 * Walk replacements in a diff and find lines that are mostly similar.
 * For those lines generate character highlighting information to
 * mark the characters that have changed.
 *
 * For any line that should be highlighted, the highlight field of
 * the diffln struct is an array of range pairs {start, end}.  The array
 * ends at the pair 0,0.  (The array is allocated with malloc and it
 * is up to the user to free.
 */
private void
highlight_diff(diffln *diff)
{
	int	a, b;

	a = 0;
	while (1) {
		/* find start of deleted region */
		while (diff[a].ld && diff[a].c != '-') a++;
		unless (diff[a].ld) return;
		
		/* find replacements */
		b = a + 1;
		while (diff[b].ld && diff[b].c == '-') b++;

		while (diff[b].ld && diff[a].c == '-' && diff[b].c == '+') {
			/* found a replacement do highlighting */
			highlight_line(&diff[a], &diff[b]);
			a++;
			b++;
		}
		a = b;
	}
}

/*
 * Parse a range string from the command line.
 * Valid formats:
 *	"44..67"	after 44 to before 67
 *	"44.."		after 44 to end of file
 *	"..67"		start of file to before 67
 */
private	int
parse_range(char *range, u32 *start, u32 *end)
{
	if (isdigit(range[0])) {
		*start = strtoul(range, &range, 10);
	}
	if (range[0] == 0 || range[0] != '.' || range[1] != '.') {
		return (-1);
	}
	range += 2;
	if (isdigit(range[0])) {
		*end = strtoul(range, 0, 10);
	}
	return(0);
}

/*--------------------------------------------------------------------
 * Automerge functions
 *--------------------------------------------------------------------
 */

/*
 * All the lines on both sides are identical.
 */
private int
merge_same_changes(conflct *c)
{
	int	i;

	if (samedata(c, LEFT, RIGHT)) {
		int	len = c->end[LEFT] - c->start[LEFT];

		for (i = 0; i < len; i++) {
			addArray(&c->merged,
			    &body[LEFT].lines[c->start[LEFT] + i]);
		}
		unless(c->merged) growArray(&c->merged, 0);
		return (1);
	}
	return (0);
}

/*
 * Only one side make changes
 */
private int
merge_only_one(conflct *c)
{
	int	i;

	if (samedata(c, GCA, LEFT)) {
		int	len = c->end[RIGHT] - c->start[RIGHT];

		for (i = 0; i < len; i++) {
			addArray(&c->merged,
			    &body[RIGHT].lines[c->start[RIGHT] + i]);
		}
		unless(c->merged) growArray(&c->merged, 0);
		return (1);
	}
	if (samedata(c, GCAR, RIGHT)) {
		int	len = c->end[LEFT] - c->start[LEFT];

		for (i = 0; i < len; i++) {
			addArray(&c->merged,
			    &body[LEFT].lines[c->start[LEFT] + i]);
		}
		unless(c->merged) growArray(&c->merged, 0);
		return (1);
	}
	return (0);
}

/*
 * Given two diff blocks and the common block between them,
 * return 1 if the full common block does not exist in either diff block.
 */
private int
uniqueSeparator(hunk **h, int side, int other)
{
	ld_t	*common, *diff;
	int	cstart, clen;	/* common block */
	int	dstart, dlen;	/* diff block */
	int	i, j, k;

	common = body[GCA].lines;
	cstart = DEND(h[side], DF_LEFT);
	clen = DSTART(h[other], DF_LEFT) - cstart;
	unless (clen) return (0);

	for (i = 0; i < 2; i++) {
		diff = body[i].lines;
		dstart = DSTART(h[i], DF_RIGHT);
		dlen = DLEN(h[i], DF_RIGHT);
		for (j = 0; j < dlen-clen+1; j++) {
			for (k = 0; k < clen; k++) {
				unless (sameline(
				    &diff[dstart+j+k], &common[cstart+k])) {
					break;
				}
			}
			if (k == clen) break;
		}
		if (j < dlen-clen+1) return (0);
	}
	return (1);		/* must be good */
}


/*
 * Use an actual diff of the contents of both sides, ignoring the
 * weave, and do a diff3-like merge of the GCA/LEFT and GCA/RIGHT
 * diffs.  Only when the diff hunks on both sides are completely
 * non-overlapping do we try to combine them together.  A diff hunk
 * that inserts lines is only allowed if the lines before and after
 * the insert are not touched by the other side.
 */
private int
merge_content(conflct *c)
{
	int	ret = 1;
	hunk	range;
	hunk	*h[2], *hs[2], *he[2];
	int	ig;
	int	i;
	int	side, other;
	smdiff	data;

	/* This function makes use of overlap in enum values */
	assert(((int)LEFT == (int)DF_LEFT) && ((int)RIGHT == (int)DF_RIGHT));

	/* we require something to be changed */
	if (c->start[GCA] == c->end[GCA]) return (0);

	range.start[DF_LEFT] = c->start[GCA];
	range.len[DF_LEFT] = c->end[GCA] - c->start[GCA];
	data.lines[DF_LEFT] = body[GCA].lines;

	for (side = 0; side < 2; side++) {
		range.start[DF_RIGHT] = c->start[side];
		range.len[DF_RIGHT] = c->end[side] - c->start[side];
		data.lines[DF_RIGHT] = body[side].lines;

		if (h[side] = diff_items(&range, 1, smergeData,
		    diff_cmpLine, diff_hashLine, diff_cost, &data)) {
			hs[side] = h[side];
			he[side] = h[side] + nLines(h[side]);
			++h[side];
		} else {
			hs[side] = he[side] = 0;
		}
	}

	/*
	 * only look for diff chunks that don't overlap
	 */
	ig = c->start[GCA];		/* lines in gca */
	while (h[DF_LEFT] || h[DF_RIGHT]) {
		for (side = 0; side < 2; side++) {
			other = !side; /* other side */

			if (!h[side]) {
				/* no hunk to apply */
				continue;

			} else if (!h[other]) {
				/* no other hunk, so no conflict */

			} else if (DEND(h[side], DF_LEFT) >
			    DSTART(h[other], DF_LEFT)) {
				/* hunks overlap, skip */
				continue;

			} else if (((DLEN(h[side], DF_LEFT) == 0) ||
			    (DLEN(h[other], DF_LEFT) == 0)) &&
			    !uniqueSeparator(h, side, other)) {
				/*
				 * If either hunk is an insert, then
				 * the lines between the two hunks
				 * cannot also appear inside the lines
				 * being inserted.
				 */
				continue;
			}

			/* first any GCA before the hunk starts */
			while (ig < DSTART(h[side], DF_LEFT)) {
				addArray(&c->merged,
				    &body[GCA].lines[ig++]);
			}
			/* skip GCA lines being deleted */
			ig += DLEN(h[side], DF_LEFT);

			/* add new lines from hunk */
			DFOREACH(h[side], DF_RIGHT, i) {
				addArray(&c->merged, &body[side].lines[i]);
			}
			if (++h[side] > he[side]) h[side] = 0;
			break;
		}
		if (side >= 2) {
			/* hunks overlap, abort */
			FREE(c->merged);
			ret = 0;
			break;
		}
	}
	if (ret) {
		/* now any GCA lines at end of conflict */
		while (ig < c->end[GCA]) {
			addArray(&c->merged, &body[GCA].lines[ig++]);
		}
		/* we might automerge to an empty region... */
		unless (c->merged) growArray(&c->merged, 0);
	}
	for (side = 0; side < 2; side++) free(hs[side]);
	return (ret);
}

/*
 * Take a conflict region and a set of 3 indexes for the 3 sides
 * and split the conflict into two such that the second conflict starts
 * with those indexes.
 */
private void
split_conflict(conflct *c, int splitidx[NFILES])
{
	conflct	*newc;
	int	i;
	u32	seq;

	seq = body[GCA].lines[c->start[GCA] + splitidx[GCA]].seq;
	assert(seq);

	/* create a new conflict region */
	newc = new(conflct);

	for (i = 0; i < NFILES; i++) {
		newc->end[i] = c->end[i];
		newc->start[i] = c->end[i] = c->start[i] + splitidx[i];
	}
	newc->end_seq = c->end_seq;

	c->end_seq = seq;
	newc->start_seq = seq;
	c->split = 1;		/* prevent merge_conflicts() from joining */

	/* link it up */
	if (c->next) c->next->prev = newc;
	newc->next = c->next;
	c->next = newc;
	newc->prev = c;
}

/*
 * Split a conflict if the first N lines of the RIGHT and the LEFT have
 * identical data.
 */
private int
merge_common_header(conflct *c)
{
	ld_t	*start[NFILES];
	int	len[NFILES];
	int	minlen;
	int	splitidx[NFILES];
	int	i, j;
	int	chkgca;

	for (i = 0; i < NFILES; i++) {
		start[i] = &body[i].lines[c->start[i]];
		len[i] = c->end[i] - c->start[i];
	}
	minlen = min(len[LEFT], len[RIGHT]);

	chkgca = 1;
	for (i = j = 0; i < minlen; i++) {
		unless (sameline(&start[LEFT][i], &start[RIGHT][i])) break;
		if (chkgca && (i < len[GCA]) &&
		    sameline(&start[LEFT][i], &start[GCA][i])) {
			j++;
		} else {
			chkgca = 0;
		}
	}
	/* i == number of matching lines at start */
	if (i == 0 || i == len[LEFT] && i == len[RIGHT]) return (0);

	splitidx[LEFT] = i;
	splitidx[RIGHT] = i;
	splitidx[GCA] = splitidx[GCAR] = j;

	split_conflict(c, splitidx);
	T_DEBUG("split %d-%d %d-%d",
	    c->start[LEFT], c->end[LEFT],
	    c->next->start[LEFT], c->next->end[LEFT]);

	return (1);
}

/*
 * Split a conflict if the last N lines of the RIGHT and the LEFT have
 * identical data.
 */
private int
merge_common_footer(conflct *c)
{
	ld_t	*start[NFILES];
	int	len[NFILES];
	int	minlen;
	int	splitidx[NFILES];
	int	i, j;
	int	chkgca;

	for (i = 0; i < NFILES; i++) {
		start[i] = &body[i].lines[c->start[i]];
		len[i] = c->end[i] - c->start[i];
	}
	minlen = min(len[LEFT], len[RIGHT]);

	chkgca = 1;
	for (i = j = 0; i < minlen; i++) {
		unless (sameline(&start[LEFT][len[LEFT] - i - 1],
			    &start[RIGHT][len[RIGHT] - i - 1])) break;
		if (chkgca && (i < len[GCA]) &&
		    sameline(&start[LEFT][len[LEFT] - i - 1],
			&start[GCA][len[GCA] - i - 1])) {
			j++;
		} else {
			chkgca = 0;
		}
	}
	/* i == number of matching lines at end */
	/* j == number of GCA matching lines at end */
	if (i == 0 || i == len[LEFT] && i == len[RIGHT]) return (0);

	splitidx[LEFT] = len[LEFT] - i;
	splitidx[RIGHT] = len[RIGHT] - i;
	splitidx[GCA] = len[GCA] - j;
	splitidx[GCAR] = len[GCAR] - j;

	split_conflict(c, splitidx);
	T_DEBUG("split %d-%d %d-%d",
	    c->start[LEFT], c->end[LEFT],
	    c->next->start[LEFT], c->next->end[LEFT]);

	return (1);
}

/*
 * Split a conflict if the same lines are deleted at the end of the
 * conflict.
 */
private int
merge_common_deletes(conflct *c)
{
	diffln	*left, *right;
	diffln	*p;
	int	splitidx[NFILES];
	int	i, j;
	int	ret = 0;
	int	cnt;

	left = unidiff(c, GCA, LEFT);
	right = unidiff(c, GCAR, RIGHT);
	/*
	 * Remove matching deletes at beginning is not implemented
	 * because it logically separates what was a modification into
	 * a delete, then add. It can also cause some bad merges to
	 * occur.
	 * ex:
	 *    <<< local
	 *    - foo
	 *    + bar
	 *    <<< remote
	 *    - foo
	 *    >>>
	 * The above shouldn't automerge. If foo & bar are different items
	 * in a list then 'bar' should stay. If 'bar' is an edit of 'foo' then
	 * 'bar' should be removed.
	 */

	/*
	 * move matching deletes at end to new region
	 */
	p = left;
	while (p->ld) p++;
	--p;
	cnt = 0;
	while (p >= left && p->c == '-') {
		++cnt;
		--p;
	}
	p = right;
	while (p->ld) p++;
	--p;
	j = 0;
	while (p >= right && p->c == '-') {
		++j;
		--p;
	}
	if (j < cnt) cnt = j;

	if (cnt > 0) {
		for (i = 0; i < NFILES; i++) {
			splitidx[i] = c->end[i] - c->start[i];
		}
		splitidx[GCA] -= cnt;
		splitidx[GCAR] -= cnt;
		split_conflict(c, splitidx);
		T_DEBUG("split %d-%d %d-%d",
		    c->start[LEFT], c->end[LEFT],
		    c->next->start[LEFT], c->next->end[LEFT]);
		ret = 1;
	}

	free(left);
	free(right);
	return (ret);
}

/*
 * Split a conflict if one side adds new code between identical
 * seq of other side and GCA.
 * Only need seq numbers for this.
 */
private int
merge_added_oneside(conflct *c)
{
	ld_t	*start[NFILES];
	int	maxlen;
	int	splitidx[NFILES];
	int	i, side;
	u32	seq;

	/* Each need to have some lines for this case to make sense */
	for (i = 0; i < NFILES; i++) {
		unless (c->end[i] != c->start[i]) return (0);
	}

	for (i = 0; i < NFILES; i++) {
		start[i] = &body[i].lines[c->start[i]];
		splitidx[i] = 0;
	}

	seq = start[GCA][0].seq;
	if ((start[LEFT][0].seq == seq) && (start[RIGHT][0].seq < seq)) {
		side = RIGHT;
	} else if ((start[RIGHT][0].seq == seq) && (start[LEFT][0].seq < seq)) {
		side = LEFT;
	} else {
		return (0);
	}
	maxlen = c->end[side] - c->start[side];

	for (i = 0; i < maxlen; i++) {
		unless (start[side][i].seq < seq) break;
	}

	splitidx[side] = i;

	split_conflict(c, splitidx);
	T_DEBUG("split %d-%d %d-%d",
	    c->start[LEFT], c->end[LEFT],
	    c->next->start[LEFT], c->next->end[LEFT]);

	return (1);
}

/*
 * Kind of a conflict rotation.  When data only exists on one side of
 * the conflict and the first line of that side matches the first
 * line in the common block after the conflict then merge the common
 * line into the conflict.  This will effectively move the common line
 * from after the conflict to before it and shift the conflict region
 * down one line.
 *
 * <<< local
 * foo
 * bar
 * <<< remote
 * >>>
 * foo
 *
 * becomes
 *
 * <<< local
 * foo
 * bar
 * foo
 * <<< remote
 * foo
 * >>>
 *
 * then common header will pop 'foo' off the top.
 */
private int
merge_with_following_gca(conflct *c)
{
	int	i, j, side;
	int	len[NFILES];
	int	ret = 0;
	int	gap;		/* number of common lines after conflict */

	for (i = 0; i < NFILES; i++) {
		len[i] = c->end[i] - c->start[i];
	}
	/*
	 * Only if only one side has lines active
	 */
	if ((len[LEFT] == 0) && (len[RIGHT] != 0)) {
		side = RIGHT;
	} else if ((len[RIGHT] == 0) && (len[LEFT] != 0)) {
		side = LEFT;
	} else {
		return (0);
	}
	if (c->next) {
		gap = c->next->start[GCA] - c->end[GCA];
	} else {
		gap = body[GCA].n - c->end[GCA];
	}
	if (len[side] < gap) gap = len[side];	/* gap = min(conf, common) */
	for (j = 0; j < gap; j++) {
		unless (sameline(
		    &body[side].lines[c->start[side] + j],
		    &body[GCA].lines[c->end[GCA] + j])) {
			break;
		}
	}
	if (j) {
		for (i = 0; i < NFILES; i++) {
			c->end[i] += j;
		}
		c->end_seq = body[GCA].lines[c->end[GCA]].seq;
		ret = 1;
	}
	return (ret);
}

private	char *
smergeData(int idx, int side, void *extra, int *len)
{
	smdiff	*data = (smdiff *)extra;
	ld_t	*dl = &data->lines[side][idx];

	if (len) *len = dl->len;
	return (dl->line);
}
