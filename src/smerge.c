#include "system.h"
#include "sccs.h"
#include <string.h>

typedef struct conflict	conflict_t;
typedef struct ld	ld_t;
typedef struct diffline	diffline_t;
typedef struct file	file_t;

private	char	*getrevs(int i, char *str);
private int	do_weave_merge(u32 start, u32 end);
private conflict_t	*find_conflicts(void);
private void	merge_conflicts(conflict_t *head);
private void	usage(void);
private int	resolve_conflict(conflict_t *curr);
private diffline_t	*unidiff(conflict_t *curr, int left, int right);
private void	show_examples(void);
private	int	parse_range(char *range, u32 *start, u32 *end);
private int	sameline(ld_t *left, ld_t *right);
private	void	file_init(file_t *f, char *filename);
private	void	file_free(file_t *f);

private	void	user_conflict(conflict_t *curr);
private	void	user_conflict_fdiff(conflict_t *curr);

/* automerge functions */
private	void	enable_mergefcns(char *list, int enable);
private	void	mergefcns_help(void);

private int	merge_same_changes(conflict_t *r);
private int	merge_only_one(conflict_t *r);
private int	merge_content(conflict_t *r);
private int	merge_common_header(conflict_t *r);
private int	merge_common_footer(conflict_t *r);
private	int	merge_common_deletes(conflict_t *r);

enum {
	MODE_GCA,
	MODE_2WAY,
	MODE_3WAY,
	MODE_NEWONLY
};

enum {
	LEFT,
	GCA,
	RIGHT
};

/* ld = line data  (Information to save about each line in the file) */
struct ld {
	char	*line;		/* Pointer to data in line */
	int	len;		/* length of line */
	u32	seq;		/* seq number for line */
};

struct diffline {
	ld_t	*ld;
	char	c;		/* character at start of line */
};

struct file {
	MMAP	*m;
	ld_t	*lines;
	int	n;
};


private	char	*revs[3];
private	file_t	body[3];
private	char	*file;
private	int	mode;
private	int	show_seq;
private	int	fdiff;
private	char	*anno = 0;
private	int	tostdout = 0;
private	FILE	*outf = 0;

int
smerge_main(int ac, char **av)
{
	int	c;
	int	i;
	char	bodytmp[3][MAXPATH];
	u32	start = 0, end = ~0;
	int	ret = 0;
	int	status;
	int	do_diff3 = 0;
	int	identical = 0;

	mode = MODE_3WAY;
	while ((c = getopt(ac, av, "234A:a:defghI:npr:s")) != -1) {
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
#ifdef	DIFF_MERGE
		    case 'd':	do_diff3 = 1; break;
#endif
		    case 'p':	tostdout = 1; break;
		    case 'f': /* fdiff output mode */
			fdiff = 1;
			tostdout = 1;
			break;
		    case 'e': /* show examples */
			show_examples();
			return(2);
		    case 'r': /* show output in the range <r> */
			if (parse_range(optarg, &start, &end)) {
				usage();
				return (2);
			}
			break;
		    case 's': /* show sequence numbers */
			show_seq = 1;
			break;
		    case 'h': /* help */
		    default:
			usage();
			return (2);
		}
	}
	if (ac - optind != 4) {
		usage();
		return (2);
	}
	if (fdiff && mode == MODE_3WAY) mode = MODE_GCA;

	file = av[optind + 3];
	for (i = 0; i < 3; i++) {
		char	*cmd;
		revs[i] = getrevs(i, av[optind + i]);
		gettemp(bodytmp[i], "smerge");
		cmd = aprintf("bk get %s%s -qkpO -r%s %s > %s",
		    anno ? "-a" : "",
		    anno ? anno : "",
		    revs[i],
		    file, bodytmp[i]);
		status = system(cmd);
		if (status) {
			fprintf(stderr, "Fetch of revision %s failed!\n",
			    revs[i]);
			ret = 2;
			goto err;
		}
		free(cmd);
		file_init(&body[i], bodytmp[i]);
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
			unlink(bodytmp[RIGHT]);
			body[RIGHT] = body[LEFT];
			identical = 1;
		}
	}

	if (tostdout) {
		outf = stdout;
	} else {
		outf = fopen(file, "w");
		unless (outf) {
			fprintf(stderr, "Can't open %s for writing\n", file);
			ret = 2;
			goto err;
		}
	}
	if (do_diff3) {
#ifdef	DIFF_MERGE
		ret = do_diff_merge(start, end);
#endif
	} else {
		ret = do_weave_merge(start, end);
	}
 err:
	if (outf) fclose(outf);
	for (i = 0; i < 3; i++) {
		unless (i == RIGHT && identical) {
			file_free(&body[i]);
			unlink(bodytmp[i]);
		}
	}
	return (ret);
}

/*
 * Open a file and populate the file_t structure.
 */
private	void
file_init(file_t *f, char *filename)
{
	char	*p;
	char	*end;
	int	l;

	f->m = mopen(filename, "r");
	unless (f->m) {
		fprintf(stderr, "Open of %s failed!\n", filename);
		exit(2);
	}

	end = f->m->end;

	/*
	 * Let's assume there are 20 chars per line (matches my source).
	 */
	f->n = f->m->size / 20;
	if (!f->n) f->n = 20;
	f->lines = malloc(f->n * sizeof(ld_t));

	l = 0;
	p = f->m->where;
	while (p) {
		int	len;

		if (l+1 >= f->n) {
			ld_t	*tmp;

			f->n *= 2;
			tmp = malloc(f->n * sizeof(ld_t));
			assert(tmp);
			memcpy(tmp, f->lines, l * sizeof(ld_t));
			free(f->lines);
			f->lines = tmp;
		}
		f->lines[l].seq = strtoul(p, &p, 10);
		f->lines[l].line = ++p;
		len = 0;
		while (p < end && *p++ != '\n') ++len;
		f->lines[l].len = len + 1;
		if (p == end) p = 0;
		++l;
	}
	f->lines[l].seq = ~0;
	f->lines[l].line = end;
	f->lines[l].len = 0;
	f->n = l;
}

