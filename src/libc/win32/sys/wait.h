#define	WEXITSTATUS(stat_val) ((unsigned) (stat_val) & 255)
#define	WIFEXITED(stat_val) (((stat_val) >> 8) == 0)
#define	WIFSIGNALED(ret) (0)  /* stub */
#define	WTERMSIG(ret) (-888)  /* stub */

#define	WNOHANG	0x00000001

int	wait(int *notused);
pid_t	waitpid(pid_t pid, int *status, int mode);
