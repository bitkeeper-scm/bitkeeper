/*
 * Copyright 1997-2016 BitMover, Inc
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

int
fix_gmode(sccs *s, int gflags)
{
	ser_t	d;

	/*
	 * Do not fix mode of symlink target, the file may not be 
	 * under BK control.
	 */
	if (S_ISLNK(s->mode)) return (0);

	if ((gflags&GET_EDIT) && WRITABLE(s))  return (0);
	if (!(gflags&GET_EDIT) && !WRITABLE(s))  return (0);

	d = sccs_top(s);
	if (MODE(s, d)) s->mode = MODE(s, d);
	unless (gflags&GET_EDIT) {
		s->mode &= ~0222;	/* turn off write mode */
	}

	if (chmod(s->gfile, s->mode)) return (1);
	return (0);
}

private int
delta_trigger(sccs *s)
{
	char	*p;
	int	ret;
	char	here[MAXPATH];

	/* set up enviroment before the cd2root */
	p = proj_relpath(s->proj, s->gfile);
	safe_putenv("BK_FILE=%s", p);
	free(p);
	putenv("BK_EVENT=delta");

	/* pushd repo root */
	strcpy(here, proj_cwd());
	if (chdir(proj_root(s->proj))) {
		perror("to pre-delta trigger");
		exit (1);
	}

	ret = trigger("delta", "pre");

	/* popd */
	if (chdir(here)) {
		perror("from pre-delta trigger");
		exit (1);
	}
	putenv("BK_FILE=");
	return (ret);
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
	ser_t	d;
	FILE	*f;

	s = sccs_init(name, INIT_WACKGRAPH);
	assert(s);
	for (d = TABLE(s); d >= TREE(s); d--) {
		if (DANGLING(s, d)) revs = addLine(revs, strdup(REV(s, d)));
	}
	sccs_free(s);
	p = aprintf("bk -?BK_NO_REPO_LOCK=YES stripdel -%sdC -", (flags&SILENT) ? "q" : "");
	f = popen(p, "w");
	EACH(revs) {
		fprintf(f, "%s|%s\n", name, revs[i]);
	}
	freeLines(revs, free);
	if (i = SYSRET(pclose(f))) {
		fprintf(stderr, "%s failed\n", p);
		free(p);
		return (i);
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
	if ((XFLAGS(s, d) & X_MONOTONIC) &&
	    !(XFLAGS(s, PARENT(s, d)) & X_MONOTONIC)) {
		char	**comments = 0;
		char	*c;

		if (HAS_COMMENTS(s, d)) {
			c = strdup(COMMENTS(s, d));
			chomp(c);
			comments = addLine(comments, c);
		}
		comments = addLine(comments, strdup("Turn on MONOTONIC flag"));
		comments_set(s, d, comments);
		freeLines(comments, free);
	}
	sccs_adminFlag(s, NEWCKSUM|ADMIN_FORCE);
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
	char	*didciFile = 0;
	char	*initFile = 0;
	char	*diffsFile = 0;
	char	*prog, *name;
	char	*encp = 0;
	char	*mode = 0;
	FILE	*diffs = 0;
	FILE	*init = 0;
	FILE	*didci = 0;
	pfile	pf = {0};
	int	dash, dbsort = 1, errors = 0, fire, dangling;
	off_t	sz;
	project	*locked_proj = 0;
	int	prefer_cfile = 0;
	char	here[MAXPATH];
	longopt	lopts[] = {
		{ "csetmark", 301 },
		{ "prefer-cfile", 302 },
		{ "did-ci;", 303 },
		{ 0, 0 }
	};

	prog = strrchr(av[0], '/');
	if (prog) prog++;
	else prog = av[0];
	if (streq(prog, "new") ||
	    streq(prog, "enter") || streq(prog, "add")) {
		dflags |= DELTA_NEWFILE;
		sflags |= SF_NODIREXPAND;
		sflags &= ~SF_WRITE_OK;
	} else if (streq(prog, "dbnew")) {
		dflags |= DELTA_NEWFILE | DELTA_DB;
		sflags |= SF_NODIREXPAND;
		sflags &= ~SF_WRITE_OK;
	}

	while ((c =
	    getopt(ac, av, "abcCdD:E|fGI;ilm|M;npPqRsSTuy|Y|Z|", lopts)) != -1) {
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
			trigger_setQuiet(1);
			dflags |= SILENT; gflags |= SILENT;
			break;
		    case 'f': dflags |= DELTA_FORCE; break;	/* doc 2.0 ci */
		    case 'i': dflags |= DELTA_NEWFILE; 		/* doc 2.0 */
			      sflags |= SF_NODIREXPAND;
			      sflags &= ~SF_WRITE_OK;
			      break;
		    case 'l': checkout |= CO_EDIT;		/* doc 2.0 */ 
			      break;
		    case 'u': checkout |= CO_GET;		/* doc 2.0 */
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
			bk_setConfig("compression", optarg ? optarg : "gzip");
			break;
		    case 'E': encp = optarg; break; 		/* doc 2.0 */
		    case 'S': dbsort = 0; break;		/* undoc */
		    case 301:	// --csetmark
			dflags |= DELTA_CSETMARK; break;	/* undoc */
		    case 302:	// --prefer-cfile
			prefer_cfile = 1; break;		/* undoc */
		    case 303:	// --did-ci=
			didciFile = optarg;
			break;

		    default: bk_badArg(c, av);
		}
	}


	if (encp) {
		unless (dflags & DELTA_NEWFILE) {
			fprintf(stderr,
			    "Encoding is allowed only when creating files\n");
			usage();
		}

		/* check that they gave us something we can parse */
		if (sccs_encoding(0, 0, encp) == -1) usage();
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
	    dash && name && !(dflags & (DELTA_NEWFILE|DELTA_CFILE)) && sfileNext()) {
		fprintf(stderr,
"%s: only one file may be specified without a checkin comment\n", av[0]);
		usage();
	}
	if (!comments_got() && prefer_cfile) {
		fprintf(stderr,
		    "%s: must have -y or -Y when using --prefer-cfile\n", prog);
		usage();
	}
	if (initFile && (dflags & DELTA_DONTASK)) {
		fprintf(stderr,
		    "%s: only init file or comment, not both.\n", av[0]);
		usage();
	}
	if ((checkout & CO_GET) && (checkout & CO_EDIT)) {
		fprintf(stderr, "%s: -l and -u are mutually exclusive.\n",
			av[0]);
		usage();
	}
	if (diffsFile && !(diffs = fopen(diffsFile, "r"))) {
		fprintf(stderr, "%s: diffs file '%s': %s.\n",
			av[0], diffsFile, strerror(errno));
	       return (1);
	}
	if (initFile && !(init = fopen(initFile, "r"))) {
		fprintf(stderr,"%s: init file '%s': %s.\n",
			av[0], initFile, strerror(errno));
		return (1);
	}
	if (didciFile && !(didci = fopen(didciFile, "w"))) {
		fprintf(stderr, "%s: didci file '%s': %s.\n",
		    av[0], didciFile, strerror(errno));
		return (1);
	}
	if (fire = (getenv("_IN_DELTA") == 0)) putenv("_IN_DELTA=YES");
	strcpy(here, proj_cwd());
	
	while (name) {
		ser_t	d = 0;
		char	*nrev;
		int	df = dflags;
		int	gf = gflags;
		int	reget = 0;
		int	co;

		unless (s = sccs_init(name, iflags)) {
			name = sfileNext();
			errors |= 1;
			continue;
		}
		if (CSET(s)) {
			unless (dash) {
				fprintf(stderr, "%s: skipping ChangeSet\n",
				    prog);
			}
			goto next;
		}
		if (mode) d = sccs_parseArg(s, d, 'O', mode, 0);
		if (df & DELTA_AUTO) {
			if (HAS_SFILE(s)) {
				df &= ~DELTA_NEWFILE;
			} else {
				df |= DELTA_NEWFILE;
			}
		}
		/* DB files must be sorted unless specified otherwise. */
		if (DB(s)) df |= DELTA_DB;
		if ((df & DELTA_DB) && dbsort) {
			if (db_sort(s->gfile, s->gfile)) {
				fprintf(stderr,
				    "Error loading DB file %s, skipped\n",
				    s->gfile);
				errors |= 4;
				goto next;
			}
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
				if (HAS_KEYWORDS(s))  {
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
			if (sccs_read_pfile(s, &pf)) {
				errors |= 2;
				goto next;
			}
			nrev = pf.newrev;
		}
		if (df & DELTA_DONTASK) {
			unless (d = comments_get(s->gfile, nrev, s, d)) {
				errors |= 1;
				break;
			}
			if (prefer_cfile) comments_readcfile(s, 0, d);
		}
		if (fire && proj_root(s->proj) &&
		    (locked_proj != s->proj)) {
			if (locked_proj) {
				chdir(proj_root(locked_proj));
				cmdlog_unlock(CMD_WRLOCK);
				proj_free(locked_proj);
			}
			chdir(proj_root(s->proj));
			cmdlog_lock(CMD_WRLOCK);
			locked_proj = proj_init(proj_root(s->proj));;
			chdir(here);
		}
		if (fire && proj_hasDeltaTriggers(s->proj)) {
			win32_close(s);
			switch (delta_trigger(s)) {
			    case 0:
				break;
			    case 2: /* trigger ran delta, we won't */
				goto next;
			    default:
				errors |= 32;
				goto next;
			}
		}

		if (df & DELTA_NEWFILE) {
			sz = diffs ? size(diffsFile) : size(s->gfile);
			s->encoding_in = s->encoding_out =
			    sccs_encoding(s, sz, encp);
		}

		dangling = MONOTONIC(s) && DANGLING(s, sccs_top(s));
		if (dangling) df |= DELTA_MONOTONIC;
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

		if (didci) {
			fprintf(didci, "%s|%s\n",
			    s->gfile, REV(s, sccs_top(s)));
		}

		if (dangling) {
			sccs_free(s);
			strip_danglers(name, dflags);
			s = sccs_init(name, iflags);
			assert(s);
			d = TABLE(s);
			assert(d);
			assert(!TAG(s, d));
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
			 * If delta -Ddiffs with no gfile, then no GFILE,
			 * and we'd like to get one.
			 *
			 * fix_gmode() will fail if we don't own
			 * the gfile, in that case set the stage
			 * to re-get it
			 */
			if (!HAS_GFILE(s) || fix_gmode(s, gf)) {
				reget = 1;
				gf &= ~GET_SKIPGET;
			}
		}
		if (df & DELTA_NEWFILE) {
			/*
			 * The 'keyword' preference for a new file might
			 * have set keywords in sccs_delta() so we need to
			 * check again.
			 */
			if (HAS_KEYWORDS(s) && !(gf & GET_EDIT)) {
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
			nrev = (rc == -3) ? pf.oldrev : REV(s, TABLE(s));
			if (sccs_get(s, nrev, 0, 0, 0, gf, s->gfile, 0)) {
				unless (BEEN_WARNED(s)) {
					fprintf(stderr,
					    "get of %s failed, skipping it.\n",
					    name);
				}
			}
		}
next:		if (init) fclose(init);
		/*
		 * No, sccs_diffs() does this.
		 * if (diffs) fclose(diffs);
		 */
		sccs_free(s);
		free_pfile(&pf);
		unless (df & DELTA_DONTASK) comments_done();
		name = sfileNext();
	}
	if (didci) fclose(didci);
	if (sfileDone()) errors |= 64;
	if (locked_proj) {
		chdir(proj_root(locked_proj));
		cmdlog_unlock(CMD_WRLOCK);
		proj_free(locked_proj);
		chdir(here);
	}
	comments_done();
	return (errors);
}
