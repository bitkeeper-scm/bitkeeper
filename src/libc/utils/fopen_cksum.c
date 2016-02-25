/*
 * Copyright 2012,2016 BitMover, Inc
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

/*
 * Implement the traditional SCCS 16 bit checksum.
 */
#include "system.h"

typedef struct {
	FILE	*f;
	u16	*cksump;
} cbuf;

private	int	cWrite(void *cookie, const char *buf, int len);
private	fpos_t	cSeek(void *cookie, fpos_t offset, int whence);
private	int	cClose(void *cookie);

FILE *
fopen_cksum(FILE *f, char *mode, u16 *cksump)
{
	cbuf	*cf = new(cbuf);

	assert(streq(mode, "w"));
	cf->f = f;
	cf->cksump = cksump;
	return (funopen(cf, 0, cWrite, cSeek, cClose));
}

/*
 * N.B. All checksum functions do the intermediate sum in an int variable
 * because 16-bit register arithmetic can be up to 2x slower depending on
 * the platform and compiler.
 */
private int
cWrite(void *cookie, const char *buf, int len)
{
	cbuf	*cf = cookie;
	int	i, ret;
	u32	sum = 0;

	if ((ret = fwrite(buf, 1, len, cf->f)) > 0) {
		for (i = 0; i < ret; i++) sum += (u8)buf[i];
		*cf->cksump += sum;
	}
	return (ret);
}

private	fpos_t
cSeek(void *cookie, fpos_t offset, int whence)
{
	cbuf	*cf = cookie;

	assert(whence == SEEK_CUR);
	assert(offset == 0);
	return (ftell(cf->f));
}


private int
cClose(void *cookie)
{
	cbuf	*cf = cookie;

	free(cf);
	return (0);
}