private	void
file_free(file_t *f)
{
	free(f->lines);
	f->lines = 0;
	mclose(f->m);
	f->m = 0;
}

/*
 * convert the version string for the input fine into arguments that
 * can be passed to get.
 *   from 1.7+2.1,2.2-1.6
 *     to 1.7 -i2.1,2.2 -x1.6
 */
private char *
getrevs(int i, char *str)
{
	static	const	char	*envnames[] = {
		"MERGE_REVS_LOCAL",
		"MERGE_REVS_GCA",
		"MERGE_REVS_REMOTE"
	};
	char	*ret = 0;
	char	*verstr;
	char	*v;
	char	*p;
	int	len;

	verstr = getenv(envnames[i]);
	unless (verstr) verstr = str;
	/* Allocate buffer for return string */
	ret = malloc(strlen(verstr) + 16);
	p = ret;
	v = verstr;
	len = strspn(v, "0123456789.");
	unless (len) goto err;
	strncpy(p, v, len);
	p += len;
	v += len;
	while (*v == '+' || *v == '-') {
		p += sprintf(p, " -%c", (*v == '+') ? 'i' : 'x');
		++v;
		len = strspn(v, "0123456789.,");
		unless (len) goto err;
		strncpy(p, v, len);
		p += len;
		v += len;
	}
	if (*v) {
err:		fprintf(stderr, "Unable to parse version number: %s\n", verstr);
		exit(2);
	}

	*p = 0;
	assert(strlen(ret) < strlen(verstr) + 16);
	return (ret);
}

private void
usage(void)
{
	system("bk help -s smerge");
	mergefcns_help();
}

/*
 * print a line from a ld_t structure and optional put the
 * first_char argument at the beginngin.
 * Handles the -s (show_seq) knob.
 */
private void
printline(ld_t *ld, char first_char)
{
	char	*p = ld->line;
	char	*end = ld->line + ld->len;

	if (fdiff && !first_char) first_char = ' ';
	if (first_char) putc(first_char, outf);
	if (show_seq) fprintf(outf, "%6d\t", ld->seq);
	while (p < end) putc(*p++, outf);
}

struct conflict {
	int	start[3];	/* lines at start of conflict */
	int	end[3];		/* lines after end of conflict */
	int	start_seq;	/* seq of lines before 'start' */
	int	end_seq;	/* seq of lines at 'end' */
	ld_t	*merged;	/* result of an automerge */
	conflict_t	*prev, *next;
	/* XXX need list of automerge algros that touched this block */
};

/*
 * Print a series of lines on one side of a conflict
 */
private void
printlines(conflict_t *curr, int side)
{
	int	len = curr->end[side] - curr->start[side];
	int	i;
	int	idx = curr->start[side];

	for (i = 0; i < len; i++) {
		printline(&body[side].lines[idx++], 0);
	}
}

private int
do_weave_merge(u32 start, u32 end)
{
	conflict_t	*clist;
	conflict_t	*curr;
	int	mk[3];
	int	i;
	int	len;
	int	ret = 0;

	/* create a linked list of all conflict regions */
	clist = find_conflicts();

	/* Merge any that *might* be incorrectly marked */
	merge_conflicts(clist);

	mk[0] = mk[1] = mk[2] = 0;
	curr = clist;
	while (curr) {
		conflict_t	*tmp;

		/* print lines up the the next conflict */
		len = curr->start[GCA] - mk[GCA];

		/* should be the same number of lines */
		for (i = 0; i < 3; i++) assert(len == curr->start[i] - mk[i]);

		for (i = 0; i < len; i++) {
			ld_t	*gcaline = &body[GCA].lines[mk[GCA] + i];

			/* All common lines should match exactly */
			assert(gcaline->seq ==
			    body[LEFT].lines[mk[LEFT]+i].seq &&
			    gcaline->seq ==
			    body[RIGHT].lines[mk[RIGHT]+i].seq);

			if (gcaline->seq <= start) continue;
			if (gcaline->seq >= end) return (ret);
			printline(gcaline, 0);
		}
		if (curr->start_seq >= start) {
			/* handle conflict */
			if (resolve_conflict(curr)) ret = 1;
		}
		for (i = 0; i < 3; i++) mk[i] = curr->end[i];

		tmp = curr;
		curr = curr->next;
		free(tmp);
	}
	/* Handle any lines after the last conflict */

	len = body[GCA].n - mk[GCA];
	for (i = 0; i < 3; i++) assert(len == body[i].n - mk[i]);

	for (i = 0; i < len; i++) {
		ld_t	*gcaline = &body[GCA].lines[mk[GCA] + i];

		assert(gcaline->seq ==
		    body[LEFT].lines[mk[LEFT]+i].seq &&
		    gcaline->seq ==
		    body[RIGHT].lines[mk[RIGHT]+i].seq);
		if (gcaline->seq <= start) continue;
		if (gcaline->seq >= end) return (ret);
		printline(gcaline, 0);
	}
	return (ret);
}

/*
 * Find all conflict sections in the file based only of sequence numbers.
 * Return a linked list of conflict regions.
 */
