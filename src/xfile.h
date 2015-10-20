int	is_xfile(char *file);
int	xfile_exists(char *path, char key);
int	xfile_delete(char *path, char key);
char	*xfile_fetch(char *path, char key);
int	xfile_store(char *path, char key, char *value);

char	**sdir_getdir(project *p, char *dir);

int	sfile_exists(project *p, char *path);
int	sfile_delete(project *p, char *path);
FILE	*sfile_open(project *p, char *path);
int	sfile_move(project *p1, char *from, char *to);
