#ifndef _BKD_H
#define	_BKD_H

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "lib_tcp.h"
#include "system.h"
#include "sccs.h"

/*
 * Functions take (int ac, char **av, int in, int out, int err),
 * do whatever, and return 0 or -1.
 */
typedef	int (*func)(int, char **, int, int, int);
int	cmd_clone(int ac, char **av, int in, int out, int err);
int	cmd_eof(int ac, char **av, int in, int out, int err);
int	cmd_help(int ac, char **av, int in, int out, int err);
int	cmd_list(int ac, char **av, int in, int out, int err);
int	cmd_pull(int ac, char **av, int in, int out, int err);
int	cmd_push(int ac, char **av, int in, int out, int err);
int	cmd_root(int ac, char **av, int in, int out, int err);
int	cmd_status(int ac, char **av, int in, int out, int err);
int	cmd_verbose(int ac, char **av, int in, int out, int err);

struct cmd {
	char	*name;		/* command name */
	char	*description;	/* one line description for help */
	func	cmd;		/* function pointer which does the work */
};

extern	struct cmd cmds[];
extern	int exists(char *);

extern	int writen(int fd, char *s);
extern	int readn(int from, char *buf, int size);
extern	int getline(int in, char *buf, int size);

#endif
