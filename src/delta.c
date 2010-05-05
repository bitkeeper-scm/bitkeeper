/* Copyright (c) 1997 L.W.McVoy */
#include "system.h"
#include "sccs.h"
#include "logging.h"

private int
hasKeyword(sccs *s)
{
	return (s->xflags & (X_RCS|X_SCCS));
}

int
fix_gmode(sccs *s, int gflags)
{
	delta	*d;

	/*
	 * Do not fix mode of symlink target, the file may not be 
	 * under BK control.
	 */
	if (S_ISLNK(s->mode)) return (0);

	if ((gflags&GET_EDIT) && WRITABLE(s))  return (0);
	if (!(gflags&GET_EDIT) && !WRITABLE(s))  return (0);

	d = sccs_top(s);
	if (d->mode) s->mode = d->mode;
	unless (gflags&GET_EDIT) {
		s->mode &= ~0222;	/* turn off write mode */
	}

	if (chmod(s->gfile, s->mode)) return (1);
	return (0);
}

private int
hasTriggers(void)
{
	char	*t, **lines;
	int	ret;
	char	dir[MAXPATH];

	if (getenv("_IN_DELTA")) return (0);
	unless (t = proj_root(0)) {
		return (0);
	}
	unless (streq(t, ".")) {
		concat_path(dir, t, "/BitKeeper/triggers");
	} else {
		strcpy(dir, "BitKeeper/triggers");
	}
	if (isdir(dir)) {
		sys("bk", "get", "-Sq", dir, SYS);
		lines = getTriggers(0, dir, "pre-delta");
		ret = (lines != 0);
		freeLines(lines, free);
	} else {
		ret = 0;
	}
	return (ret);
}

private int
delta_trigger(sccs *s)
{
	char	*e = aprintf("BK_FILE=%s", s->gfile);
	int	i;

	putenv(e);
	putenv("BK_EVENT=delta");
	i = trigger("delta", "pre");
	putenv("BK_FILE=");
	free(e);
	return (i);
}

/*
 * Remove dangling deltas from the file.
 */
private int
strip_danglers(char *name, u32 flags)
{
	char	*p, **revs = 0;
	int	i;
	sccs	*s;
	delta	*d;
	FILE	*f;

	s = sccs_init(name, INIT_WACKGRAPH);
	assert(s);
	for (d = s->table; d; d = NEXT(d)) {
		if (d->dangling) revs = addLine(revs, strdup(d->rev));
	}
	sccs_free(s);
	p = aprintf("bk stripdel -%sdC -", (flags&SILENT) ? "q" : "");
	f = popen(p, "w");
	EACH(revs) {
		fprintf(f, "%s|%s\n", name, revs[i]);
	}
	freeLines(revs, free);
	if (i = pclose(f)) {
		fprintf(stderr, "%s failed\n", p);
		free(p);
		return (i);	// XXX - need to get exit status
	}
	free(p);
	s = sccs_init(name, INIT_WACKGRAPH);
	assert(s);
	sccs_renumber(s, (flags&SILENT)|INIT_WACKGRAPH);
	/*
	 * If we are preserving the monotonic flag but our parent didn't
	 * have it, make a note of that.
	 */
	d = sccs_top(s);
	if ((d->xflags & X_MONOTONIC) && !(PARENT(s, d)->xflags & X_MONOTONIC)) {
		comments_load(s, d);
		comments_append(d, strdup("Turn on MONOTONIC flag"));
	}
	sccs_admin(s, 0, NEWCKSUM|ADMIN_FORCE, 0, 0, 0, 0, 0, 0, 0);
	sccs_free(s);
	return (0);
}

