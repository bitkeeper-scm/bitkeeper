/*
 * Copyright 2000-2007,2010,2016 BitMover, Inc
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

#include "system.h"
#include "sccs.h"
#include "bkd.h"
#include "tomcrypt.h"
#include "progress.h"

#define	SEND_SUCCESS	0
#define	SEND_FAILURE	1
#define	SEND_BOGUS	2
#define	SEND_NOCONNECT	3

extern void save_cached_proxy(char *proxy);

#define	SOCKS_REQUEST_GRANTED		90
#define	SOCKS_REQUEST_REJECT		91
#define	SOCKS_REQUEST_IDENT_FAILED	92
#define	SOCKS_REQUEST_IDENT_CONFLICT	93

/* socks4 client request */
struct	socks_op {
	unsigned char vn;
	unsigned char cd;
	unsigned char dstport[2];
	unsigned char dstip[4];
	unsigned char userid[10];
};

/* socks4 server reply */
struct	socks_reply {
	unsigned char vn;
	unsigned char cd;
	unsigned char dstport[2];
	unsigned char dstip[4];
};



/*
 * Convert "s" to base64m
 * Caller should free memory when done
 */
private char *
str2base64(const char *s)
{
	long	len, len2;
	char	*buf;
	unsigned char *p;
	int	err;

	len = strlen(s);
	len2 = (4 * ((len + 2) / 3)) + 1;
	buf = p = malloc(len2);
	if ((err = base64_encode(s, len, buf, &len2)) != CRYPT_OK) {
		fprintf(stderr, "%s", error_to_string(err));
		exit(1);
	}
  	return (buf);
}

/*
 * Parse url to host and path
 */
void
parse_url(char *url, char *host, char *path)
{
	char	buf[MAXPATH], *p;

	strcpy(buf, url);
	p = strchr(&buf[7], '/');
	if (p) *p = 0;
	strcpy(host, &buf[7]);
	if (p) {
		*p = '/';
		strcpy(path, p);
	} else {
		path[0] = 0;
	}
}

unsigned long
host2ip(char *host, int trace)
{
	struct	hostent *hostp;
	struct	sockaddr_in inaddr;

	if ((host == NULL) || (*host == '\0')) {
		if (trace) fprintf(stderr, "host2ip: bad args\n");
		return (INADDR_ANY);
	}

	memset ((char *) &inaddr, 0, sizeof(inaddr));
	if ((inaddr.sin_addr.s_addr = inet_addr(host)) == -1) {
		/*
		 * XXX TODO: This should time out, in case we do not have
		 * a network connection, or the DNS server is down
		 */
		if (trace) {
			fprintf(stderr, "looking up IP address for %s\n", host);
		}
		if ((hostp = gethostbyname(host)) == NULL) {
			if (trace) {
				fprintf(stderr, "%s IP: lookup failed\n", host);
			}
			errno = EINVAL;
			return (-1);
		}
		if (hostp->h_addrtype != AF_INET) {
			if (trace) {
				fprintf(stderr, "%s not a IP address\n", host);
			}
			errno = EINVAL;
			return (-1);
		}
		memcpy((char *) &inaddr.sin_addr, (char *) hostp->h_addr,
			sizeof(inaddr.sin_addr));
		if (trace) {
			fprintf(stderr, "%s => %s\n",
					    host, inet_ntoa(inaddr.sin_addr));
		}
	}
	return (inaddr.sin_addr.s_addr);
}


