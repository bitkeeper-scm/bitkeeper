/*
 * Copyright 2008,2016 BitMover, Inc
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

/* All static functions in this file assume the cache is already locked */

typedef struct {
	char	*dir;		/* directory cached here */
	int	inRepo;		/* is this dir in a repo? */
	time_t	t;		/* last time used for LRU*/
	hash	*h;		/* filename -> (mtime, status) */
} cache_t;

typedef struct {
	mode_t	mode;		/* file mode (chmod) */
	time_t	mtime;		/* mtime at time of caching */
	int	status;		/* status of file at time of caching */
} pair_t;

static	cache_t	cache[MAXCACHE];
static	int	find_lru(void);
static	int	find_cache(char *dir);
static	int	fill_cache(char *path, int index);
static	int	recache(char *dir);
static	char**	mailslot_read(HANDLE mailslot);
static	int	staleFile(char *file);
int	cache_shutdown;		/* tells the mailslot listener to die */

/* Mutex */
HANDLE	cache_mutex;
static	void	Lock(void);
static	void	Unlock(void);

/* Fetch the status of a file. The stat buffer is optional. Migh cause
 * a recache. */
int
cache_fileStatus(char *file, struct stat *sb)
{
	char	*dir;
	int	i;
	pair_t	*p;
	int	times = 0;
	int	status = BK_EXTERNAL;
	struct	stat sb1;

	TRACE("%s", file);
	Lock();
	dir = dirname(file);
	if ((i = find_cache(dir)) < 0) {
		i = recache(dir);
	}
	unless (cache[i].inRepo) {
		TRACE("cache says not in repo");
		goto out;
	}
again:	unless (p = hash_fetch(cache[i].h, file, (int)strlen(file)+1)) {
		TRACE("file not found");
		i = recache(dir);
		if (times++) {
			/* we really failed */
error:			TRACE("caching failed two times, disabling");
			goto out;
		}
		goto again;
	}
	unless (p->mtime) {
		/* mtime == 0 means we need to recache */
		TRACE("mtime mismatch");
		i = recache(dir);
		if (times++) goto error;
		goto again;
	}
	unless (sb) {
		if (stat(file, &sb1)) {
			TRACE("Could not stat %s", file);
			goto out;
		}
		sb = &sb1;
	}
	if ((sb->st_mtime != p->mtime) || (sb->st_mode != p->mode)) {
		/* cache is stale */
		TRACE("stale cache");
		i = recache(dir);
		if (times++) goto error;
		goto again;
	}
	/* now we know cache is good, just use it */
	TRACE("cache hit for %s", file);
	status = p->status;
out:	free(dir);
	Unlock();
	return (status);
}

DWORD WINAPI
cache_mailslot(LPVOID data)
{
	HANDLE	mailslot;
	char	**message = 0, **oldmessage = 0, **notify = 0;
	int	i;

	/* create the mailslot */
	mailslot = (HANDLE) data;
	TRACE("mailslot started");
	/* read from it */
	while (1) {
		if (cache_shutdown) break;
		if (message = mailslot_read(mailslot)) {
			TRACE("got message");
			if (oldmessage) {
				TRACE("append");
				/* append new message to old message */
				EACH(message) {
					oldmessage = addLine(oldmessage,
					    message[i]);
				}
				freeLines(message, 0);
			} else {
				TRACE("copy");
				/* just copy it */
				oldmessage = message;
			}
			message = 0;
			Sleep(MAILSLOT_DELAY);
			continue;
		}
		Sleep(MAILSLOT_DELAY);
		/* delayed processing, we're only here if we have not
		 * received anyting in at least MAILSLOT_DELAY */
		if (oldmessage) {
			/* now process the file list */
			uniqLines(oldmessage, free);
			TRACE("processing %u messages", nLines(oldmessage));
			EACH(oldmessage) {
				if (staleFile(oldmessage[i])) {
					notify = addLine(notify,
					    dirname(oldmessage[i]));
				}
			}
			freeLines(oldmessage, free);
			uniqLines(notify, free);
			EACH(notify) {
				/* tell explorer.exe it changed */
				TRACE("SHChangeNotify(%s)", notify[i]);
				SHChangeNotify(SHCNE_UPDATEITEM,
				    SHCNF_PATH | SHCNF_FLUSH, notify[i], 0);
			}
			freeLines(notify, free);
			notify = 0;
			oldmessage = 0;
		}
	}
	/* tear down the mailslot */
	CloseHandle(mailslot);
	if (message) freeLines(message, free);
	if (oldmessage) freeLines(oldmessage, free);
	return (0);
}

