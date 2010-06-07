#ifndef	_PROGRESS_H
#define	_PROGRESS_H

typedef struct {
	u64	start;		/* start time_t in milliseconds */
	u64	max;		/* n/max == percent done */
	char	*name;		/* prefix */
	u32	rate;		/* update rate in milliseconds */
	u8	i;		/* spin cycle */
	u8	percent;	/* so we don't print unless it changed */
	u8	style;		/* output style */
	u8	multi;		/* 1 if part of multiple progress bars */
} ticker;

#define	PROGRESS_SPIN	0
#define	PROGRESS_MINI	1
#define	PROGRESS_BAR	2

/*
 * progress		- indicate one tick of progress
 * progress_active	- call before fprintf'ing a progress bar
 * progress_nldone	- call when cr injection is no longer needed
 * progress_nlneeded	- indicates that progress bar(s) have been started
 *			  by some process but progress_end() has not yet
 *			  been called, so the next regular fprintf to stderr
 *			  needs a cr injection
 * progress_delayStderr	- begin buffering all output to stderr for later output
 * progress_done	- take the current progress bar to 100%; if not in a
 *			  progress-multi, also do a progress_end()
 * progress_end		- finish the current progress bar using the given msg
 *			  and output a cr
 * progress_pauseDelayed - temporarily stop the stderr buffering started by
 *			  progress_delayStderr
 * progress_restoreStderr - stop buffering stderr and print all stored output
 * progress_resumeDelayed - resume the stderr buffering temporarily stopped
 *			  by progress_delayStderr
 * progress_start	- begin a progress bar
 * progress_startMulti	- indicate that multiple progress bars follow; so
 *			  progress_done should not output a cr
 */
void	progress(ticker *t, u64 n);
void	progress_active(void);
void	progress_nldone(void);
void	progress_nlneeded(void);
void	progress_delayStderr(void);
void	progress_done(ticker *t, char *msg);
void	progress_end(u32 style, char *msg);
void	progress_pauseDelayed(void);
void	progress_restoreStderr(void);
void	progress_resumeDelayed(void);
void	progress_startMulti(void);
ticker	*progress_start(int style, u64 max);

int	progress_syswrite(void *cookie, const char *buf, int n);

#endif /* _PROGRESS_H */
