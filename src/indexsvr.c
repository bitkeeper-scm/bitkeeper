#include "sccs.h"

/* files smaller than SMALL are kept in memory */
#define	SMALL	1024

/*
 * Table of command opcodes for messages from bk to index server.
 * This commands can be cross version so the list needs to be stable.
 * See the handle_cmd() switch statement below for syntax.
 */
#define	IDX_QUIT	1
#define	IDX_READ	2
#define	IDX_UNLINK	3
#define	IDX_RENAME	4
#define	IDX_CHMOD	5
#define	IDX_GETDIR	6
#define	IDX_MKDIR	7
#define	IDX_RMDIR	8
#define	IDX_UPDATE	9
#define	IDX_UPDATEFS	10
#define	IDX_LSTAT	11
#define	IDX_LINK	12
#define	IDX_ACCESS	13
#define IDX_UTIME	14

typedef struct {
	char	*name;
	int	blob;		/* index of blob -1 == on disk */
	u32	offset;
	u32	st_size;
	mode_t	st_mode;
	u8	*data;		/* if dir, lines array */
} entry;

typedef	struct	{
	u32	foreground:1;
} opts;

private	int	num_ios[20];
private int	no_remap;


private	int	indexsvr(opts *opts, int sock);
private	int	handle_cmd(opts *opts, int sock, u8 *buf, int len);
private	int	mkstatus(int rc);
private	int	getu16(u8 *p);
private	void	putu16(u8 **t, u16 val);
#if 0
private	int	getu32(u8 *p);
#endif
private	void	putu32(u8 **t, u32 val);

private	char	*remap_path(char *path);
private void	unremap_name(char *name);


int
indexsvr_main(int ac, char **av)
{
	int	i, c;
	u8	rc;
	int	sock = -1, startsock;
	int	server = 0;	/* port for startsock */
	int	kill = 0;
	opts	opts;
	FILE	*f;
	char	*nav[10];
	char	call[10];

	memset(&opts, 0, sizeof(opts));
	while ((c = getopt(ac, av, "Dks;", 0)) != -1) {
		switch (c) {
		    case 'D': opts.foreground = 1; break;
		    case 'k': kill = 1; break;
		    case 's': server = atoi(optarg); break;
		    default:
usage:			sys("bk", "help", "-s", av[0], SYS);
			return (2);
		}
	}
	if (av[optind]) {
		if (chdir(av[optind])) goto usage;
		if (av[optind+1]) goto usage;
	}

	unless (isdir(BKROOT)) {
		fprintf(stderr, "%s: must be run at repository root\n", av[0]);
		return (2);
	}
	if (isdir("SCCS")) {
		no_remap = 1;
	}
	if (kill) {
		doidx_quit(0);
		return (0);
	}
	if (server || opts.foreground) {
		rc = 0;
		if (sccs_lockfile(".bk/INDEX.lock", 0, 1)) {
			/* lost race, no problem */
			rc = 1;
			goto out;
		}
		if ((sock = tcp_server(0, 0, 0)) < 0) {
			fprintf(stderr, "%s: cannot start socket\n", av[0]);
			sccs_unlockfile(".bk/INDEX.lock");
			rc = 2;
			goto out;
		}
		f = fopen(".bk/INDEX", "w");
		assert(f);
		// XXX localhost only
		fprintf(f, "127.0.0.1:%d\n", sockport(sock));
		fclose(f);

out:
		if (!opts.foreground &&
		    ((c = tcp_connect("127.0.0.1", server)) >= 0)) {
			write(c, &rc, 1);
			closesocket(c);
		}
		unless (rc) rc = indexsvr(&opts, sock);
		unless (sock == -1) closesocket(sock);
		f = fopen(".bk/NUM_IOS", "w");
		for (i = 0; i < 20; i++) {
			if (num_ios[i]) fprintf(f, "%d: %d\n", i, num_ios[i]);
		}
		fclose(f);
		unlink(".bk/INDEX");
		sccs_unlockfile(".bk/INDEX.lock");
	} else {
		mkdir(".bk", 0777);
		startsock = tcp_server("127.0.0.1", 0, 0);
		sprintf(call, "-s%d", sockport(startsock));
		nav[i=0] = "bk";
		nav[++i] = av[0];
		nav[++i] = call;
		nav[++i] = 0;
		spawnvp(_P_DETACH, nav[0], nav);
		/* wait for process to start */
		rc = 2;
		if ((sock = tcp_accept(startsock)) >= 0) {
			read(sock, &rc, 1);
			closesocket(sock);
			closesocket(startsock);
		}
	}
	return (rc);
}

