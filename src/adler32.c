#include "system.h"
#include "sccs.h"
#include "zlib/zlib.h"

private	void do_checksum(void);

int
adler32_main(void)
{
	platformSpecificInit(NULL);
	do_checksum();
	exit(0);
}

/*
 * Compute a checksum over the interesting part of the output.
 * This is from the PATCH_VERSION line (inclusive) all the way to the end.
 * The "# Patch checksum=..." line is not checksummed.
 *
 * If there are human readable diffs above PATCH_VERSION, they get their
 * own checksum.
 *
 * adler32() is in zlib.
 */
private void
do_checksum(void)
{
	char buf[MAXLINE];
	int len;
	int doXsum = 0;
	uLong sum = 0;
#define EOT 0x04

	while (fnext(buf, stdin)) {
		if (buf[0] == EOT) break; /* EOF ndicator  */
		if (streq(buf, PATCH_CURRENT)) {
			if (!doXsum) doXsum = 1;
			else {
				printf("# Human readable diff checksum=%.8lx\n", sum);
				sum = 0;
			}
		} else if (streq(buf,
		 "# that BitKeeper cares about, is below these diffs.\n")) {
			doXsum = 1;
		}

more:		len = strlen(buf);
		assert(len < (sizeof(buf)));
		assert(buf[len] == 0);  

		if (doXsum) {
			len = strlen(buf);
			sum = adler32(sum, buf, len);
		}
		fputs(buf, stdout);
		/*
		 * If this line is longer the buffer size,
		 * get the rest of the line.
		 */
		if (buf[len-1] != '\n') {
			assert(!feof(stdin));
			if (fnext(buf, stdin)) goto more;
		}       
		if (feof(stdin)) break;
	}
	printf("# Patch checksum=%.8lx\n", sum);
}
