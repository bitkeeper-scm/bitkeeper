/*
 * Copyright 2016 BitMover, Inc
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
 * Synth a bk file randomly.
 * Use bk to fix up file: renumber.
 * Here, we see that neither parent is in history of other,
 * And do the bk trunk branch topology.
 * Note: Includes and excludes might be out of DAG closure.
 * More work to fix (prune outliers or map).  So leaving.
 * That mimics branches.
 */

typedef struct Graph {
	int	serial;	// serial 1..
	int	size;	// number of nodes in graph closure
	int	include[];
	int	exclude[];
	int	parent;
	int	merge;
	int	red;
	int	blue;
} Graph;

int	nextser;
int	quiet;

void
main(string av[])
{
	string	file, arg;
	int	size = 100;	/* default how big is the file */
	Graph	table[];	

	quiet = 0;
	while (arg = getopt(av, "qs;", undef)) {
		switch (arg) {
		    case 'q':
			quiet = 1;
			break;
		    case 's':
		    	size = (int)optarg;
			break;
		    case "":
			fprintf(stderr,
			    "usage: %s [-q][-s<size>] file\n", av[0]);
			exit(1);
		}
	}
	unless (file = av[optind]) {
		fprintf(stderr, "usage: %s [-q][-s<size>] file\n", av[0]);
		exit(1);
	}

	nextser = 0;
	table = mkGraph(size);
	table = bkify(table, size);
	mkSfile(table, size, file);
}

/* between lower and upper inclusive, biased toward upper */
int
squaredist(int lower, int upper)
{
	int	x;

	x = (upper + 1 - lower);
	x *= x;
	x = (int)(::tcl::mathfunc::sqrt(::tcl::mathfunc::rand() * x));
	return (x + lower);
}

/*
 * If we know how many nodes are under each parent, compute how many 
 * under the merge.  This was initially part of a scheme to get
 * include and exclude within the DAG, but I punted.
 * Now it is used to see if one node is in the history of the other.
 * Could make it simpler, but it works!
 */
int
closedsize(Graph table[], int parent, int merge)
{
	int	marked;
	int	start;
	int	size;
	int	lowest;
	Graph	graph, p;

	if (parent == merge) return (table[parent].size);

	if (parent > merge) {
		start = parent;
		table[merge].blue = 1;
		size = table[merge].size;
		lowest = table[merge].serial;
	} else {
		start = merge;
		table[parent].blue = 1;
		size = table[parent].size;
		lowest = table[parent].serial;
	}
	table[start].red = 1;
	marked = 1;

	for (/* start */; start && marked; start--) {
		graph = table[start];
		unless (graph.red || graph.blue) continue;
		if (graph.red && !graph.blue) {
			size++;
			marked--;
		}
		if (graph.parent) {
			p = table[graph.parent];
			if (p.red && !p.blue) marked--;
			if (graph.red) p.red = 1;
			if (graph.blue) p.blue = 1;
			if (p.red && !p.blue) marked++;
			if (p.serial < lowest) lowest = p.serial;
			table[graph.parent] = p;
		}
		if (graph.merge) {
			p = table[graph.merge];
			if (p.red && !p.blue) marked--;
			if (graph.red) p.red = 1;
			if (graph.blue) p.blue = 1;
			if (p.red && !p.blue) marked++;
			if (p.serial < lowest) lowest = p.serial;
			table[graph.merge] = p;
		}
		table[start].red = 0;
		table[start].blue = 0;
	}
	while (start >= lowest) {
		table[start].red = 0;
		table[start].blue = 0;
		start--;
	}
	return (size);
}

/* Synthesize a graph, but don't worry about bk rules */
Graph[]
mkGraph(int size)
{
	Graph	graph, empty;
	Graph	table[size+1];	
	int	i, x;
	int	tips{int};
	int	numtips;

	unless (size) return (table);

	table[1].size = 1;
	table[1].serial = 1;
	table[1].parent = 0;
	table[1].merge= 0;

	tips{1} = 1;
	numtips = 1;

	for (i = 2; i <= size; i++) {
		graph = empty;
		graph.serial = i;
		/* Near the end, don't make a new tip, but replace existing */
		if (size - i + 2 <= numtips) {
			foreach (x in keys(tips)) {
				if (tips{x}) {
					graph.parent = x;
					undef(tips{x});
					numtips--;
					break;
				}
			}
		} else {
			/* randomly make new or replace existing */
			graph.parent = squaredist(1, i-1);
		}
		/* Do we make a merge?  Do we have to reduce # tips? */
		if (size - i + 1 == numtips) {
			foreach (x in keys(tips)) {
				if (tips{x}) {
					graph.merge = x;
					undef(tips{x});
					numtips--;
					break;
				}
			}
		} else {	/* just make merges some of the time */
			if (::tcl::mathfunc::rand() > 0.5) {
				graph.merge = squaredist(1, i-1);
			} else {
				graph.merge = 0;
			}
		}
		if (tips{graph.parent}) {
			undef(tips{graph.parent});
			numtips--;
		}
		if (graph.merge) {
			if (tips{graph.merge}) {
				undef(tips{graph.merge});
				numtips--;
			}
			graph.size = 1 +
			    closedsize(table, graph.parent, graph.merge);
		} else {
			graph.size = table[graph.parent].size + 1;
		}
		x = (int)(::tcl::mathfunc::rand() * 10 + 1);
		while (x--) {
			// Use (graph.size - 1) if also map to DAG
			push(&graph.include, squaredist(1, graph.serial - 1));
		}
		x = (int)(::tcl::mathfunc::rand() * 10 + 1);
		while (x--) {
			// Use (graph.size - 1) if also map to DAG
			push(&graph.exclude, squaredist(1, graph.serial - 1));
		}
		
		// include and exclude have to be in the closure
		// map that out later

		tips{graph.serial} = 1;
		numtips++;

		table[i] = graph;
	}
	return (table);
}

