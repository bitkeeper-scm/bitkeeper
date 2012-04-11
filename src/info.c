/*
 * Copyright (c) 2012, BitMover, Inc.  All rights reserved.
 */
#include "sccs.h"
#include "regex.h"

#define	INFO_TABLE	1	/* open / create a table */
#define	INFO_DELETE	2	/* delete items */
#define	INFO_GET	3	/* print items */
#define	INFO_INSERT	4	/* insert if not already there */
#define	INFO_SET	5	/* set, replacing any earlier value */
#define	INFO_UNIQUE	6	/* return a unique integer */
#define	INFO_VERSION	7	/* print info version */
#define	INFO_COUNT	8	/* count up items matching a regexp */

/* All hidden (metadata) entries start with space */
#define	DB_VERSION	" version"
#define	DB_UNIQUE	" unique"

/* if this doesn't match we need to do a conversion */
#define	VERSION		"1.0"

#define	PORT		0x6569	/* Wayne's birthday 6/5/69 */

private	int	info_cmds(FILE *in, FILE *out, int x);
private	void	op(FILE *o, FILE *l, int cmd, hash *db, hash *h, char *regexp);
private	void	unique(FILE *out, FILE *log, hash *db, char *arg);
private	void	version(FILE *out, hash *db);
private	int	flags;
private	int	do_sync = 1;

int
info_server_main(int ac, char **av)
{
	int	c, sock, nsock;
	int	port = PORT;
	int	dashx = 0;
	char	*peer;
	FILE	*fin, *fout;

	while ((c = getopt(ac, av, "fp:qx", 0)) != EOF) {
		switch (c) {
		    case 'f': do_sync = 0; break;
		    case 'p': port = atoi(optarg); break;
		    case 'q': flags |= SILENT; break;
		    case 'x': dashx = 1; break;
		}
	}
	sock = tcp_server(0, port, 0);
	if (sock == -1) exit(1);
	verbose((stderr, "started server on port %d\n", sockport(sock)));

	while (1) {
		if ((nsock = tcp_accept(sock)) < 0) continue;
		peer = peeraddr(nsock);
		verbose((stderr, "connection from %s\n", peer));
		fin = fdopen(nsock, "r");
		fout = fdopen(nsock, "w");
		info_cmds(fin, fout, dashx);
		fclose(fin);
		fclose(fout);
		verbose((stderr, "%s is done\n", peer));
	}
	return (1);
}

int
info_shell_main(int ac, char **av)
{
	int	c;
	int	dashx = 0;

	while ((c = getopt(ac, av, "fx", 0)) != EOF) {
		switch (c) {
		    case 'f': do_sync = 0; break;
		    case 'x': dashx = 1; break;
		}
	}
	info_cmds(stdin, stdout, dashx);
	return (0);
}

private int
info_cmds(FILE *in, FILE *out, int dashx)
{
	int	fd, cmd, space, new;
	char	*p, *t, *arg;
	hash	*h = 0;
	hash	*db = 0;
	char	*here;
	int	oflags = O_WRONLY|O_APPEND|O_CREAT;
	FILE	*log = 0;

#ifdef	O_SYNC
	if (do_sync) oflags |= O_SYNC;
#endif
	setlinebuf(in);
	here = strdup(proj_cwd());

	/*
	 * Parse the command and put the trailing, if any, in arg.
	 * Load the hash, if any.
	 * Then do the command.
	 */
	while (1) {
		fflush(out);
		if (log) fflush(log);
#ifndef	O_SYNC
		if (log) fsync(fileno(log));
#endif
		unless (p = fgetline(in)) break;
		if (dashx) fprintf(out, "IN-%s\n", p);
		arg = 0;
		space = 0;
		for (t = p; *t && !isspace(*t); t++);
		if (isspace(*t)) {
			space = *t;
			*t = 0;
		}
		if (streq(p, "table")) {
			cmd = INFO_TABLE;
		} else if (streq(p, "delete")) {
			cmd = INFO_DELETE;
		} else if (streq(p, "get")) {
			cmd = INFO_GET;
		} else if (streq(p, "insert")) {
			cmd = INFO_INSERT;
		} else if (streq(p, "set")) {
			cmd = INFO_SET;
		} else if (streq(p, "unique")) {
			cmd = INFO_UNIQUE;
		} else if (streq(p, "version")) {
			cmd = INFO_VERSION;
		} else if (streq(p, "count")) {
			cmd = INFO_COUNT;
		} else if (streq(p, "quit")) {
			fprintf(out, "OK-Good Bye\n");
			break;
		} else {
			if (space) *t = space;
bad:			fprintf(out,
			    "ERROR-bad cmd '%s' (%u bytes)\n",
			    p, (u32)strlen(p));
			continue;
		}
		if (space) {
			*t = space;
			for (t++; *t && isspace(*t); t++);
			if (*t) arg = t;
		}

		switch (cmd) {
		    case INFO_TABLE:
			if (db) hash_close(db);
			if (log) fclose(log);
			log = 0;
			db = 0;
			(void)chdir(here);
			unless (arg) goto bad;
			mkdirp(arg);
			if (chdir(arg)) {
				fprintf(out, "ERROR-table %s not found\n", arg);
				continue;
			}
			new = !exists("db");
			db = hash_open(HASH_MDBM, "db", O_RDWR|O_CREAT, 0664);
			unless (db) {
				fprintf(out, "ERROR-hash open of %s/db\n", arg);
				continue;
			}
			if (new && !hash_insertStr(db, DB_VERSION, VERSION)) {
				fprintf(out,
				    "ERROR-hash store version in %s\n", arg);
				hash_close(db);
				db = 0;
				continue;
			}
			fd = open("log", oflags, 0664);
			unless (log = fdopen(fd, "a")) {
				fprintf(out, "ERROR-open of %s/log\n", arg);
				hash_close(db);
				db = 0;
				continue;
			}
			if (new) fprintf(log, "table %s\n", arg);
			fprintf(out, "OK-table %s opened\n", arg);
			break;

		    case INFO_INSERT:
		    case INFO_SET:
			if (arg) goto bad;
		    case INFO_DELETE:
		    case INFO_GET:
			unless (db) {
table_first:			fprintf(out, "ERROR-call table first\n");
				continue;
			}
			if (h) {
				hash_free(h);
				h = 0;
			}
			unless (arg) h = hash_fromStream(0, in);
			op(out, log, cmd, db, h, arg);
			break;

		    case INFO_COUNT:
		    	unless (db) goto table_first;
			op(out, log, cmd, db, h, arg);
			break;

		    case INFO_UNIQUE:
		    	unless (db) goto table_first;
			unique(out, log, db, arg);
			break;

		    case INFO_VERSION:
			if (db) {
				version(out, db);
			} else {
				fprintf(out, "OK-info version=%s\n", VERSION);
			}
			break;
		}
	}
	chdir(here);
	free(here);
	if (db) hash_close(db);
	if (log) fclose(log);
	fflush(out);
	return (0);
}

