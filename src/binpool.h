/* functions only used by binpool.c and bkd_binpool.c */

typedef struct {
	int	version;
	off_t	size;
	char	*hash;
	char	**keys;
} bpattr;

int	bp_loadAttr(char *file, bpattr *a);
int	bp_saveAttr(bpattr *a, char *file);
void	bp_freeAttr(bpattr *a);
char	*bp_lookupkeys(project *p, char *hash, char *keys);
int	bp_insert(project *p, char *file, char *hash, char *keys, int canmv);

