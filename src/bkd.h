#ifndef _BKD_H
#define	_BKD_H

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "lib_tcp.h"
#include "system.h"
#include "sccs.h"
#include "zlib/zlib.h"

#define	BKD_VERSION	"BKD version 1.1"

/*
 * Functions take (int ac, char **av)
 * do whatever, and return 0 or -1.
 * Commands are allowed to read/write state from the global Opts.
 */
typedef	int (*func)(int, char **);
int	cmd_cd(int ac, char **av);
int	cmd_clone(int ac, char **av);
int	cmd_eof(int ac, char **av);
int	cmd_help(int ac, char **av);
int	cmd_pull(int ac, char **av);
int	cmd_push(int ac, char **av);
int	cmd_pwd(int ac, char **av);
int	cmd_rootkey(int ac, char **av);
int	cmd_status(int ac, char **av);
int	cmd_verbose(int ac, char **av);
int	cmd_version(int ac, char **av);

struct cmd {
	char	*name;		/* command name */
	char	*description;	/* one line description for help */
	func	cmd;		/* function pointer which does the work */
};

typedef struct {
	u32	interactive:1;		/* show prompts, etc */
	u32	errors_exit:1;		/* exit on any error */
	u32	daemon:1;		/* listen for TCP connections */
	FILE	*log;			/* if set, log commands to here */
	int	alarm;			/* exit after this many seconds */
	u16	port;			/* listen on this port */
	char	*pidfile;		/* write the daemon pid here */
	uid_t	uid;			/* run as uid */
	char	remote[16];		/* a.b.c.d of client */
} bkdopts;

/*
 * BK "URL" formats are:
 *	bk://user@host:port/pathname
 *	user@host:pathname
 * In most cases, everything except the pathname is optional.
 */
typedef struct {
	u16	port;		/* remote port if set */
	char	*user;		/* remote user if set */
	char	*host;		/* remote host if set */
	char	*path;		/* pathname (must be set) */
} remote;

/*
 * Default BitKeeper port.
 * This is will change when we get a reserved port number.
 */
#define	BK_PORT	0x3962

extern	struct cmd cmds[];
extern	int exists(char *);
extern	bkdopts Opts;

remote	*remote_parse(char *url);
char	*remote_unparse(remote *r);
pid_t	bkd(int compress, remote *r, int *r_pipe, int *w_pipe);
void	bkd_reap(pid_t resync, int r_pipe, int w_pipe);
int	getline(int in, char *buf, int size);
int	gunzip2fd(char *input, int len, int fd);
int	gzip2fd(char *input, int len, int fd);
void	gzip_done(void);
void	gzip_init(int level);
int	in(char *s, int len);
int	out(char *s);
int	readn(int from, char *buf, int size);
void	remote_free(remote *r);
void	remote_print(remote *r, FILE *f);
int	outfd(int fd, char*buf);

#endif