struct	client
{
	int	sock;
};

private int
indexsvr(opts *opts, int sock)
{
	fd_set	fds;
	char	**clients = 0;
	int	i, len;
	int	maxfd;
	int	ready;
	int	nsock;
	int	flags = SILENT;
	struct	client	*c;
	struct	timeval	delay;
	u8	buf[4096];

	if (opts->foreground) flags &= ~SILENT;
	while (1) {
		FD_ZERO(&fds);
		FD_SET(sock, &fds);
		maxfd = sock;
		EACH(clients) {
			c = (struct client *)clients[i];
			FD_SET(c->sock, &fds);
			if (c->sock > maxfd) maxfd = c->sock;
		}
		++maxfd;
		delay.tv_sec = 30;
		delay.tv_usec = 0;
		ready = select(maxfd, &fds, 0, 0, &delay);
		if (ready == 0) {
			/* timeout */
			ttyprintf("timeout occurred\n");
			return (1);  /* XXX */
		} else if (ready < 0) {
			perror("select");
			continue; /* ignore for now */
		}
		if (FD_ISSET(sock, &fds)) {
			--ready;
			/* new connection */
			if ((nsock = tcp_accept(sock)) >= 0) {
				c = new(struct client);
				c->sock = nsock;
				// XXX read initial header
				clients = addLine(clients, c);
				verbose((stderr, "new client\n"));
			}
		}
again:		EACH(clients) {
			unless (ready > 0) break;
			c = (struct client *)clients[i];
			unless (FD_ISSET(c->sock, &fds)) continue;
			--ready;
			if (read(c->sock, buf, 2) != 2) {
quit:				close(c->sock);
				removeLineN(clients, i, free);
				goto again;
			}
			len = getu16(buf);
			assert(len <= sizeof(buf));
			if (read(c->sock, buf, len) != len) {
				ttyprintf("short read\n");
				goto quit;
			}
			verbose((stderr, "cmd %d\n", buf[0]));
			if (handle_cmd(opts, c->sock, buf, len)) return (1);
		}
	}
}

/*
 * Encodings for various enties in commands:
 *  <u16> == 2 bytes low-bytes first
 *  <u32> == 4 bytes
 *  <path> == path from root followed by \0
 *  <status> = 1 bytes status (see table)
 */

