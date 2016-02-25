/*
 * Copyright 1999-2001,2004,2006-2007,2015-2016 BitMover, Inc
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
#include "sccs.h"

private	int	do_checksum(int);

int
adler32_main(int ac, char **av)
{
	setmode(0, _O_BINARY); /* for win32 */
	return (do_checksum((ac == 2) && streq(av[1], "-w")));
}

u32
adler32_file(char *file)
{
	MMAP	*m = mopen(file, "b");
	u32	sum;

	unless (m) return (0);
	unless (m->size) {
		mclose(m);
		return (0);
	}
	sum = adler32(0, m->mmap, m->size);
	mclose(m);
	return (sum);
}

/*
 * Compute a checksum envelop over a data stream.
 * There are two possible data sections, the human and the BK part.
 * The human part comes first as
 *	^A Diff start
 *	diff -u output
 *	^A end
 * and then we ignore data until we see a 
 *	^A Patch start
 *	patch output
 *	^A end
 *
 * Because the diffs can contain stuff that will screw up the unpacker, we
 * preceed all the human readable diffs with "#";  It's up to the other side
 * to strip that after checksumming the diffs.
 *
 * This is from the PATCH_VERSION line (inclusive) all the way to the end.
 * The "# Patch checksum=..." line is not checksummed.
 *
 * If there are human readable diffs above PATCH_VERSION, they get their
 * own checksum.
 *
 * adler32() is in zlib.
 */
private int
do_checksum(int dataOnly)
{
	char	buf[8<<10];
	int	n, doDiffs = 0;
	uLong	sum;
	unsigned int byte_count = 0;

	if (dataOnly) goto Patch;

	/*
	 * Just pass through anything up until a patch or diffs start.
	 */
	while (fnext(buf, stdin)) {
		if (streq(buf, PATCH_ABORT)) {
abort:			fprintf(stderr, "adler32 aborting\n");
			while (fnext(buf, stdin));
			return (1);
		}
		if (streq(buf, PATCH_DIFFS)) {
			doDiffs = 1;
			break;
		}
		if (streq(buf, PATCH_PATCH)) {
			goto Patch;
		}
		fputs(buf, stdout);
		byte_count += strlen(buf);
		if (feof(stdin)) {
eof:			fprintf(stderr, "adler32: did not see patch EOF\n");
			return (1);
		}
	}

	/*
	 * Wrap the diffs in their own envelope.
	 */
	if (doDiffs) {
		sum = 0;
		while (fnext(buf, stdin)) {
			if (streq(buf, PATCH_ABORT)) goto abort;
			if (streq(buf, PATCH_END)) {
				/* the \n is important, we need a blank line. */
				printf("\n# Diff checksum=%.8lx\n\n", sum);
				break;
			}
			sum = adler32(sum, "#", 1);
			fputs("#", stdout);
			byte_count++;
			sum = adler32(sum, buf, strlen(buf));
			fputs(buf, stdout);
			byte_count += strlen(buf);
			if (feof(stdin)) goto eof;
		}
	}

	/*
	 * Just pass through anything up until a patch start.
	 */
	while (fnext(buf, stdin)) {
		if (streq(buf, PATCH_PATCH)) break;
		fputs(buf, stdout);
		byte_count += strlen(buf);
		if (feof(stdin)) goto eof;
	}

	/*
	 * Wrap the patch in it's own envelope.
	 */
Patch:
	sum = 0;
	while (fnext(buf, stdin)) {
		if (streq(buf, PATCH_ABORT)) goto abort;
		if (streq(buf, PATCH_END)) {
end:			sprintf(buf, "# Patch checksum=%.8lx\n", sum);
			fputs(buf, stdout);
			byte_count += strlen(buf);
			fflush(stdout);
			/*
			 * Pass through any other trailing data, (sfio)
			 */
			while (n = fread(buf, 1, sizeof(buf), stdin)) {
				fwrite(buf, 1, n, stdout);
			}
			save_byte_count(byte_count);
			return (0);
		}
		sum = adler32(sum, buf, strlen(buf));
		fputs(buf, stdout);
		byte_count += strlen(buf);
		if (feof(stdin)) goto eof;
	}
	unless (dataOnly) goto eof;
	goto end;
	/* gotta love them thar gotos */
}
