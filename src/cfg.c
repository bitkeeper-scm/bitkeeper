#include "system.h"
#include "sccs.h"
#include "proj.h"
#include "cfg.h"

struct {
	int	type;
	char	*defval;
	char	*names[CFG_MAX_ALIASES];
} cfg[] = {

#define CONFVAR(def, type, defval, defival, ...)	\
	{CFG_##type, defval, defival, __VA_ARGS__},
#include "confvars.h"
#undef CONFVAR

};

private	void	dumpvar(int idx, int defaults);
private	int	isOn(char *str);
private	int	isBoolean(char *str);

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

#define CONFVAR(def, ...) dumpvar(CFG_##def, defaults);
#include "confvars.h"
#undef CONFVAR
	return (0);
}

private void
dumpvar(int idx, int defaults)
{
	char	*val;

	val = defaults ? cfg[idx].defval : cfg_str(0, idx);
	printf ("%s: %s\n", cfg[idx].names[0], val ? val : "(null)");
}

/*
 * Return the value of the `idx` variable as set by the user. If the
 * variable has not been set, return the default from the confvars.h
 * file.
 */
char *
cfg_str(project *p, int idx)
{
	int	i;
	char	*val;
	MDBM	*db;

	db = proj_config(p);
	assert(db);
	assert(cfg[idx].names[0]);
	for (i = 0; (i < CFG_MAX_ALIASES) && cfg[idx].names[i] ; i++) {
		if (val = mdbm_fetch_str(db, cfg[idx].names[i])) return (val);
	}
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