private int
handle_cmd(opts *opts, int sock, u8 *buf, int len)
{
	int	cmd, i, rc;
	u8	*p, *t;
	struct	stat	sb;
	char	**dir;
	u8	obuf[4096];

	cmd = *buf++;
	--len;
	t = obuf+2;	/* ptr for output data */

	num_ios[cmd] += 1;

	switch (cmd) {
	    case IDX_QUIT:
		/*  input: none */
		/* output: <status> */
		*t++ = 0;	/* status */
		break;
	    case IDX_READ:
		/*  input: <path> */
		/* output: <err status>
		 *         0x00 0x00 <path>
		 *	   0x00 0x01 <u16 blob#> <u32 offset>
		 *	   0x00 0x02 <data>
		 */
		*t++ = 0;	/* status */
		*t++ = 0;	/* remapped path */
		p = remap_path(buf);
		TRACE("%s->%s", buf, p);
		while (*t++ = *p++);
		break;
	    case IDX_UTIME:
		/* input: <path> <utimbuf> */
		/* output: <status> */
		p = buf + strlen(buf) + 1;
		ttyprintf("utime(%s, %p)\n", buf, p);
		*t++ = mkstatus(utime(remap_path(buf),
			(const struct utimbuf *)p));
		break;
	    case IDX_LSTAT:
		/*  input: <path> */
		/*
		 * output: <status> <u16 st_mode> <u32 st_size> <u32 st_mtime>
		 */
		if (opts->foreground) fprintf(stderr, "lstat(%s)\n", buf);
		p = remap_path(buf);
		rc = lstat(p, &sb);
		*t++ = mkstatus(rc);
		unless (rc) {
			putu16(&t, sb.st_mode);
			putu32(&t, sb.st_size);
			putu32(&t, sb.st_mtime);
		}
		break;
	    case IDX_UNLINK:
		/*  input: <path> */
		/* output: <status> */
		*t++ = mkstatus(unlink(remap_path(buf)));
		break;
	    case IDX_RENAME:
		/*  input: <path> <path> */
		/* output: <status> */
		p = buf + strlen(buf) + 1;
		*t++ = mkstatus(rename(remap_path(buf), remap_path(p)));
		break;
	    case IDX_LINK:
		/*  input: <path> <path> */
		/* output: <status> */
		p = buf + strlen(buf) + 1;
		*t++ = mkstatus(link(remap_path(buf), remap_path(p)));
		break;
	    case IDX_CHMOD:
		/*  input: <u16 mode> <path>*/
		/* output: <status> */
		*t++ = mkstatus(chmod(remap_path(buf+2), getu16(buf)));
		break;
	    case IDX_ACCESS:
		/*  input: <u8 mode> <path>*/
		/* output: <status> */
		*t++ = mkstatus(access(remap_path(buf+1), *buf));
		break;
	    case IDX_GETDIR:
		/*  input: <path> */
		/* output: file1\0file2\0file3\0 */
		if (opts->foreground) fprintf(stderr, "getdir(%s)\n", buf);
		if (isSCCS(buf)) {
			dir = getdir(remap_path(buf));
		} else {
			dir = getdir(buf);
			if (streq(buf, ".")) removeLine(dir, ".bk", free);
			concat_path(buf, buf, "SCCS");
			if (isdir(remap_path(buf))) {
				dir = addLine(dir, strdup("SCCS"));
				uniqLines(dir, free);
			}
		}
		EACH(dir) {
			len = strlen(dir[i])+1;
			if (len + 1 + (t-obuf) > sizeof(obuf)) {
				/* send partial packet */
				p = obuf;
				putu16(&p, (t-obuf)-2);
				write(sock, obuf, (t-obuf));
				t = p;
			}
			strcpy(t, dir[i]);
			t += len;
		}
		*t++ = 0;	/* null termination */
		freeLines(dir, free);
		break;
	    case IDX_MKDIR:
		/*  input: <u16 mode> <path> */
		/* output: <status> */
		if (isSCCS(buf+2)) {
			*t++ = mkstatus(mkdirp(remap_path(buf+2)));
			mkdirp(dirname(buf+2));
		} else {
			*t++ = mkstatus(mkdir(buf+2, getu16(buf)));
		}
		break;
	    case IDX_RMDIR:
		/*  input: <path> */
		/* output: <status> */
		if (isSCCS(buf)) {
			*t++ = mkstatus(rmdir(remap_path(buf)));
		} else {
			*t++ = mkstatus(rmdir(buf));
		}
		break;
	    case IDX_UPDATE:
		/*  input: <path> <data> */
		/* output: <status> */
		break;
	    case IDX_UPDATEFS:
		/*  input: <path> <path> */
		/* output: <status> */
		break;
	    default:
		fprintf(stderr, "unknown cmd: %d\n", cmd);
		assert(0);
	}
	len = (t - obuf) - 2;
	t = obuf;
	putu16(&t, len);
	len += 2;
	assert(len < sizeof(obuf));
	i = write(sock, obuf, len);
	if (i != len) {
		ttyprintf("partial write %d/%d\n", i, len);
	}
	if (cmd == IDX_QUIT) return (1);
	return (0);
}

