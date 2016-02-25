/*
 * Copyright 1999-2011,2013-2014,2016 BitMover, Inc
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

#ifndef _BKD_H
#define	_BKD_H

#include "system.h"
#include "sccs.h"

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
 * Transfer size for bk remote commands.  Both sides agree to never send
 * a chunk (before compression, after uncompression) bigger than the below.
 * DO NOT change this after shipping.
 */
#undef	BSIZE		/* hpux exposes it; they shouldn't */
#define	BSIZE		(32<<10)


/*
 * Functions take (int ac, char **av)
 * do whatever, and return 0 or -1.
 * Commands are allowed to read/write state from the global Opts.
 */
typedef	int (*func)(int, char **);
int	cmd_abort(int ac, char **av);
int	cmd_bk(int ac, char **av);
int	cmd_cd(int ac, char **av);
int	cmd_clone(int ac, char **av);
int	cmd_check(int ac, char **av);
int	cmd_help(int ac, char **av);
int	cmd_httpget(int ac, char **av);
int	cmd_pwd(int ac, char **av);
int	cmd_quit(int ac, char **av);
int	cmd_rootkey(int ac, char **av);
int	cmd_status(int ac, char **av);
int	cmd_version(int ac, char **av);

int	cmd_putenv(int ac, char **av);
int	cmd_push_part1(int ac, char **av);
int	cmd_push_part2(int ac, char **av);
int	cmd_push_part3(int ac, char **av);
int	cmd_pull_part1(int ac, char **av);
int	cmd_pull_part2(int ac, char **av);
int	cmd_synckeys(int ac, char **av);
int	cmd_chg_part1(int ac, char **av);
int	cmd_chg_part2(int ac, char **av);

int	cmd_rclone_part1(int ac, char **av);
int	cmd_rclone_part2(int ac, char **av);
int	cmd_rclone_part3(int ac, char **av);
int	cmd_kill(int ac, char **av);

int	cmd_rdlock(int ac, char **av);
int	cmd_rdunlock(int ac, char **av);
int	cmd_wrlock(int ac, char **av);
int	cmd_wrunlock(int ac, char **av);

int	cmd_nested(int ac, char **av);

struct cmd {
	char	*name;		/* command name */
	char	*realname;	/* real command name */
	char	*description;	/* one line description for help */
	func	cmd;		/* function pointer which does the work */
};

typedef struct {
	u32	foreground:1;		/* don't fork for daemons, etc. */
	u32	http_hdr_out:1;		/* print http header to output */
	u32	quiet:1;		/* quiet mode */
	u32	safe_cd:1;		/* do not allow chdir up */
	u32	symlink_ok:1;		/* follow symlinks into unsafe */
	u32	kill_ok:1;		/* enable kill socket */
	u32	unsafe:1;		/* allow unsafe (aka remote) commands */
	u32	use_stdio:1;		/* read stdin from stdio in bkd */
	int	alarm;			/* exit after this many seconds */
	char	*pidfile;		/* write the daemon pid here */
	char	*portfile;		/* write the port number here */
	char	*logfile;		/* if set, log commands to here */
	char	*vhost_dirpath;		/* directory path to start from */
} bkdopts;

/*
 * Default BitKeeper port.
 * This is will change when we get a reserved port number.
 */
#define	BK_PORT		0x3962
#define	WEB_PORT	80
#define	SSH_PORT	22

extern	struct cmd cmds[];
extern	bkdopts Opts;

void	bkd_server(int ac, char **av);

#define	REMOTE_BKDURL	1	/* URL is for BKD (affects http headers) */
#define	REMOTE_ROOTKEY	2	/* add in rootkey if we are a component */
#define	REMOTE_ROOTREL	4	/* if relative, URL is rel to root not cwd */
remote	*remote_parse(const char *url, u32 flags);
char	*remote_unparse(remote *r);
void	remote_free(remote *r);
void	remote_print(remote *r, FILE *f);
void	remote_error(remote *r, char *msg);
int	remote_valid(char *url);
char	*remoteurl_normalize(remote *r, char *url);
int	remote_checked(char *url);

