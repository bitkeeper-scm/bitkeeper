/*
 * Stolen from get.c and stripped down.
 *
 * Copyright (c) 1997-2001 L.W.McVoy
 */
#include "system.h"
#include "sccs.h"
#include "range.h"
WHATSTR("@(#)%K%");

typedef struct	{
	char	*key;	/* if set, we have the whole key */
	char	*user;	/* if set, we look for user matches */
	char	*host;	/* if set, we look for host.domain matches */
	char	*email;	/* if set, we look for u@h.d matches */
	char	*path;	/* if set, we look for pathname matches */
	time_t	utc;	/* if set, we look for UTC matches */
	sum_t	cksum;	/* if set, we look for cksum matches */
	char	*random;/* if set, look for random bits matche s*/
	u32	pkey:1;	/* if set, print the key as well */
} look;
private void	findkey(sccs *s, look l);
private	time_t	utc(char *date);

int
findkey_main(int ac, char **av)
{
	sccs	*s;
	int	errors = 0, c;
	look	look;
	char	*name;

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help findkey");
		return (1);
	}

	/* user@host.domain|path/name|UTC|CKSUM|RANDOM
	 * -b random
	 * -c cksum
	 * -e user@host.domain
	 * -h host.domain
	 * -p path
	 * -t utc
	 * -u user
	 *
	 * Usage: findkey [opts | key] [files]
	 */
	bzero(&look, sizeof(look));
	while ((c = getopt(ac, av, "b;c;e;h;kp;t;u;")) != -1) {
		switch (c) {
		    case 'b': look.random = optarg; break;
		    case 'c': look.cksum = atoi(optarg); break;
		    case 'e': look.email = optarg; break;
		    case 'h': look.host = optarg; break;
		    case 'k': look.pkey = 1; break;
		    case 'p': look.path = optarg; break;
		    case 't': look.utc = utc(optarg); break;
		    case 'u': look.user = optarg; break;
		    default:
			system("bk help -s findkey");
			return (1);
		}
	}
	unless (look.random || look.cksum ||
	    look.email || look.host || look.path || look.utc || look.user) {
	    	unless (av[optind] && isKey(av[optind])) {
			fprintf(stderr, "%s: missing or invalid key\n", av[0]);
			return (1);
		}
		look.key = av[optind++];
	}
	if (look.email && !strchr(look.email, '@')) {
		fprintf(stderr, "%s: email address must have @ sign.\n", look.email);
		return (1);
	}
	name = sfileFirst("findkey", &av[optind], 0);
	for (; name; name = sfileNext()) {
		unless (s = sccs_init(name, 0)) continue;
		unless (HASGRAPH(s)) {
			sccs_free(s);
			continue;
		}
		findkey(s, look);
		sccs_free(s);
	}
	sfileDone();
	return (errors);
}

/*
 * If it is a 99/03/09 style data, just pass it in.
 * Otherwise convert it and pass it in.
 */
private time_t
utc(char *date)
{
	int	year, mon, day, min, hour, sec;
	char	buf[100];

	if (strchr(date, '/')) return (sccs_date2time(date, 0));
	unless (sscanf(date, "%4d%02d%02d%02d%02d%02d",
	    &year, &mon, &day, &min, &hour, &sec) == 6) {
	    	fprintf(stderr, "Bad date format %s\n", date);
		exit(1);
	}
	sprintf(buf,
	    "%d/%02d/%02d %02d:%02d:%02d", year, mon, day, min, hour, sec);
	return (sccs_date2time(buf, 0));
}

private void
findkey(sccs *s, look l)
{
	delta	*d;
	char	key[MAXKEY];

	if (l.key) {
		if (d = sccs_findKey(s, l.key)) {
			printf("%s|%s", s->gfile, d->rev);
			if (l.pkey) printf("\t%s", l.key);
			printf("\n");
		}
		return;
	}
	for (d = s->table; d; d = d->next) {
		/* continues if no match, print if we get through all */
		if (l.random) {
			unless (d->random && streq(d->random, l.random)) {
				continue;
			}
		}
		if (l.cksum) unless (d->sum == l.cksum) continue;
		if (l.utc) unless (d->date == l.utc) continue;
		if (l.path) unless (streq(d->pathname, l.path)) continue;
		if (l.user) unless (streq(d->user, l.user)) continue;
		if (l.host) unless (streq(d->hostname, l.host)) continue;
		if (l.email) {
			char	*at = strchr(l.email, '@');

			*at = 0;
			unless (streq(d->user, l.email) &&
			    streq(d->hostname, at+1)) {
				*at = '@';
				continue;
			}
			*at = '@';
		}
		printf("%s|%s", s->gfile, d->rev);
		if (l.pkey) {
			sccs_sdelta(s, d, key);
			printf("\t%s", key);
		}
		printf("\n");
	}
}