/* add new stuff to end, not alpha! */
const int const errno_map[] = {
	0,
	E2BIG,	EACCES, EAGAIN,	EBADF,	ECHILD, EEXIST,	EFAULT,	EINTR,
	EINVAL,	EISDIR,	ELOOP,	EMFILE,	ENOENT,	ENOEXEC, ENOMEM, ENOSPC,
	ENOTDIR, ENOTEMPTY, EPIPE,EROFS,	EXDEV,
	0
};

private int
mkstatus(int rc)
{
	int	i;

	if (rc) {
		for (i = 1; errno_map[i]; i++) {
			if (errno == errno_map[i]) return (i);
		}
		ttyprintf("unknown status %d\n", errno);
		return (0xff);
	}
	return (0);
}

private int
rdstatus(int c)
{
	unless (c) return (0);
	if (c >= sizeof(errno_map)/sizeof(errno_map[0])) {
		errno = EINVAL;
	}
	errno = errno_map[c];
	return (-1);
}

private int
getu16(u8 *p)
{
	return (p[0] + (p[1] << 8));
}

#if 0
private int
getu32(u8 *p)
{
	return (p[0] + (p[1] << 8) + (p[2] << 16) + (p[3] << 24));
}
#endif

private void
putu16(u8 **p, u16 val)
{
	u8	*t = *p;

	*t++ = val & 0xff;
	*t++ = (val >> 8);
	*p = t;
}

private void
putu32(u8 **p, u32 val)
{
	u8	*t = *p;

	*t++ = val & 0xff;
	val >>= 8;
	*t++ = val & 0xff;
	val >>= 8;
	*t++ = val & 0xff;
	val >>= 8;
	*t++ = val & 0xff;
	*p = t;
}

/* REPO/dir/SCCS/X.file -> REPO/.bk/dir/SCCS/file,X */
private char *
remap_path(char *path)
{
#define NBUFS   2
        static  int     cnt = 0;
        static  char    buf[NBUFS][MAXPATH];
	char	*p;
	char	suffix[3] = ",X";

        char    *ret;

	if (no_remap || !isSCCS(path)) return (path);

	cnt = (cnt + 1) % NBUFS;
	ret = buf[cnt];
	concat_path(ret, ".bk", path);

	/* p -> s.foo */
	p = strrchr(ret, '/') + 1;

	/* we're sometimes called with path/SCCS */
	unless (*(p + 1) == '.') return (ret);

	/* save the prefix char, it'll become suffix */
	suffix[1] = *p;

	strcpy(p, p + 2);
	strcat(p, suffix);

	return (ret);
}

private	void
full_remap_path(char *buf, project *proj, char *rel)
{
	int	len;

	unless (proj) {
		strcpy(buf, rel);
		return;
	}
	concat_path(buf, proj_root(proj), remap_path(rel));

	/* buf may end in /., we'll remove that */
	len = strlen(buf);
	if (len < 2) return;
	if (buf[len - 2] == '/' && buf[len - 1] == '.') buf[len - 2] = 0;
}

private void
unremap_name(char *name)
{
	int	l;
	char	prefix;

	l = strlen(name);
	if (l < 3) return;
	unless (name[l - 2] == ',') return;

	prefix = name[l - 1];
	name[l - 2] = 0;
	memmove(name + 2, name, l - 2);
	name[0] = prefix;
	name[1] = '.';
	//name[l] = 0;
}


/* client side code ------------------------------------------ */

