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
	char 	*h;

	if (done) return (host[0] ? host : 0);
	done = 1; host[0] = 0;

	if (h = getenv("BK_HOST")) {
		assert(strlen(h) <= 256);
		strcpy(host, h);
		return(host);
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
	unless (strchr(hp->h_name, '.')) {
		int	i;

		for (i = 0; hp->h_aliases && hp->h_aliases[i]; ++i) {
			if (strchr(hp->h_aliases[i], '.')) {
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
	if (streq(host, "localhost") || streq(host, "localhost.localdomain")) {
		host[0] = 0;
		return (0);
	}
	return (host);
}
