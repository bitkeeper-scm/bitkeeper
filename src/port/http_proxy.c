#ifdef WIN32
#include <windows.h>
#endif
#include "../system.h"
#include "../sccs.h"
#ifndef WIN32

void
get_http_proxy(char *proxy_host, int *proxy_port)
{
	char *p, *q, buf[MAXLINE];
	FILE *f;
	extern char *getHomeDir();
	
	p = getHomeDir();
	assert(p);
	sprintf(buf, "%s/.netscape/preferences.js", p);
	f = fopen(buf, "rt");
	if (f == NULL) return;
	while (fgets(buf, sizeof(buf), f)) {
		if (strneq("user_pref(\"network.proxy.http\",", buf, 31)) {
			p = &buf[33];
				
			q = proxy_host;
			while (*p && *p != '\"') *q++ = *p++;
			assert(*p == '\"');
			*q = 0;
		}
		if (strneq("user_pref(\"network.proxy.http_port\",", buf, 36)) {
			p = &buf[37];
			*proxy_port = atoi(p);
			assert(proxy_port > 0);
		}
	}
	fclose(f);
}

#else /* WIN32 */
int
getReg(char *key, char *valname, char *valbuf, int *buflen)
{
        int rc;
        HKEY    hKey;
        DWORD   valType = REG_SZ;

        rc = RegOpenKeyEx(HKEY_CURRENT_USER, key, 0, KEY_QUERY_VALUE, &hKey);
        if (rc != ERROR_SUCCESS) {
                fprintf(stderr,
                        "Can not open registry HKEY_LOCAL_MACHINE\\%s\n", key);
                return (0);
        }

        rc = RegQueryValueEx(hKey,valname, NULL, &valType, valbuf, buflen);
        if (rc != ERROR_SUCCESS) {
                fprintf(stderr,
"Can not get registry value \"%s\" in HKEY_LOCAL_MACHINE\\%s\n", key);
                return (0);
        };
        RegCloseKey(hKey);
        return (1);
}

/*
 * Note: This works for both IE and netscape on win32
 */
void
get_http_proxy(char *proxy_host, int *proxy_port)
{
#define KEY "Software\\Microsoft\\Windows\\CurrentVersion\\internet Settings"
	char *p, *q, *r, buf[MAXLINE];
	int len = sizeof(buf);

	getReg(KEY, "ProxyServer", buf, &len);
	/* simple form */
	if (strchr(buf, '=') == 0) {
		q = strchr(buf, ':');
		assert(q);
		*q = 0;
		strcpy(proxy_host, buf);
		*proxy_port = atoi(++q);
		return; 
	}

	/* non-simple form */
	p = strtok(buf, ";");
	while (p) {
		if (!strneq("http=", p, 5)) {
			p = strtok(NULL, ";");
			continue;
		}
		p = &buf[5];
		q = strtok(p, ":");
		assert(q);
		strcpy(proxy_host, q);
		q = strtok(NULL, ";");
		*proxy_port = atoi(q);
		break;
	}
	
}
#endif
