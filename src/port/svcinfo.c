/*
 * Copyright 2005-2006,2015-2016 BitMover, Inc
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

#include "../sccs.h"

#ifdef	WIN32
int
svcinfo_main(int ac, char **av)
{
	HKEY	hKey, subKey;
	int	rc, i, off, c;
	int	list = 0;
	char	*info = 0;
	char	*key = "SYSTEM\\CurrentControlSet\\Services";
	char	*p;
	char	**cmds;
	DWORD	blen;
	char	buf[1024], cmdline[1024];

	while ((c = getopt(ac, av, "i:l", 0)) != -1) {
		switch(c) {
		case 'i': info = optarg; break;
		case 'l': list = 1; break;
		default: bk_badArg(c, av); return (1);
		}
	}
	if (info && list) {
		fprintf(stderr, "_svcinfo: -l or -i, but not both\n");
		return (1);
	}
	if (list) {
		rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE, key, 0, KEY_READ, &hKey);
		if (rc != ERROR_SUCCESS) {
			fprintf(stderr, "_svcinfo: RegOpenKeyEx failed\n");
			return (1);
		}
		off = sprintf(buf, "%s\\", key);
		for (i = 0; ; i++) {
			blen = sizeof(buf) - off;
			rc = RegEnumKeyEx(hKey, i, buf + off, &blen, 0, 0, 0, 0);
			if (rc == ERROR_NO_MORE_ITEMS) break;
			if (rc != ERROR_SUCCESS) {
				fprintf(stderr,
					"_svcinfo: RegEnumKeyEx failed\n");
				return (1);
			}
			strcpy(buf + off + blen, "\\SvcMgr");
			rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
					  buf, 0, KEY_READ, &subKey);
			if (rc == ERROR_FILE_NOT_FOUND) continue;
			if (rc != ERROR_SUCCESS) {
				fprintf(stderr,
				  "_svcinfo: RegOpenKeyEx(%s) failed\n", buf);
				return (1);
			}
			buf[off + blen] = 0;
			p = buf + off;
			if (strneq("BK.", p, 3)) p += 3;
			printf("%s\n", p);
			RegCloseKey(subKey);
		}
		RegCloseKey(hKey);
	}
	if (info) {
		sprintf(buf, "%s\\%s\\SvcMgr", key, info);
		rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE, buf, 0, KEY_READ, &hKey);
		if (rc != ERROR_SUCCESS) {
			fprintf(stderr, "_svcinfo: RegOpenKeyEx failed\n");
			return (1);
		}
		blen = sizeof(cmdline);
		rc = RegQueryValueEx(hKey, "CommandLine", 0, 0, cmdline, &blen);
		if (rc != ERROR_SUCCESS) {
			fprintf(stderr, "_svcinfo: RegQueryValueEx failed\n");
			return (1);
		}
		cmds = shellSplit(cmdline);
		printf("Directory: %s\n", popLine(cmds));
		rc = 0;
		printf("Options:  ");
		EACH (cmds) {
			if (rc) printf(" %s", cmds[i]);
			if (streq(cmds[i], "-dD")) rc = 1;
		}
		printf("\n");
		freeLines(cmds, free);
		RegCloseKey(hKey);

		sprintf(buf, "%s\\%s", key, info);
		rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE, buf, 0, KEY_READ, &hKey);
		if (rc != ERROR_SUCCESS) {
			fprintf(stderr, "_svcinfo: RegOpenKeyEx failed\n");
			return (1);
		}
		blen = sizeof(buf);
		rc = RegQueryValueEx(hKey, "ObjectName", 0, 0, buf, &blen);
		if (rc != ERROR_SUCCESS) {
			fprintf(stderr, "_svcinfo: RegQueryValueEx failed\n");
			return (1);
		}
		if (p = strrchr(buf, '\\')) {
			p++;
		} else {
			p = buf;
		}
		printf("User:      %s\n", p);
		RegCloseKey(hKey);
	}
	return (0);
}

#else

int
svcinfo_main(int ac, char **av)
{
	fprintf(stderr, "svcinfo not supported on UNIX\n");
	return (1);
}

#endif