private int
send_cmd(int sock, u8 *buf, int len)
{
	len -= 2;
	buf[0] = len & 0xff;
	buf[1] = len >> 8;
	len += 2;
	TRACE("len=%d", len);
	if (write(sock, buf, len) != len) {
		ttyprintf("short write in send_cmd\n");
		return (-1);
	}
	if (read(sock, buf, 2) != 2) {
		ttyprintf("no len in send_cmd\n");
		return (-1);
	}
	len = getu16(buf);
	TRACE("rlen=%d", len);
	assert(len < 4096);
	if (read(sock, buf, len) != len) {
		ttyprintf("short read in send_cmd\n");
		return (-1);
	}
	return (len);
}

static	char	retbuf[MAXPATH];

int
doidx_quit(project *proj)
{
	int	sock = proj_idxsock(proj);
	int	len;
	char	buf[MAXPATH];

	buf[2] = IDX_QUIT;
	len = send_cmd(sock, buf, 3);
	if (rdstatus(buf[0])) return (-1);	/* non-zero status */
	return (0);
}

#if 0

int
doidx_remap(project *proj, char *rel, char **file)
{
	int	sock = proj_idxsock(proj);
	int	len;
	char	buf[MAXPATH];

	buf[2] = IDX_READ;
	strcpy(buf+3, rel);
	len = 4 + strlen(rel);
	len = send_cmd(sock, buf, len);
	TRACE("len=%d", len);

	if (rdstatus(buf[0])) return (-1);	/* non-zero status */
	HERE();
	if (buf[1]) return (-1);	/* not remapped */
	concat_path(retbuf, proj_root(proj), buf+2);
	*file = retbuf;
	TRACE("ret=%s", *file);
	return (0);
}

int
doidx_utime(project *proj, char *rel, const struct utimbuf *utb)
{
	int	sock = proj_idxsock(proj);
	int	len;
	char	buf[MAXPATH];

	buf[2] = IDX_UTIME;
	strcpy(buf+3, rel);
	len = 4 + strlen(rel);
	memcpy(buf+len, buf, sizeof(*utb));
	len += sizeof(*utb);
	len = send_cmd(sock, buf, len);
	return (rdstatus(buf[0]));
}

int
doidx_lstat(project *proj, char *rel, struct stat *sb)
{
	int	sock = proj_idxsock(proj);
	int	len;
	char	buf[MAXPATH];

	buf[2] = IDX_LSTAT;
	strcpy(buf+3, rel);
	len = 4 + strlen(rel);
	len = send_cmd(sock, buf, len);

	if (rdstatus(buf[0])) return (-1);	/* non-zero status */
	memset(sb, 0, sizeof(*sb));
	sb->st_mode = getu16(buf+1);
	sb->st_size = getu32(buf+3);
	sb->st_mtime = getu32(buf+7);
	sb->st_nlink = 1;
	return (0);
}

int
doidx_unlink(project *proj, char *rel)
{
	int	sock = proj_idxsock(proj);
	int	len;
	char	buf[MAXPATH];

	buf[2] = IDX_UNLINK;
	strcpy(buf+3, rel);
	len = 4 + strlen(rel);
	len = send_cmd(sock, buf, len);

	return (rdstatus(buf[0]));
}

int
doidx_rename(project *proj, char *old, char *new)
{
	int	sock = proj_idxsock(proj);
	int	len;
	char	buf[MAXPATH];

	buf[2] = IDX_RENAME;
	strcpy(buf+3, old);
	len = 4 + strlen(old);
	strcpy(buf+len, new);
	len += strlen(new) + 1;
	len = send_cmd(sock, buf, len);
	return (rdstatus(buf[0]));
}

int
doidx_link(project *proj, char *old, char *new)
{
	int	sock = proj_idxsock(proj);
	int	len;
	char	buf[MAXPATH];

	buf[2] = IDX_LINK;
	strcpy(buf+3, old);
	len = 4 + strlen(old);
	strcpy(buf+len, new);
	len += strlen(new) + 1;
	len = send_cmd(sock, buf, len);
	return (rdstatus(buf[0]));
}