private conflict_t *
find_conflicts(void)
{
	int	i, j;
	int	mk[3];
	ld_t	*lines[3];
	conflict_t	*list = 0;
	conflict_t	*end;
	conflict_t	*p;

	for (i = 0; i < 3; i++) mk[i] = 0;

	while (1) {
		int	minlen;
		int	at_end;
		u32	seq[3];

		/* Assume everything matches here */
		minlen = INT_MAX;
		for (i = 0; i < 3; i++) {
			lines[i] = &body[i].lines[mk[i]];
			if (minlen > body[i].n - mk[i]) {
				minlen = body[i].n - mk[i];
			}
		}
		j = 0;
		while (j < minlen &&
		    lines[0][j].seq == lines[1][j].seq &&
		    lines[0][j].seq == lines[2][j].seq) {
			j++;
		}
		/* at start of conflict or end of file */
		at_end = 1;
		for (i = 0; i < 3; i++) {
			mk[i] += j;
			if (mk[i] < body[i].n) at_end = 0;
		}
		if (at_end) break;

		new(p);

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
		for (i = 0; i < 3; i++) {
			p->start[i] = mk[i];
			lines[i] = body[i].lines;
		}

		/* find end */
		for (i = 0; i < 3; i++) {
			seq[i] = lines[i][mk[i]].seq;
		}
		do {
			u32	maxseq = 0;
			at_end = 1;
			for (i = 0; i < 3; i++) {
				if (seq[i] > maxseq) maxseq = seq[i];
			}
			for (i = 0; i < 3; i++) {
				while (seq[i] < maxseq) {
					++mk[i];
					seq[i] = lines[i][mk[i]].seq;
					at_end = 0;
				}
				if (seq[i] > maxseq) maxseq = seq[i];
			}
		} while (!at_end);

		assert(seq[0] == seq[1] && seq[0] == seq[2]);
		for (i = 0; i < 3; i++) {
			p->end[i] = mk[i];
		}
		p->end_seq = seq[0];
	}
	return (list);
}

/*
 * Return true if a line appears anywhere inside a conflict.
 */
private int
contains_line(conflict_t *c, ld_t *line)
{
	int	i, j;

	for (i = 0; i < 3; i++) {
		for (j = c->start[i]; j < c->end[i]; j++) {
			if (sameline(&body[i].lines[j], line)) return (1);
		}
	}
	return (0);
}

/*
 * Walk list of conflicts and merge any that are only
 * seperated by one line that exists in one of the conflict
 * regions.
 * XXX should handle N
 */
private void
merge_conflicts(conflict_t *head)
{
	while (head) {
		if (head->next && head->next->start[0] - head->end[0] == 1) {
			ld_t	*line = &body[0].lines[head->end[0]];
			
			if (contains_line(head, line) ||
			    contains_line(head->next, line)) {
				conflict_t	*tmp;
				int	i;

				/* merge the two regions */
				tmp = head->next;
				for (i = 0; i < 3; i++) {
					head->end[i] = tmp->end[i];
				}
				head->end_seq = tmp->end_seq;
				head->next = tmp->next;
				if (tmp->next) tmp->next->prev = head;
				free(tmp);
				/*
				 * don't update head so we try this
				 * conflict again.
				 */
			} else {
				head = head->next;
			}
		} else {
			head = head->next;
		}
	}
}

#ifdef	DIFF_MERGE

private char *
strdup_afterchar(const char *s, int c)
{
        char    *p;
        char    *ret;

        if (p = strchr(s, c)) {
                ret = malloc(p - s + 2);
                p = ret;
                while (*s != c)
                        *p++ = *s++;
		*p++ = c;
                *p = 0;
        } else {
                ret = strdup(s);
        }
        return (ret);
}

/*
 * The structure for a class that creates and then
 * walks the diffs of two files and returns the
 * lines for the right file as requested.  All struct members
 * should be considered private and all operations are done
 * with the member functions below.
 */
typedef struct diffwalk diffwalk_t;
struct diffwalk {
	char	**lines;
	FILE	*diff;
	int	rline;
	MMAP	*right;
	int	offset;		/* difference between gca lineno and right */
	int	start, end;
	struct {
		int	gcastart, gcaend;
		int	lines;
	} cmd;
};

/*
 * Internal function.
 * Read nead command from diff stream
 */
private void
diffwalk_readcmd(diffwalk_t *dw)
{
	char	buf[MAXLINE];
	char	*p;
	int	start, end;
	char	cmd;

	unless (fgets(buf, sizeof(buf), dw->diff)) {
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
			fgets(buf, sizeof(buf), dw->diff);
			assert(buf[0] == '<');
			assert(buf[1] == ' ');
		}
		if (cmd == 'c') {
			fgets(buf, sizeof(buf), dw->diff);
			assert(buf[0] == '-');
		}
	} else {
		assert(dw->cmd.gcaend == dw->cmd.gcastart);
		dw->cmd.gcaend = ++dw->cmd.gcastart;
	}
}

/*
 * create a new diffwalk struct from two filenames
 */
private diffwalk_t *
diffwalk_new(char *left, char *right)
{
	diffwalk_t	*dw;
	char	*cmd;

	new(dw);
	cmd = aprintf("bk mydiff -a'\t' %s %s", left, right);
	dw->diff = popen(cmd, "r");
	free(cmd);

	dw->right = mopen(right, "r");
	dw->rline = 1;
	diffwalk_readcmd(dw);
	return (dw);
}

/*
 * return the gca linenumber of the start of the next diff in the
 * file.
 */
private int
diffwalk_nextdiff(diffwalk_t *dw)
{
	return (dw->cmd.gcastart);
}

/*
 * Extend the current diff region to at least contain the given
 * GCA line number.
 */
