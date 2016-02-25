/*
 * Copyright 2000-2002,2005-2007,2016 BitMover, Inc
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

#include "system.h"

private	char	*av[200];

void
syserr(const char *postfix)
{
	int	n;

	for (n = 0; av[n]; n++) {
		fprintf(stderr, "{%s} ", av[n]);
	}
	fprintf(stderr, "%s", postfix);
}

private int
swap(int fd, char *file, int rw_mode, int access_mode)
{
	int fd_sav, fd_new;

	fd_sav = (dup)(fd);
	assert(fd_sav >= 0);
	close(fd);
	make_fd_uninheritable(fd_sav);
	fd_new = open(file, rw_mode, access_mode);
	if (fd_new < 0) {
		perror(file);
		dup2(fd_sav, fd);
		close(fd_sav);
		return (-1);
	}
	if (fd_new != fd) {
		dup2(fd_new, fd);
		close(fd_new);
	}
	return (fd_sav);
}

private void
restore_fd(int fd, int fd_sav)
{
	if (fd_sav >= 0) {
		dup2(fd_sav, fd);
		close(fd_sav);
	}
}

/*
 * in: NULL (means stdin) or "file"
 * out: NULL (means stdout) or "file" means redirect to the file
 * err: same as output.
 *
 * out and err to the same file is supported.  (think 2>&1)
 *
 * Note: we used to do ">file" and ">>file" but since we allow ">" in file
 * names, that's no good.  So we have no append mode.
 */
int
sysio(char *in, char *out, char *err, char *p, ...)
{
	va_list	ap;
	static	int debug = -1;
	int	n = 0, fd0 = -1, fd1 = -1, fd2 = -1, rc = 0;

	va_start(ap, p);
	while (p) {
		if (n == 199) {
			fprintf(stderr, "sysio: Too many arguments\n");
			return (0xa00);
		}
		av[n++] = p;
		p = va_arg(ap, char *);
	}
	av[n] = 0;
	unless ((n = va_arg(ap, int)) == 0xdeadbeef) {
		fprintf(stderr, "sysio: bad arguments signature\n");
		assert(0);
	}
	va_end(ap);
	if (debug == -1) debug = (getenv("BK_DEBUG_CMD") != 0);
	if (debug > 0) {
		fprintf(stderr, "SYSIO");
		for (n = 0; av[n]; n++) {
			fprintf(stderr, " {%s}", av[n]);
		}
		fprintf(stderr, "\n");
	}
	if ((in && ((fd0 = swap(0,in,O_RDONLY, 0)) < 0)) ||
	    (out && ((fd1 = swap(1,out,O_CREAT|O_WRONLY|O_TRUNC,0664)) < 0))) {
		rc = -1;
	}
	if (!rc && err) {
		if (out && streq(out, err)) {
			if (((fd2 = dup(2)) < 0) || (dup2(1, 2) < 0)) rc = -1;
		} else {
			fd2 = swap(2, err, O_CREAT|O_WRONLY|O_TRUNC, 0664);
			if (fd2 < 0) rc = -1;
		}
	}
	unless (rc) rc = spawnvp(_P_WAIT, av[0], av);
	if (debug > 0) fprintf(stderr, "SYSIO EXIT=0x%x\n", rc);
	if (in) restore_fd(0, fd0);
	if (out) restore_fd(1, fd1);
	if (err) restore_fd(2, fd2);
	return (rc);
}

int
sys(char *p, ...)
{
	va_list	ap;
	int	n = 0;
	static	int debug = -1;

	va_start(ap, p);
	while (p) {
		if (n == 199) {
			fprintf(stderr, "sys: Too many arguments\n");
			return (0xa00);
		}
		av[n++] = p;
		p = va_arg(ap, char *);
	}
	av[n] = 0;
	unless ((n = va_arg(ap, int)) == 0xdeadbeef) {
		fprintf(stderr, "sys: Bad argument signature\n");
		assert(0);
	}
	va_end(ap);
	if (debug == -1) debug = (getenv("BK_DEBUG_CMD") != 0);
	if (debug > 0) {
		fprintf(stderr, "SYS");
		for (n = 0; av[n]; n++) {
			fprintf(stderr, " {%s}", av[n]);
		}
		fprintf(stderr, "\n");
	}
	n = spawnvp(_P_WAIT, av[0], av);
	if (debug > 0) fprintf(stderr, "SYS EXIT=0x%x\n", n);
	return (n);
}
