#ifdef WIN32
#include <windows.h>
#endif
#include "../system.h"
#include "../sccs.h"

/* XXX - takes 100nusecs in a hot cache */
private	char	*
gethost(int real)
{
	static	char host[257];
	static	int cache = 0;
	struct	hostent *hp;
	char 	*h;

	if (real) {
		host[0] = 0;
		cache = 0;
	}
	if (cache) return (host[0] ? host : 0);

	if (!real && (h = getenv("BK_HOST"))) {
		assert(strlen(h) <= 256);
		strcpy(host, h);
		cache = 1;
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
			    !streq(host, "localhost.localdomain")) {
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
#endif
	/* Fold case. */
	for (h = host; *h; h++) *h = tolower(*h);
	/* localhost isn't what we want.  */
	if (streq(host, "localhost") || streq(host, "localhost.localdomain")) {
		host[0] = 0;
		return (0);
	}

	/*
	 * XXX - if we don't have a domain name but there is a line
	 * in resolve.conf like "search foo.com", we could assume that
	 * is the domain.
	 */
	if (host[0] && !real) cache = 1;
	return (host);
}

char	*
sccs_gethost(void)
{
	return (gethost(0));
}

char	*
sccs_realhost(void)
{
	return (gethost(1));
}

