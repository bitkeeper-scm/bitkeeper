#ifdef WIN32
#include <windows.h>
#endif
#include "../system.h"
#include "../sccs.h"

/* XXX - takes 100 usecs in a hot cache */
char	*
sccs_gethost(void)
{
	static	char host[257];
	static	int done = 0;
	struct	hostent *hp;
	char 	*h, *p, *q, buf[MAXLINE], domain[MAXPATH];
	FILE	*f;

	if (done) return (host[0] ? host : UNKNOWN_HOST);

	if (h = getenv("BK_HOST")) {
		assert(strlen(h) <= 256);
		strcpy(host, h);
		done = 1;
		return (host);
	}
	/*
	 * Some system (e.g. win32)
	 * reuires loading a library
	 * before we call gethostbyname()
	 */
	loadNetLib();
	if (gethostname(host, sizeof(host)) == -1) {
		unLoadNetLib();
		goto out;
	}

	/*
	 * XXX FIXME: We should have a short timeout here
	 * in case the DNS server is down, or we are on a
	 * disconnected lap top PC.
	 * Note also that a host which uses DHCP haa no permanent
	 * IP address. Thus they cannot put their IP address
	 * in /etc/hosts.
	 */
	unless (hp = gethostbyname(host)) {
		unLoadNetLib();
		goto out;
	}
	unLoadNetLib();
	unless (hp->h_name) goto out;
	unless (strchr(hp->h_name, '.') &&
	    !streq(hp->h_name, "localhost.localdomain")) {
		int	i;

		for (i = 0; hp->h_aliases && hp->h_aliases[i]; ++i) {
			if (strchr(hp->h_aliases[i], '.') &&
			    !streq(hp->h_aliases[i], "localhost.localdomain")) {
				strcpy(host, hp->h_aliases[i]);
				break;
			}
		}
	} else if (hp) strcpy(host, hp->h_name);

out:
#ifdef WIN32
	unless (host[0]) {
		int len = sizeof(host);
		GetComputerName(host, &len);
	}
#else
	/*
	 * XXX - if we don't have a domain name but there is a line
	 * in resolv.conf like "search foo.com", we could assume that
	 * is the domain.
	 */
	if (host[0] && !strchr(host, '.')) {
		f = fopen("/etc/resolv.conf", "rt");
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
	 * XXX FIXME: the location of sendmail.cf is different
	 * 	different Unix platform, need to handle that.
	 */
	if (!host[0] || !strchr(host, '.')) {
		f = fopen("/etc/sendmail.cf", "r") ;
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
	
	/*
	 * Still no fully qualified hostname ?
	 * Try extracting it from netscape config file
	 * XXX FIXME: the location of sendmail.cf is different
	 * 	different Unix platform, need to handle that.
	 */
	if (!host[0] || !strchr(host, '.')) {
		extern char *getHomeDir();

 		p = getHomeDir();
        	assert(p);
        	sprintf(buf, "%s/.netscape/preferences.js", p);
        	f = fopen(buf, "r"); 
		if (f) {
			while (fgets(buf, sizeof(buf), f)) {
				if (strneq(
				    "user_pref(\"network.hosts.smtp_server\",",
				    buf, 38)) {
					p = &buf[40];
					q = host;
					while (*p && *p != '\"') *q++ = *p++;
					assert(*p == '\"');
					*q = 0;
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
	if (streq(host, "localhost") || streq(host, "localhost.localdomain")) {
		host[0] = 0;
		return (0);
	}

	if (host[0]) done = 1;
	return (host);
}
