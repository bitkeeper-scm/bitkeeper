/*
 * Copyright 2015-2016 BitMover, Inc
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
#include "proj.h"
#include "cfg.h"

struct {
	char	*defval;
	char	*insetup;
	char	*name;
} cfg[] = {

#define CONFVAR(def, defval, insetup, name)	\
	{defval, insetup, name},
#include "confvars.h"
#undef CONFVAR

};

struct alias {
	char	*alias;
	int	idx;
} alias[] = {
	{"autopopulate",	CFG_AUTOPOPULATE},
	{"auto-populate",	CFG_AUTOPOPULATE},
	{"binpool_hardlinks",	CFG_BAM_HARDLINKS},
	{"trust_window",	CFG_CLOCK_SKEW},
	{"clone-default",	CFG_CLONE_DEFAULT},
	{"mail-proxy",		CFG_MAIL_PROXY},
	{"upgrade-url",		CFG_UPGRADE_URL},
	{0,			-1}
};

private	void	dumpvar(int idx, int defaults);
private	int	isOn(char *str);
private	int	isBoolean(char *str);
private	void	dbSetup(MDBM *db, int idx);

int
dumpconfig_main(int ac, char **av)
{
	int	c;
	int	defaults = 0;
	longopt	lopts[] = {
		{ "defaults", 330},
		{ 0, 0 }
	};

	while ((c = getopt(ac, av, "d", lopts)) != -1) {
		switch (c) {
		    case 330:	/* --defaults */
			defaults = 1;
			break;
		    default: bk_badArg(c, av);
			break;
		}
	}
	if (av[optind]) {
		int	idx;

		if (av[optind+1]) usage();
		if ((idx = cfg_findVar(av[optind])) < 0) {
			return (1);
		}
		puts(cfg_str(0, idx));
		return (0);
	}

#define CONFVAR(def, ...) dumpvar(CFG_##def, defaults);
#include "confvars.h"
#undef CONFVAR
	return (0);
}

/*
 * Load variables that should be included in the config file created
 * by "bk setup" into an MDBM.
 */
MDBM *
cfg_loadSetup(MDBM *db)
{
	assert(db);
#define CONFVAR(def, ...) dbSetup(db, CFG_##def);
#include "confvars.h"
#undef CONFVAR
	return (db);
}

/*
 * Helper for cfg_loadSetup
 * Note: this will not be directly used by config mechanism,
 * and is only for setup to write out a config file.
 * In particular, it is okay to have a null string in the db.
 */
private void
dbSetup(MDBM *db, int idx)
{
	char	*def;

	unless (cfg[idx].insetup) return;

	def = *cfg[idx].insetup ? cfg[idx].insetup : notnull(cfg[idx].defval);
	mdbm_store_str(db, cfg[idx].name, def, MDBM_INSERT);
}

/*
 * Helper for dumpvars_main()
 */
private void
dumpvar(int idx, int defaults)
{
	char	*val;

	val = defaults ? cfg[idx].defval : cfg_str(0, idx);
	printf ("%s: %s\n", cfg[idx].name, val ? val : "(null)");
}

/*
 * See if name is an alias.  If so, return the core name.
 */
char *
cfg_alias(char *name)
{
	struct	alias	*x;

	for (x = alias; x->alias; x++) {
		if (streq(name, x->alias)) return (cfg[x->idx].name);
	}
	return (name);
}

/*
 * Return the default value of the `idx` variable.
 * It can be 0, or string, but not "".
 */
char *
cfg_def(int idx)
{
	assert(cfg[idx].name);
	assert(!cfg[idx].defval || *cfg[idx].defval);	/* "" illegal */
	return (cfg[idx].defval);
}

/*
 * Return the value of the `idx` variable as set by the user. If the
 * variable has not been set, return the default from the confvars.h
 * file.
 */
char *
cfg_str(project *p, int idx)
{
	char	*val;
	MDBM	*db;

	db = proj_config(p);
	assert(db);
	assert(cfg[idx].name);
	if (val = mdbm_fetch_str(db, cfg[idx].name)) return (val);
	assert(!cfg[idx].defval || *cfg[idx].defval);	/* "" illegal */
	return (cfg[idx].defval);
}

