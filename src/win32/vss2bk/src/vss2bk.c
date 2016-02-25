/*
 * Copyright 2002-2004,0 BitMover, Inc
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
 * Copyright (c) 2004 BitMover Inc 
 * Do not redistribute source code in this directory. You are only
 * licensed to use this code to convert a Visual SourceSafe DB to a
 * BitKeeper repository.
 *
 * This is a Visual SourceSafe to BitKeeper converter. This program uses
 * the VSS command line interface "ss.exe" to extract data from VSS,
 * and uses "bk.exe" and the bk init file to import data into BitKeeper.
 * It uses only four "ss" sub-commands:
 * 1) ss dir vssDir -R
 * 2) ss history
 * 3) ss get 
 * 4) ss filetype
 *
 * This program performs only one task. It converts VSS delta to BitKeeper
 * delta. It does not commit the delta and it does not detect changeset
 * boundary. The advance feature is provided by the "bk _findcset" command.
 * The command is avaliable in a BitKeeper internal release, contact
 * support@bitmover when your VSS DB is converted and is ready
 * for "bk _findcset".
 * 
 * Note: You may need to set SSDIR to the path of VSS database.
 *
 * Usage:
 *	vss2bk [-p N] [-B] -L user,passwd vssDir
 * 	-p N    skip the first N'th component of the VSS file path.
 *	-B	skip any version that ss.exe fail to get (for corrupt VSS db)
 * 	-L	for specifying VSS user and password.
 *	VssDir	the VSS directory you want to convert.
 *
 * KNOWN BUG: 
 * a) This program does not understand VSS branch and rollback message yet.
 * b) If you supply the wrong username and/or password, the command will hang
 *    because ss.exe wants to drop into interactive mode.
 */


#include "vss2bk.h"
#include "liblines.h"

char    *ss;		/* Path to ss.exe */
char	*timeZone;	/* Time zone for the converted DB */
meta 	*mstack = 0;	/* Stack for extracted VSS meta data */

private int recordType(char *);
private int isRecordSeperator(char *);
private int getVssFile(char *, int, char *, char *);
private int parseUserLine(meta *, FILE *, char *, int);
private int mkBkDelta(opts *, meta *, char *, int *, char **);
private int chk_av(char **, int);
private int isJunkLine(char *, int);
private void push(meta *);
private void freeMeta(meta *);
private void mkInitFile(char *, int, meta *, char *, char *);
private void listHistory(opts *, char *, char *);
private void init(opts *);
private void chkEncoding(char *, char *);
private void sortTag(char **, int, char *);
private void convertFile(opts *, char *, char *, char ***, int *);
private char *getVssPath(void);
private char *filetype(opts *, char *);
private char *getline(char *, int, FILE *);
private char *bkEncoding(char *, char *);
private char *listVssTree(opts *, char *);
private meta *pop(void);
private meta *parseVersionRecord(FILE *, char *, int, char *);
private meta *parseMiscRecord(FILE *, char *, int, char *);

int
main(int ac, char **av)
{
	opts	opts;
	int	c;
	int	tagCount = 0;
	char	**tags = 0;
	char	*bkRoot, *vssDir;
	char	*vssCurrentDir = 0, *fileList = 0;

	init(&opts);
	while ((c = getopt(ac, av, "BdL:p:")) != -1) {
		switch (c) {
		    case 'B': opts.skipBadVersion = 1; break;
		    case 'd': opts.debug = 1; break;
		    case 'L': opts.login = aprintf("-Y%s", optarg); break;
		    case 'p': opts.skipSubPath = atoi(optarg); break;
		    default:
usage:			    fprintf(stderr,
				"Usage; bk vss2bk [-L UESER,PASSWORD] "
				"vssDir bkRoot\n");
			    return(1);
		}
	}
	unless (av[optind] && av[optind + 1])  goto usage;

	if(chk_av(av, optind)) return (1);
	vssDir = av[optind];
	bkRoot = av[optind + 1];

	/*
	 * After this, the rest of this program runs with pwd = bkRoot
	 */
	if (chdir(bkRoot = av[optind + 1])) {
		perror(bkRoot);
		return (1);
	}

	fileList = listVssTree(&opts, vssDir);
	tags = convertTree(&opts, fileList, &tagCount);
	sortTag(tags, tagCount, "BitKeeper/tmp/tags");
	
	/*
	 * Clean up
	 */
	if (fileList) {
		if (*fileList) unlink(fileList);
		free(fileList);
	}
	if (vssCurrentDir) free(vssCurrentDir);
	if (opts.login[0]) free(opts.login);
	freeLines(tags, free);
	return (0);
}

