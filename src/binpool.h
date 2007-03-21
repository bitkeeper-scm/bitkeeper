/* functions only used by binpool.c and bkd_binpool.c */

char	*bp_lookupkeys(project *p, char *keys);
int	bp_insert(project *p, char *file, char *hash, char *keys, int canmv);

