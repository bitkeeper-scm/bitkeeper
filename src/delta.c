/* Copyright (c) 1997 L.W.McVoy */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

int	newrev(sccs *s, pfile *pf);

private int
hasKeyword(sccs *s)
{
	return (s->xflags & (X_RCS|X_SCCS));
}

private int
fix_gmode(sccs *s, int gflags)
{
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

int
delta_main(int ac, char **av)
{
	sccs	*s;
	int	iflags = INIT_SAVEPROJ|INIT_FIXSTIME;
	int	Lflag = 0;
	int	dflags = 0;
	int	gflags = 0;
	int	sflags = SF_GFILE|SF_WRITE_OK;
	int	isci = 0;
	int	checkout = 0, ignorePreference = 0;
	int	c, rc, enc;
	int	save_gfile_if_no_key_word = 0;
	char	*initFile = 0;
	char	*diffsFile = 0;
	char	*name;
	char	*compp = 0, *encp = 0, *ckopts = "";
	char	*mode = 0, buf[MAXPATH];
	MMAP	*diffs = 0;
	MMAP	*init = 0;
	pfile	pf;
	int	dash, errors = 0;
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
	}

	if (ac > 1 && streq("--help", av[1])) {
		sprintf(buf, "bk help %s", name);
		system(buf);
		return (1);
	}

	while ((c = getopt(ac, av,
			   "1abcdD:E|fg;GhI;ilLm|M;npPqRrsuy|YZ|")) != -1) {
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
			      break;
		    case 'L': Lflag = 1;;
			      /* fall thru */
		    case 'l': gflags |= GET_SKIPGET|GET_EDIT; 	/* doc 2.0 */
		    	      dflags |= DELTA_SAVEGFILE;
			      checkout = 1;
			      break;
		    case 'u': gflags |= GET_EXPAND;		 /* doc 2.0 */
			      checkout = 1;
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
		    case 'Y': dflags |= DELTA_DONTASK; break; 	/* doc 2.0 */
		    case 'Z': 					/* doc 2.0 */
			compp = optarg ? optarg : "gzip"; break;
		    case 'E': encp = optarg; break; 		/* doc 2.0 */

		    default:
usage:			sprintf(buf, "bk help -s %s", name);
			system(buf);
			return (1);
		}
	}

	unless (ignorePreference || checkout) {
		ckopts  = user_preference("checkout");
	}

	if ((encp || compp) && !(dflags & NEWFILE)) {
		fprintf(stderr, "-Z is allowed with -i option only\n");
		goto usage;
	}

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
	    dash && name && !(dflags & NEWFILE) && sfileNext()) {
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

	while (name) {
		delta	*d = 0;
		char	*nrev;

		if (dflags & DELTA_DONTASK) {
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
		if (dflags & DELTA_AUTO) {
			if (HAS_SFILE(s)) {
				dflags &= ~NEWFILE;
			} else {
				dflags |= NEWFILE;
			}
		}

		/*
		 * Checkout option does not applies to ChangeSet file
		 * see rev 1.118
		 */
		unless (CSET(s)) {
			if (streq(ckopts, "edit")) {
				gflags |= GET_SKIPGET|GET_EDIT;
				dflags |= DELTA_SAVEGFILE;
				s->initFlags &= ~INIT_FIXSTIME;
				checkout = 1;
			} else if (streq(ckopts, "EDIT")) { /* edit+fixmtime */
				gflags |= GET_SKIPGET|GET_EDIT;
				dflags |= DELTA_SAVEGFILE;
				checkout = 1;
			} else if (streq(ckopts, "get")) {
					gflags |= GET_EXPAND;
					checkout = 1;
			} else if (streq(ckopts, "GET")) {
				if (hasKeyword(s))  {
					gflags |= GET_EXPAND;
					s->initFlags &= ~INIT_FIXSTIME;
					checkout = 1;
				} else {
					checkout = 2;
					dflags |= DELTA_SAVEGFILE;
				}
			} else unless (Lflag) {
				s->initFlags &= ~INIT_FIXSTIME;
			}
		}

		nrev = NULL;
		if (HAS_PFILE(s)) {
			if (newrev(s, &pf) == -1) {
				errors |= 2;
				goto next;
			}
			if (checkout && (gflags &GET_EDIT)) nrev = pf.newrev;
		}

		s->encoding = sccs_encoding (s, encp, compp);
		rc = sccs_delta(s, dflags, d, init, diffs, 0);
		if (rc == -2) goto next; /* no diff in file */
		if (rc == -1) {
			sccs_whynot("delta", s);
			errors |= 4;
			goto next;
		}

		s = sccs_restart(s);
		unless (s) {
			fprintf(stderr,
			    "%s: can't restart %s\n", av[0], name);
			errors |= 8;
			goto next;
		}

		if ((checkout == 2) ||
		    ((checkout == 1) && (dflags&NEWFILE))) {
			if (fix_gmode(s, gflags)) {
				errors |= 16;
			}
		}

		if (checkout == 1) {
			if (rc == -3) nrev = pf.oldrev;
			if (sccs_get(s, nrev, 0, 0, 0, gflags, "-")) {
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