private void
init(opts *opts)
{
	bzero(opts, sizeof(*opts));
	opts->login = "";
        _setmode(1, _O_BINARY);
        _setmode(2, _O_BINARY);
	timeZone = getTimeZone();
	unless (ss = getVssPath()) {
		fprintf(stderr,
		    "vss2bk needs the vss program ss.exe, "
		    "which was not found in your PATH.\n");
    		exit(1);
	}
}

private int
chk_av(char **av, int optind)
{
	char buf[MAXPATH];

	unless (strneq("$/", av[optind], 2)) {
		fprintf(stderr,
		    "%s: vssDir must be an absolute path\n"
		    "e.g. $/proj1\n",
		    av[optind]);
		return (1);
	}

	sprintf(buf, "%s/%s", av[optind + 1], BKROOT);
	unless (exists(buf)) {
		fprintf(stderr,
		    "%s is not a BitKeeper root.\n"
		    "You may want to run \"bk setuptool\" first\n",
		    av[optind]);
		return (1);
	}
	return (0);
}

char *
listVssTree(opts *opts, char *vssDir)
{
	char	*cmd;
	char	*fileList = calloc(MAXPATH, 1);

	bktemp(fileList);

	/*
	 * What we want is "ss dir opts ... > fileList"
	 */
	cmd = aprintf("%s dir \"%s\" -R -O@%s %s",
		       			ss, vssDir, fileList, opts->login);
	if (system(cmd)) {
		fprintf(stderr, "\"%s\" command failed\n", cmd);
		free(cmd);
		exit(1);
	}
	free(cmd);
	return(fileList);
}

/*
 * Walk the fileList and convert each vss file into BitKeeper file
 */
char **
convertTree(opts *opts, char *fileList, int *tagCount)
{
	FILE	*f;
	char	*file;
	char	*vssCurrentDir = 0;
	char	**tags = 0;
	char	buf[MAXPATH];
	char	initFile[MAXPATH] = "";

	bktemp(initFile);
	unless (f = fopen(fileList, "rt")) {
		perror(fileList);
		exit(1);
	}
	while (fnext(buf, f)) {
		int	len;

		if (opts->debug) fputs(buf, stderr);
		chomp(buf);
		len = strlen(buf);
		if (strneq(buf, "Username:", 9)) {
			fprintf(stderr,
			    "VSS is prompting for a passwd?\n"
			    "Please make sure you supply the correct "
			    "credential via the -u option\n");
			return (0);
		} else if (isJunkLine(buf, len)) {
			continue; /* skip junk */
		} else if (buf[len - 1] == ':') {
			/*
			 * Save current vss dir, we need it make a
			 * full path. (see below)
			 */
			buf[len - 1] = 0;
			if (vssCurrentDir) free(vssCurrentDir);
			vssCurrentDir = strdup(buf);
		} else {
			/*
			 * Maks a full file path and do the real work
			 */
			assert(vssCurrentDir);
			file = aprintf("%s/%s", vssCurrentDir, buf);
			convertFile(opts, file, initFile, &tags, tagCount);
			free(file);
		}
	}
	fclose(f);
	if (initFile[0]) unlink(initFile);
	return (tags);
}

/*
 * Given a VSS file, convert it to a BitKeeper file
 */