/* remove parent in history of parent, and get trunk and branch right */
Graph[]
bkify(Graph table[], int size)
{
	Graph	graph;
	int	i, p, m, x, list[];
	int	h{int};

	for (i = 2; i <= size; i++) {
		graph = table[i];
		// if merge, is one in the history of the other?
		if (graph.parent == graph.merge) {
			graph.merge = 0;
		}
		if (graph.merge) {
			if (graph.parent > graph.merge) {
				if (graph.size ==
				    (table[graph.parent].size + 1)) {
					graph.merge = 0;
				}
			} else {
				if (graph.size ==
				    (table[graph.merge].size + 1)) {
					graph.parent = graph.merge;
					graph.merge = 0;
				}
			}
		}
		// if still merge, which one is branch and trunk?
		if (graph.merge) {
			p = graph.parent;
			m = graph.merge;
			while (p != m) {
				if (p > m) {
					p = table[p].parent;
				} else {
					m = table[m].parent;
					if (p == m) {
						x = graph.parent;
						graph.parent = graph.merge;
						graph.merge = x;
					}
				}
			}
		}
		/* see that no x is in both include and exclude */
		graph.include = lsort(integer:, decreasing:, graph.include);
		undef(h);
		foreach (x in graph.include) {
			h{x} = 1;
		}
		graph.exclude = lsort(integer:, decreasing:, graph.exclude);
		undef(list);
		foreach (x in graph.exclude) {
			unless (h{x}) push(&list, x);
		}
		graph.exclude = list;
		table[i] = graph;
	}
	return (table);
}

/* Synthesize an sfile */
void
mkSfile(Graph table[], int size, string file)
{
	FILE	f;
	Graph	graph;
	int	d;
	int	base;
	int	time;
	string	format;
	string	date, kd, hd;

	base = 1330218616;
	format = "%Y%m%d%H%M%S %y/%m/%d %H:%M:%S";

	f = stdout;
	fprintf(f, "%cH12345\n", 1);

	for (d = size; d >= 1; d--) {
		graph = table[d];

		// As added/removed/same
		fprintf(f, "%cs 1/0/1\n", 1);
		time = base + d;
		date = Clock_format(time, format: format);
		{kd, hd} = split(/ /, date, 2);
		// kdate[i] = kd;

		// Ad D 1.2701 15/03/10 10:55:24 wscott 10359 10358
		fprintf(f,
		    "%cd D %s %s bk %d %d\n", 1, "1.2", hd, d, graph.parent);

		// Inc list, Exc list, cset mark,
		// checksum, merge parent, block end
		if (length(graph.include)) {
			fprintf(f, "%ci %s\n", 1, join(' ', graph.include));
		}
		if (length(graph.exclude)) {
			fprintf(f, "%cx %s\n", 1, join(' ', graph.exclude));
		}
		// fprintf(f, "%ccC\n", 1);
		// can do random here:
		fprintf(f, "%ccK00000\n", 1);	// bk checksum -f will fix
		if (graph.merge) {
			fprintf(f, "%ccM%d\n", 1, graph.merge);
		}
		fprintf(f, "%ce\n", 1);
	}

	/* 1.1, 1.0 csets */
	fprintf(f, "%cs 1/0/0\n", 1);
	fprintf(f, "%cd D 1.1 04/06/27 16:07:39 bk 2 1\n", 1);
	fprintf(f, "%cc Initial repository create\n", 1);
	// fprintf(f, "%ccC\n", 1);
	fprintf(f, "%ccF1\n", 1);
	fprintf(f, "%ccK00000\n", 1);
	fprintf(f, "%ce\n", 1);
	fprintf(f, "%cs 0/0/0\n", 1);
	fprintf(f, "%cd D 1.0 04/06/27 16:07:39 bk 1 0\n", 1);
	fprintf(f, "%cc BitKeeper file /home/bk/stubrepo/ChangeSet\n", 1);
	fprintf(f, "%ccBbk@bitkeeper.com|ChangeSet|"
	    "20040627230739|04490|80744df6d363a810\n", 1);
	fprintf(f, "%ccHbitkeeper.com\n", 1);
	fprintf(f, "%ccK04490\n", 1);
	fprintf(f, "%ccP%s\n", 1, file);
	fprintf(f, "%ccR80744df6d363a810\n", 1);
	fprintf(f, "%ccV4\n", 1);
	fprintf(f, "%ccX0x21\n", 1);
	fprintf(f, "%ccZ-07:00\n", 1);
	fprintf(f, "%ce\n", 1);

	// per file junk
	fprintf(f, "%cu\n", 1);
	fprintf(f, "%cU\n", 1);
	fprintf(f, "%cf e %u\n", 1, 64);
	fprintf(f, "%cf x 0x21\n", 1);
	fprintf(f, "%ct\n", 1);
	fprintf(f, "%cT\n", 1);
	for (d = size; d > 2; d--) {
		fprintf(f, "%cI %d\n", 1, d);
		fprintf(f, "%d\n", d);
		fprintf(f, "%cE %d\n", 1, d);
	}
	fprintf(f, "%cI 1\n", 1);
	fprintf(f, "%cE 1\n", 1);
}