/*
 * Return a u64 representing the size from the `idx` variable set by
 * the user. It returns 0 if the config variable is not found.
 */
u64
cfg_size(project *p, int idx)
{
	char	*val, *end;
	u64	sz;

	unless (val = cfg_str(p, idx)) return (0);
	unless (isdigit(*val) || (*val == '-')) val = cfg[idx].defval;
	sz = strtoull(val, &end, 10);
	unless (sz) return (0);
	switch (*end) {
	    case 'k': case 'K': return (sz << 10);
	    case 'm': case 'M': return (sz << 20);
	    case 'g': case 'G': return (sz << 30);
	    case 0:		return (sz);
	    default:		return (0);
	}
	/* NOT REACHED */
}

/*
 * Return 1 if the config variable is found and it contains one of
 * '1', 'on', 'true', 'yes'. 0 otherwise.
 */
int
cfg_bool(project *p, int idx)
{
	char	*val;

	unless (val = cfg_str(p, idx)) return (0);
	if (isBoolean(val)) return (isOn(val));
	/*
	 * If it's not boolean and we're here it's set to something
	 * that is not false. Therefore it's true.
	 */
	return (1);
}

/*
 * Return the config variable `idx` as an int.
 */
i64
cfg_int(project *p, int idx)
{
	char	*val;

	unless (val = cfg_str(p, idx)) return (0);
	unless (isdigit(*val) || (*val == '-')) val = cfg[idx].defval;
	return (strtoll(val, 0, 10));
}


private int
isBoolean(char *str)
{
	assert(str);
	switch(tolower(*str)) {
	    case '0': if (streq(str, "0")) return (1); break;
	    case '1': if (streq(str, "1")) return (1); break;
	    case 'f': if (strieq(str, "false")) return (1); break;
	    case 't': if (strieq(str, "true")) return (1); break;
	    case 'n': if (strieq(str, "no")) return (1); break;
	    case 'y': if (strieq(str, "yes")) return (1); break;
	    case 'o':
		if (strieq(str, "on")) return (1);
		if (strieq(str, "off")) return (1);
		break;
	}
	return (0);
}

private int
isOn(char *str)
{
	assert(str);
	switch(tolower(*str)) {
	    case '1': if (streq(str, "1")) return (1); break;
	    case 't': if (strieq(str, "true")) return (1); break;
	    case 'y': if (strieq(str, "yes")) return (1); break;
	    case 'o': if (strieq(str, "on")) return (1); break;
	}
	return (0);
}

/*
 * Given a config variable name (e.g. "auto_populate") find its index
 * by looking through all the different aliases it might have.
 *
 * Returns -1 if not found.
 */
int
cfg_findVar(char *name)
{
	int	idx;
	int	n = sizeof(cfg)/sizeof(*cfg);

	for (idx = 0; idx < n; idx++) {
		if (streq(cfg[idx].name, name)) return (idx); /* found */
	}
	return (-1);
}

/*
 * Given a config MDBM 'db', generate 2 new dbs -- 'defs' which is just the
 * defaults, and 'merge' which has keys from defs, and values from db
 * if different than defs, else defs.
 *
 * This is helper function used by "bk config -v" for printing defaults
 * in the context of the overall config.
 */
void
cfg_printDefaults(MDBM *db, MDBM *defs, MDBM *merge)
{
	int	idx;
	int	n;
	char	*v;

	assert(db && defs && merge);
	n = sizeof(cfg)/sizeof(*cfg);
	for (idx = 0; idx < n; idx++) {
		unless (cfg[idx].defval) continue;
		mdbm_store_str(
		    defs, cfg[idx].name, cfg[idx].defval, MDBM_INSERT);
		if ((v = mdbm_fetch_str(db, cfg[idx].name)) &&
		    !streq(v, cfg[idx].defval)) {
			/* modified */
			mdbm_store_str(merge, cfg[idx].name, v,
			    MDBM_INSERT);
		} else {
			mdbm_store_str( merge,
			    cfg[idx].name, cfg[idx].defval, MDBM_INSERT);
		}
	}
}
