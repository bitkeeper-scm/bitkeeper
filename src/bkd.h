#ifndef _BKD_H
#define	_BKD_H

#include "system.h"
#include "sccs.h"
#include "lib_tcp.h"
#include "zlib/zlib.h"

/*
 * Version 1.2 - removes the @DONE@ in the push path.
 *	We slipped the "to/from host:path" into the pull/push in this
 *	version without bumping the version number; it caused no problems.
 */
#define	BKD_VERSION1_2	"BKD version 1.2"
#define BKD_VERSION	"1.3"	/* bkd protocol version */

/*
 * These need to be one byte so that we can do char at a time I/O for status.
 */
#define	BKD_EXITOK	'\004'	/* ^D */
#define	BKD_DATA	'D'
#define	BKD_RC		'R'
#define	BKD_NUL		'\0'

/*
 * Options for prunekey()
 */
#define	PK_REVPREFIX	0x00001 /* input key has rev + tag prefix */
#define	PK_LSER		0x00002 /* want local cset in serial format */
#define	PK_LREV		0x00004 /* want local cset in rev/tag format */
#define	PK_LKEY		0x00008 /* want local cset in key format */
#define	PK_RSER		0x00010	/* want remote cset in serial format */
#define	PK_RREV		0x00020	/* want remote cset in rev/tag format */
#define	PK_RKEY		0x00040	/* want remote cset in key format */

/*
 * Functions take (int ac, char **av)
 * do whatever, and return 0 or -1.
 * Commands are allowed to read/write state from the global Opts.
 */
typedef	int (*func)(int, char **);
int	cmd_abort(int ac, char **av);
int	cmd_cd(int ac, char **av);
int	cmd_clone(int ac, char **av);
int	cmd_check(int ac, char **av);
int	cmd_eof(int ac, char **av);
int	cmd_help(int ac, char **av);
int	cmd_httpget(int ac, char **av);
int	cmd_pwd(int ac, char **av);
int	cmd_rootkey(int ac, char **av);
int	cmd_status(int ac, char **av);
int	cmd_verbose(int ac, char **av);
int	cmd_version(int ac, char **av);

int	cmd_putenv(int ac, char **av);
int	cmd_push_part1(int ac, char **av);
int	cmd_push_part2(int ac, char **av);
int	cmd_pull_part1(int ac, char **av);
int	cmd_pull_part2(int ac, char **av);
int	cmd_synckeys(int ac, char **av);
int	cmd_chg_part1(int ac, char **av);
int	cmd_chg_part2(int ac, char **av);

int	cmd_rclone_part1(int ac, char **av);
int	cmd_rclone_part2(int ac, char **av);

struct cmd {
	char	*name;		/* command name */
	char	*realname;	/* real command name */
	char	*description;	/* one line description for help */
	func	cmd;		/* function pointer which does the work */
};

typedef struct {
	u32	interactive:1;		/* show prompts, etc */
	u32	errors_exit:1;		/* exit on any error */
	u32	debug:1;		/* don't fork for daemons, etc. */
	u32	daemon:1;		/* listen for TCP connections */
	u32	start:1;		/* start NT bkd service */
	u32	remove:1;		/* remove NT bkd service */
	u32	http_hdr_out:1;		/* print http header to output */
	u32	quiet:1;		/* quiet mode */
	u32	safe_cd:1;		/* do not allow chdir up */
	FILE	*log;			/* if set, log commands to here */
	int	alarm;			/* exit after this many seconds */
	int	count;			/* exit after this many connections */
	u16	port;			/* listen on this port */
	char	*uid;			/* desired uid or null */
	char	*gid;			/* desired gid or null */
	char	*pidfile;		/* write the daemon pid here */
	char	*startDir;		/* start up directory for daemon */
	char	remote[16];		/* a.b.c.d of client */
} bkdopts;

/*
 * Default BitKeeper port.
 * This is will change when we get a reserved port number.
 */
#define	BK_PORT		0x3962
#define	WEB_PORT	80

extern	struct cmd cmds[];
extern	bkdopts Opts;
extern	char cmdlog_buffer[];
extern	char *logRoot;

void	bkd_server(char **);
remote	*remote_parse(const char *url, int is_clone);
char	*remote_unparse(remote *r);
pid_t	bkd(int compress, remote *r);
void	bkd_reap(pid_t resync, int r_pipe, int w_pipe);
int	gunzip2fd(char *input, int len, int fd, int hflag);
int	gzip2fd(char *input, int len, int fd, int hflag);
void	gzip_done(void);
void	gzip_init(int level);
int	gzipAll2fd(int rfd, int wfd, int level, int *in, int *out,
							int hflag, int verbose);
int	gunzipAll2fd(int rfd, int wfd, int level, int *in, int *out);
int	in(char *s, int len);
void	remote_free(remote *r);
void	remote_print(remote *r, FILE *f);
int	outfd(int fd, char*buf);

int	read_blk(remote *r, char *c, int len);
int	write_blk(remote *r, char *c, int len);
sccs *	mk_probekey(FILE *f);
int	getline2(remote *r, char *buf, int size); 
int	get_ok(remote *r, char *read_ahead, int verbose); 
int	send_msg(remote *r, char *msg, int mlen, int extra, int compress);
int	send_file(remote *r, char *file, int extra, int gzip);
int	skip_hdr(remote *r);
int	getTriggerInfoBlock(remote *r, int verbose); 
int	bkd_connect(remote *r, int compress, int verbose);
void	disconnect(remote *r, int how);
void	drain(void);
char	**getClientInfoBlock(void);
void	sendServerInfoBlock(int);
int	prunekey(sccs *, remote *, int, int, int, int *, int *, int *);
int	buf2fd(int gzip, char *buf, int len, int fd);
void	add_cd_command(FILE *f, remote *r);
int	skip_http_hdr(remote *r);
int	getServerInfoBlock(remote *r);
void	sendEnv(FILE *f, char **envVar, remote *r, int isClone);
void	setLocalEnv(int in_out);
void	wait_eof(remote *r, int verbose);
void	flush2remote(remote *r);
int	flushSocket(int fd);
void	try_clone1_2(int quiet, int gzip,
				char *rev, remote *r, char *local, char *msg);
int	remote_lock_fail(char *buf, int verbose);
void	drainErrorMsg(remote *r, char *buf, int bsize);
int	listType(char *type);
void	send_flush_block(remote *r);
#endif
