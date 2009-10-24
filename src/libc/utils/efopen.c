#include "system.h"

FILE *
efopen(char *env)
{
	char	*t, *p;
	int	port, sock;
	FILE	*f = 0;

	unless (t = getenv(env)) return (0);
	if (IsFullPath(t)) {
		f = fopen(t, "a");
	} else if ((p = strchr(t, ':')) && ((port = atoi(p+1)) > 0)) {
		*p = 0;
		sock = tcp_connect(t, port);
		*p = ':';
		if (sock >= 0) f = fdopen(sock, "w");
	} else {
		unless (f = fopen(DEV_TTY, "w")) f = fdopen(2, "w");
	}
	if (f) setvbuf(f, 0, _IONBF, 0);
	return (f);
}

int
efprintf(char *env, char *fmt, ...)
{
	va_list	ap;
	int	ret = -1;
	FILE	*f;

	va_start(ap, fmt);
	if (f = efopen(env)) {
		ret = vfprintf(f, fmt, ap);
		fclose(f);
	}
	va_end(ap);
	return (ret);
}