private void
convertFile(opts *opts, char *vssfile,
				char *initFile, char ***tags, int *ntags)
{
	FILE	*f;
	int	count = 0;
	meta	*m = NULL;
	char	*type = "";
	char	buf[MAXLINE];
	char	history[MAXPATH] = "";

	assert(strneq("$/", vssfile, 2)); /* must be absolute path */

	bktemp(history);
	listHistory(opts, vssfile, history);
	unless (f = fopen(history, "rt")) {
		fprintf(stderr,
		   "%s: Cannot read history file %s\n", vssfile, history);
		exit(1);
	}

	/* Skip headers */
	while (getline(buf, MAXLINE, f)) if (isRecordSeperator(buf)) break;

	/*
	 * Parse the output of "ss history ..."
	 * and extract the meta data in to the "m" struct.
	 *
	 * XXX VSS print history in decending chronological order.
	 * So push the info onto a stack to get it back
	 * in the reverse order.
	 */
	while (1) {
		int	t  = recordType(buf);

		if (t == REC_EOF) {
			break;
		} else if (t == REC_VERSION) {
			m = parseVersionRecord(f, buf, MAXLINE, vssfile);
		} else {
			m = parseMiscRecord(f, buf, MAXLINE, vssfile);
		}
		push(m);
	}
	fclose(f);
	
	/*
	 * Pop the history stack and make BitKeeper delta
	 * We also convert VSS label to BitKeeper tag.
	 */
	while (mstack) {
		m = pop();
		if (m->file && m->ver) {
			mkBkDelta(opts, m, initFile, &count, &type);
		}
		if (m->label && m->date && m->time) {
			*tags = addLine(*tags,
			    aprintf("|%s|%s %s", m->label, m->date, m->time));
			(*ntags)++;
		}
		freeMeta(m);
	}
	if (*type) free(type);
	fputs("\n", stderr);
	unlink(history);
}

private void
listHistory(opts *opts, char *vssfile, char *history)
{
	char *cmd;

	/*
	 * What we want is "ss history opts ... > history"
	 */
	cmd = aprintf("%s history \"%s\" -O@%s %s",
				     ss, vssfile, history, opts->login);
	system(cmd);
	free(cmd);
}

/*
 * Get BK file encoding
 */
private char *
bkEncoding(char *file, char *buf)
{
	FILE	*f;

	sprintf(buf, "bk prs -hnr+ -d:ENC: \"%s\"", file);
	unless (f = popen(buf, "rb")) {
		perror(buf);
		exit(1);
	}
	unless (fgets(buf, MAXPATH, f)) {
		fprintf(stderr, "cannot get file type for %s\n", file);
		return ("");
	}
	chomp(buf);
	pclose(f);
	return (buf);
}

/*
 * If vssType is "Text", warn user if BitKeeper has to
 * convert it to binary. This happens when the file has control character.
 */
private void
chkEncoding(char *file, char *vssType)
{
	char	buf[MAXPATH];

	if (streq("Text", vssType) && !streq("ascii", bkEncoding(file, buf))) {
		fprintf(stderr,
			"%s: Warning, auto text to binary conversion\n",
			file);
	}
}

