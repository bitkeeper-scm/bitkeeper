/* Copyright (c) 1997 L.W.McVoy */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

int	newrev(sccs *s, pfile *pf);

char *
user_preference(char *what, char buf[MAXPATH])
{
	char *p;

	unless (bk_proj) return "";
	unless (bk_proj->config) {
		unless (bk_proj->root) return "";
		bk_proj->config = loadConfig(bk_proj->root, 0);
		unless (bk_proj->config) return "";
	}
	p = mdbm_fetch_str(bk_proj->config, what);
	unless (p) p = "";
	return (p);
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
	char	**syms = 0;
	char	*compp = 0, *encp = 0, *p;
	char	*mode = 0, buf[MAXPATH];
	MMAP	*diffs = 0;
	MMAP	*init = 0;
	pfile	pf;
	int	dash;
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
	} else if (streq(name, "new")) {
		dflags |= NEWFILE;
		sflags |= SF_NODIREXPAND;
	}

	if (ac > 1 && streq("--help", av[1])) {
		sprintf(buf, "bk help %s", name);
		system(buf);
		return (1);
	}

	while ((c = getopt(ac, av,
			   "1abcdD:E|fg;GhI;ilm|M;npPqRrS;suy|YZ|")) != -1) {
		switch (c) {
		    /* SCCS flags */
		    case 'n': dflags |= DELTA_SAVEGFILE; break;
		    case 'p': dflags |= PRINT; break;
		    case 'y':
comment:		comments_save(optarg);
			dflags |= DELTA_DONTASK;
			break;
		    case 's': /* fall through */

		    /* RCS flags */
		    case 'q': dflags |= SILENT; gflags |= SILENT; break;
		    case 'f': dflags |= DELTA_FORCE; break;
		    case 'i': dflags |= NEWFILE;
			      sflags |= SF_NODIREXPAND;
			      break;
		    case 'l': gflags |= GET_SKIPGET|GET_EDIT;
		    	      dflags |= DELTA_SAVEGFILE;
			      checkout = 1;
			      break;
		    case 'u': gflags |= GET_EXPAND;
			      checkout = 1;
			      break;

		    /* flags with different meaning in RCS and SCCS */
		    case 'm':
			    if (isci) goto comment;
			    /* else fall through */

		    /* obsolete SCCS flags */
		    case 'g':
		    case 'r':
			    fprintf(stderr, "-%c not implemented.\n", c);
			    goto usage;

		    /* LM flags */
		    case '1': iflags |= INIT_ONEROOT; break;
		    case 'a':
		    	dflags |= DELTA_AUTO;
			dflags &= ~DELTA_FORCE;
			break;
		    case 'b':	/* -b == -Ebinary */
			if (streq(name, "new")) {
		    		enc = E_BINARY;
			} else {
				goto usage;
			}
			break;
		    case 'c': iflags |= INIT_NOCKSUM; break;
		    case 'd': /* internal interface, do not document */
			      dflags |= DELTA_NOPENDING; break;
		    case 'D': diffsFile = optarg;
			      sflags = ~(SF_GFILE | SF_WRITE_OK);
			      break;
		    case 'G': iflags |= INIT_GTIME; break;
		    case 'h': dflags |= DELTA_HASH; break;
		    case 'I': initFile = optarg; break;
		    case 'M': mode = optarg; break;
		    case 'P': ignorePreference = 1;  break;
		    case 'R': dflags |= DELTA_PATCH; break;
		    case 'S': syms = addLine(syms, strdup(optarg)); break;
		    case 'Y': dflags |= DELTA_DONTASK; break;
		    case 'Z': compp = optarg ? optarg : "gzip"; break;
		    case 'E': encp = optarg; break;

		    default:
usage:			sprintf(buf, "bk help -s %s", name);
			system(buf);
			return (1);
		}
	}

	unless (ignorePreference || checkout) {
		p = user_preference("checkout", buf);
		if (streq(p, "edit")) {
			gflags |= GET_SKIPGET|GET_EDIT;
			dflags |= DELTA_SAVEGFILE;
			checkout = 1;
		} else if (streq(p, "get")) {
			gflags |= GET_EXPAND;
			checkout = 1;
		}
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

		nrev = NULL;
		if (HAS_PFILE(s)) {
			if (newrev(s, &pf) == -1) goto next;
			if (checkout && (gflags &GET_EDIT)) nrev = pf.newrev;
		}
		s->encoding = sccs_encoding (s, encp, compp);
		rc = sccs_delta(s, dflags, d, init, diffs, syms);
		if (rc == -2) goto next; /* no diff in file */
		if (rc == -1) {
			sccs_whynot("delta", s);
			if (init) mclose(init);
			sccs_free(s);
			comments_done();
			sfileDone();
			freeLines(syms);
			return (1);
		}
		if (checkout) {
			s = sccs_restart(s);
			unless (s) {
				fprintf(stderr,
				    "%s: can't restart %s\n", av[0], name);
				goto next;
			}
			if (rc == -3) nrev = pf.oldrev;

			/*
			 * If GET_SKIPGET is set, sccs_get() will
			 * remove the gfile if it is readonly. (don't know why)
			 * So need to fix up the mode before we call sccs_get()
			 * note that this only happen with new file, For
			 * non-new file, if the gfile is not writable
			 * the sccs_delta() above should have failed.
			 */
			if ((dflags&NEWFILE) &&
					(gflags&GET_EDIT) && !IS_WRITABLE(s)) {
				if (chmod(s->gfile, s->mode|0200)) {
					perror(s->gfile);
				}
				s->mode |= 0200;
			}

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
	freeLines(syms);
	if (proj) proj_free(proj);
	return (0);
}