private void
diffwalk_extend(diffwalk_t *dw, int gcalineno)
{
	char	*line;

	if (gcalineno < dw->end) return;

	while (gcalineno > dw->end && dw->end < dw->cmd.gcastart) {
		line = mnext(dw->right);
		dw->lines = addLine(dw->lines, strdup_afterchar(line, '\n'));
		++dw->rline;
		++dw->end;
	}
	if (gcalineno >= dw->cmd.gcastart) {
		char	buf[MAXLINE];
		dw->offset += dw->cmd.lines -
			(dw->cmd.gcaend - dw->cmd.gcastart);
		while (dw->cmd.lines--) {
			fgets(buf, sizeof(buf), dw->diff);
			assert(buf[0] == '>');
			assert(buf[1] == ' ');
			line = strdup_afterchar(buf+2, '\n');
			dw->lines = addLine(dw->lines, line);
		}
		dw->end = dw->cmd.gcaend;
		diffwalk_readcmd(dw);
		diffwalk_extend(dw, gcalineno);
	}
}

/*
 * Start a new diff at the given line number in the GCA file.
 */
private void
diffwalk_start(diffwalk_t *dw, int gcalineno)
{
	assert(gcalineno <= dw->cmd.gcastart);
	while (dw->rline < gcalineno + dw->offset) {
		char	*line;
		line = mnext(dw->right);
		++dw->rline;
	}
	dw->start = dw->end = gcalineno;
	if (gcalineno == dw->cmd.gcastart) diffwalk_extend(dw, dw->cmd.gcaend);
}

/*
 * Return the ending GCA line number for the current diff region
 */
private int
diffwalk_end(diffwalk_t *dw)
{
	return (dw->end);
}

/*
 * Return the lines on the right of the current diff region
 * and prepare for a new diff
 */
private char **
diffwalk_return(diffwalk_t *dw)
{
	char	**ret = dw->lines;
	dw->lines = 0;
	return (ret);
}

/*
 * free a diffwalk struct
 */
private void
diffwalk_free(diffwalk_t *dw)
{
	mclose(dw->right);
	assert(dw->lines == 0);
	free(dw);
}

