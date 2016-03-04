/*
 * Copyright 1999-2003,2010,2015-2016 BitMover, Inc
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
#include "bkd.h"
#include "nested.h"

int
status_main(int ac, char **av)
{
	int	i, c;
	int	verbose = 0;
	int	isnest;
	nested	*n;
	comp	*cp;
	int	ncomps = 0, nhere = 0;
	int	nmods = 0, npend = 0;
	char	*p;
	FILE	*fchg, *fsfile;
	char	**parents = 0;
	hash	*pcount;
	u32	bits;
	char	**attr = 0;
	struct item {
		int	cnt[2];
		char	*err;
	} *pi = 0;
	longopt	lopts[] = {
		{ 0, 0 }
		};

	putenv("PAGER=cat");
	while ((c = getopt(ac, av, "v", lopts)) != -1) {
		switch (c) {
		    case 'v': verbose++; break;			/* doc 2.0 */
		    default: bk_badArg(c, av);
		}
	}
	if (av[optind]) {
		if (av[optind+1]) usage();
		if (chdir(av[optind])) {
			perror(av[optind]);
			return (1);
		}
	}

	isnest = bk_nested2root(0);

	/* 7.0 status starts here */
	/* start these early to reduce latency */
	fchg = popen("bk changes -aLR -nd. 2>&1", "r");
	fsfile = popen("bk -e gfiles -Ucvhp", "r"); /* XXX no scancomps */

	printf("Repo: %s:%s\n", sccs_realhost(), proj_root(0));
	if (isnest) {
		n = nested_init(0, 0, 0, NESTED_PENDING);
		assert(n);
		EACH_STRUCT(n->comps, cp, i) {
			++ncomps;
			if (C_PRESENT(cp)) ++nhere;
		}
		nested_free(n);
		printf("Nested repo: %d/%d components present\n",
		    nhere, ncomps);
	}

	/* per-parent information */
	pcount = hash_new(HASH_MEMHASH);
	pcount->vptr = 0;
	c = 0;		/* for compiler */
	while (p = fgetline(fchg)) {
		if (strneq(p, "==== changes -", 14)) {
			/* get URL and reset counters */
			i = strlen(p);
			assert(streq(p+i-5, " ===="));
			p[i-5] = 0;	    /* strip " ====" at end */
			c = (p[14] == 'R'); /* 0 == L, 1 == R */

			/* save in hash */
			if (pi = hash_insert(pcount,
				p+16, strlen(p+16)+1,
				0, sizeof(*pi))) {
				/* new parent, save order */
				parents = addLine(parents, pcount->kptr);
				pi->cnt[0] = pi->cnt[1] = -1;
			}
			pi = pcount->vptr;
			pi->cnt[c] = 0;
		} else if (streq(p, ".")) {	/* one cset per line */
			++pi->cnt[c];
		} else if (begins_with(p, "This repository has no parent")) {
			fgetline(fchg);	/* ignore next line too */
		} else if (begins_with(p, "ERROR")) {
			unless (pi->err) pi->err = strdup(p);
		} else {
			fprintf(stderr, "%s: ignoring unexpected line: %s\n",
			    prog, p);
		}
	}
	EACH(parents) {
		pi = (struct item *)hash_fetchStr(pcount, parents[i]);
		assert(pi);
		if (pi->cnt[0] >= 0) {
			if (pi->cnt[1] >= 0) {
				printf("Push/pull parent: ");
			} else {
				printf("Push parent: ");
			}
		} else {
			if (pi->cnt[1] >= 0) {
				printf("Pull parent: ");
			} else {
				assert(0);
			}
		}
		printf("%s\n", parents[i]);
		if (pi->err) {
			printf("\t%s\n", pi->err);
			free(pi->err);
		} else {
			if (pi->cnt[0] > 0) {
				printf("\t%d csets can be pushed\n",
				    pi->cnt[0]);
			}
			if (pi->cnt[1] > 0) {
				printf("\t%d csets can be pulled\n",
				    pi->cnt[1]);
			}
			if ((pi->cnt[0] <= 0) && (pi->cnt[1] <= 0)) {
				printf("\t(up to date)\n");
			}
		}
	}
	unless (parents) {
		fflush(stdout);
		system("bk parent");
	}
	freeLines(parents, 0);
	pclose(fchg);
	hash_free(pcount);

	/* file counts */
	while (p = fgetline(fsfile)) {
		if (p[2] == 'c') ++nmods;
		if (p[3] == 'p') ++npend;
	}
	pclose(fsfile);
	if (nmods) printf("%d locally modified files\n", nmods);
	if (npend) printf("%d locally pending files\n", npend);

	/* latest repository features */
	bits = (FEAT_FILEFORMAT|FEAT_SCANDIRS) & ~FEAT_BWEAVEv2;
	/* remove current features */
	bits &= ~features_bits(0);
	if (bits) {
		p = features_fromBits(bits);
		printf("BK features not enabled: %s\n", p);
		free(p);
	}

	i = getlevel();
	if (i > 1) attr = addLine(attr, aprintf("level=%d", i));
	if (nested_isGate(0)) attr = addLine(attr, strdup("GATE"));
	if (nested_isPortal(0)) attr = addLine(attr, strdup("PORTAL"));
	if (attr) {
		p = joinLines(", ", attr);
		printf("Repo attributes: %s\n", p);
		free(p);
		freeLines(attr, free);
	}

	printf("BK version: %s (repository requires bk-%d.0 or later)\n",
	    *bk_tag ? bk_tag : bk_vers, features_minrelease(0, 0));
	return (0);
}