int
connect_srv(char *srv, int port, int trace)
{
	struct	sockaddr_in inaddr;
	fd_set	wfds;
	int	fd;
	struct	timeval tv[1];

	if ((srv[0] == 0) || (port == 0)) {
		if (trace) fprintf(stderr, "connect_srv: bad args: %s %d\n",
								    srv, port);
		return (-1);
	}
	memset((char *) &inaddr, 0, sizeof(inaddr));

	inaddr.sin_addr.s_addr	= host2ip(srv, trace);
	inaddr.sin_family	= AF_INET;

	inaddr.sin_port = htons(SOCK_PORT_CAST port);

	if ((fd = socket(inaddr.sin_family, SOCK_STREAM, 0)) < 0) {
		if (trace) {
			fprintf(stderr,
			    "Cannot create tcp socket for %s:%d\n", srv, port);
			perror("socket");
			return (-1);
		}
	}

	if (connect(fd, (struct sockaddr *) & inaddr, sizeof(inaddr)) == -1) {
		(void) close(fd);
		if (trace) {
			fprintf(stderr, "Cannot connect %s:%d\n", srv, port);
		}
		return (-1);
	}

	/* wait for connection */
	FD_ZERO(&wfds);
	FD_SET(fd, &wfds);

	tv->tv_sec  = 30;
	tv->tv_usec = 0;

	if (select(fd + 1, NULL, &wfds, NULL, tv) <= 0) {
		if (trace) fprintf(stderr, "connect_srv: select failed\n");
		(void) close(fd);
		return (-1);
	}
	return (fd);
}

private int
connect_socks4_srv(remote *r, char *host, int port)
{
	unsigned char sbuf[BUFSIZ];
	struct	socks_op    c;
	struct	socks_reply *s = (struct socks_reply *)sbuf;
	int	web_ip, web_port, n, sfd;
	int	trace = r->trace;

	/* build a socks4 request */
	web_ip = htonl(ns_sock_host2ip(r->host, r->trace));
	web_port = r->port;

	c.vn = 4;
	c.cd = 1;
	c.dstport[0] = (web_port >> 8) & 0xff;
	c.dstport[1] = (web_port) & 0xff;
	c.dstip[0] = (web_ip >> 24) & 0xff;
	c.dstip[1] = (web_ip >> 16) & 0xff;
	c.dstip[2] = (web_ip >>  8) & 0xff;
	c.dstip[3] = (web_ip) & 0xff;
	strcpy(c.userid, "anonymous");

	sfd = connect_srv(host, port, trace);
	if (sfd < 0) return (-1);
	if ((n = send(sfd, (void *) &c, sizeof(c), 0)) != sizeof(c)) {
		if (trace) {
			fprintf(stderr, "SOCKS4 negotiation write failed...\n");
		}
		closesocket(sfd);
		return (-1);
	}

	if ((n = recv(sfd, sbuf, sizeof(sbuf), 0)) != sizeof(*s)) {
		if (trace) {
			fprintf(stderr,
			    "SOCKS4 negotiation read failed, n=%d,sbuf=%s\n",
			    n, sbuf);
		}
		closesocket(sfd);
		return (-1);
	}

	switch (s->cd) {
	case SOCKS_REQUEST_GRANTED:
		return (sfd);
		break;
	case SOCKS_REQUEST_REJECT:
		if (trace) fprintf(stderr, "SOCKS4 request rejected\n");
		errno = EINVAL;
		break;
	case SOCKS_REQUEST_IDENT_FAILED:
		if (trace) fprintf(stderr, "SOCKS4 ident failed\n");
		errno = EACCES;
		break;
	case SOCKS_REQUEST_IDENT_CONFLICT:
		if (trace) fprintf(stderr, "SOCKS4 ident conflict\n");
		errno = EACCES;
		break;
	default:
		errno = ENOENT;
		if (trace) {
			fprintf(stderr,
				"SOCKS request rejected for reason code %d\n",
				s->cd);
		}
	}

	closesocket(sfd);
	return (-1);
}

private int
http_connect_srv(remote *r, char *type, char *host, int port)
{
	int	fd;

	if (streq(type, "PROXY")) {
		fd = connect_srv(host, port, r->trace);
	} else if (streq(type, "SOCKS")) {
		fd = connect_socks4_srv(r, host, port);
	} else {
		fprintf(stderr, "unknown proxy type %s\n", type);
		fd = -1;
	}
	return (fd);
}

