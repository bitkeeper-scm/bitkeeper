#ifndef	_BAM_H
#define	_BAM_H
/* functions only used by bam.c and bkd_bam.c */

#define	BAM_ROOT	"BitKeeper/BAM"
#define	BAM_MARKER	"BitKeeper/log/BAM"
#define	BAM_INDEX	"BitKeeper/log/BAM.index"
#define	BAM_DB		"BitKeeper/BAM/index.db"
#define	BAM_SERVER	"BitKeeper/log/BAM_SERVER"

char	*bp_lookupkeys(project *p, char *keys);
int	bp_logUpdate(project *p, char *key, char *val);
int	bp_hashgfile(char *gfile, char **hashp, sum_t *sump);
int	bp_index_check(int quiet);
int	bp_check_hash(char *want, char ***missing, int fast);
int	bp_check_findMissing(int quiet, char **missing);
#endif