int
delta_main(int ac, char **av)
{
	sccs	*s;
	int	iflags = 0;
	int	dflags = 0;
	int	gflags = 0;
	int	sflags = SF_GFILE|SF_WRITE_OK|SF_NOHASREVS;
	int	checkout = 0, ignorePreference = 0;
	int	c, rc;
	char	*initFile = 0;
	char	*diffsFile = 0;
	char	*prog, *name;
	char	*compp = 0, *encp = 0;
	char	*def_compp;
	char	*mode = 0;
	MMAP	*diffs = 0;
	MMAP	*init = 0;
	pfile	pf;
	int	dash, errors = 0, fire, dangling;
	off_t	sz;

	prog = strrchr(av[0], '/');
	if (prog) prog++;
	else prog = av[0];
	if (streq(prog, "new") ||
	    streq(prog, "enter") || streq(prog, "add")) {
		dflags |= NEWFILE;
		sflags |= SF_NODIREXPAND;
		sflags &= ~SF_WRITE_OK;
	}

	while ((c =
	    getopt(ac, av, "abcCdD:E|fGI;ilm|M;npPqRsTuy|Y|Z|", 0)) != -1) {
		switch (c) {
		    /* SCCS flags */
		    case 'n': dflags |= DELTA_SAVEGFILE; break;	/* undoc? 2.0 */
		    case 'p': dflags |= PRINT; break; 		/* doc 2.0 */
		    case 'm':					/* ci compat */
		    case 'y': 					/* doc 2.0 */
			if (comments_save(optarg)) return (1);
			dflags |= DELTA_DONTASK;
			break;
		    case 's': /* fall through */		/* undoc 2.0 */

		    /* RCS flags */
		    case 'q': 					/* doc 2.0 */
			dflags |= SILENT; gflags |= SILENT; break;
		    case 'f': dflags |= DELTA_FORCE; break;	/* doc 2.0 ci */
		    case 'i': dflags |= NEWFILE; 		/* doc 2.0 */
			      sflags |= SF_NODIREXPAND;
			      sflags &= ~SF_WRITE_OK;
			      break;
		    case 'l': checkout = CO_EDIT;		/* doc 2.0 */ 
			      break;
		    case 'u': checkout = CO_GET;		/* doc 2.0 */
			      break;

		    /* LM flags */
		    case 'a':					/* doc 2.0 */
		    	dflags |= DELTA_AUTO;
			dflags &= ~DELTA_FORCE;
			sflags &= ~SF_WRITE_OK;
			break;
		    case 'b':	/* -b == -Ebinary */		/* doc 2.0 */
			if (streq(prog, "new")) {
		    		encp = "binary";
			} else {
				usage();
			}
			break;
		    case 'C': iflags |= INIT_NOCKSUM; break; 	/* undoc */
		    case 'c': dflags |= DELTA_CFILE; break;	/* doc */
		    case 'd': /* internal interface */ 		/* undoc 2.0 */
			      dflags |= DELTA_NOPENDING; break;
		    case 'D': diffsFile = optarg;		 /* doc 2.0 */
			      sflags = ~(SF_GFILE | SF_WRITE_OK);
			      break;
		    case 'I':
			initFile = optarg;
			dflags |= DELTA_FORCE;
			break;
		    case 'M': mode = optarg; break;		/* doc 2.0 */
		    case 'P': ignorePreference = 1;  break;	/* undoc 2.0 */
		    case 'R': dflags |= DELTA_PATCH; break;	/* undoc? 2.0 */
		    case 'G': 	/* remove -G in 2009 .  Replaced by -T */
		    case 'T': iflags |= INIT_FIXDTIME; break;	/* undoc? 2.0 */
		    case 'Y':
			if (optarg && comments_savefile(optarg)) {
				fprintf(stderr,
					"delta: can't read comments from %s\n",
					optarg);
				return (1);
			}
			dflags |= DELTA_DONTASK;
			break; 	/* doc 2.0 */
		    case 'Z': 					/* doc 2.0 */
			compp = optarg ? optarg : "gzip"; break;
		    case 'E': encp = optarg; break; 		/* doc 2.0 */

		    default: bk_badArg(c, av);
		}
	}

	def_compp  = proj_configval(0, "compression");
	unless (def_compp && *def_compp) def_compp = "gzip";

	if (encp || compp) {
		unless (dflags & NEWFILE) {
			fprintf(stderr,
			    "Encoding is allowed only when creating files\n");
			usage();
		}
		/* check that they gave us something we can parse */
		if (sccs_encoding(0, 0, encp, compp) == -1) usage();
	}

	if (chk_host() || chk_user()) return (1);

	dash = av[optind] && streq(av[optind], "-");
	name = sfileFirst(av[0], &av[optind], sflags);
	/* They can only have an initFile for one file...
	 * So we go get the next file and error if there
	 * is one.
	 */
	if ((initFile || diffsFile) && name && sfileNext()) {
		fprintf(stderr,
"%s: only one file may be specified with init or diffs file.\n", av[0]);
		usage();
	}

	/* force them to do something sane */
	if (!comments_got() &&
	    dash && name && !(dflags & (NEWFILE|DELTA_CFILE)) && sfileNext()) {
		fprintf(stderr,
"%s: only one file may be specified without a checkin comment\n", av[0]);
		usage();
	}
	if (initFile && (dflags & DELTA_DONTASK)) {
		fprintf(stderr,
		    "%s: only init file or comment, not both.\n", av[0]);
		usage();
	}
	if ((gflags & GET_EXPAND) && (gflags & GET_EDIT)) {
		fprintf(stderr, "%s: -l and -u are mutually exclusive.\n",
			av[0]);
		usage();
	}
	if (diffsFile && !(diffs = mopen(diffsFile, "b"))) {
		fprintf(stderr, "%s: diffs file '%s': %s.\n",
			av[0], diffsFile, strerror(errno));
	       return (1);
	}
	if (initFile && !(init = mopen(initFile, "t"))) {
		fprintf(stderr,"%s: init file '%s': %s.\n",
			av[0], initFile, strerror(errno));
		return (1);
	}
	if (fire = hasTriggers()) putenv("_IN_DELTA=YES");

	while (name) {
		delta	*d = 0;
		char	*nrev;
		int	df = dflags;
		int	gf = gflags;
		int	reget = 0;
		int	co;

		if (df & DELTA_DONTASK) {
			unless (d = comments_get(0)) usage();
		}
		if (mode) d = sccs_parseArg(d, 'O', mode, 0);
		unless (s = sccs_init(name, iflags)) {
			if (d) sccs_freetree(d);
			name = sfileNext();
			errors |= 1;
			continue;
		}
		lease_check(s->proj, O_WRONLY, s);
		if (df & DELTA_AUTO) {
			if (HAS_SFILE(s)) {
				df &= ~NEWFILE;
			} else {
				df |= NEWFILE;
			}
		}
		if (dflags & NEWFILE) {
			unless (ignorePreference || compp) compp = def_compp;
		}

		/*
		 * Checkout option does not applies to ChangeSet file
		 * see rev 1.118
		 * XXX - delta -l|u will be ignored for ChangeSet.
		 */
		co = checkout;		// This is from -l/-u, it goes first
		unless (co || ignorePreference) co = CO(s);
		unless (CSET(s)) {
			if (co & (CO_EDIT|CO_LAST)) {
				gf |= GET_SKIPGET|GET_EDIT;
				df |= DELTA_SAVEGFILE;
				reget = 1;	/* to write pfile */
			} else if (co & CO_GET) {
				if (hasKeyword(s))  {
					gf |= GET_EXPAND;
					reget = 1;
				} else {
					gf |= GET_SKIPGET;
					df |= DELTA_SAVEGFILE;
				}
			}
		}

		nrev = NULL;
		if (HAS_PFILE(s)) {
			if (sccs_read_pfile("delta", s, &pf)) {
				errors |= 2;
				goto next;
			}
			nrev = pf.newrev;
		}

		if (fire) {
			win32_close(s);
			switch (delta_trigger(s)) {
			    case 0:
				win32_open(s);
				break;
			    case 2: /* trigger ran delta, we won't */
				goto next;
			    default:
				errors |= 32;
				goto next;
			}
		}

		if (df & NEWFILE) {
			sz = diffs ? (off_t)msize(diffs) : size(s->gfile);
			s->encoding = sccs_encoding(s, sz, encp, compp);
		}

		dangling = MONOTONIC(s) && sccs_top(s)->dangling;
		if (dangling) df |= DELTA_MONOTONIC;
		rc = sccs_delta(s, df, d, init, diffs, 0);
		if (rc == -4) {	/* interrupt in comment prompt */
			errors |= 4;
			break;
		}
		if (rc == -2) goto next; /* no diff in file */
		if (rc == -1) {
			if (BAM(s) && bk_notLicensed(s->proj, LIC_BAM, 0)) {
				errors |= 4;
				break;
			}
			sccs_whynot("delta", s);
			errors |= 4;
			goto next;
		}

		if (dangling) {
			sccs_free(s);
			strip_danglers(name, dflags);
			s = sccs_init(name, iflags);
			assert(s);
			d = s->table;
			assert(d);
			assert(d->type == 'D');
			nrev = d->rev;
		} else {
			s = sccs_restart(s);
		}
		unless (s) {
			fprintf(stderr,
			    "%s: can't restart %s\n", av[0], name);
			errors |= 8;
			goto next;
		}
		if (df & DELTA_SAVEGFILE) {
			/*
			 * fix_gmode() will fail if we don't own
			 * the gfile, in that case set the stage
			 * to re-get it
			 */
			if (fix_gmode(s, gf)) {
				reget = 1;
				gf &= ~GET_SKIPGET;
			}
		}
		if (df & NEWFILE) {
			/*
			 * The 'keyword' preference for a new file might
			 * have set keywords in sccs_delta() so we need to
			 * check again.
			 */
			if (hasKeyword(s) && !(gf & GET_EDIT)) {
				gf |= GET_EXPAND;
				gf &= ~GET_SKIPGET;
				reget = 0;
				unless (co = checkout) co = CO(s);
				reget = co & CO_GET;
			}
		}

		if (reget) {
			// XXX - what if we are dangling?
			// The pf.oldrev is definitely wrong.
			if (rc == -3) nrev = pf.oldrev;
			if (sccs_get(s, nrev, 0, 0, 0, gf, "-")) {
				unless (BEEN_WARNED(s)) {
					fprintf(stderr,
					    "get of %s failed, skipping it.\n",
					    name);
				}
			}
		}
next:		if (init) mclose(init);
		/*
		 * No, sccs_diffs() does this.
		 * if (diffs) fclose(diffs);
		 */
		sccs_free(s);
		name = sfileNext();
	}
	if (sfileDone()) errors |= 64;
	comments_done();
	return (errors);
}