int
doidx_chmod(project *proj, char *rel, mode_t mode)
{
	int	sock = proj_idxsock(proj);
	u8	*t;
	int	len;
	u8	buf[MAXPATH];

	t = buf+2;
	*t++ = IDX_CHMOD;
	putu16(&t, mode);
	strcpy(t, rel);
	t += strlen(rel) + 1;
	len = t - buf;
	len = send_cmd(sock, buf, len);
	return (rdstatus(buf[0]));
}

int
doidx_mkdir(project *proj, char *dir, mode_t mode)
{
	int	sock = proj_idxsock(proj);
	u8	*t;
	int	len;
	u8	buf[MAXPATH];

	t = buf+2;
	*t++ = IDX_MKDIR;
	putu16(&t, mode);
	strcpy(t, dir);
	t += strlen(dir) + 1;
	len = t - buf;
	len = send_cmd(sock, buf, len);
	return (rdstatus(buf[0]));
}

int
doidx_rmdir(project *proj, char *dir)
{
	int	sock = proj_idxsock(proj);
	int	len;
	char	buf[MAXPATH];

	buf[2] = IDX_RMDIR;
	strcpy(buf+3, dir);
	len = 4 + strlen(dir);
	len = send_cmd(sock, buf, len);
	return (rdstatus(buf[0]));
}

char **
doidx_getdir(project *proj, char *dir)
{
	int	sock = proj_idxsock(proj);
	int	len, i;
	char	*t;
	char	**ret = 0;
	char	buf[4096];

	buf[2] = IDX_GETDIR;
	strcpy(buf+3, dir);
	len = 4 + strlen(dir);
	TRACE("dir=%s", dir);
	len = send_cmd(sock, buf, len);
	assert(len < sizeof(buf));
	t = buf;
	while (1) {
		if (len == 0) {
			read(sock, buf, 2);
			len = getu16(buf);
			TRACE("next block len=%d", len);
			read(sock, buf, len);
			t = buf;
		}
		unless (*t) break;
		TRACE("ret=%s len=%d", t, len);

		ret = addLine(ret, strdup(t));
		i = strlen(t)+1;
		len -= i;
		t += i;
	}
	return (ret);
}

int
doidx_access(project *proj, char *rel, int mode)
{
	int	sock = proj_idxsock(proj);
	u8	*t;
	int	len;
	u8	buf[MAXPATH];

	t = buf+2;
	*t++ = IDX_ACCESS;
	*t++ = mode;
	strcpy(t, rel);
	t += strlen(rel) + 1;
	len = t - buf;
	len = send_cmd(sock, buf, len);
	return (rdstatus(buf[0]));
}

#else

/* Just access files directly */

int
doidx_remap(project *proj, char *rel, char **file)
{
	full_remap_path(retbuf, proj, rel);
	*file = retbuf;
	return (0);
}

int
doidx_utime(project *proj, char *rel, const struct utimbuf *utb)
{
	char	buf[MAXPATH];

	full_remap_path(buf, proj, rel);
	return (utime(buf, utb));
}

int
doidx_lstat(project *proj, char *rel, struct stat *sb)
{
	char	buf[MAXPATH];

	full_remap_path(buf, proj, rel);
	return (lstat(buf, sb));
}

char *
doidx_realBasename(project *proj, char *rel, char *realname)
{
	char	*ret;
	char	buf[MAXPATH];

	full_remap_path(buf, proj, rel);
	ret = realBasename(buf, realname);
	if (isSCCS(rel)) {
		unremap_name(realname);
	}
	return (ret);
}

int
doidx_unlink(project *proj, char *rel)
{
	char	buf[MAXPATH];

	full_remap_path(buf, proj, rel);
	return (unlink(buf));
}

