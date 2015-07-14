#include "sccs.h"
#include "bkd.h"
#include "nested.h"

private	void	compat_status(int verbose, FILE *f);

int
status_main(int ac, char **av)
{
	int	i, c;
	int	compat = 0, verbose = 0;
	int	isnest;
	nested	*n;
	comp	*cp;
	int	ncomps = 0, nhere = 0;
	int	nmods = 0, npend = 0;
	char	*p;
	FILE	*fchg, *fsfile;
	char	**parents = 0;
	hash	*pcount;
	i32	*pcnt;
	u32	bits;
	char	**attr = 0;
	longopt	lopts[] = {
		{ "compat", 300 },
		{ 0, 0 }
		};

	putenv("PAGER=cat");
	while ((c = getopt(ac, av, "v", lopts)) != -1) {
		switch (c) {
		    case 'v': verbose++; break;			/* doc 2.0 */
		    case 300: // --compat
			compat++;
			break;
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
	if (compat) {
		compat_status(verbose, stdout);
		return (0);
	}
	/* 7.0 status starts here */
	isnest = bk_nested2root(0);

	/* start these early to reduce latency */
	fchg = popen("bk changes -aLR -nd. 2>" DEVNULL_WR, "r");
	fsfile = popen("bk -e sfiles -Ucgvhp", "r"); /* XXX no scancomps */

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
	pcnt = 0;
	while (p = fgetline(fchg)) {
		if (strneq(p, "==== changes -", 14)) {
			i32	cnt[2] = {-1, -1};

			/* get URL and reset counters */
			i = strlen(p);
			assert(streq(p+i-5, " ===="));
			p[i-5] = 0;	    /* strip " ====" at end */
			c = (p[14] == 'R'); /* 0 == L, 1 == R */

			/* save in hash */
			if (hash_insert(pcount,
				p+16, strlen(p+16)+1,
				cnt, sizeof(cnt))) {
				/* new parent, save order */
				parents = addLine(parents, pcount->kptr);
			}
			pcnt = pcount->vptr;
			pcnt[c] = 0;
		} else if (streq(p, ".")) {	/* one cset per line */
			++pcnt[c];
		} else {
			assert(0); /* bad output */
		}
	}
	EACH(parents) {
		pcnt = (i32 *)hash_fetchStr(pcount, parents[i]);
		assert(pcnt);
		if (pcnt[0] >= 0) {
			if (pcnt[1] >= 0) {
				printf("Push/pull parent: ");
			} else {
				printf("Push parent: ");
			}
		} else {
			if (pcnt[1] >= 0) {
				printf("Pull parent: ");
			} else {
				assert(0);
			}
		}
		printf("%s\n", parents[i]);
		if (pcnt[0] > 0) printf("\t%d csets can be pushed\n", pcnt[0]);
		if (pcnt[1] > 0) printf("\t%d csets can be pulled\n", pcnt[1]);
		if ((pcnt[0] <= 0) && (pcnt[1] <= 0)) {
			printf("\t(up to date)\n");
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
	    bk_vers, features_minrelease(0, 0));
	return (0);
}

private void
compat_status(int verbose, FILE *f)
{
	char	buf[MAXLINE], parent_file[MAXPATH];
	char	tmp_file[MAXPATH];
	FILE	*f1;

	fprintf(f, "Status for BitKeeper repository %s:%s\n",
	    sccs_gethost(), proj_cwd());
	bkversion(f);
	sprintf(parent_file, "%slog/parent", BitKeeper);
	if (exists(parent_file)) {
		fprintf(f, "Parent repository is ");
		f1 = fopen(parent_file, "r");
		while (fgets(buf, sizeof(buf), f1)) fputs(buf, f);
		fclose(f1);
	}
	unless (repository_lockers(0)) {
		if (isdir("PENDING")) {
			fprintf(f, "Pending patches\n");
		}
	}

	if (verbose) {
		bktmp(tmp_file);
		f1 = fopen(tmp_file, "w");
		assert(f1);
		bkusers(0, 0, f1);
		fclose(f1);
		f1 = fopen(tmp_file, "rt");
		while (fgets(buf, sizeof(buf), f1)) {
			fprintf(f, "User:\t%s", buf);
		}
		fclose(f1);
		sprintf(buf, "bk sfiles -x > '%s'", tmp_file);
		system(buf);
		f1 = fopen(tmp_file, "rt");
		while (fgets(buf, sizeof(buf), f1)) {
			fprintf(f, "Extra:\t%s", buf);
		}
		fclose(f1);
		sprintf(buf, "bk sfiles -gc > '%s'", tmp_file);
		system(buf);
		f1 = fopen(tmp_file, "rt");
		while (fgets(buf, sizeof(buf), f1)) {
			fprintf(f, "Modified:\t%s", buf);
		}
		fclose(f1);
		sprintf(buf, "bk sfiles -gpC > '%s'", tmp_file);
		system(buf);
		f1 = fopen(tmp_file, "rt");
		while (fgets(buf, sizeof(buf), f1)) {
			fprintf(f, "Uncommitted:\t%s", buf);
		}
		fclose(f1);
		unlink(tmp_file);
	} else {
		fprintf(f,
		    "%6d people have made deltas.\n", bkusers(0, 0, 0));
		f1 = popen("bk sfiles -ES", "r");
		while (fgets(buf, sizeof (buf), f1)) fputs(buf, f);
		pclose(f1);
	}
}