private int
mkBkDelta(opts *opts, meta *m, char *initFile, int *count, char **type)
{
	int	i, rc;
	char	*p, *typeOpt, *cmd, *bkPath, *bk_dir, *sfile;
	char	dbuf[MAXPATH];


	if (opts->debug) {
		fprintf(stderr,
		    ">>>  mkBkdelta: %s, vss ver %d, count %d\n",
		    m->file, m->ver, *count);
			
	}

	bkPath = &(m->file[2]);

	/*
	 * Trim the top N dir in bkPath
	 */
	if (opts->skipSubPath) {
		p = bkPath;
		for (i = 0; i < opts->skipSubPath; i++) {
			unless (p = strchr(p, '/')) {
				fprintf(stderr,
				    "Cannot trim path: %s\n", bkPath);
				exit(1);
			} else {
				p++; /* skip slash */
			}
		}
		bkPath = p;
	}

	if (*count == 0) {
		if (mkdirf(bkPath)) {
			perror (bkPath);
			exit(1);
		}

		sfile = name2sccs(bkPath);
		if (exists(sfile)) {
			fprintf(stderr,
			    "file %s already exists, skipped\n",
			    sfile);
			free(sfile);
			exit(1);
		}

		/*
		 * Make 1.0 delta 
		 */
		unlink(bkPath);
		close(open(bkPath, _O_CREAT|_O_SHORT_LIVED));
		unless (*type = filetype(opts, m->file)) {
			fprintf(stderr, "bad file type\n");
			exit(1);
		}
		mkInitFile(initFile, 0, m, bkPath, *type); 
		if (!streq("Text", *type) && !streq("Binary", *type)) {
			fprintf(stderr,
			    "%s: Bad file type: %s\n", m->file, *type);
			exit(1);
		}
		typeOpt = streq("Text", *type) ? "" : "-b";
		cmd = aprintf("bk new %s -q -l -I%s \"%s\"",
						typeOpt, initFile, bkPath);
		rc = system(cmd);
		unlink(bkPath);
		if (rc) {
			fprintf(stderr, "\"%s\" failed\n", cmd);
			free(cmd);
			exit(1);
		}
		free(cmd);
		chkEncoding(bkPath, *type);
		(*count)++;
	}

	strcpy(dbuf, bkPath); /* because dirname() stomp */
	bk_dir = dirname(dbuf);
	if (rc = getVssFile(m->file, m->ver, opts->login, bk_dir)) {
		if (opts->skipBadVersion) {
			fprintf(stderr,
			    "skipping %s version %d\n", 
			    m->file, m->ver);
			return (0);
		} else {
			fprintf(stderr,
			    "%s: cannot get vss file version %d\n", 
			    m->file, m->ver);
			exit(1);
		}
	}
	mkInitFile(initFile, *count, m, bkPath, *type);
	cmd = aprintf("bk delta -q -l -I%s \"%s\"", initFile, bkPath);
	if (rc = system(cmd)) {
		fprintf(stderr, "\"%s\" failed\n", cmd);
		free(cmd);
		exit(1);
	}
	free(cmd);
	(*count)++;
	fprintf(stderr, "%-20s\t%d deltas\t%s\r", bkPath, *count, *type);
	return (rc);
}

private u32
sum32(u32 randbits, char *str, int len)
{
	int	i;

	for (i = 0; i < len; i++) randbits += (unsigned char ) str[i];
	return (randbits);
} 

/*
 * compute a random bits from the comment string
 */
private u32
getRandbits(meta *m)
{
	u32	randbits = 0;
	int	i;

	EACH(m->comment) {
		unless (m->comment[i]) continue;
		randbits += sum32(randbits,
					m->comment[i], strlen(m->comment[i]));
	}

	unless (m->comment) randbits = sum32(randbits,
						m->file, strlen(m->file));
	return (randbits);
}

private int
zeroDiff(int rev, char *bkPath)
{
	int	rc;
	char	cmd[2048], diffs[MAXPATH];

	if (rev == 0) return (0);

	bktemp(diffs);
	sprintf(cmd, "bk diffs \"%s\" > \"%s\"", bkPath, diffs);
	system(cmd);
	rc = size(diffs) ? 0 : 1;  
	unlink(diffs);
	return (rc);
}

/*
 * Make BitKeeper Init File
 * The init file is where we store the meta data extracted from VSS
 */
private void
mkInitFile(char *initFile, int rev, meta *m, char *bkPath, char *type)
{
	FILE	*f;
	int	i;
	u32	randbits;

	randbits  = getRandbits(m);
	f = fopen(initFile, "wb");
	assert(f);
	fprintf(f, "D 1.%d %s %s %s@vss 0 0 0/0/0\n",
	   rev, m->date, m->time, m->user);
	EACH(m->comment) {
		fprintf(f, "c %s\n", m->comment[i]);
	}
	
	if (zeroDiff(rev, bkPath) ||	
				(streq(type, "Text") && (size(bkPath) == 0))) {
		fprintf(f, "K %-u\n", randbits & 0xffff);
	}
	 
	fprintf(f, "P %s\n", bkPath); /* store the path */
	if (rev == 0) fprintf(f, "R %-x\n", randbits);
	fclose(f);
}

/*
 * Get the path to ss.exe
 */
private char *
getVssPath(void)
{
	long	vlen;
	char	*p;
	char	vsspath[MAXPATH];
#define VSS_KEY	"Software\\Microsoft\\SourceSafe"

	/*
	 * Look for the path to ss.exe
	 */
	vlen = MAXPATH;
	if (getReg(HKEY_LOCAL_MACHINE,
			VSS_KEY, "SCCServerPath", vsspath, &vlen)) {
		char vssShortPath[MAXPATH];

		GetShortPathName(vsspath, vssShortPath, MAXPATH);
		localName2bkName(vssShortPath, vssShortPath);
		p = aprintf("%s/ss.exe", dirname(vssShortPath));
		if (p && exists(p)) return (p);
	}
	if (p = prog2path("ss"))  return (p);
	return (0);
}