int	gzipAll2fh(int rfd, FILE *wf, int level, int *in, int *out,
    int verbose);
int	gunzipAll2fh(int rfd, FILE *wf, int *in, int *out);
int	bkd_getc(void);

int	read_blk(remote *r, char *c, int len);
sccs *	mk_probekey(FILE *f);
int	getline2(remote *r, char *buf, int size); 
int	get_ok(remote *r, char *read_ahead, int verbose); 
#ifdef	_AIX
#define	send_file bk_send_file
#endif
int	send_file(remote *r, char *file, int extra);
int	send_file_extra_done(remote *r);
int	skip_hdr(remote *r);
int	getTriggerInfoBlock(remote *r, int quiet);
int	bkd_connect(remote *r, int opts);
void	disconnect(remote *r);
void	drain(void);
char	**getClientInfoBlock(void);
int	sendServerInfo(u32 cmdlog_flags);

/*
 * Options for probekey, listkey & prunekey()
 * The lowest nibble is left clear similar to the bit fields in sccs.h
 * to leave room for common flags.  Of the common flags, SILENT is used.
 */
#define	SK_REVPREFIX	0x00000010	/* input key has rev + tag prefix */
#define	SK_LKEY		0x00000080	/* want local cset in key format */
#define	SK_RKEY		0x00000400	/* want remote cset in key format */
#define	SK_SYNCROOT	0x00000800	/* use sccs_syncRoot(), not newest */
#define	SK_FORCEFULL	0x00001000	/* force a "makepatch -r.. */
#define	SK_SENDREV	0x00002000	/* listkey send rev */
#define	SK_OKAY		0x00004000	/* send @OK@ if no error */
int	probekey(sccs *s, char *rev, u32 flags, FILE *fout);
int	listkey(sccs *s, u32 flags, FILE *fin, FILE *fout);
int	prunekey(sccs *s, remote *r, hash *skip, int outfd, u32 flags,
	    int *local_only, int *remote_csets, int *remote_tags);
int	synckeys(remote *r, sccs *s, int flags, FILE *fout);

int	buf2fd(int gzip, char *buf, int len, int fd);
void	add_cd_command(FILE *f, remote *r);
int	skip_http_hdr(remote *r);
int	getServerInfo(remote *r, hash *bkdEnv);
char	*vpath_translate(char *path);

#define	SENDENV_NOREPO	   1 /* don't assume we are called from a repo */
#define	SENDENV_FORCEREMAP 4 /* send BK_REMAP even if not true */
#define	SENDENV_FORCENOREMAP 8 /* don't send BK_REMAP even if true */
#define	SENDENV_SENDFMT	   0x10 /* send sfile format */
void	sendEnv(FILE *f, char **envVar, remote *r, u32 flags);

void	setLocalEnv(int in_out);
void	wait_eof(remote *r, int verbose);
void	try_clone1_2(int quiet, int gzip,
				char *rev, remote *r, char *local, char *msg);
int	remote_lock_fail(char *buf, int verbose);
void	drainErrorMsg(remote *r, char *buf, int bsize);
int	listType(char *type);
int	bkd_isSafe(char *file);
int	unsafe_cd(char *path);
int	clone_sfioCompat(int bkd);

int	bkd_doResolve(char *me, int quiet, int verbose);

// like sccs.h -- use high bits to leave low bits for things like SILENT
#define	REMOTE_GZ_SND	0x1000	/* gzip the stdin we send */
#define	REMOTE_GZ_RCV	0x2000	/* ungzip the stdout we receive */
#define	REMOTE_STREAM	0x4000	/* whether to send the input as part of
				 * the send_file() message, or as a second
				 * stream.
				 */
#define	REMOTE_BKDERRS	0x8000	/* print bkd_connect() errors to stderr */

int	remote_cmd(char **av, char *url,
    FILE *in, FILE *out, FILE *err, hash *bkdEnv, int opts);

#endif
