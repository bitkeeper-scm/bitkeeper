#ifdef WIN32
#include <windows.h>
#endif
#include "../system.h"
#include "../sccs.h"

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
		while (*p && (*p != '\"') && (*p != ';')) p++;
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
	int fd;
	char header[1024], buf[MAXLINE];

	fd = connect_srv(host, 80, 0);
	if (fd < 0) {
		fprintf(stderr,
			"http_get_file: cannot connect to to host %s\n", host);
		return NULL;
	}
	sprintf(header,
"GET %s HTTP/1.0\n\
User-Agent: BitKeeper\n\
Accept: text/html\n\n",
path);
	writen(fd, header, strlen(header));
	while (recv(fd, buf, sizeof(buf), 0)) {
		proxies = extract(buf, "\"PROXY ", proxies);
		proxies = extract(buf, "\"SOCKS ", proxies);
	}
	close(fd);
	return (proxies);
}

char **
local_get_file(char *path,  char **proxies)
{
	FILE *f;
	char buf[MAXLINE];

	f = fopen(path, "r");
	unless (f) {
		fprintf(stderr,
			"local_get_file: cannot open %s\n", path);
		return NULL;
	}
	while (fgets(buf, sizeof(buf), f)) {
		proxies = extract(buf, "\"PROXY ", proxies);
		proxies = extract(buf, "\"SOCKS ", proxies);
	}
	fclose(f);
	return (proxies);
}

char **
get_config(char *url, char **proxies)
{
	char host[MAXPATH], path[MAXPATH];

	if (strneq(url, "http://", 7)) {
		parse_url(url, host, path);
		return (http_get_file(host, path, proxies));
	} else if (strneq(url, "file:/", 6)) {
		return (local_get_file(&url[5], proxies));
	} else {
		fprintf(stderr, "unsupported url %s\n", url);
		return NULL;
	}

}

#ifndef WIN32
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
			sprintf(buf, "PROXY %s:%d", proxy_host, proxy_port);
			proxies = addLine(proxies, strdup(buf));
		}
		if (socks_server[0]) {
			sprintf(buf, "SOCKS %s:%d", socks_server, socks_port);
			proxies = addLine(proxies, strdup(buf));
		}
	}
done:	p = getenv("HTTP_PROXY_HOST"); 
	q = getenv("HTTP_PROXY_PORT"); 
	if (p && *p) {
		sprintf(buf, "PROXY %s:%s", p, q ? q : "8000");
		proxies = addLine(proxies, strdup(buf));
	}
	q = getenv("SOCKS_PORT");
	p = getenv("SOCKS_HOST"); 
	if (p && *p) {
		sprintf(buf, "SOCKS %s:%s", p, q ? q : "1080");
		proxies = addLine(proxies, strdup(buf));
	}
	p = getenv("SOCKS_SERVER");
	if (p && *p) {
		sprintf(buf, "SOCKS %s:%s", p, q ? q : "1080");
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

	valbuf[0] = 0;
        rc = RegOpenKeyEx(HKEY_CURRENT_USER, key, 0, KEY_QUERY_VALUE, &hKey);
        if (rc != ERROR_SUCCESS) return (0);

        rc = RegQueryValueEx(hKey,valname, NULL, &valType, valbuf, buflen);
        if (rc != ERROR_SUCCESS) return (0);
        RegCloseKey(hKey);
        return (1);
}

char **
addProxy(char *type, char *line, char **proxies)
{
	char	*q, buf[MAXLINE], proxy_host[MAXPATH];
	int	proxy_port;

	unless (line) return proxies;
	q = strchr(line, ':');
	unless (q) {
		fprintf(stderr,
"===========================================================================\n"
"Unknown proxy entry: \"%s\"\n"
"If you believe this entry is legal, please file a bug report and\n"
"ask Bitmover to add support for it.\n"
"===========================================================================\n",
		 line);
		return (proxies);
	}
	*q = 0;
	strcpy(proxy_host, line);
	proxy_port = atoi(++q);
	sprintf(buf, "%s %s:%d", type, proxy_host, proxy_port);
	if (getenv("BK_HTTP_PROXY_DEBUG")) {
		fprintf(stderr, "Adding proxy: \"%s\"\n", buf);
	}
	return (addLine(proxies, strdup(buf)));
}

/*
 * Note: This works for both IE and netscape on win32
 */
char **
get_http_proxy()
{
#define KEY "Software\\Microsoft\\Windows\\CurrentVersion\\internet Settings"
	char *p, *q, *type, buf[MAXLINE] = "", proxy_host[MAXPATH];
	int proxy_port, len = sizeof(buf);
	char **proxies = NULL;

	getReg(KEY, "AutoConfigURL", buf, &len);
	if (buf[0]) {
		proxies = get_config(buf, proxies);
	} else {
		len = sizeof(buf);  /* important */
		if (getReg(KEY, "ProxyServer", buf, &len) == 0) return NULL;
		/*
		 * We support 3 froms:
		 * 1) host:port
		 * 2) http=host:port
		 * 3) http://host:port
		 * form 1 can only exist in a single field line
		 * form 2 and 3 can be part of a multi-field line
		 */
		if (getenv("BK_HTTP_PROXY_DEBUG")) {
			fprintf(stderr, "ProxyServer= \"%s\"\n", buf);
		}
		if (strchr(buf, ';') == 0) {
			if (strneq("http://", buf, 7)) {
				proxies = addProxy("PROXY", &buf[7], proxies);
			} else if (strneq("http=", buf, 5)) {
				proxies = addProxy("PROXY", &buf[5], proxies);
			} else if (strneq("socks=", buf, 6)) {
				proxies = addProxy("SOCKS", &buf[6], proxies);
			} else {
				proxies = addProxy("PROXY", buf, proxies);
			}
		} else {
			q = buf;
			p = strchr(q, ';'); 
			if (p) *p++ = 0;
			while (q) {
				if (strneq("http://", q, 7)) {
					proxies =
					    addProxy("PROXY", &q[7], proxies);
				} else if (strneq("http=", q, 5)) {
					proxies =
					    addProxy("PROXY", &q[5], proxies);
				} else if (strneq("socks=", q, 6)) {
					proxies =
					    addProxy("SOCKS", &q[6], proxies);
				} 
				q = p;
				if (q) {
					p = strchr(q, ';');
					if (p) *p++ = 0;
				}
			}
		}
	}
	return (proxies);
}
#endif
