#include "bkd.h"

bkdopts	Opts;	/* has to be declared here, other people use this code */

/*
 * Write the data to either the gzip channel or to 1.
 */
int
out(char *buf)
{
	return (writen(1, buf));
}

int
in(char *buf, int n)
{
	return (readn(0, buf, n));
}

int
writen(int fd, char *buf)
{
	assert(fd >= 0);
	assert(buf);
	return (write(fd, buf, strlen(buf)));
}

int
readn(int from, char *buf, int size)
{
	int	done;
	int	n;

	for (done = 0; done < size; ) {
		n = read(from, buf + done, size - done);
		if (n <= 0) {
			break;
		}
		done += n;
	}
	return (done);
}

/*
 * Currently very lame because it does 1 byte at a time I/O
 */
int
getline(int in, char *buf, int size)
{
	int	i = 0;
	char	c;

	buf[0] = 0;
	size--;
	unless (size) return (-3);
	for (;;) {
		switch (read(in, &c, 1)) {
		    case 1:
			if ((buf[i] = c) == '\n') {
				buf[i] = 0;
				return (i + 1);	/* we did read a newline */
			}
			if (++i == size) {
				buf[i] = 0;
				return (-2);
			}
			break;

		    default:
			unless (errno == EINTR) {
				return (-1);
			}
		}
	}
}

/*
 * Prompt the user and get an answer.
 * The buffer has to be MAXPATH bytes long.
 */
int
prompt(char *msg, char *buf)
{
	write(2, msg, strlen(msg));
	write(2, " ", 1);
	if (getline(0, buf, MAXPATH) > 1) return (1);
	return (0);
}

int
confirm(char *msg)
{
	char	buf[100];

	write(2, msg, strlen(msg));
	write(2, " (y/n) ", 7);
	if (getline(0, buf, sizeof(buf)) <= 1) return (0);
	return ((buf[0] == 'y') || (buf[0] == 'Y'));
}
