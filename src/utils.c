#include "bkd.h"

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
	int	c;

	for (;;) {
		switch (read(in, &c, 1)) {
		    case 1:
			if ((buf[i] = c) == '\n') {
				buf[i] = 0;
				return (i);
			}
			if (++i == size) return (-2);
			break;
		    case 0:
			usleep(100000);
			break;	/* try again */
		    default:
			return (-1);
		}
	}
}
