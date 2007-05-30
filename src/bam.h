/* functions only used by bam.c and bkd_bam.c */

char	*bp_lookupkeys(project *p, char *keys);
int	bp_logUpdate(char *key, char *val);
int	bp_hashgfile(char *gfile, char **hashp, sum_t *sump);
int	bp_index_check(int quiet);
int	bp_check_hash(char *want, char ***missing, int fast);
int	bp_check_findMissing(int quiet, char **missing);
