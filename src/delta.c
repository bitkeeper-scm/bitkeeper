/* Copyright (c) 1997 L.W.McVoy */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

private int
hasKeyword(sccs *s)
{
	return (s->xflags & (X_RCS|X_SCCS));
}

int
fix_gmode(sccs *s, int gflags)
{
	/*
	 * Do not fix mode of symlink target, the file may not be 
	 * under BK control.
	 */
	if (S_ISLNK(s->mode)) return (0);

	if ((gflags&GET_EDIT) && IS_WRITABLE(s))  return (0);
	if (!(gflags&GET_EDIT) && !IS_WRITABLE(s))  return (0);

	if (gflags&GET_EDIT) {
		 s->mode |= 0200;	/* turn on write mode */
	} else {
		 s->mode |= 0400;	/* turn on read mode */ 
		 s->mode &= ~0222;	/* turn off write mode */
	}

	if (chmod(s->gfile, s->mode)) {
		perror(s->gfile);
		return (1);
	}
	return (0);
}

private int
hasTriggers(void)
{
	char	*t, **lines;
	char	*dir;
	int	ret;

	if (getenv("_IN_DELTA")) return (0);
	if (bk_proj && bk_proj->root) {
		t = strdup(bk_proj->root);
	} else unless (t = sccs_root(0)) {
		return (0);
	}
	unless (streq(t, ".")) {
		dir = aprintf("%s/BitKeeper/triggers", t);
	} else {
		dir = strdup("BitKeeper/triggers");
	}
	free(t);
	lines = getTriggers(dir, "pre-delta");
	ret = lines != 0;
	freeLines(lines, free);
	free(dir);
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

private int
strip_danglers(char *name, u32 flags)
{
	char	*p;
	int	ret;

	p = aprintf("bk prs -hnd'$if(:DANGLING:){:GFILE:|:I:}' %s"
	    " | bk stripdel -%sdC -", name, (flags&SILENT) ? "q" : "");
	ret = system(p);
	if (ret) {
err:		fprintf(stderr, "%s failed\n", p);
		free(p);
		return (ret);
	}
	free(p);
	p = aprintf("bk renumber %s %s", (flags&SILENT) ? "-q" : "", name);
	if (ret = system(p)) goto err;
	free(p);
	return (0);
}

int
delta_main(int ac, char **av)
{
	sccs	*s;
	int	iflags = INIT_SAVEPROJ;
	int	dflags = 0;
	int	gflags = 0;
	int	sflags = SF_GFILE|SF_WRITE_OK;
	int	isci = 0;
	int	checkout = 0, ignorePreference = 0;
	int	c, rc, enc;
	char	*initFile = 0;
	char	*diffsFile = 0;
	char	*name;
	char	*compp = 0, *encp = 0, *ckopts = "";
	char	*mode = 0, buf[MAXPATH];
	MMAP	*diffs = 0;
	MMAP	*init = 0;
	pfile	pf;
	int	dash, errors = 0, fire;
	project	*proj = 0;

	debug_main(av);
	name = strrchr(av[0], '/');
	if (name) name++;
	else name = av[0];
	if (streq(name, "ci")) {
		if (!isdir("SCCS") && isdir("RCS")) {
			rcs("ci", ac, av);
			/* NOTREACHED */
		}
		isci = 1;
	} else if (streq(name, "delta")) {
		dflags = DELTA_FORCE;
	} else if (streq(name, "new") ||
	    streq(name, "enter") || streq(name, "add")) {
		dflags |= NEWFILE;
		sflags |= SF_NODIREXPAND;
		sflags &= ~SF_WRITE_OK;
	}

	if (ac > 1 && streq("--help", av[1])) {
		sprintf(buf, "bk help %s", name);
		system(buf);
		return (1);
	}

	while ((c =
	    getopt(ac, av, "1abcCdD:E|fg;GhI;ilm|M;npPqRrsuy|Y|Z|")) != -1) {
		switch (c) {
		    /* SCCS flags */
		    case 'n': dflags |= DELTA_SAVEGFILE; break;	/* undoc? 2.0 */
		    case 'p': dflags |= PRINT; break; 		/* doc 2.0 */
		    case 'y': 					/* doc 2.0 */
comment:		comments_save(optarg);
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
		    case 'l': ckopts = "edit";			/* doc 2.0 */ 
			      checkout = 1;
			      break;
		    case 'u': ckopts = "get";			/* doc 2.0 */
			      break;

		    /* flags with different meaning in RCS and SCCS */
		    case 'm':					/* undoc? 2.0 */
			    if (isci) goto comment;
			    /* else fall through */

		    /* obsolete SCCS flags */
		    case 'g':					/* undoc 2.0 */
		    case 'r':					/* undoc 2.0 */
			    fprintf(stderr, "-%c not implemented.\n", c);
			    goto usage;

		    /* LM flags */
		    case '1': iflags |= INIT_ONEROOT; break;	/* undoc 2.0 */
		    case 'a':					/* doc 2.0 */
		    	dflags |= DELTA_AUTO;
			dflags &= ~DELTA_FORCE;
			break;
		    case 'b':	/* -b == -Ebinary */		/* doc 2.0 */
			if (streq(name, "new")) {
		    		encp = "binary";
			} else {
				goto usage;
			}
			break;
		    case 'c': iflags |= INIT_NOCKSUM; break; 	/* doc 2.0 */
		    case 'C': dflags |= DELTA_CFILE; break;	/* doc */
		    case 'd': /* internal interface */ 		/* undoc 2.0 */
			      dflags |= DELTA_NOPENDING; break;
		    case 'D': diffsFile = optarg;		 /* doc 2.0 */
			      sflags = ~(SF_GFILE | SF_WRITE_OK);
			      break;
		    case 'G': iflags |= INIT_FIXDTIME; break;	/* undoc? 2.0 */
		    case 'h': dflags |= DELTA_HASH; break;	/* doc 2.0 */
		    case 'I': initFile = optarg; break;		/* doc 2.0 */
		    case 'M': mode = optarg; break;		/* doc 2.0 */
		    case 'P': ignorePreference = 1;  break;	/* undoc 2.0 */
		    case 'R': dflags |= DELTA_PATCH; break;	/* undoc? 2.0 */
		    case 'Y':
			if (optarg) comments_savefile(optarg);
			dflags |= DELTA_DONTASK;
			break; 	/* doc 2.0 */
		    case 'Z': 					/* doc 2.0 */
			compp = optarg ? optarg : "gzip"; break;
		    case 'E': encp = optarg; break; 		/* doc 2.0 */

		    default:
usage:			sprintf(buf, "bk help -s %s", name);
			system(buf);
			return (1);
		}
	}

	unless (ignorePreference || *ckopts) { 
		ckopts  = user_preference("checkout");
	}

	if (strieq("get", ckopts) || strieq("edit", ckopts)) {
		iflags |= INIT_FIXSTIME;
	}

	if (dflags & NEWFILE) {
		if (bk_mode() != BK_PRO) {
			compp = "gzip";
		}
		unless (ignorePreference || compp) { 
			compp  = user_preference("compression");
			unless (compp && *compp) compp = NULL;
		}
	}

	if ((encp || compp) && !(dflags & NEWFILE)) {
		fprintf(stderr, "-Z is allowed with -i option only\n");
		goto usage;
	}

	if (chk_host() || chk_user()) return (1);

	enc = sccs_encoding(0, encp, compp);
	if (enc == -1) goto usage;

	dash = av[optind] && streq(av[optind], "-");
	name = sfileFirst(av[0], &av[optind], sflags);
	/* They can only have an initFile for one file...
	 * So we go get the next file and error if there
	 * is one.
	 */
	if ((initFile || diffsFile) && name && sfileNext()) {
		fprintf(stderr,
"%s: only one file may be specified with init or diffs file.\n", av[0]);
		goto usage;
	}

	/* force them to do something sane */
	if (!comments_got() &&
	    dash && name && !(dflags & (NEWFILE|DELTA_CFILE)) && sfileNext()) {
		fprintf(stderr,
"%s: only one file may be specified without a checkin comment\n", av[0]);
		goto usage;
	}
	if (initFile && (dflags & DELTA_DONTASK)) {
		fprintf(stderr,
		    "%s: only init file or comment, not both.\n", av[0]);
		goto usage;
	}
	if ((gflags & GET_EXPAND) && (gflags & GET_EDIT)) {
		fprintf(stderr, "%s: -l and -u are mutually exclusive.\n",
			av[0]);
		goto usage;
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
		int	co = checkout;

		if (df & DELTA_DONTASK) {
			unless (d = comments_get(0)) goto usage;
		}
		if (mode) d = sccs_parseArg(d, 'O', mode, 0);
		unless (s = sccs_init(name, iflags, proj)) {
			if (d) sccs_freetree(d);
			name = sfileNext();
			errors |= 1;
			continue;
		}
		unless (proj) proj = s->proj;
		if (df & DELTA_AUTO) {
			if (HAS_SFILE(s)) {
				df &= ~NEWFILE;
			} else {
				df |= NEWFILE;
			}
		}

		/*
		 * Checkout option does not applies to ChangeSet file
		 * see rev 1.118
		 */
		unless (CSET(s)) {
			if (strieq(ckopts, "edit")) {
				gf |= GET_SKIPGET|GET_EDIT;
				df |= DELTA_SAVEGFILE;
				co = 1;
			} else if (strieq(ckopts, "get")) {
				if (hasKeyword(s))  {
					gf |= GET_EXPAND;
					s->initFlags &= ~INIT_FIXSTIME;
					co = 1;
				} else {
					co = 2;
					df |= DELTA_SAVEGFILE;
				}
			}
		}

		nrev = NULL;
		if (HAS_PFILE(s)) {
			if (newrev(s, &pf) == -1) {
				errors |= 2;
				goto next;
			}
			if (co && (gf & GET_EDIT)) nrev = pf.newrev;
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

		s->encoding = sccs_encoding(s, encp, compp);
		rc = sccs_delta(s, df, d, init, diffs, 0);
		if (rc == -4) {	/* interrupt in comment prompt */
			errors |= 4;
			break;
		}
		if (rc == -2) goto next; /* no diff in file */
		if (rc == -1) {
			sccs_whynot("delta", s);
			errors |= 4;
			goto next;
		}

		if (MONOTONIC(s) && sccs_top(s)->dangling) {
			delta	*d = sccs_getrev(s, nrev, 0, 0);
			char	key[MAXKEY];

			assert(d);
			sccs_sdelta(s, d, key);
			sccs_free(s);
			strip_danglers(name, dflags);
			s = sccs_init(name, iflags, proj);
			d = sccs_findKey(s, key);
			assert(d);
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

		if ((co == 2) || ((co == 1) && (df&NEWFILE))) {
			if (fix_gmode(s, gf)) {
				errors |= 16;
			}
			/*
			 * The 'keyword' preference for a new file might
			 * have set keywords in sccs_delta() so we need to
			 * check again.
			 */
			if (co == 2 && hasKeyword(s)) {
				gf |= GET_EXPAND;
				co = 1;
			}
		}

		if (co == 1) {
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
	sfileDone();
	comments_done();
	if (proj) proj_free(proj);
	return (errors);
}


