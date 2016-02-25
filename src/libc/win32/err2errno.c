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

#define	_RE_MAP_H_	/* Don't remap API */
#include "system.h"

struct errentry {
	unsigned long oscode;	/* OS return value */
	int errnocode;		/* System V error code */
};

static struct errentry errtable[] = {
	{  ERROR_INVALID_FUNCTION,	EINVAL	  },  /* 1 */
	{  ERROR_FILE_NOT_FOUND,	ENOENT	  },  /* 2 */
	{  ERROR_PATH_NOT_FOUND,	ENOENT	  },  /* 3 */
	{  ERROR_TOO_MANY_OPEN_FILES,	EMFILE	  },  /* 4 */
	{  ERROR_ACCESS_DENIED,		EACCES	  },  /* 5 */
	{  ERROR_INVALID_HANDLE,	EBADF	  },  /* 6 */
	{  ERROR_ARENA_TRASHED,		ENOMEM	  },  /* 7 */
	{  ERROR_NOT_ENOUGH_MEMORY,	ENOMEM	  },  /* 8 */
	{  ERROR_INVALID_BLOCK,		ENOMEM	  },  /* 9 */
	{  ERROR_BAD_ENVIRONMENT,	E2BIG	  },  /* 10 */
	{  ERROR_BAD_FORMAT,		ENOEXEC   },  /* 11 */
	{  ERROR_INVALID_ACCESS,	EINVAL	  },  /* 12 */
	{  ERROR_INVALID_DATA,		EINVAL	  },  /* 13 */
	{  ERROR_INVALID_DRIVE,		ENOENT	  },  /* 15 */
	{  ERROR_CURRENT_DIRECTORY,	EACCES	  },  /* 16 */
	{  ERROR_NOT_SAME_DEVICE,	EXDEV	  },  /* 17 */
	{  ERROR_NO_MORE_FILES,		ENOENT	  },  /* 18 */
	{  ERROR_WRITE_PROTECT,		EACCES	  },  /* 19 */
	{  ERROR_BAD_UNIT,		EACCES	  },  /* 20 */
	{  ERROR_NOT_READY,		EACCES	  },  /* 21 */
	{  ERROR_BAD_COMMAND,		EACCES	  },  /* 22 */
	{  ERROR_CRC,			EACCES	  },  /* 23 */
	{  ERROR_BAD_LENGTH,		EACCES	  },  /* 24 */
	{  ERROR_SEEK,			EACCES	  },  /* 25 */
	{  ERROR_NOT_DOS_DISK,		EACCES	  },  /* 26 */
	{  ERROR_SECTOR_NOT_FOUND,	EACCES	  },  /* 27 */
	{  ERROR_OUT_OF_PAPER,		EACCES	  },  /* 28 */
	{  ERROR_WRITE_FAULT,		EACCES	  },  /* 29 */
	{  ERROR_READ_FAULT,		EACCES	  },  /* 30 */
	{  ERROR_GEN_FAILURE,		EACCES	  },  /* 31 */
	{  ERROR_SHARING_VIOLATION,	EACCES	  },  /* 32 */
	{  ERROR_LOCK_VIOLATION,	EACCES	  },  /* 33 */
	{  ERROR_WRONG_DISK,		EACCES	  },  /* 34 */
	{  ERROR_SHARING_BUFFER_EXCEEDED, EACCES  },  /* 36 */
	{  ERROR_BAD_NETPATH,		ENOENT	  },  /* 53 */
	{  ERROR_NETWORK_ACCESS_DENIED, EACCES	  },  /* 65 */
	{  ERROR_BAD_NET_NAME,		ENOENT	  },  /* 67 */
	{  ERROR_FILE_EXISTS,		EEXIST	  },  /* 80 */
	{  ERROR_CANNOT_MAKE,		EACCES	  },  /* 82 */
	{  ERROR_FAIL_I24,		EACCES	  },  /* 83 */
	{  ERROR_INVALID_PARAMETER,	EINVAL	  },  /* 87 */
	{  ERROR_NO_PROC_SLOTS,		EAGAIN	  },  /* 89 */
	{  ERROR_DRIVE_LOCKED,		EACCES	  },  /* 108 */
	{  ERROR_BROKEN_PIPE,		EPIPE	  },  /* 109 */
	{  ERROR_DISK_FULL,		ENOSPC	  },  /* 112 */
	{  ERROR_INVALID_TARGET_HANDLE, EBADF	  },  /* 114 */
	{  ERROR_INVALID_HANDLE,	EINVAL	  },  /* 124 */
	{  ERROR_WAIT_NO_CHILDREN,	ECHILD	  },  /* 128 */
	{  ERROR_CHILD_NOT_COMPLETE,	ECHILD	  },  /* 129 */
	{  ERROR_DIRECT_ACCESS_HANDLE,  EBADF	  },  /* 130 */
	{  ERROR_NEGATIVE_SEEK,		EINVAL	  },  /* 131 */
	{  ERROR_SEEK_ON_DEVICE,	EACCES	  },  /* 132 */
	{  ERROR_DIR_NOT_EMPTY,		ENOTEMPTY },  /* 145 */
	{  ERROR_NOT_LOCKED,		EACCES	  },  /* 158 */
	{  ERROR_BAD_PATHNAME,		ENOENT	  },  /* 161 */
	{  ERROR_MAX_THRDS_REACHED,	EAGAIN	  },  /* 164 */
	{  ERROR_LOCK_FAILED,		EACCES	  },  /* 167 */
	{  ERROR_ALREADY_EXISTS,	EEXIST	  },  /* 183 */
	{  ERROR_INVALID_STARTING_CODESEG,ENOEXEC },  /* 188 */
	{  ERROR_INVALID_STACKSEG,	ENOEXEC	  },  /* 189 */
	{  ERROR_INVALID_MODULETYPE,	ENOEXEC	  },  /* 190 */
	{  ERROR_INVALID_EXE_SIGNATURE,	ENOEXEC	  },  /* 191 */
	{  ERROR_EXE_MARKED_INVALID,	ENOEXEC	  },  /* 192 */
	{  ERROR_BAD_EXE_FORMAT,	ENOEXEC	  },  /* 193 */
	{  ERROR_ITERATED_DATA_EXCEEDS_64k,ENOEXEC},  /* 194 */
	{  ERROR_INVALID_MINALLOCSIZE,	ENOEXEC	  },  /* 195 */
	{  ERROR_DYNLINK_FROM_INVALID_RING,ENOEXEC},  /* 196 */
	{  ERROR_IOPL_NOT_ENABLED,	ENOEXEC	  },  /* 197 */
	{  ERROR_INVALID_SEGDPL,	ENOEXEC	  },  /* 198 */
	{  ERROR_AUTODATASEG_EXCEEDS_64k,ENOEXEC  },  /* 199 */
	{  ERROR_RING2SEG_MUST_BE_MOVABLE,ENOEXEC },  /* 200 */
	{  ERROR_RELOC_CHAIN_XEEDS_SEGLIM,ENOEXEC },  /* 201 */
	{  ERROR_INFLOOP_IN_RELOC_CHAIN,ENOEXEC	  },  /* 202 */
	{  ERROR_FILENAME_EXCED_RANGE,  ENOENT	  },  /* 206 */
	{  ERROR_NESTING_NOT_ALLOWED,	EAGAIN	  },  /* 215 */
	{  ERROR_NOT_ENOUGH_QUOTA,	ENOMEM	  }   /* 1816 */
};

#define ERRTABLESIZE (sizeof(errtable)/sizeof(errtable[0]))

int
err2errno(unsigned long oserrno)
{
	int	i;

	for (i = 0; i < ERRTABLESIZE; ++i) {
		if (oserrno == errtable[i].oscode) {
			return (errtable[i].errnocode);
		}
	}
	return (EINVAL);
}
