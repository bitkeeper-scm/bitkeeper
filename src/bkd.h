#ifndef _BKD_H
#define	_BKD_H

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "lib_tcp.h"
#include "system.h"
#include "sccs.h"

#define	BKD_VERSION	"bkd version 1"

/*
 * Functions take (int ac, char **av, int in, int out)
 * do whatever, and return 0 or -1.
 */
typedef	int (*func)(int, char **, int, int);
int	cmd_clone(int ac, char **av, int in, int out);
int	cmd_eof(int ac, char **av, int in, int out);
int	cmd_help(int ac, char **av, int in, int out);
int	cmd_pull(int ac, char **av, int in, int out);
int	cmd_push(int ac, char **av, int in, int out);
int	cmd_root(int ac, char **av, int in, int out);
int	cmd_status(int ac, char **av, int in, int out);
int	cmd_verbose(int ac, char **av, int in, int out);
int	cmd_version(int ac, char **av, int in, int out);

struct cmd {
	char	*name;		/* command name */
	char	*description;	/* one line description for help */
	func	cmd;		/* function pointer which does the work */
	u32	readonly:1;	/* if set, then this is a readonly command */
};

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

int	writen(int fd, char *s);
int	readn(int from, char *buf, int size);
int	getline(int in, char *buf, int size);
remote	*remote_parse(char *url);
void	remote_free(remote *r);
char	*remote_unparse(remote *r);
void	remote_print(remote *r, FILE *f);
pid_t	bkd(int compress, remote *r, int fds[2]);
void	bkd_reap(pid_t resync, int fds[2]);

#endif