private void
op(FILE *out, FILE *log, int cmd, hash *db, hash *h, char *regexp)
{
	int	n = 0;
	hash	*h2 = hash_new(HASH_MEMHASH);

	if ((cmd == INFO_COUNT) && !regexp) {
		EACH_HASH(db) {
			if (*(char *)db->kptr == ' ') continue;
			n++;
		}
	}

	/*
	 * delete | get | count
	 */
	if (regexp) {
		assert(!h);
		if (re_comp(regexp)) {
			fprintf(out, "ERROR-bad regexp %s\n", regexp);
			goto err;
		}
		switch (cmd) {
		    case INFO_DELETE:
		    case INFO_GET:
		    case INFO_COUNT:
		    	break;
		    default:
		    	fprintf(out,
			    "ERROR-bad command %d with regexp %s\n",
			    cmd, regexp);
			hash_free(h2);
			return;
		}
		unless (cmd == INFO_COUNT) h = hash_new(HASH_MEMHASH);
		EACH_HASH(db) {
			if (*(char *)db->kptr == ' ') continue;
			unless (re_exec(db->kptr)) continue;
			if (cmd == INFO_COUNT) {
				n++;
			} else {
				hash_store(h, db->kptr, db->klen, "", 1);
			}
		}
	}

	if (cmd == INFO_COUNT) {
		fprintf(out, "OK-%d keys\n", n);
		hash_free(h2);
		return;
	}

	/*
	 * delete | get | insert | set
	 */
	EACH_HASH(h) {
		/* save original value from db */
		if (hash_fetch(db, h->kptr, h->klen)) {
			hash_store(h2,
			    h->kptr, h->klen,
			    db->vptr, db->vlen);
		}
		switch (cmd) {
		    case INFO_DELETE:
			unless (hash_delete(db, h->kptr, h->klen) == 0) {
				fprintf(out, "ERROR-delete %s failed\n",
				    (char *)h->kptr);
				goto err;
			}
			n++;
			break;

		    case INFO_GET:
			if (db->vlen) n++;	/* XXX: when is vlen 0 ? */
			break;

		    case INFO_INSERT:
			unless (hash_insert(db,
				h->kptr, h->klen, h->vptr, h->vlen)) {
				fprintf(out, "ERROR-insert of %s failed\n",
				    (char *)h->kptr);
				goto err;
			}
			n++;
			break;

		    case INFO_SET:
			unless (hash_store(db,
				h->kptr, h->klen, h->vptr, h->vlen)) {
				fprintf(out, "ERROR-set of %s failed\n",
				    (char *)h->kptr);
				goto err;
			}
			n++;
			break;
		}
	}
	switch (cmd) {
	    case INFO_INSERT:
	    	fprintf(log, "insert\n");
	    case INFO_SET:
		unless (cmd == INFO_INSERT) fprintf(log, "set\n");
		hash_toStream(h, log);
		fprintf(log, "@\n");
		fprintf(out, "OK-%d %s\n",
		    n, cmd == INFO_SET ? "set" : "inserted");
		break;
	    case INFO_DELETE:
		fprintf(log, "delete\n");
		hash_toStream(h, log);
		fprintf(log, "@\n");
		fprintf(out, "OK-%d deleted\n", n);
		break;
	    case INFO_GET:
		fprintf(out, "OK-%d found\n", n);
		hash_toStream(h2, out);
		fprintf(out, "@\n");
		break;
	}
	hash_free(h2);
	return;

err:	/* restore any entry changed before error */
	EACH_HASH(h2) {
		hash_store(db, h2->kptr, h2->klen, h2->vptr, h2->vlen);
	}
	hash_free(h2);
}

/*
 * Return a monotonically increasing integer.
 */
private	void
unique(FILE *out, FILE *log, hash *db, char *arg)
{
	u32	u = 0;

	u = hash_fetchStrNum(db, DB_UNIQUE);
	u++;
	if (arg) u = atoi(arg);
	fprintf(out, "OK-%u\n", u);
	fprintf(log, "unique %u\n", u);
	hash_storeStrNum(db, DB_UNIQUE, u);
}

private	void
version(FILE *out, hash *db)
{
	char	*v = hash_fetchStr(db, DB_VERSION);

	if (v) {
		fprintf(out, "OK-stored version=%s\n", v);
	} else {
		fprintf(out, "ERROR-no stored version\n");
	}
}
