/*
 * Couldn't use the names open, lstat, link, unlink, etc...
 * because many of these are macros
 */
typedef struct {
	int	(*_open)(project *p, char *rel, int flags, mode_t mode);
	int	(*_lstat)(project *p, char *rel, struct stat *buf);
	int	(*_isdir)(project *p, char *rel);
	int	(*_linkcount)(project *p, char *rel, struct stat *sb);
	int	(*_access)(project *p, char *rel, int mode);

	char	**(*_getdir)(project *p, char *rel);
	int	(*_link)(project *p1, char *rel1, project *p2, char *rel2);
	char	*(*_realBasename)(project *p, char *rel, char *basename);
	int	(*_unlink)(project *p, char *rel);
	int	(*_rename)(project *p1, char *rel1, project *p2, char *rel2);
	int	(*_mkdir)(project *p, char *rel);
	int	(*_rmdir)(project *p, char *rel);
} fsfuncs;

extern	fsfuncs	remap_funcs;