char *
getTimeZone(void)
{
	char	*t;
	FILE	*f;

	unless (t = getenv("VSS_TIME_ZONE")) {
		char	zbuf[100];

		f = popen("bk zone", "rb");
		fscanf(f, "%s", zbuf);
		pclose(f);
		t = strdup(zbuf);
		fprintf(stderr, "Warning: no VSS_TIME_ZONE defined, "
		    "default to %s.\n", t);
	} else {
		t = strdup(t);
	}
	if (((t[0] != '-') && (t[0] != '+')) || 
	    !isdigit(t[1]) || !isdigit(t[2]) ||
	    (t[3] != ':') || !isdigit(t[4]) ||
	    !isdigit(t[5])) {
		fprintf(stderr,
		    "VSS_TIME_ZONE must be in [+|-]hh:mm format.\n");
		exit(1);
	}
	return (t);
}


private const char *
getDateTimeField(const char *line)
{
	const char	*p;

	/* line looks like "|tag|yyyy/mm/dd hh:mm:ss+hh:mm" */
	p = strrchr(line, '|');
	unless (p) return (0);
	return (++p);
}

/*
 * Sort in accending order of time
 */
private int
tagCompare(const void *l1, const void *l2)
{
	const char	*d1, *d2;

	d1 = getDateTimeField(*(char**)l1);
	d2 = getDateTimeField(*(char**)l2);
	assert(d1 && d2);
	/*
	 * Due to our date/time string format,
	 * string order is same as time order.
	 */
	return (strcmp(d1, d2));

}


/*
 * Sort the tag because _findcset wants it in this format
 */
private void
sortTag(char **tags, int tag_count, char *tagfile)
{
	char	*last = 0;
	int	i;
	FILE	*f;

	unless (tags) return;
	qsort((void *) &tags[1], tag_count, sizeof(char *), tagCompare);
	f = fopen(tagfile, "wb");
	EACH (tags) {
		if (last && streq(last, tags[i])) continue; /* skip duplicate */
		fprintf(f, "%s\n", tags[i]);
		last =  tags[i];
	}
	fclose(f);
}

/*
 * Catches all noise messages from VSS
 * if noise message return 1, else 0;
 */
private int
isJunkLine(char *buf, int len)
{
        if (buf[0] == '\0') {
                return (1);
        } else if ((buf[0] == '$') && (buf[len - 1] != ':')) {
                return (1);
        } else if (strneq("No item found under", buf, 19)) {
                return (1);
        } else if (streq(" item(s)", &buf[len - 8])) {
                return (1);
        } else if (strneq("No items found under ", buf, 21)) {
                return (1);
        }
        return (0);
}

/*
 * Convert mm/dd/yy => yyyy/mm/dd
 */
private char *
fixupDate(char *date)
{
	char	*p, *year, *month, *day;
	int	 yy, yyyy, mm, dd;

	month = date;
	p = date;
	while (*p) {
		if (*p == '/') {
			*p++ = 0;
			day = p;
			break;
		}
		p++;
	}

	assert(p);
	while (*p) {
		if (*p == '/') {
			*p++ = 0;
			year = p;
			break;
		}
		p++;
	}
	assert(p);
	
	/* Convert 2 digits year to 4 digits year if needed */
	yy = atoi(year);
	if (yy < 100) {
		if (yy < 69) {
			yyyy = 2000 + yy;
		} else {
			yyyy = 1900 + yy;
		}
	} else {
		yyyy = yy;
	}
	mm = atoi(month);
	dd = atoi(day);
	if (mm > 12) {
		fprintf(stderr, "Month %d is illegal.\n", mm);
err:		fprintf(stderr,
		    "Please make sure your system date is in the "
		    "mm/dd/yy format.\n"
		    "Otherwise you will need to modify the fixupDate() "
		    "function in vss2bk.c to match the ss.exe date format." );
		exit(1);
	}
	if (dd > 31) {
		fprintf(stderr, "Day %d is illegal.\n", dd);
		goto err;
	}
	return (aprintf("%04d/%02d/%02d", yyyy, mm, dd));
}

