/*
 * UUencode/decode for BitSCCS.
 * 
 * This version doesn't bother converting spaces to backquotes since we aren't
 * mailing SCCS files around (yet).
 *
 * %W% %@%
 */
#include <stdio.h>

int main(int ac, char **av);
void uuencode(FILE *in, FILE *out);
void uudecode(FILE *in, FILE *out);

#define	uchar	unsigned char

int
main(int ac, char **av)
{
	if (ac == 1)
		uuencode(stdin, stdout);
	else 
		uudecode(stdin, stdout);
	return(0);
}

/* Make a character printable */
#define ENC(c)  (((c) & 0x3f) + ' ')

/* put it back */
#define DEC(c)  (((c) - ' ') & 0x3f)

inline void
uuencode1(register uchar *from, register char *to, int n)
{
	int     space[4];
	register int *c = space;
	register int i;

	*to++ = ENC(n);
	for (i = 0; i < n; i += 3) {
		c[0] = from[i] >> 2;
		c[1] = ((from[i]<<4)&0x30) | ((from[i+1]>>4)&0xf);
		c[2] = ((from[i+1]<<2)&0x3c) | ((from[i+2]>>6)&3);
		c[3] = from[i+2] & 0x3f;
		*to++ = ENC(c[0]);
		*to++ = ENC(c[1]);
		*to++ = ENC(c[2]);
		*to++ = ENC(c[3]);
	}
	*to++ = '\n';
}

/*
 * A little weird because it doesn't bother to generate an exact line - the
 * length encoding at the beginning will make it ignore any trailing garbage.
 *
 * This one is 1.6 times faster than the BSD uuencode.  On Intel, you need to
 * compile with "-O -fomit-frame-pointer" to get the good numbers.
 */
void
uuencode(FILE *in, FILE *out)
{
	uchar	ibuf[450];
	uchar	obuf[650];
	register uchar *buf;
	register uchar *p = obuf;
	register int n;
	register int length;

#ifdef	ENVELOPE
	fputs("begin 664 X.UU\n", out);
#endif
	while ((length = fread(ibuf, 1, 450, in)) > 0) {
		p = obuf;
		buf = ibuf;
		while (length > 0) {
			n = (length > 45) ? 45 : length;
			length -= n;
			uuencode1(buf, p, n);
			buf += n;
		}
		*p = 0;
		fputs(obuf, out);
	}
#ifdef ENVELOPE
	fputs("`\nend\n", out);
#endif
}

inline int
uudecode1(register uchar *from, register char *to)
{
	register int	length = DEC(*from++);
	int	save = length;

	if (!length) return (0);
	while (length > 0) {
		if (length-- > 0)
			*to++ = (uchar)((DEC(from[0])<<2) | (DEC(from[1])>>4));
		if (length-- > 0)
			*to++ = (uchar)((DEC(from[1])<<4) | (DEC(from[2])>>2));
		if (length-- > 0)
			*to++ = (uchar)((DEC(from[2]) << 6) | DEC(from[3]));
		from += 4;
	}
	return (save);
}

/*
 * This one is 1.1 times faster than the BSD uuencode.  On Intel, you need to
 * compile with "-O -fomit-frame-pointer" to get the good numbers.
 */
void
uudecode(FILE *in, FILE *out)
{
	uchar	ibuf[100];
	uchar	obuf[BUFSIZ];
	register int	n;
	register uchar *to;

#ifdef	ENVELOPE
	while (fgets(ibuf, sizeof(ibuf), in)) {
		if (strncmp(ibuf, "begin ", 6) == 0) break;
	}
#endif
	to = obuf;
	while (fgets(ibuf, sizeof(ibuf), in)) {
		if ((n = uudecode1(ibuf, to)) <= 0) break;
		to += n;
		if ((to - obuf) > (BUFSIZ - 100)) {
			fwrite(obuf, to - obuf, 1, out);
			to = obuf;
		}
	}
	if (to != obuf) fwrite(obuf, to - obuf, 1, out);
}
