#ifdef WIN32
#include <windows.h>
#endif
#include "../system.h"
#include "../sccs.h"
#ifndef WIN32

#define CACHED_PROXY "BitKeeper/etc/.cached_proxy"

void
save_cached_proxy(char *proxy)
{
	char *p = sccs_root(0);
	char cached_proxy[MAXPATH];
	FILE *f;
	
	unless (p) return;
	sprintf(cached_proxy, "%s/%s", p, CACHED_PROXY);
	f = fopen(cached_proxy, "wb");
fprintf(stderr, "saving proxy:<%s>\n", proxy);
	fputs(proxy, f);
	fclose(f);
}

char **
get_cached_proxy(char **proxies)
{
	char *p = sccs_root(0);
	char buf[MAXPATH];
	FILE *f;

	unless (p) return NULL;
	sprintf(buf, "%s/%s", p, CACHED_PROXY);
	f = fopen(buf, "rb");
	unless (f) return NULL;
	buf[0] = 0;
	fgets(buf, sizeof(buf), f);
	fclose(f);
	unless (buf[0]) return NULL;
	return (addLine(proxies, strdup(buf)));
}

char **
extract(char *buf, char *type, char **proxies)
{
	char *p, *q;

	q = buf;
	while (p = strstr(q, type)) {
		char *t;
		t = ++p;
		while ((*p != '\"') && (*p != ';')) p++;
		*p = 0;
		proxies = addLine(proxies, strdup(t));
		*p = '\"';
		q = ++p;
	}
	return proxies;
}

char **
http_get_file(char *host, char *path, char **proxies)
{
	int fd, rc, i;
	char header[1024];
	char buf[4096], *p, *q;

	fd = connect_srv(host, 80);
	if (fd < 0) {
		fprintf(stderr,
			"http_get_file: cannot connect to to host %s\n", host);
	}
	sprintf(header,
"GET %s HTTP/1.0\n\
User-Agent: BitKeeper\n\
Accept: text/html\n\n",
path);
	send_request(fd, header, strlen(header));
	while (recv(fd, buf, sizeof(buf), 0)) {
		proxies = extract(buf, "\"PROXY ", proxies);
		proxies = extract(buf, "\"SOCKS ", proxies);
	}
	close(fd);
	return proxies;
}

char **
get_config(char *url, char **proxies)
{
	char host[MAXPATH], path[MAXPATH];

	if (strneq(url, "http://", 7)) {
		parse_url(url, host, path);
		return (http_get_file(host, path, proxies));
	} else {
		fprintf(stderr, "unsupported url %s\n", url);
		return;
	}

}

char **
get_http_proxy()
{
	char *p, *q, buf[MAXLINE], autoconfig[MAXPATH] = "";
	char proxy_host[MAXPATH], socks_server[MAXPATH];
	char **proxies = NULL;
	int proxy_port = -1, proxy_type = -1, socks_port = 1080;
	FILE *f;
	extern char *getHomeDir();
	
	p = getHomeDir();
	assert(p);
	sprintf(buf, "%s/.netscape/preferences.js", p);
	f = fopen(buf, "rt");
	if (f == NULL) goto done;
	while (fgets(buf, sizeof(buf), f)) {
		if (strneq("user_pref(\"network.proxy.http\",", buf, 31)) {
			p = &buf[33];
				
			q = proxy_host;
			while (*p && *p != '\"') *q++ = *p++;
			assert(*p == '\"');
			*q = 0;
		} else if (strneq("user_pref(\"network.proxy.http_port\",", buf, 36)) {
			p = &buf[37];
			proxy_port = atoi(p);
			assert(proxy_port >= 0);
		} else if (strneq("user_pref(\"network.proxy.type\",", buf, 30)) {
			p = &buf[31];
			proxy_type = atoi(p);
		} else if (strneq("user_pref(\"network.proxy.autoconfig_url\",", buf, 41)) {
			p = &buf[43];
			q = autoconfig;
			while (*p && *p != '\"') *q++ = *p++;
			assert(*p == '\"');
			*q = 0;
		} else if (strneq("user_pref(\"network.hosts.socks_server\",", buf, 39)) {
			p = &buf[41];
			q = socks_server;
			while (*p && *p != '\"') *q++ = *p++;
			assert(*p == '\"');
			*q = 0;
		} else if (strneq("user_pref(\"network.hosts.socks_serverport\",", buf, 43)) {
			p = &buf[44];
			socks_port = atoi(p);
			assert(socks_port >= 0);
		}
	}
	fclose(f);
	proxies = get_cached_proxy(proxies);
	if (proxy_type == 2) {
		proxies = get_config(autoconfig, proxies);
	} else {

		if (proxy_host[0] && (proxy_port != -1)) {
			sprintf(buf, "PROXY %s:%d\n", proxy_host, proxy_port);
			proxies = addLine(proxies, strdup(buf));
		}
		if (socks_server[0]) {
			sprintf(buf, "SOCKS %s:%d\n", socks_server, socks_port);
			proxies = addLine(proxies, strdup(buf));
		}
	}
done:	p = getenv("HTTP_PROXY_HOST"); 
	q = getenv("HTTP_PROXY_PORT"); 
	if (p && *p) {
		sprintf(buf, "PROXY %s:%s\n", p, q ? q : "8000");
		proxies = addLine(proxies, strdup(buf));
	}
	q = getenv("SOCKS_PORT");
	p = getenv("SOCKS_HOST"); 
	if (p && *p) {
		sprintf(buf, "SOCKS %s:%s\n", p, q ? q : "1080");
		proxies = addLine(proxies, strdup(buf));
	}
	p = getenv("SOCKS_SERVER");
	if (p && *p) {
		sprintf(buf, "SOCKS %s:%s\n", p, q ? q : "1080");
		proxies = addLine(proxies, strdup(buf));
	}
	return (proxies);
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
