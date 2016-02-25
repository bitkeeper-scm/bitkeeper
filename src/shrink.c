/*
 * Copyright 2001-2006,2015-2016 BitMover, Inc
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

private	void	token(MDBM *m, char *t);
private	void	print(MDBM *m, char *t);
private	void	_print(MDBM *m, char *t);
private	void	add(MDBM *m, char *t);
private	void	addn(MDBM *m, char *t, int i);
private	void	sort(MDBM *m);
private	void	convert_init(void);
private	u8	*toc(u32 val);

/*
 * TODO - keep track of any root keys which occur more than once.
 * Encode them and replace them with the encoding.
 *
 * I can use the high bit to indicate that this is the last char in a
 * token and then not put the | or space there.
 */
int
shrink_main(int ac, char **av)
{
	char	buf[MAXKEY*2];
	char	*s, *t;
	FILE	*f;
	int	start, end;
	MDBM	*m = mdbm_mem();

	if (proj_cd2root()) {
		fprintf(stderr, "Cannot find package root.\n");
		exit(1);
	}
	unless (f = fopen(CHANGESET, "r")) {
		fprintf(stderr, "Cannot find package root.\n");
		exit(1);
	}
	convert_init();
	while (fnext(buf, f)) if (streq(buf, "\001T\n")) break;
	start = 0;
	while (fnext(buf, f)) {
		start += strlen(buf);
		if (buf[0] == '\001') continue;
		for (s = t = buf; *s; s++) {
			switch (*s) {
			    case '\n':
				*s = 0;
				token(m, t);
				break;
			    case ' ':
			    case '|':
				*s = 0;
				token(m, t);
				t = s+1;
				break;
			}
		}
	}
	sort(m);
	rewind(f);
	freopen(".weave", "w", stdout);
	while (fnext(buf, f)) if (streq(buf, "\001T\n")) break;
	while (fnext(buf, f)) {
		if (buf[0] == '\001') {
			unless (buf[1] == 'E') {
				printf("\001%s\n", toc(atoi(&buf[3])));
			}
			continue;
		}
		for (s = t = buf; *s; s++) {
			switch (*s) {
			    case '\n':
				*s = 0;
				print(m, t);
				printf("\n");
				break;
			    case ' ':
			    case '|':
				*s = 0;
				print(m, t);
				t = s+1;
				break;
			}
		}
	}
	end = size(".weave") + size(".encode");
	fprintf(stderr, "%d -> %d, %.2fX shrinkage\n", start, end, (float)start/end);
	return (0);
}

private void
token(MDBM *m, char *t)
{
	add(m, t);
}

private void
print(MDBM *m, char *t)
{
	_print(m, t);
}

private void
_print(MDBM *m, char *t)
{
	datum	k, v;
	int	len, n;

	k.dptr = t;
	len = strlen(t);
	k.dsize = len + 1;
	v = mdbm_fetch(m, k);
	unless (v.dsize) {
		fprintf(stderr, "No '%s'\n", t);
		assert(0);
	}
	n = *(int*)v.dptr;
	printf("%s", toc(n));
}

private void
add(MDBM *m, char *t)
{
	datum	k, v;
	int	len, n;

	k.dptr = t;
	len = strlen(t);
	k.dsize = len + 1;
	v = mdbm_fetch(m, k);
	if (v.dsize) {
		n = *(int*)v.dptr;
	} else {
		n = 0;
	}
	n += len;
	v.dptr = (void*)&n;
	v.dsize = sizeof(n);
	mdbm_store(m, k, v, MDBM_REPLACE);
}

private void
addn(MDBM *m, char *t, int i)
{
	datum	k, v;

	k.dptr = t;
	k.dsize = strlen(t) + 1;
	v.dptr = (void*)&i;
	v.dsize = sizeof(i);
	mdbm_store(m, k, v, MDBM_REPLACE);
}

private void
sort(MDBM *m)
{
	kvpair	kv;
	int	i;
	char	*s;
	char	buf[MAXKEY];
	FILE	*sort = popen("bk sort -nr > .sort", "w");
	FILE	*encode = fopen(".encode", "w");

	for (kv = mdbm_first(m); kv.key.dsize != 0; kv = mdbm_next(m)) {
		i = *(int*)kv.val.dptr;
		fprintf(sort, "%u %s\n", i, kv.key.dptr);
	}
	pclose(sort);
	sort = fopen(".sort", "r");
	i = 1;
	while (fnext(buf, sort)) {
		chop(buf);
		s = strchr(buf, ' ');
		*s++ = 0;
		addn(m, s, i);
		fprintf(encode, "%s %s\n", toc(i), s);
		i++;
	}
	fclose(encode);
	fclose(sort);
}

