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
	char	buf[MAXLINE];
	int	len;
	int	doXsum = 0;
	uLong	sum = 0;
	int	type = 0;
#define EOT 0x04

	while (fnext(buf, stdin)) {
		if (streq(buf, PATCH_ABORT)) {
			type = -1;
		} else if (streq(buf, PATCH_OK)) {
			type = 1;
		} else {
			type = 0;
		}

		/*
		 * Might be embedded data, in which case we pass it.
		 */
		if (type && fnext(buf, stdin)) {
			char	*t = (type == -1) ? PATCH_ABORT : PATCH_OK;
			if (doXsum) {
				len = strlen(t);
				sum = adler32(sum, t, len);
			}
			fputs(t, stdout);
		} else if (type == -1) {
			fprintf(stderr, "adler32: aborting\n");
			exit(1);
		} else if (type == 1) {
			break;
		}

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
		if (doXsum) {
			len = strlen(buf);
			sum = adler32(sum, buf, len);
		}
		fputs(buf, stdout);
		if (feof(stdin)) {
			fprintf(stderr, "adler32: did not see patch EOF\n");
			exit(1);
		}
	}
	printf("# Patch checksum=%.8lx\n", sum);
}