/* read a single message from the mailslot */
static char **
mailslot_read(HANDLE mailslot)
{
	DWORD	msg_size = 0, msgs = 0;
	DWORD	read;
	HANDLE	event;
	char	*buf, *p;
	char	**messages = 0;
	OVERLAPPED	ov;

	unless (event = CreateEvent(0, 0, 0, 0)) return (0);
	ov.Offset = 0;
	ov.OffsetHigh = 0;
	ov.hEvent = event;

	unless (GetMailslotInfo(mailslot, 0, &msg_size, &msgs, 0)) {
		TRACE("GetMailslotInfo failed: %d", GetLastError());
		goto out;
	}

	if (msg_size == MAILSLOT_NO_MESSAGE) goto out;

	while (msgs) {
		buf = malloc(msg_size + 1);
		unless (ReadFile(mailslot, buf, msg_size, &read, &ov)) {
			TRACE("ReadFile failed: %d", GetLastError());
			free(buf);
			goto out;
		}
		buf[msg_size] = 0;
		/* bk gives us / instead of \ */
		for (p = buf; *p; p++) if (*p == '/') *p = '\\';

		TRACE("message = %s", buf);
		messages = splitLine(buf, "\n", messages);
		free(buf);

		unless (GetMailslotInfo(mailslot, 0, &msg_size, &msgs, 0)) {
			TRACE("GetMailslotInfo failed: %d", GetLastError());
			goto out;
		}
	}
out:	CloseHandle(event);
	return (messages);
}

static int
staleFile(char *file)
{
	char	*dir;
	int	i, rc = 0;
	pair_t	*p;

	TRACE("STALE: %s", file);
	dir = dirname(file);
	Lock();
	if ((i = find_cache(dir)) < 0) {
		/* we're not caching this dir */
		TRACE("%s not in cache", dir);
		goto out;
	}
	unless (p = hash_fetch(cache[i].h, file, (int)strlen(file)+1)) {
		/* file not in cache */
		TRACE("%s not in cache", file);
		goto out;
	}
	p->mtime = 0;		/* stale it */
	rc = 1;
	TRACE("staled %s", file);
out:	Unlock();
	free(dir);
	return (rc);
}

static int
recache(char *dir)
{
	int	i;

	TRACE("recaching %s", dir);
	if ((i = find_cache(dir)) < 0) {
		/* it wasn't cached, so just blow away the oldest one */
		i = find_lru();
		if (cache[i].dir) free(cache[i].dir);
		if (cache[i].h) hash_free(cache[i].h);
		cache[i].dir = strdup(dir);
		cache[i].h = hash_new(HASH_MEMHASH);
	}
	fill_cache(dir, i);
	return (i);
}

/* Run a cmd in 'dir' and return the output as an
 * allocated string */
char*
cmd2buf(char *cmd, char *args)
{
	HANDLE	hPipe, hProc;
	char	*p, *buf;
	long	len, n, read;
	char	tmp[MAXPATH];

	TRACE(0);
	hPipe = MakeReadPipe(cmd, args, &hProc);
	if (hPipe == (void *)-1) {
		TRACE("%s %s failed", cmd, args);
		return (0);
	}
	len = MAXPATH;
	p = buf = malloc(len);
	read = 0;
	while (ReadFile(hPipe, tmp, MAXPATH, &n, NULL)) {
		read += n;
		if (read > len) {
			len *= 2;
			buf = realloc(buf, len);
			p = buf + read - n;
		}
		memcpy(p, tmp, n);
		p += n;
	}
	*p = 0;
	ClosePipe(hPipe, hProc);
	return (buf);
}