private int
in_no_proxy(char *host)
{
	char	*list;
	char	*p, *e;
	int	found = 0;

	unless (list = getenv("no_proxy")) return (0);
	p = list = strdup(list);
	while (p) {
		if (e = strchr(list, ',')) *e++ = 0;
		if (*p && match_one(host, p, 1)) {
			found = 1;
			break;
		}
		p = e;
	}
	free(list);
	return (found);
}

int
http_connect(remote *r)
{
	int	i, proxy_port;
	char	*p, **proxies;
	char	*proxy_type;
	char	*proxy_host;
	char	*cred;

	unless (r && r->host) return (-1);
	if (streq(r->host, "localhost") || in_no_proxy(r->host)) goto no_proxy;

	/*
	 * Try proxy connection if available
	 */
	proxies = get_http_proxy(r->host);
	EACH(proxies) {
		if (r->trace) {
			fprintf(stderr, "trying %s\n", proxies[i]);
			fflush(stderr);
		}
		proxy_type = proxies[i];
		p = strchr(proxy_type, ' ');
		assert(p);
		*p++ = 0;
		proxy_host = p;
		p = strchr(proxy_host, ':');
		assert(p);
		*p++ = 0;
		proxy_port = strtol(p, &p, 10);
		assert(p);
		cred = (*p == ' ') ? p+1 : 0;
		r->rfd =
		    http_connect_srv(r, proxy_type, proxy_host, proxy_port);
		r->wfd = r->rfd;
		r->isSocket = 1;
		if (r->rfd >= 0) {
			if (r->trace) fprintf(stderr, "connected\n");
			if (r->cred) {
				free(r->cred);
				r->cred = 0;
			}
			/* Save the credential, needed in http_send() */
			if (cred) r->cred = strdup(cred);
			if (streq(proxy_type, "PROXY")) r->withproxy = 1;
			freeLines(proxies, free);
			return (0);
		}
	}
	freeLines(proxies, free);

 no_proxy:
	/*
	 * Try direct connection
	 */
	if (r->trace) {
		fprintf(stderr, "Trying direct connection\n");
		fflush(stderr);
	}
	r->wfd = r->rfd = connect_srv(r->host, r->port, r->trace);
	r->isSocket = 1;
	if (r->rfd >= 0) {
		if (r->trace) fprintf(stderr, "Connected\n");
		return (0);
	}

	if (r->trace) fprintf(stderr, "connection failed\n");
	r->rfd = r->wfd = -1;
	return (-1);
}


private int
http_sendheader(remote *r, char *user_agent, char *cmd, size_t len)
{
	int	rc;
	char	*p, *header;
	char	**hh = 0;

	/*
	 * make HTTP header
	 */
	assert(r);
	assert(r->host);

#define	APPEND(h)	(hh = addLine(hh, aprintf h))

	APPEND(("%s ", cmd));
	/*
	 * setup Request-URI for message
	 * see rfc2616 sec 5.1.2
	 */
	if (r->withproxy) APPEND(("http://%s:%d", r->host, r->port));
	if (r->httppath) {
		if (r->httppath[0] != '/') APPEND(("/"));
		APPEND(("%s", r->httppath));
	} else {
		APPEND(("/"));
	}
	APPEND((" HTTP/1.0\r\n"));
	if (r->cred) {
		if (r->trace){
			fprintf(stderr, "Proxy Authorization => (%s)\n",
			    r->cred);
		}
		p = str2base64(r->cred);
		APPEND(("Proxy-Authorization: BASIC %s\r\n", p));
		free(p);
	}
	APPEND((
	    "User-Agent: BitKeeper-%s/%s\r\n"
	    "Accept: */*\r\n"
	    "Host: %s:%d\r\n",
	    user_agent, bk_vers,
	    r->host, r->port));
	if (len) {
		APPEND((
		    "Content-type: application/octet-stream\r\n"
		    "Content-length: %u\r\n",
		    (unsigned)len));
	}
	APPEND(("\r\n"));	/* blank line at end of header */
	header = joinLines("", hh);
	freeLines(hh, free);
#undef	APPEND

	if (r->trace) fprintf(stderr, "Sending http header:\n%s", header);
	len = strlen(header);
	rc = 0;
	if (writen(r->wfd, header, len) != len) {
		if (r->trace) fprintf(stderr, "Send failed\n");
		rc = -1;
	}
	free(header);
	return (rc);
}

