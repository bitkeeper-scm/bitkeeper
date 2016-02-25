/*
 * Copyright 2010-2011,2015-2016 BitMover, Inc
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

#ifndef	_PROGRESS_H
#define	_PROGRESS_H

typedef struct {
	u64	start;		/* start time_t in milliseconds */
	u64	last;		/* last time we updated in milliseconds */
	u64	base;		/* base to start counting from */
	u64	cur;		/* current ticker count */
	u64	max;		/* max ticker count; cur/max = % done */
	float	m;		/* slope of progress-bar trajectory line */
	float	b;		/* y intercept of trajectory line */
	float	scale;		/* tick scale factor */
	char	*name;		/* prefix */
	u64	rate;		/* spinner update rate in milliseconds */
	u8	i;		/* spin cycle */
	u8	percent;	/* so we don't print unless it changed */
	u8	style;		/* output style */
	u8	direction:1;	/* whether we're going forward or backwards */
	u8	multi:1;	/* 1 if part of multiple progress bars */
	u8	inherited:1;	/* 1 if inheriting state from parent */
	u8	always:1;	/* always update progress bar */
	u8	debug:1;	/* print progress-bar debug */
} ticker;

/* For style arg to progress_start() and progress_end(). */
#define	PROGRESS_SPIN	0
#define	PROGRESS_MINI	1
#define	PROGRESS_BAR	2
#define	PROGRESS_BAR_I	3

/* For action arg to progress_end(). */
#define PROGRESS_MSG	0	/* append given msg arg to progress bar */
#define PROGRESS_SUM	1	/* output BK/log/progress-sum over prog bar */


/*
 * progress		- indicate one tick of progress
 * progress_active	- call before fprintf'ing a progress bar to disable
 *			  newline injection
 * progress_adjustMax	- change the ticker max and adjust the progress-bar
 *			  trajectory smoothly
 * progress_delayStderr	- begin buffering all output to stderr for later output
 * progress_done	- take the current progress bar to 100%; if not in a
 *			  progress-multi, also do a progress_end()
 * progress_end		- finish the current progress bar using the given msg
 *			  and output a newline
 * progress_inherit	- cause child processes to inherit the current progress
 *			  bar state
 * progress_inheritEnd	- update our state to indicate that a child process
 *			  advanced the progress bar a certain amount, and stop
 *			  inheriting state to any further children
 * progress_isMulti	- true if a progress bar already has been started
 * progress_nldone	- call when newline injection is no longer needed
 * progress_nlneeded	- indicates that progress bar(s) have been started
 *			  by some process but progress_end() has not yet
 *			  been called, so the next regular fprintf to stderr
 *			  needs a newline injection
 * progress_pauseDelayed - temporarily stop the stderr buffering started by
 *			  progress_delayStderr
 * progress_restoreStderr - stop buffering stderr and print all stored output
 * progress_resumeDelayed - resume the stderr buffering temporarily stopped
 *			  by progress_delayStderr
 * progress_start	- begin a progress bar
 * progress_startMulti	- indicate that multiple progress bars follow; so
 *			  progress_done should not output a newline
 */
void	progress(ticker *t, u64 n);
void	progress_active(void);
void	progress_adjustMax(ticker *t, i64 adj);
void	progress_delayStderr(void);
void	progress_done(ticker *t, char *msg);
void	progress_end(u32 style, char *msg, u32 action);
void	progress_inherit(ticker *t);
void	progress_inheritEnd(ticker *t, u64 n);
int	progress_isMulti(void);
void	progress_nldone(void);
void	progress_nlneeded(void);
void	progress_injectnl(void);
void	progress_pauseDelayed(void);
void	progress_restoreStderr(void);
void	progress_resumeDelayed(void);
ticker	*progress_start(int style, u64 max);
void	progress_startMulti(void);
ticker	*progress_startScaled(int style, u64 want, u64 told);

/* stderr write callback; never call this directly */
int	progress_syswrite(void *cookie, const char *buf, int n);

#endif /* _PROGRESS_H */