/*
 * Routines to convert to/from some large base for compression
 */
#include	<stdio.h>
#define	u8	unsigned char
#define	u32	unsigned int
#define	BASE	124

u8	a2i[256], i2a[256];

private void
convert_init(void)
{
	a2i[2] = 0; i2a[0] = 2;
	a2i[3] = 1; i2a[1] = 3;
	a2i[4] = 2; i2a[2] = 4;
	a2i[5] = 3; i2a[3] = 5;
	a2i[6] = 4; i2a[4] = 6;
	a2i[7] = 5; i2a[5] = 7;
	a2i[8] = 6; i2a[6] = 8;
	a2i[9] = 7; i2a[7] = 9;
	a2i[11] = 8; i2a[8] = 11;
	a2i[12] = 9; i2a[9] = 12;
	a2i[14] = 10; i2a[10] = 14;
	a2i[15] = 11; i2a[11] = 15;
	a2i[16] = 12; i2a[12] = 16;
	a2i[17] = 13; i2a[13] = 17;
	a2i[18] = 14; i2a[14] = 18;
	a2i[19] = 15; i2a[15] = 19;
	a2i[20] = 16; i2a[16] = 20;
	a2i[21] = 17; i2a[17] = 21;
	a2i[22] = 18; i2a[18] = 22;
	a2i[23] = 19; i2a[19] = 23;
	a2i[24] = 20; i2a[20] = 24;
	a2i[25] = 21; i2a[21] = 25;
	a2i[26] = 22; i2a[22] = 26;
	a2i[27] = 23; i2a[23] = 27;
	a2i[28] = 24; i2a[24] = 28;
	a2i[29] = 25; i2a[25] = 29;
	a2i[30] = 26; i2a[26] = 30;
	a2i[31] = 27; i2a[27] = 31;
	a2i[32] = 28; i2a[28] = 32;
	a2i[33] = 29; i2a[29] = 33;
	a2i[34] = 30; i2a[30] = 34;
	a2i[35] = 31; i2a[31] = 35;
	a2i[36] = 32; i2a[32] = 36;
	a2i[37] = 33; i2a[33] = 37;
	a2i[38] = 34; i2a[34] = 38;
	a2i[39] = 35; i2a[35] = 39;
	a2i[40] = 36; i2a[36] = 40;
	a2i[41] = 37; i2a[37] = 41;
	a2i[42] = 38; i2a[38] = 42;
	a2i[43] = 39; i2a[39] = 43;
	a2i[44] = 40; i2a[40] = 44;
	a2i[45] = 41; i2a[41] = 45;
	a2i[46] = 42; i2a[42] = 46;
	a2i[47] = 43; i2a[43] = 47;
	a2i[48] = 44; i2a[44] = 48;
	a2i[49] = 45; i2a[45] = 49;
	a2i[50] = 46; i2a[46] = 50;
	a2i[51] = 47; i2a[47] = 51;
	a2i[52] = 48; i2a[48] = 52;
	a2i[53] = 49; i2a[49] = 53;
	a2i[54] = 50; i2a[50] = 54;
	a2i[55] = 51; i2a[51] = 55;
	a2i[56] = 52; i2a[52] = 56;
	a2i[57] = 53; i2a[53] = 57;
	a2i[58] = 54; i2a[54] = 58;
	a2i[59] = 55; i2a[55] = 59;
	a2i[60] = 56; i2a[56] = 60;
	a2i[61] = 57; i2a[57] = 61;
	a2i[62] = 58; i2a[58] = 62;
	a2i[63] = 59; i2a[59] = 63;
	a2i[64] = 60; i2a[60] = 64;
	a2i[65] = 61; i2a[61] = 65;
	a2i[66] = 62; i2a[62] = 66;
	a2i[67] = 63; i2a[63] = 67;
	a2i[68] = 64; i2a[64] = 68;
	a2i[69] = 65; i2a[65] = 69;
	a2i[70] = 66; i2a[66] = 70;
	a2i[71] = 67; i2a[67] = 71;
	a2i[72] = 68; i2a[68] = 72;
	a2i[73] = 69; i2a[69] = 73;
	a2i[74] = 70; i2a[70] = 74;
	a2i[75] = 71; i2a[71] = 75;
	a2i[76] = 72; i2a[72] = 76;
	a2i[77] = 73; i2a[73] = 77;
	a2i[78] = 74; i2a[74] = 78;
	a2i[79] = 75; i2a[75] = 79;
	a2i[80] = 76; i2a[76] = 80;
	a2i[81] = 77; i2a[77] = 81;
	a2i[82] = 78; i2a[78] = 82;
	a2i[83] = 79; i2a[79] = 83;
	a2i[84] = 80; i2a[80] = 84;
	a2i[85] = 81; i2a[81] = 85;
	a2i[86] = 82; i2a[82] = 86;
	a2i[87] = 83; i2a[83] = 87;
	a2i[88] = 84; i2a[84] = 88;
	a2i[89] = 85; i2a[85] = 89;
	a2i[90] = 86; i2a[86] = 90;
	a2i[91] = 87; i2a[87] = 91;
	a2i[92] = 88; i2a[88] = 92;
	a2i[93] = 89; i2a[89] = 93;
	a2i[94] = 90; i2a[90] = 94;
	a2i[95] = 91; i2a[91] = 95;
	a2i[96] = 92; i2a[92] = 96;
	a2i[97] = 93; i2a[93] = 97;
	a2i[98] = 94; i2a[94] = 98;
	a2i[99] = 95; i2a[95] = 99;
	a2i[100] = 96; i2a[96] = 100;
	a2i[101] = 97; i2a[97] = 101;
	a2i[102] = 98; i2a[98] = 102;
	a2i[103] = 99; i2a[99] = 103;
	a2i[104] = 100; i2a[100] = 104;
	a2i[105] = 101; i2a[101] = 105;
	a2i[106] = 102; i2a[102] = 106;
	a2i[107] = 103; i2a[103] = 107;
	a2i[108] = 104; i2a[104] = 108;
	a2i[109] = 105; i2a[105] = 109;
	a2i[110] = 106; i2a[106] = 110;
	a2i[111] = 107; i2a[107] = 111;
	a2i[112] = 108; i2a[108] = 112;
	a2i[113] = 109; i2a[109] = 113;
	a2i[114] = 110; i2a[110] = 114;
	a2i[115] = 111; i2a[111] = 115;
	a2i[116] = 112; i2a[112] = 116;
	a2i[117] = 113; i2a[113] = 117;
	a2i[118] = 114; i2a[114] = 118;
	a2i[119] = 115; i2a[115] = 119;
	a2i[120] = 116; i2a[116] = 120;
	a2i[121] = 117; i2a[117] = 121;
	a2i[122] = 118; i2a[118] = 122;
	a2i[123] = 119; i2a[119] = 123;
	a2i[124] = 120; i2a[120] = 124;
	a2i[125] = 121; i2a[121] = 125;
	a2i[126] = 122; i2a[122] = 126;
	a2i[127] = 123; i2a[123] = 127;
}