int
http_send(remote *r, char *msg, size_t mlen, size_t extra, char *user_agent)
{
	unsigned int start, len, l;
	ticker	*tick = 0;

	if (http_sendheader(r, user_agent, "POST", mlen + extra)) return (-1);
	if (r->trace) fprintf(stderr, "Sending data file");

	if (r->trace) tick = progress_start(PROGRESS_SPIN, mlen);
	for (start = 0; start < mlen; ) {
		len = mlen - start;

		if (len > 4096) len = 4096;

		l = writen(r->wfd, msg + start, len);
		if (l <= 0) {
			perror("http_send");
			break;
		}
		start += l;
		if (tick) progress(tick, start);
	}
	if (tick) progress_done(tick, 0);
	if (r->trace) fputc('\n', stderr);
	if (start < mlen)  {
		if (r->trace) {
			fprintf(stderr,
				"Send failed, wanted %d, got %d\n",
				(int)mlen, start);
		}
		return (-1);
	}
	return (0);
}

int
http_fetch(remote *r, char *file)
{
	int	i;
	u64	got, len;
	int	rc = -1;
	int	binary = 1;
	char	*p;
	FILE	*f;
	ticker	*tick = 0;
	char	buf[MAXLINE];

	r->rf = fdopen(r->rfd, "r");
	assert(r->rf);

	if (http_sendheader(r, "fetch", "GET", 0)) goto out;

	/* skip http header */
	buf[0] = 0;
	getline2(r, buf, sizeof(buf));
	if (r->trace) fprintf(stderr, "-> %s\n", buf);
	if ((sscanf(buf, "HTTP/%*s %d", &i) != 1) || (i != 200)) goto out;
	len = 0;
	while (getline2(r, buf, sizeof(buf)) >= 0) {
		if (r->trace) fprintf(stderr, "-> %s\n", buf);
		if (buf[0] == 0) break; /*ok */
		unless (p = strchr(buf, ':')) continue;
		*p++ = 0;
		while (isspace(*p)) ++p;
		if (strieq(buf, "Content-Length")) {
			len = atoi(p);
		} else if (strieq(buf, "Content-Type")) {
			if (strlen(p) > 5) {
				char	saved = p[5];
				p[5] = 0;
				if (strieq(p, "text/")) binary = 0;
				p[5] = saved;
			}
		}
	}
	if (f = streq(file, "-") ? stdout : fopen(file, "w")) {
		if (binary && len) {
			got = 0;
			if (r->progressbar) {
				tick = progress_start(PROGRESS_BAR, len);
			}
			while (got < len) {
				i = min(sizeof(buf), len);
				i = read_blk(r, buf, sizeof(buf));
				if (i <= 0) break;
				fwrite(buf, 1, i, f);
				got += i;
				if (tick) progress(tick, got);
			}
			if (tick) {
				progress_done(tick,
				    (got<len) ? "FAILED" : "OK");
			}
		} else {
			while (getline2(r, buf, sizeof(buf)) > 0) {
				if (r->trace) fprintf(stderr, "-> %s\n", buf);
				fprintf(f, "%s\n", buf);
			}
		}
		if (f != stdout) fclose(f);
		rc = 0;
	}
 out:
	disconnect(r);
	return (rc);
}

int
httpfetch_main(int ac, char **av)
{
	remote	*r;

	unless (av[1] && !av[2]) usage();
	unless (r = remote_parse(av[1], 0)) {
		fprintf(stderr, "httpfetch: can't parse url '%s'\n", av[1]);
		return (1);
	}
	r->httppath = r->path;
	if (http_connect(r)) return (1);
	return (http_fetch(r, "-"));
}