static int
fill_cache(char *path, int index)
{
	int	i;
	char	*cmd, *output;
	char	**lines;

	if (isDrive(path) ||
	    !rootDirectory(path) ||
	    !validDrive(path)) {
		cache[index].inRepo = 0;
		TRACE("%s not in a repo", path);
		return (0);
	}

	cmd = aprintf("sfiles -1cdDgGiRxv \"%s\"", path);
	TRACE("running \"%s\"", cmd);
	output = cmd2buf(bkexe, cmd);
	free(cmd);
	unless (output) {
		cache[index].inRepo = 0;
		TRACE("sfiles failed");
		return (0);
	}
	cache[index].inRepo = 1;

	lines = splitLine(output, "\n", 0);
	EACH (lines) {
		pair_t	pair;
		char	*fn, *p;
		struct	stat sb;

		bzero(&pair, sizeof(pair));

		if ((lines[i][0] == 'd') || (lines[i][0] == 'D')) {
			pair.status = BK_DIR;
		} else if (lines[i][0] == 'R') {
			pair.status = BK_PROJROOT | BK_DIR;
		} else if (lines[i][0] == 'i') {
			pair.status = BK_IGNORED;
		} else if (lines[i][2] == 'c') {
			pair.status = BK_MODIFIED;
		} else if (lines[i][4] == 'G') {
			if (lines[i][1] == 'l') {
				pair.status = BK_EDITED;
			} else {
				pair.status = BK_READONLY;
			}
		} else if (lines[i][0] == 'x') {
			pair.status = BK_EXTRA;
		}
		fn = strchr(lines[i], ' ');
		fn++;
		/*
		 * BK is the only source of forward slashes so we
		 * keep everything in windows backslash format.
		 */
		/* XXX: tried switch_char() here, but it's
		 * weird... investigate later. */
		for (p = fn; *p; p++) if (*p == '/') *p = '\\';
		if (stat(fn, &sb)) {
			TRACE("Could not stat %s: %d", fn, errno);
			/* We don't know what happened to the file, so we just
			 * set the mtime to zero hoping a later recache will
			 * find it there */
			pair.mtime = 0;
			pair.mode = 0;
		} else {
			pair.mtime = sb.st_mtime;
			pair.mode  = sb.st_mode;
		}
		hash_store(cache[index].h,
		    fn, (int)strlen(fn)+1, &pair, sizeof(pair));
	}
	free(output);
	freeLines(lines, free);
	return (1);
}

static int
find_cache(char *dir)
{
	int	i;

	for (i = 0; i < MAXCACHE; i++) {
		if (cache[i].dir && streq(dir, cache[i].dir)) {
			cache[i].t = time(0);
			TRACE("found cache for %s at slot %d (%u)",
			    dir, i, cache[i].t);
			return (i);
		}
	}
	return (-1);
}

static int
find_lru(void)
{
	int	i;
	int	index = 0;
	time_t	min;

	min = cache[index].t;
	for (i = 0; i < MAXCACHE; i++) {
		TRACE("cache[%d].t == %u, min = %u",
		    i, (unsigned int)cache[i].t, (unsigned int)min);
		if (cache[i].t < min) {
			min = cache[i].t;
			index = i;
		}
	}
	assert((index >= 0) && (index < MAXCACHE));
	TRACE("LRU is %d: %s", index,
	    cache[index].dir ? cache[index].dir : "EMPTY");
	return (index);
}

static void
Lock(void)
{
	unless (cache_mutex) {
		TRACE("cache_mutex is not set");
		return; /* look ma' no locks! */
	}
	TRACE("waiting for lock");
	switch (WaitForSingleObject(cache_mutex, INFINITE)) {
	    case WAIT_OBJECT_0:
		/* got the mutex */
		TRACE("got the lock");
		break;
	    case WAIT_ABANDONED:
		/* something screwed up */
		TRACE("we're hosed");
		break;
	}
}

static void
Unlock(void)
{
	unless (cache_mutex) return; /* look ma' no locks! */
	unless (ReleaseMutex(cache_mutex)) {
		TRACE("could not unlock, we're hosed");
	}
	TRACE("released lock");
}