private u8*
toc(u32 val)
{
	u8	reverse[6];
	static	u8 buf[7];
	int	i, j, d;

	for (i = 0; val; i++, val /= BASE) {
		d = val % BASE;
		reverse[i] = i2a[d];
	}
	for (j = 0; i--; ) buf[j++] = reverse[i];
	buf[j] = 0;
	return (buf);
}

#ifdef	TEST
main()
{
	u32	i;
	u8	*s;

	convert_init();
	for (i = 1; i < 0x8ffffff; i <<= 1) {
		s = toc(i);
		printf("%10u %6s\n", i, s);
	}
	//for (i = 0; i < 0xffffffff; i++) {
	for (i = 0; i < 0xffffff; i++) {
		s = toc(i);
		if (tou(s) != i) {
			printf("i=%u s=%s b=%u\n", i, s, tou(s));
			break;
		}
		if ((i % 100000) == 0) fprintf(stderr, "%u\r", i);
	}
	fprintf(stderr, "\n");
}
#endif

#ifdef	TOC
main(int ac, char **av)
{
	u8	buf[100];

	convert_init();
	while (fgets(buf, sizeof(buf), stdin)) {
		printf("%s\n", toc(atoi(buf)));
	}
	exit(0);
}
#endif

#ifdef	TOU
main(int ac, u8 **av)
{
	convert_init();
	if (av[1] == 0) exit(1);
	printf("%u\n", tou(av[1]));
	exit(0);
}
#endif
