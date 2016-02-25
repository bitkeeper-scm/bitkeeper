/*
 * Copyright 2004-2006,2015-2016 BitMover, Inc
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

/* Copyright 2004 Andrew Isaacson */

/* tty.c - implement a very basic TTY interface
 *
 * Functions provided:
 *  - tty_init()
 *      Opens any necessary devices, configures state.  May call atexit(),
 *      sigaaction(SIGWINCH), GetConsoleScreenBufferInfo(), et cetera.
 *      Must be called before any other tty_ function is called.
 *  - tty_done()
 *      Frees any state allocated by tty_init.  After this is called, no
 *      other tty_ function may be called.  (Exception:  you can call
 *      tty_init() again to start over.)
 *  - tty_rows(), tty_cols()
 *      Returns the number of rows and columns in the current window.
 *      Should be correct even after a window size change.
 *      XXX this works correctly on UNIX (assuming SIGWINCH works) but
 *          does not work on Win32.
 *  - tty_getch()
 *      Blocks until a single character is received from the user, then
 *      return it.  Should do the right thing with function keys, arrows,
 *      et cetera.  Returns EOF if the OS returned EOF.
 *      XXX does not yet do the right thing with multibyte keys
 *
 * If any of the above functions is called out of its place (for example,
 * tty_rows() before tty_init()), it will abort().
 *
 * Not part of the defined API:
 *  - tty_cleanup()
 *      Like tty_done(), but it can be called multiple times or before
 *      tty_init().  For use in atexit() and related places.
 */

#include "system.h"

#define DEF_COL	80
#define DEF_ROW	24

private	void	tty_cleanup(void);

private	int	tty_active, rows, cols;

#ifdef WIN32
# include <CONIO.H>
# include <WINDOWS.H>

private int
init(void)
{
	HANDLE	h; 
	CONSOLE_SCREEN_BUFFER_INFO	sbi; 

	h = GetStdHandle(STD_OUTPUT_HANDLE); 
	if (GetConsoleScreenBufferInfo(h, &sbi)) {
		cols = sbi.srWindow.Right - sbi.srWindow.Left + 1;
		rows = sbi.srWindow.Bottom - sbi.srWindow.Top + 1;
		return (1);
	}
	return (0);
}

private void
done(void)
{
}

#else

# if defined(__sun__) && defined(__svr4__)
#  define __EXTENSIONS__
# endif

# include <unistd.h>

# if defined(_AIX)
#  include <termios.h>
# else
#  include <sys/termios.h>
# endif

# include <sys/ioctl.h>
# include <fcntl.h>

private	struct	termios t_orig;
private	int	tty_fd = -1;

private	void
reset_cbreak(void)
{
	if (tty_fd == -1) return;
	tcsetattr(tty_fd, TCSANOW, &t_orig);
}

# ifdef SIGWINCH

private	struct	sigaction oa;

private	void
winch(int x)
{
	struct	winsize win;

	if (ioctl(tty_fd, TIOCGWINSZ, &win) < 0) {
		win.ws_col = DEF_COL;
		win.ws_row = DEF_ROW;
	}
	rows = win.ws_row;
	cols = win.ws_col;
}

private	void
reset_winch(void)
{
	struct	sigaction sa;

	if (tty_fd == -1) return;
	(void)sigaction(SIGWINCH, &oa, &sa);
	if (sa.sa_handler != winch) {
		/* oops, we stepped on somebody, put it back */
		(void)sigaction(SIGWINCH, &sa, 0);
	}

}

# endif

private	int
init(void)
{
	struct	termios t;
	struct	winsize win;

	if (ioctl(1, TIOCGWINSZ, &win) < 0) {
		return (0);
	} else {
# if defined(SIGWINCH)
		struct	sigaction sa;

		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = winch;
		(void)sigaction(SIGWINCH, &sa, &oa);
#endif
	}
	rows = win.ws_row;
	cols = win.ws_col;

	tty_fd = open("/dev/tty", O_RDONLY, 0);
	if (tty_fd == -1) {
		fprintf(stderr, "/dev/tty: %s\n", strerror(errno));
		return (0);
	}

	if (tcgetattr(tty_fd, &t) == -1) {
		fprintf(stderr, "tcgetattr: %s\n", strerror(errno));
		return (0);
	}

	memcpy(&t_orig, &t, sizeof(t));
	atexit(reset_cbreak);

	t.c_lflag &= ~(ICANON|ECHO);
	t.c_cc[VMIN] = 1;
	t.c_cc[VTIME] = 0;
	if (tcsetattr(tty_fd, TCSANOW, &t) == -1) {
		fprintf(stderr, "tcsetattr: %s\n", strerror(errno));
		return (0);
	}
	return (1);
}

private	void
done(void)
{
	if (tty_fd != -1) {
		reset_cbreak();
# ifdef SIGWINCH
		reset_winch();
# endif
		close(tty_fd);
		tty_fd = -1;
	}
}

#endif

int
tty_init(void)
{
	assert(tty_active == 0);
	unless (isatty(1)) return (0);
	unless (init()) {
		tty_cleanup();
		return (0);
	}
	tty_active = 1;
	return (1);
}

/* the internal implementation -- can be called from atexit() if necessary */
private void
tty_cleanup(void)
{
	done();
	tty_active = 0;
}

/* the published interface - with assert to catch inappropriate usage */
void
tty_done(void)
{
	assert(tty_active);
	tty_cleanup();
}

#ifdef WIN32
int
tty_getchar(void)
{
	assert(tty_active);
	return _getch();
}
#else
int
tty_getchar(void)
{
	char	buf[1];

	assert(tty_active);
	while (read(tty_fd, buf, sizeof(buf)) == -1) {
		if (errno == EINTR) continue;
		fprintf(stderr, "read: %s\n", strerror(errno));
		return (EOF);
	}
	return (buf[0]);
}
#endif

int
tty_rows(void)
{
	assert(tty_active);
	return (rows);
}

int
tty_cols(void)
{
	assert(tty_active);
	return (cols);
}

void
tty_clear(void)
{
#ifdef WIN32
	system("cls");
#else
	system("clear");
#endif
}

#undef	isatty

int
myisatty(int fd)
{
	int	ret;
	char	*p;
	char	buf[16];

	if (getenv("_BK_IN_BKD") && !getenv("_BK_BKD_IS_LOCAL")) return (0);

	sprintf(buf, "BK_ISATTY%d", fd);
	if (p = getenv(buf)) {
		ret = atoi(p);
	} else if (getenv("BK_NOTTY")) {
		ret = 0;
	} else {
		ret = isatty(fd);
	}
	return (ret);
}

#define	isatty	myisatty