int
doidx_rename(project *proj1, char *old, project *proj2, char *new)
{
	int	rc;
	char	buf1[MAXPATH];
	char	buf2[MAXPATH];

	full_remap_path(buf1, proj1, old);
	full_remap_path(buf2, proj2, new);
	if (!(rc = rename(buf1, buf2)) &&
	    proj1 && (proj1 == proj2) && !isSCCS(old)) {
		/*
		 * maybe we just did:
		 *    mv old new
		 * add a:
		 *    mv .bk/old .bk/new
		 * just in case.
		 */
		concat_path(buf1, proj_root(proj1), ".bk");
		concat_path(buf1, buf1, old);
		if (isdir(buf1)) {
			/*
			 * support the magic: RESYNC is a repo with its .bk
			 * in the product.  Renaming it to something else
			 * means the .bk needs to be put back inside of the
			 * new repo
			 */
			if (streq(old, "RESYNC") && !streq(new, "RESYNC")) {
				concat_path(buf2, proj_root(proj2), new);
				concat_path(buf2, buf2, ".bk");
				assert(!exists(buf2));
				mkdirf(buf2);
				rename(buf1, buf2);
			} else {
				concat_path(buf2, proj_root(proj2), ".bk");
				concat_path(buf2, buf2, new);
				mkdirf(buf2);
				rename(buf1, buf2);
			}
		}
	}
	return (rc);
}

int
doidx_link(project *proj1, char *old, project *proj2, char *new)
{
	char	buf1[MAXPATH];
	char	buf2[MAXPATH];

	full_remap_path(buf1, proj1, old);
	full_remap_path(buf2, proj2, new);
	return (link(buf1, buf2));
}

int
doidx_chmod(project *proj, char *rel, mode_t mode)
{
	u8	buf[MAXPATH];

	full_remap_path(buf, proj, rel);
	return (chmod(buf, mode));
}

int
doidx_mkdir(project *proj, char *dir, mode_t mode)
{
	u8	buf[MAXPATH];

	full_remap_path(buf, proj, dir);
	if (isSCCS(dir)) {
		mkdirp(buf);
		full_remap_path(buf, proj, dirname(dir));
		return (mkdirp(buf));
	} else {
		return (mkdir(buf, mode));
	}
}

int
doidx_rmdir(project *proj, char *dir)
{
	int	idf = 0;
	char	buf[MAXPATH], buf2[MAXPATH];

	if (!isSCCS(dir) && !streq(dir, ".")) {
		/*
		 * Can't remove a directory with a SCCS subdir, and
		 * need to keep .bk directories in sync
		 */
		sprintf(buf2, "%s/.bk/%s", proj_root(proj), dir);
		if (isdir(buf2)) {
			unless (emptyDir(buf2)) {
				errno = ENOTEMPTY;
				return (-1);
			}
			rmdir(buf2);
		}
	}
	full_remap_path(buf, proj, dir);

	/*
	 * Being called with "." means we are deleting the whole repository
	 * so make sure we delete .bk as well.
	 */
	if (streq(dir, ".")) {
		concat_path(buf2, buf, ".bk");
		idf = isdir(buf2);
		assert(idf);
		rmdir(buf2);
	}

	return (rmdir(buf));
}

char **
doidx_getdir(project *proj, char *dir)
{
	int	i;
	char	**ret;
	char	tmp[MAXPATH];
	char	buf[MAXPATH];

	full_remap_path(buf, proj, dir);
	ret = getdir(buf);
	if (isSCCS(dir)) {
		EACH(ret) {
			unremap_name(ret[i]);
		}
	} else {
		if (streq(dir, ".")) removeLine(ret, ".bk", free);
		concat_path(tmp, dir, "SCCS");
		full_remap_path(buf, proj, tmp);
		if (isdir(buf)) {
			ret = addLine(ret, strdup("SCCS"));
			uniqLines(ret, free);
		}
	}
	return (ret);
}

int
doidx_access(project *proj, char *rel, int mode)
{
	char	buf[MAXPATH];

	full_remap_path(buf, proj, rel);
	return (access(buf, mode));
}
#endif
