/*
 * Copyright 1999-2006,2010,2016 BitMover, Inc
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

private void	gethost(char *host, int hlen, int envOK);
private u32	cached;

void
sccs_resethost(void)
{
	cached = 0;
}

char	*
sccs_gethost(void)
{
	static	char host[257];

	unless (cached & 1) {
		gethost(host, 256, 1);
		cached |= 1;
	}
	return (host[0] ? host : UNKNOWN_HOST);
}

char	*
sccs_realhost(void)
{
	static	char host[257];

	unless (cached & 2) {
		gethost(host, 256, 0);
		cached |= 2;
	}
	return (host[0] ? host : "127.0.0.1");
}

char *
sccs_host(void)
{
	char	*r = sccs_realhost();
	char	*e = sccs_gethost();
	static	char *ret = 0;

	if ((r == e) || streq(r, e)) return (e);
	if (ret) free(ret);
	if (getenv("_BK_NO_UNIQ")) {
		ret = aprintf("%s", e);
		return (ret);
	}
	ret = aprintf("%s/%s", e, r);
	return (ret);
}

private void
gethost(char *host, int hlen, int envOK)
{
	struct	hostent *hp;
	char 	*h, *p;
#ifndef	WIN32
	char	buf[MAXPATH], domain[MAXPATH];
	FILE	*f;
#endif

	host[0] = 0;
	if (envOK && (h = getenv("BK_HOST")) && !getenv("BK_EVENT")) {
		assert(strlen(h) <= 256);
		strcpy(host, h);
		goto check;
	}
	/*
	 * Win32 requires loading a library before we call
	 * gethostbyname()
	 */
#ifdef	WIN32
	nt_loadWinSock();
#endif
	if (gethostname(host, hlen) == -1) goto out;

	/*
	 * XXX FIXME: We should have a short timeout here
	 * in case the DNS server is down, or we are on a
	 * disconnected lap top PC.
	 * Note also that a host which uses DHCP haa no permanent
	 * IP address. Thus they cannot put their IP address
	 * in /etc/hosts.
	 */
	unless (hp = gethostbyname(host)) goto out;
	unless (hp->h_name) goto out;
	unless (strchr(hp->h_name, '.') &&
	    !strneq(hp->h_name, "localhost", 9)) {
		int	i;
		char	domain[257];

		for (i = 0; hp->h_aliases && hp->h_aliases[i]; ++i) {
			if (strchr(hp->h_aliases[i], '.') &&
			    !strneq(hp->h_aliases[i], "localhost", 9)) {
				strcpy(host, hp->h_aliases[i]);
				goto out;
			}
		}
		if (getdomainname(domain, sizeof(domain)) == 0) {
#ifdef sun
			/*
			 * Sun's convention, strip off the first component
			 */
			p = strchr(domain, '.');
			p = p ? ++p : domain;
#else
			p = domain;
#endif
			unless (*p == '.') strcat(host, ".");
			strcat(host, p);
		}
	} else if (hp) strcpy(host, hp->h_name);

out:
#ifdef WIN32
	unless (host[0]) {
		DWORD len = hlen;
		GetComputerName(host, &len);
	}
#else
	/*
	 * XXX - if we don't have a domain name but there is a line
	 * in resolv.conf like "search foo.com", we could assume that
	 * is the domain.
	 */
	if (host[0] && !strchr(host, '.')) {
		FILE	*f = fopen("/etc/resolv.conf", "rt");
		if (f) {
			while (fgets(buf, sizeof(buf), f)) {
				if (strneq("search", buf, 6)) {
					sscanf(&buf[7], "%s", domain); 
					strcat(host, ".");
					strcat(host, domain);
					break;
				}
			}
			fclose(f);
		}
	}
	
	/*
	 * Still no fully qualified hostname ?
	 * Try extracting it from sendmail.cf
	 * XXX FIXME: the location of sendmail.cf is different on
	 * 	different Unix platform, need to handle that.
	 */
	if (!host[0] || !strchr(host, '.')) {
		f = fopen("/etc/sendmail.cf", "r") ;
		unless (f) f = fopen("/etc/mail/sendmail.cf", "r") ;
		if (f) {
			while (fgets(buf, sizeof(buf), f)) {
				if (strneq("DM", buf, 2)) {
					sscanf(&buf[2], "%s", host); 
					break;
				}
			}
			fclose(f);
		}
	}

#endif
	/* Fold case. */
	for (h = host; *h; h++) *h = tolower(*h);
	/* localhost isn't what we want.  */
	if (isLocalHost(host)) {
		host[0] = 0;
		return;
	}
check:
	if (host[0]) {
		if (strchrs(host, "\n\r|/")) {
			fprintf(stderr,
			    "bad host name: host name must not contain LF, CR,"
			    " | or /  character\n");
			host[0] = 0; /* erase bad host name */
		}
	}
}
