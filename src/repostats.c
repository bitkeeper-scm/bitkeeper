/*
 * Copyright 2013-2016 BitMover, Inc
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

typedef	struct	histo	histo;

private	histo	*histo_new(void);
private	void	histo_data(histo *h, u32 d);
private	void	histo_print(histo *h, char *title, char *(*print)(u64 data));
private	void	histo_free(histo *h);

/*
 * age/cset
 * csets/component
 * files/component
 * deleted/component
 * deltas/file
 * size/file
 */

int
repostats_main(int ac, char **av)
{
	FILE	*f, *f2;
	char	*t, *dir;
	int	c, i, cnt, del;
	sccs	*s;
	histo	*sizes, *deltas, *files, *dirs, *csets, *deleted;
	char	*sfile;
	int	standalone = 0;
	char	buf[MAXLINE];

	while ((c = getopt(ac, av, "S", 0)) != -1) {
		switch (c) {
		    case 'S': standalone = 1; break;
		    default: bk_badArg(c, av);
		}
	}

	bk_nested2root(standalone);

	printf("%20s %7s %6s %6s %6s ",
	    "title", "sum", "cnt", "ave", "min");
	for (i = 1; i < 10; i++) printf("%5d%% ", 10*i);
	printf("%6s\n", "max");
	printf("-\n");

	f = popen("bk -A", "r");
	sizes = histo_new();
	deltas = histo_new();
	dirs = histo_new();
	buf[0] = 0;
	cnt = 0;		/* dir count */
	while (t = fgetline(f)) {
		if (streq(t, "ChangeSet") || ends_with(t, "/ChangeSet")) {
			continue;
		}
		sfile = name2sccs(t);
		histo_data(sizes, size(sfile));
		s = sccs_init(sfile, SILENT);
		histo_data(deltas, s->tip);
		sccs_free(s);
		dir = dirname(sfile);
		++cnt;
		unless (streq(dir, buf)) {
			histo_data(dirs, cnt);
			cnt = 0;
			strcpy(buf, dir);
		}
		free(sfile);
	}
	pclose(f);
	histo_data(dirs, cnt);
	histo_print(sizes, "size/file", psize);
	histo_free(sizes);
	histo_print(deltas, "deltas/file", 0);
	histo_free(deltas);
	histo_print(dirs, "files/dir", 0);
	histo_free(dirs);

	if (proj_isProduct(0)) {
		files = histo_new();
		csets = histo_new();
		deleted = histo_new();
		f = popen("bk comps -ch", "r");
		while (t = fgetline(f)) {
			sprintf(buf, "%s/%s", t, CHANGESET);
			s = sccs_init(buf, SILENT);
			histo_data(csets, s->tip);
			sccs_free(s);
			sprintf(buf, "bk gfiles '%s'", t);
			c = streq(t, ".") ? 0 : strlen(t)+1;
			cnt = 0;
			del = 0;
			f2 = popen(buf, "r");
			while (t = fgetline(f2)) {
				++cnt;
				if (strneq(t+c, "BitKeeper/deleted/", 18)) ++del;
			}
			pclose(f2);
			histo_data(files, cnt);
			histo_data(deleted, del);
		}
		pclose(f);
		histo_print(files, "files/component", 0);
		histo_free(files);
		histo_print(deleted, "deleted/component", 0);
		histo_free(deleted);
		histo_print(csets, "csets/component", 0);
		histo_free(csets);
	}
	return (0);
}

struct histo {
	u32	*data;
	u32	num;
	u64	sum;
};

private histo *
histo_new(void)
{
	histo	*h = new(histo);
	return (h);
}

private	void
histo_data(histo *h, u32 d)
{
	addArray(&h->data, &d);
	h->sum += d;
	++h->num;
}

private int
numcmp(const void *a, const void *b)
{
	return (*(u32 *)a - *(u32 *)b);
}

private char *
pnum(u64 num)
{
	static	char	buf[64];

	sprintf(buf, "%lld", num);
	return (buf);
}

private void
histo_print(histo *h, char *title, char *(*print)(u64 data))
{
	float	ave;
	int	i, j;

	unless (print) print = pnum;

	printf("%20s %7s %6d ", title, print(h->sum), h->num);
	ave = (float)h->sum/h->num;
	if (ave > 100) {
		printf("%6s ", print(ave));
	} else {
		printf("%6.1f ", ave);
	}
	sortArray(h->data, numcmp);
	for (i = 0; i < 10; i++) {
		j = (i * h->num) / 10;
		printf("%6s ", print(h->data ? h->data[j+1] : 0));
	}
	printf("%6s", print(h->data ? h->data[h->num] : 0)); /* max */
	printf("\n");
}

private void
histo_free(histo *h)
{
	free(h->data);
	free(h);
}
