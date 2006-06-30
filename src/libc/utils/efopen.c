#include "system.h"

FILE *
efopen(char *env)
{
	char	*t, *p;
	int	port, sock;

	unless (t = getenv(env)) return (0);
	if (IsFullPath(t)) return (fopen(t, "a"));
	if ((p = strchr(t, ':')) && ((port = atoi(p+1)) > 0)) {
		*p = 0;
		sock = tcp_connect(t, port);
		return (fdopen(sock, "w"));
	}
	return (fopen(DEV_TTY, "w"));
}