private int
do_diff_merge(int start, int end)
{
	int	i;
	char	*cmd;
	char	files[3][MAXPATH];
	char	**lines[3];
	diffwalk_t	*ldiff;
	diffwalk_t	*rdiff;
	MMAP	*gcafile;
	int	gcaline;
	int	ret = 0;

	for (i = 0; i < 3; i++) {
		int	status;
		gettemp(files[i], "smerge");
		cmd = aprintf("bk get %s%s -qkpO -r%s %s > %s",
		    anno ? "-a" : "",
		    anno ? anno : "",
		    revs[i],
		    file, files[i]);
		status = system(cmd);
		if (status) {
			fprintf(stderr, "Fetch of revision %s failed!\n",
			    revs[i]);
			exit(2);
		}
		free(cmd);
		lines[i] = 0;
	}
	gcafile = mopen(files[GCA], "r");
	ldiff = diffwalk_new(files[GCA], files[LEFT]);
	rdiff = diffwalk_new(files[GCA], files[RIGHT]);

	gcaline = 1;
	while (1) {
		int	start, end;
		int	tmp;

		start = diffwalk_nextdiff(ldiff);
		tmp = diffwalk_nextdiff(rdiff);
		if (tmp < start) start = tmp;

		while (gcaline < start) {
			char	*line = mnext(gcafile);

			unless (line) break;
			line = strdup_afterchar(line, '\n');
			printline(line, 0);
			free(line);
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
		while (gcaline < end) {
			char	*line = strdup_afterchar(mnext(gcafile), '\n');
			lines[GCA] = addLine(lines[GCA], line);
			++gcaline;
		}
		lines[LEFT] = diffwalk_return(ldiff);
		lines[RIGHT] = diffwalk_return(rdiff);
		if (resolve_conflict(lines, start, end)) ret = 1;
		for (i = 0; i < 3; i++) lines[i] = 0;
	}

	diffwalk_free(ldiff);
	diffwalk_free(rdiff);
	mclose(gcafile);
	for (i = 0; i < 3; i++) {
		unlink(files[i]);
	}
	return (ret);
}

#endif	/* DIFF_MERGE */

#if 0
private char *
fagets(FILE *fh)
{
	int	size;
	int	len;
	char	*ret;

	size = 128;
	ret = malloc(size);
	len = 0;

	/*
	 * Read a line from the file with NO limit on linesize
	 */
	while (1) {
		if (!fgets(ret + len, size - len, fh)) {
			if (len) return (ret);
			free(ret);
			return 0;
		}
		len += strlen(ret + len);
		if (ret[len-1] == '\n') break;  /* stop on complete line */
		size <<= 1;
#ifdef	HAVE_REALLOC
		ret = realloc(ret, size);
#else
		{
			/* cheap realloc()  bah! */
			char	*new;
			new = malloc(size);
			strcpy(new, ret);
			free(ret);
			ret = new;
		}
#endif
	}
	return (ret);
}
#endif

/*
 * Define of set of autoresolve function for processing a
 * conflict. Each function takes conflict regresion and looks at it to
 * determine if a merge is possible.  If a merge is possible then the
 * ->merged array is initialized with the lines in the merged output
 * and 1 is returned.  The conflict can be split into to adjectent
 * conflicts that exactly cover the original conflict region.  (use
 * split_conflict() to make this split) If a split occurs then a 1 is
 * returns.  If the function changes nothing then 0 is returned.
 *
 * The numbers used below, once shipped, must always mean the same thing.
 * If you evolve this code, use new numbers.
 */
struct mergefcns {
	char	*name;		/* "name" of function for commandline */
	int	enable;		/* is this function enabled by default? */
	int	(*fcn)(conflict_t *r);
	char	*help;
} mergefcns[] = {
	{"1",	1, merge_same_changes,
	"Merge identical changes made by both sides"},
	{"2",	1, merge_only_one,
	"Merge when only one side changes"},
	{"3",	1, merge_content,
	"Merge adjacent non-overlapping modifications on both sides"},
	{"4",	1, merge_common_header,
	"Merge identical changes at the start of a conflict"},
	{"5",	1, merge_common_footer,
	"Merge identical changes at the end of a conflict"},
	{"6",	1, merge_common_deletes,
	"Merge identical deletions made by both sides"},
};
#define	N_MERGEFCNS (sizeof(mergefcns)/sizeof(struct mergefcns))

/*
 * Called by the command line parses to enable/disable different functions.
 * This function takes a comma or space seperated list of function names
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
"The following is a list of merge algorthms which may be enabled or disabled\n\
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
resolve_conflict(conflict_t *curr)
{
	int	i;
	int	ret = 0;
	int	changed;

	/*
	 * Try every enabled automerge function on the current conflict
	 * until an automerge occurs or no more changes are possible.
	 */
	do {
		changed = 0;
		for (i = 0; i < N_MERGEFCNS; i++) {
			unless (mergefcns[i].enable) continue;
			changed |= mergefcns[i].fcn(curr);
			if (curr->merged) break;
		}
	} while (changed && !curr->merged);
	if (curr->merged) {
		ld_t	*p;
		/* This region was automerged */
		if (fdiff) fprintf(outf, "M %d\n", curr->start_seq);
		for (p = curr->merged; p->line; p++) {
			printline(p, 0);
		}
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
		fputs("E", outf);
		assert(curr->end_seq);
		fprintf(outf, " %d", curr->end_seq);
		fputc('\n', outf);
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
samedata(conflict_t *c, int l, int r)
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
user_conflict(conflict_t *curr)
{
	int	i;
	diffline_t	*diffs;
	int	sameleft;
	int	sameright;

	switch (mode) {
	    case MODE_GCA:
		sameleft = samedata(curr, GCA, LEFT);
		sameright = samedata(curr, GCA, RIGHT);
		unless (sameleft && !sameright) {
			fprintf(outf, "<<<<<<< local %s %s vs %s\n",
			    file, revs[GCA], revs[LEFT]);
			diffs = unidiff(curr, GCA, LEFT);
			for (i = 0; diffs[i].ld; i++) {
				printline(diffs[i].ld, diffs[i].c);
			}
			free(diffs);
		}
		unless (sameright && !sameleft) {
			fprintf(outf, "<<<<<<< remote %s %s vs %s\n",
			    file, revs[GCA], revs[RIGHT]);
			diffs = unidiff(curr, GCA, RIGHT);
			for (i = 0; diffs[i].ld; i++) {
				printline(diffs[i].ld, diffs[i].c);
			}
			free(diffs);
		}
		fprintf(outf, ">>>>>>>\n");
		break;
	    case MODE_2WAY:
/* #define MATCH_DIFF3 */
#ifdef MATCH_DIFF3
		fprintf(outf, "<<<<<<< L\n");
		printlines(curr, LEFT);
		fprintf(outf, "=======\n");
		printlines(curr, RIGHT);
		fprintf(outf, ">>>>>>> R\n");
#else
		fprintf(outf, "<<<<<<< local %s %s\n", file, revs[LEFT]);
		printlines(curr, LEFT);
		fprintf(outf, "<<<<<<< remote %s %s\n", file, revs[RIGHT]);
		printlines(curr, RIGHT);
		fprintf(outf, ">>>>>>>\n");
#endif
		break;
	    case MODE_3WAY:
		fprintf(outf, "<<<<<<< gca %s %s\n", file, revs[GCA]);
		printlines(curr, GCA);
		fprintf(outf, "<<<<<<< local %s %s\n", file, revs[LEFT]);
		printlines(curr, LEFT);
		fprintf(outf, "<<<<<<< remote %s %s\n", file, revs[RIGHT]);
		printlines(curr, RIGHT);
		fprintf(outf, ">>>>>>>\n");
		break;
	    case MODE_NEWONLY:
		sameleft = samedata(curr, GCA, LEFT);
		sameright = samedata(curr, GCA, RIGHT);
		unless (sameleft && !sameright) {
			fprintf(outf, "<<<<<<< local %s %s vs %s\n",
			    file, revs[GCA], revs[LEFT]);
			diffs = unidiff(curr, GCA, LEFT);
			for (i = 0; diffs[i].ld; i++) {
				if (diffs[i].c != '-') {
					printline(diffs[i].ld, diffs[i].c);
				}
			}
			free(diffs);
		}
		unless (sameright && !sameleft) {
			fprintf(outf, "<<<<<<< remote %s %s vs %s\n",
			    file, revs[GCA], revs[RIGHT]);
			diffs = unidiff(curr, GCA, RIGHT);
			for (i = 0; diffs[i].ld; i++) {
				if (diffs[i].c != '-') {
					printline(diffs[i].ld, diffs[i].c);
				}
			}
			free(diffs);
		}
		fprintf(outf, ">>>>>>>\n");
		break;
	}
}

/*
 * Print a conflict in fdiff format
 */
private void
user_conflict_fdiff(conflict_t *c)
{
	int	i, j;
	diffline_t	*left, *right;
	diffline_t	*diffs;
	diffline_t	*rightbuf;
	diffline_t	*lp, *rp;
	ld_t	*p, *end;
	ld_t	blankline;

	switch (mode) {
	    case MODE_GCA:
		left = unidiff(c, GCA, LEFT);
		right = unidiff(c, GCA, RIGHT);
		break;
	    case MODE_2WAY:
		/* fake a diff output */
		left = malloc((c->end[LEFT] - c->start[LEFT] + 1) *
		    sizeof(diffline_t));
		p = &body[LEFT].lines[c->start[LEFT]];
		end = &body[LEFT].lines[c->end[LEFT]];
		i = 0;
		while (p < end) {
			left[i].ld = p++;
			left[i].c = ' ';
			i++;
		}
		left[i].ld = 0;
		right = malloc((c->end[RIGHT] - c->start[RIGHT] + 1) *
		    sizeof(diffline_t));
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
		left = malloc((i+1) * sizeof(diffline_t));
		j = 0;
		for (i = 0; diffs[i].ld; i++) {
			if (diffs[i].c != '-') {
				left[j++] = diffs[i];
			}
		}
		left[j].ld = 0;
		free(diffs);
		diffs = unidiff(c, GCA, RIGHT);
		for (i = 0; diffs[i].ld; i++);
		right = malloc((i+1) * sizeof(diffline_t));
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
	 * Need to allocate rightbuf to hold data that will be printed
	 * on the right.
	 * The length can never be longer that right + left + 1
	 */
	i = 0;
	for (diffs = left; diffs->ld; diffs++) i++;
	for (diffs = right; diffs->ld; diffs++) i++;
	rightbuf = malloc((i+1) * sizeof(diffline_t));

	blankline.line = "\n";
	blankline.len = 1;
	blankline.seq = 0;

	fputs("L", outf);
	unless (c->merged) fprintf(outf, " %d", c->start_seq);
	fputs("\n", outf);
	lp = left;
	rp = right;
	i = 0;
	while (lp->ld || rp->ld) {
		if (!rp->ld || lp->ld && lp->ld->seq < rp->ld->seq) {
			/* line on left */
			printline(lp->ld, lp->c);
			rightbuf[i].ld = &blankline;
			rightbuf[i].c = 's';
			++lp;
		} else if (!lp->ld || rp->ld->seq < lp->ld->seq) {
			/* line on right */
			printline(&blankline, 's');
			rightbuf[i].ld = rp->ld;
			rightbuf[i].c = rp->c;
			++rp;
		} else {
			/* matching line */
			printline(lp->ld, lp->c);
			rightbuf[i].ld = rp->ld;
			rightbuf[i].c = rp->c;
			++lp;
			++rp;
		}
		++i;
	}

	fputs("R\n", outf);
	for (j = 0; j < i; j++) printline(rightbuf[j].ld, rightbuf[j].c);

	free(left);
	free(right);
	free(rightbuf);
}

/*
 * do a diff of two sides of a conflict region and return
 * add array of diffline_t structures.  The diff is purely based on
 * sequence numbers and is very quick.
 * The returned array is allocated with malloc and needs to be freed by
 * the user and the end of the array is marked by a ld pointer set to null.
 * Typical usage:
 *
 *    diffs = unidiff(curr, GCA, LEFT);
 *    for (i = 0; diffs[i].ld; i++) {
 *	     printline(diffs[i].ld, diffs[i].c);
 *    }
 *    free(diffs);
 */
private diffline_t *
unidiff(conflict_t *curr, int left, int right)
{
	ld_t	*llines, *rlines;
	ld_t	*el, *er;
	diffline_t	*out;
	diffline_t	*p;
	diffline_t	*del, *delp;
	diffline_t	*ins, *insp;

	llines = &body[left].lines[curr->start[left]];
	rlines = &body[right].lines[curr->start[right]];
	el = &body[left].lines[curr->end[left]];
	er = &body[right].lines[curr->end[right]];

	/* allocate the maximum space that might be needed. */
	p = out = malloc((el - llines + er - rlines + 1) * sizeof(diffline_t));
	delp = del = malloc((el - llines) * sizeof(diffline_t));
	insp = ins = malloc((er - rlines) * sizeof(diffline_t));

	while (llines < el || rlines < er) {
		if (rlines == er || llines < el && llines->seq < rlines->seq) {
			delp->ld = llines++;
			delp->c = '-';
			++delp;
		} else if (llines == el || rlines->seq < llines->seq) {
			insp->ld = rlines++;
			insp->c = '+';
			++insp;
		} else {
			assert(llines->len == rlines->len);
			if (delp > del) {
				int	cnt = delp - del;
				memcpy(p, del, cnt * sizeof(diffline_t));
				p += cnt;
				delp = del;
			}
			if (insp > ins) {
				int	cnt = insp - ins;
				memcpy(p, ins, cnt * sizeof(diffline_t));
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
		memcpy(p, del, cnt * sizeof(diffline_t));
		p += cnt;
		delp = del;
	}
	free(del);
	if (insp > ins) {
		int	cnt = insp - ins;
		memcpy(p, ins, cnt * sizeof(diffline_t));
		p += cnt;
		insp = ins;
	}
	free(ins);
	p->ld = 0;
	p->c = 0;

	return (out);
}

private void
show_examples(void)
{
	fputs(
"Summary of bk smerge output formats\n\
default		(3 way format (shows gca))\n\
    <<<<<<< gca slib.c 1.642.1.6\n\
    		  sc = sccs_init(file, INIT_NOCKSUM|INIT_SAVEPROJ, s->proj);\n\
    		  assert(sc->tree);\n\
    		  sccs_sdelta(sc, sc->tree, file);\n\
    <<<<<<< local slib.c 1.645\n\
    		  sc = sccs_init(file, INIT_NOCKSUM|INIT_SAVEPROJ, s->proj);\n\
    		  assert(HASGRAPH(sc));\n\
    		  sccs_sdelta(sc, sccs_ino(sc), file);\n\
    <<<<<<< remote slib.c 1.642.2.1\n\
    		  sc = sccs_init(file, INIT_NOCKSUM|INIT_SAVEPROJ, p);\n\
    		  assert(sc->tree);\n\
    		  sccs_sdelta(sc, sc->tree, file);\n\
    >>>>>>>\n\
\n\
-g	(Shows local and remove files as a diff from the GCA)\n\
    <<<<<<< local slib.c 1.642.1.6 vs 1.645\n\
    		  sc = sccs_init(file, INIT_NOCKSUM|INIT_SAVEPROJ, s->proj);\n\
    -             assert(sc->tree);\n\
    -             sccs_sdelta(sc, sc->tree, file);\n\
    +             assert(HASGRAPH(sc));\n\
    +             sccs_sdelta(sc, sccs_ino(sc), file);\n\
    <<<<<<< remote slib.c 1.642.1.6 vs 1.642.2.1\n\
    -             sc = sccs_init(file, INIT_NOCKSUM|INIT_SAVEPROJ, s->proj);\n\
    +             sc = sccs_init(file, INIT_NOCKSUM|INIT_SAVEPROJ, p);\n\
    		  assert(sc->tree);\n\
    		  sccs_sdelta(sc, sc->tree, file);\n\
    >>>>>>>\n\
", stdout);
	fputs(
"-2	(2 way format (like diff3))\n\
    <<<<<<< local slib.c 1.645\n\
    		  sc = sccs_init(file, INIT_NOCKSUM|INIT_SAVEPROJ, s->proj);\n\
    		  assert(HASGRAPH(sc));\n\
    		  sccs_sdelta(sc, sccs_ino(sc), file);\n\
    <<<<<<< remote slib.c 1.642.2.1\n\
    		  sc = sccs_init(file, INIT_NOCKSUM|INIT_SAVEPROJ, p);\n\
    		  assert(sc->tree);\n\
    		  sccs_sdelta(sc, sc->tree, file);\n\
    >>>>>>>\n\
\n\
-n	(newonly (like -2 except marks added lines))\n\
    <<<<<<< local slib.c 1.642.1.6 vs 1.645\n\
    		  sc = sccs_init(file, INIT_NOCKSUM|INIT_SAVEPROJ, s->proj);\n\
    +             assert(HASGRAPH(sc));\n\
    +             sccs_sdelta(sc, sccs_ino(sc), file);\n\
    <<<<<<< remote slib.c 1.642.1.6 vs 1.642.2.1\n\
    +             sc = sccs_init(file, INIT_NOCKSUM|INIT_SAVEPROJ, p);\n\
    		  assert(sc->tree);\n\
    		  sccs_sdelta(sc, sc->tree, file);\n\
    >>>>>>>\n\
", stdout);
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
merge_same_changes(conflict_t *c)
{
	int	i;

	if (samedata(c, LEFT, RIGHT)) {
		int	len = c->end[LEFT] - c->start[LEFT];
		c->merged = malloc((len + 1) * sizeof(ld_t));
		for (i = 0; i < len; i++) {
			c->merged[i] = body[LEFT].lines[c->start[LEFT] + i];
		}
		c->merged[i].line = 0;
		return (1);
	}
	return (0);
}

/*
 * Only one side make changes
 */
private int
merge_only_one(conflict_t *c)
{
	int	i;

	if (samedata(c, GCA, LEFT)) {
		int	len = c->end[RIGHT] - c->start[RIGHT];
		c->merged = malloc((len + 1) * sizeof(ld_t));
		for (i = 0; i < len; i++) {
			c->merged[i] = body[RIGHT].lines[c->start[RIGHT] + i];
		}
		c->merged[i].line = 0;
		return (1);
	}
	if (samedata(c, GCA, RIGHT)) {
		int	len = c->end[LEFT] - c->start[LEFT];
		c->merged = malloc((len + 1) * sizeof(ld_t));
		for (i = 0; i < len; i++) {
			c->merged[i] = body[LEFT].lines[c->start[LEFT] + i];
		}
		c->merged[i].line = 0;
		return (1);
	}
	return (0);
}

/*
 * Look at a diff and return the list of sequence numbers that were
 * modified in that diff.
 * Return NULL if the diff doesn't match the 'modification' pattern.
 */
private u32 *
lines_modified(diffline_t *diff)
{
	u32	*out = 0;
	u32	*op;
	int	i;
	int	saw_deletes;
	int	saw_adds;
	diffline_t	*p;

	/* count number of deleted lines */
	i = 0;
	for (p = diff; p->ld; ++p) if (p->c == '-') ++i;
	out = malloc((i+1) * sizeof(u32));

	/* same seq of deleted lines and require right pattern */
	op = out;
	saw_adds = saw_deletes = 0;
	for (p = diff; p->ld; ++p) {
		switch (p->c) {
		    case ' ':
			saw_adds = saw_deletes = 0;
			break;
		    case '-':
			if (saw_adds) goto bad;
			saw_deletes = 1;
			*op++ = p->ld->seq;
			break;
		    case '+':
			unless (saw_deletes) goto bad;
			saw_adds = 1;
			break;
		}
	}
	*op = 0;
	return (out);
 bad:
	free(out);
	return (0);
}

/*
 * Take a diff and a list of sequence numbers and determine
 * if those numbers are unmodified in the diff.
 */
private int
are_unmodified(diffline_t *diff, u32 *lines)
{
	int	lcnt;
	u32	s;

	lcnt = 0;
	s = lines[lcnt];
	while (diff->ld) {
		if (*lines < diff->ld->seq) return (0);
		if (*lines == diff->ld->seq) {
			if (diff->c != ' ') return (0);
			++lines;
			unless (*lines) return (1);
		}
		++diff;
	}
	return (0);
}

/*
 * Determine if both sides made modifications to non-overlaping sections.
 * A modification is a delete of 1 or more lines follow by an addition
 * of 0 or more lines.
 */
private int
merge_content(conflict_t *c)
{
	diffline_t	*left, *right;
	diffline_t	*lp, *rp;
	u32	*modified;
	int	i;
	int	ret = 0;
	int	ok;

	left = unidiff(c, GCA, LEFT);
	right = unidiff(c, GCA, RIGHT);

	modified = lines_modified(left);
	unless (modified) goto bad;
	ok = are_unmodified(right, modified);
	free(modified);
	unless (ok) goto bad;

	modified = lines_modified(right);
	unless (modified) goto bad;
	ok = are_unmodified(left, modified);
	free(modified);
	unless (ok) goto bad;

	/*
	 * Allocate room for merged lines.  Worst case we need room for all
	 * the left and right lines in the output, plus a null
	 */
	c->merged = malloc(
	    (c->end[LEFT] - c->start[LEFT] +
	     c->end[RIGHT] - c->start[RIGHT] + 1) * sizeof(ld_t));

	/* we are good to go, do merge */
	i = 0;
	lp = left;
	rp = right;
	while (lp->ld || rp->ld) {
		if (!rp->ld || lp->ld && lp->ld->seq < rp->ld->seq) {
			/* line on left */
			if (lp->c != '+') {
				/* error? */
				fprintf(stderr,
"ERROR: merge_content expected add on left\n");
				exit(2);
			}
			c->merged[i++] = *lp->ld;
			++lp;
		} else if (!lp->ld || rp->ld->seq < lp->ld->seq) {
			/* line on right */
			if (rp->c != '+') {
				/* error? */
				fprintf(stderr,
"ERROR: merge_content expected add on right\n");
				exit(2);
			}
			c->merged[i++] = *rp->ld;
			++rp;
		} else {
			/* matching line */
			if (lp->c == ' ' && rp->c == ' ') {
				/* common line add to merge */
				c->merged[i++] = *lp->ld;
			} else if ((lp->c == '-' && rp->c == ' ') ||
			    (lp->c == ' ' && rp->c == '-')) {
				/* deleted line ignore */
			} else {
				fprintf(stderr,
"ERROR: merge_content can't see adds on matching lines!\n");
				exit(2);
			}
			++lp;
			++rp;
		}
	}
	c->merged[i].line = 0;
	ret = 1;
 bad:
	free(left);
	free(right);
	return (ret);
}

/*
 * Take a conflict region and a set of 3 indexes for the 3 sides
 * and split the conflict into two such that the second conflict starts
 * and those indexes.
 */
private void
split_conflict(conflict_t *c, int splitidx[3])
{
	conflict_t	*newc;
	int	i;
	u32	seq;

	seq = body[GCA].lines[c->start[GCA] + splitidx[GCA]].seq;

	/* create a new conflict region */
	new(newc);

	for (i = 0; i < 3; i++) {
		newc->end[i] = c->end[i];
		newc->start[i] = c->end[i] = c->start[i] + splitidx[i];
	}
	c->end_seq = seq;
	newc->start_seq = seq;

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
merge_common_header(conflict_t *c)
{
	ld_t	*start[3];
	int	len[3];
	int	minlen;
	int	splitidx[3];
	int	i;
	u32	seq;

	for (i = 0; i < 3; i++) {
		start[i] = &body[i].lines[c->start[i]];
		len[i] = c->end[i] - c->start[i];
	}
	minlen = min(len[LEFT], len[RIGHT]);

	for (i = 0; i < minlen; i++) {
		unless (sameline(&start[LEFT][i], &start[RIGHT][i])) break;
	}
	/* i == number of matching lines at start */
	if (i == 0 || i == len[LEFT] && i == len[RIGHT]) return (0);

	splitidx[LEFT] = i;
	splitidx[RIGHT] = i;

	seq = start[LEFT][i-1].seq;
	if (start[RIGHT][i-1].seq < seq) seq = start[RIGHT][i-1].seq;

	for (i = 0; i < len[GCA]; i++) {
		if (start[GCA][i].seq > seq) break;
	}
	splitidx[GCA] = i;

	split_conflict(c, splitidx);

	return (1);
}

/*
 * Split a conflict if the last N lines of the RIGHT and the LEFT have
 * identical data.
 */
private int
merge_common_footer(conflict_t *c)
{
	ld_t	*start[3];
	int	len[3];
	int	minlen;
	int	splitidx[3];
	int	i;
	u32	seq;

	for (i = 0; i < 3; i++) {
		start[i] = &body[i].lines[c->start[i]];
		len[i] = c->end[i] - c->start[i];
	}
	minlen = min(len[LEFT], len[RIGHT]);

	for (i = 0; i < minlen; i++) {
		unless (sameline(&start[LEFT][len[LEFT] - i - 1],
			    &start[RIGHT][len[RIGHT] - i - 1])) break;
	}
	/* i == number of matching lines at end */
	if (i == 0 || i == len[LEFT] && i == len[RIGHT]) return (0);

	splitidx[LEFT] = len[LEFT] - i;
	splitidx[RIGHT] = len[RIGHT] - i;

	seq = start[LEFT][splitidx[LEFT]].seq;
	i = start[RIGHT][splitidx[RIGHT]].seq;
	if (i < seq) seq = i;

	for (i = 0; i < len[GCA]; i++) {
		if (start[GCA][i].seq > seq) break;
	}
	splitidx[GCA] = i;

	split_conflict(c, splitidx);

	return (1);
}

/*
 * Split a conflict if the same lines are deleted at the end of the
 * conflict.
 */
private int
merge_common_deletes(conflict_t *c)
{
	diffline_t	*left, *right;
	diffline_t	*p;
	int	splitidx[3];
	int	i, j;
	int	ret = 0;
	int	cnt;

	left = unidiff(c, GCA, LEFT);
	right = unidiff(c, GCA, RIGHT);
	/*
	 * remove matching deletes at beginning
	 *
	 * XXX not implimented.  Why?
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
	while (p >= left && p->c == '-') {
		++j;
		--p;
	}
	if (j < cnt) cnt = j;

	if (cnt > 0) {
		for (i = 0; i < 3; i++) {
			splitidx[i] = c->end[i] - c->start[i];
		}
		splitidx[GCA] -= cnt;
		split_conflict(c, splitidx);
		ret = 1;
	}

	free(left);
	free(right);
	return (ret);
}