/*
 * Convert hh:mm<a|p> to hh:mm:ss 
 */
private char *
fixupTime(char *time)
{
	int hh, mm;
	char am_pm = 0;

	sscanf(time, "%d:%d%c", &hh, &mm, &am_pm);
	assert((am_pm == 'a') || (am_pm == 'p'));
	/* 12am => 0'th hour, 12pm =>12'th hour on 24 hour clock */
	if (hh == 12) {
		hh = (am_pm == 'a') ? 0 : 12;
	} else {
		if (am_pm == 'p') hh += 12;
	}
	assert((hh < 24) && (hh >= 0));
	return (aprintf("%02d:%02d:00%s", hh, mm, timeZone));
}

/*
 * Extract file type info from VSS. File type is either "Text" or "Binary".
 */
private char *
filetype(opts *opts, char *fpath)
{
	FILE	*f;
	char	*cmd = aprintf("%s filetype \"%s\" %s",
						ss, fpath, opts->login);
	char	*p;
	char 	buf[MAXLINE];

	f = popen(cmd, "rt");
	unless (fnext(buf, f)) {
		fprintf(stderr, "cannot get file type for %s\n", fpath);
		return (NULL);
	}
	chomp(buf);

	/*
	 * Scan backward, because VSS use basename only when path is long
	 */
	p = &buf[strlen(buf) - 1];
	while (*p) {
		if (*p == ' ') break;
		p--;
	}
	pclose(f);
	p++;
	return (strdup(p));

}

private char *
getline(char *buf, int size, FILE *f)
{
	char *p;

	unless (p = fgets(buf, size, f)) return (0);
	chomp(buf);
	return (p);

}


private int
isRecordSeperator(char *buf)
{
	return (strneq(buf, "**", 2));
}

private int
recordType(char *buf)
{

	if (streq(buf, VSS_EOF)) {
		return (REC_EOF);
	} else if (strneq(buf, "*****************  Version ", 27)) {
		return (REC_VERSION);
	} 
	return (REC_MISC);
}

private void
push(meta *m)
{
	m->next = mstack;
	mstack = m;
}

private meta *
pop(void)
{
	meta *m;

	m = mstack;
	mstack = mstack->next;
	return (m);
}

private void
freeMeta(meta *m)
{
	if (m->file) free(m->file);
	if (m->user) free(m->user);
	if (m->date) free(m->date);
	if (m->time) free(m->time);
	if (m->label) free(m->label);
	freeLines(m->comment, free);
	free(m);
}

private int
parseUserLine(meta *m, FILE *f, char *buf, int size)
{
	char	*p;
	char	user[100], date[100], time[100];

	user[0] = date[0] = time[0] = '\0';
	sscanf(buf,  "User: %s Date: %s Time: %s", user, date, time);

	/*
	 * Force lower case user name
	 */
	p = user;
	while (*p) {
		*p = tolower(*p);
		p++;
	}
	m->user = strdup(user);
	m->date = fixupDate(date);
	m->time = fixupTime(time);
	return (!getline(buf, size, f));
}

private int
parseLabel(meta *m, FILE *f, char *buf, int size)
{
	int	len;
	char	*p;

	len =strlen(buf);
	if (buf[len - 1] == '\"') buf[len -1] = 0;
	p = &buf[8];
	while (*p) {
		/* Should we issue a warning for the auto conversion ? */
		if (isspace(*p)) *p = '_'; 
		if (*p == '/') *p = '_'; 
		p++;
	}
	if (isdigit(buf[8])) {
		/* Tag must not start with digit */
		m->label = aprintf("V%s", &buf[8]);
	} else {
		m->label = strdup(&buf[8]);
	}
	return (!getline(buf, size, f));
}

/*
 * Break up long comment line
 */
