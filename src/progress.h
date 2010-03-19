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
} ticker;

#define	PROGRESS_SPIN	0
#define	PROGRESS_MINI	1
#define	PROGRESS_BAR	2

ticker	*progress_start(int style, u64 max);
void	progress(ticker *t, u64 n);
void	progress_done(ticker *t, char *msg);
void	progress_end(u32 style, char *msg);

#endif /* _PROGRESS_H */