private void
saveComment(meta *m, char *cmt)
{
	int	len;
	char	*p;

	len = strlen(cmt);

	if (len <= 80) {
		m->comment = addLine(m->comment, strdup(cmt));
		return;
	}
	p = &cmt[80];
	while (p > &cmt[40]) {
		if (isspace(*p)) {
			*p++ = 0;
			m->comment = addLine(m->comment, strdup(cmt));
			saveComment(m, p);
			return;
		}
		p--;
	}
	fprintf(stderr,
	    "Warning: Cannot break up long comment line:\n\t%s", cmt);
	m->comment = addLine(m->comment, strdup(cmt));
}

private int
parseCommentLines(meta *m, FILE *f, char *buf, int size)
{
	char	*p = 0;
	int	rc;

	if (buf[9]) saveComment(m, &buf[9]);
	while (1) {
		unless (getline(buf, size, f)) {
			rc = 1; /* EOF */
			goto done;
		}
		if (isRecordSeperator(buf)) break;
		if (p) {
			saveComment(m, p);
			free(p);
		}
		p = strdup(buf);
	}
done:	if (p && p[0]) {
		saveComment(m, p);
	}
	if (p) free(p);
	return (0);
}

private int
parseLabelCommentLines(meta *m, FILE *f, char *buf, int size)
{
	while (1) {
		unless (getline(buf, size, f)) return(1); /* EOF */
		if (isRecordSeperator(buf)) break;
	}
	return (0);
}

private meta *
parseVersionRecord(FILE *f, char *buf, int size, char *vssfile)
{
	meta	*m;
	int	last = 0;

	m = calloc(1, sizeof (struct meta));
	m->file = strdup(vssfile);
	m->ver = atoi(&buf[27]);
	unless (getline(buf, size, f)) {
		fprintf(stderr, "parseVersionRecord: unexecpected EOF\n");
		exit(1);
	}
	while (1) {
		if (strneq(buf, "User:", 5)) {
			last = parseUserLine(m, f, buf, size);
		} else if (strneq(buf, "Created", 7)) {
			last = !getline(buf, size, f);
		} else if (strneq(buf, "Checked", 7)) {
			last = !getline(buf, size, f);
		} else if (strneq(buf, "Branched", 7)) {
			last = !getline(buf, size, f);
		} else if (strneq(buf, "Rolled back", 7)) {
			last = !getline(buf, size, f);
		} else if (strneq(buf, "Label:", 6)) {
			last = parseLabel(m, f, buf, size);
		} else if (strneq(buf, "Labeled", 7)) {
			last = !getline(buf, size, f);
		} else if (strneq(buf, "Label comment:", 14)) {
			last = parseLabelCommentLines(m, f, buf, size);
		} else if (strneq(buf, "Comment:", 8)) {
			last = parseCommentLines(m, f, buf, size);
		} else if (buf[0] == 0) {
			last = !getline(buf, size, f); /* skip blank line */
		} else {
			fprintf(stderr,
			    "%s: unknown entry: %s\n", vssfile, buf);
			exit(1);
		}
		if (last || isRecordSeperator(buf)) break;
	}
	if (last) strcpy(buf, VSS_EOF);
	return (m);
}

private struct meta *
parseMiscRecord(FILE *f, char *buf, int size, char *vssfile)
{
	int	last = 0;
	meta	*m = 0;

	unless (getline(buf, size, f)) {
		fprintf(stderr, "parseMiscRecord: unexecpected EOF\n");
		exit(1);
	}
	m = calloc(1, sizeof (meta));
	while (1) {
		if (strneq(buf, "Label:", 6)) {
			assert(m->label == 0);
			last = parseLabel(m, f, buf, size);
		} else if (strneq(buf, "User:", 5)) {
			last = parseUserLine(m, f, buf, size);
		} else {
			last = !getline(buf, size, f);
		}
		if (last || isRecordSeperator(buf)) break;
	}
	if (last) strcpy(buf, VSS_EOF);
	if (m->label) assert(m->date);
	return (m);
}

private int
getVssFile(char *vssfile, int version, char *login, char *to)
{
	char	*cmd;
	int	rc;

	/*
	 *  What we want is "ss get opts vssfile" => a gfile in bk repo
	 */
	cmd = aprintf("%s get \"%s\" -W -GL\"%s\" -V%d %s> nul",
		ss, vssfile, to, version, login);
	rc = system(cmd);
	free(cmd);
	return (rc);
}
