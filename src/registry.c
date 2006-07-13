#include "system.h"

#ifndef WIN32
int
registry_main(int ac, char **av)
{
	fprintf(stderr, "bk _registry: Only supported on Windows\n");
	return (1);
}
#else

private int	registry_delete(int ac, char **av);
private int	registry_dump(int ac, char **av);
private int	registry_get(int ac, char **av);
private int	registry_keys(int ac, char **av);
private	int	registry_set(int ac, char **av);
private int	registry_type(int ac, char **av);
private	int	registry_values(int ac, char **av);
private void	printData(long type, void *data, int len);
private	void	recursiveDump(char *key);
private void	usage(void);

int
registry_main(int ac, char **av)
{
	char	*cmd = av[1];

	unless (cmd) usage();

	if (streq(cmd, "delete")) {
		return (registry_delete(ac-1, av+1));
	} else if (streq(cmd, "dump")) {
		return (registry_dump(ac-1, av+1));
	} else if (streq(cmd, "get")) {
		return (registry_get(ac-1, av+1));
	} else if (streq(cmd, "keys")) {
		return (registry_keys(ac-1, av+1));
	} else if (streq(cmd, "set")) {
		return (registry_set(ac-1,av+1));
	} else if (streq(cmd, "type")) {
		return (registry_type(ac-1, av+1));
	} else if (streq(cmd, "values")) {
		return (registry_values(ac-1, av+1));
	} else {
		usage();
	}

	/* NOT REACHED */
	assert("Unreachable statement reached" == 0);
	return (0);
}

private int
registry_get(int ac, char **av)
{
	char	*val;
	long	len;
	DWORD	type;

	unless (av[1]) usage();
	if (val = reg_get(av[1], av[2], &len)) {
		type = reg_type(av[1], av[2]);
		printData(type, val, len);
		free(val);
		return (0);
	} else {
		printf("entry not found\n");
		return (1);
	}
}

private int
registry_delete(int ac, char **av)
{
	int	err;

	unless (av[1]) usage();
	if (err = reg_delete(av[1], av[2])) {
		fprintf(stderr, "Could not delete\n");
	}
	return (err);
}

private int
registry_dump(int ac, char **av)
{
	char	*start = av[1];

	unless(start) {
		start = "HKEY_LOCAL_MACHINE";
	}
	recursiveDump(start);
	return (0);
}

private void
recursiveDump(char *key)
{
	char	**keys;
	char	**values;
	char	*v, *buf;
	int	i, type;
	long	len;

	printf("[%s]\n", key);
	values = reg_values(key);
	EACH(values) {
		printf("%s = ", values[i]);
		v = reg_get(key, values[i], &len);
		type = reg_type(key, values[i]);
		if (v) {
			printData(type, v, len);
			free(v);
		} else {
			printf("Could not get value\n");
		}
	}
	freeLines(values, free);
	keys = reg_keys(key);
	EACH(keys) {
		buf = aprintf("%s\\%s", key, keys[i]);
		recursiveDump(buf);
		free(buf);
	}
	free(keys);
}

private int
registry_keys(int ac, char **av)
{
	char	**result;
	int	i;

	unless (av[1]) usage();
	unless (result = reg_keys(av[1])) {
		fprintf(stderr, "no keys\n");
		return (1);
	}
	EACH(result) {
		printf("%s\n", result[i]);
	}
	return (0);
}

private int
registry_set(int ac, char **av)
{
	int	err;

	unless (av[1]) usage();
	if (err = reg_set(av[1], av[2], 0, av[3], av[3]?strlen(av[3]):0)) {
		fprintf(stderr, "Could not set entry: ");
		if (av[2] && !av[3]) printf("no data");
		printf("\n");
		return (1);
	}
	return (0);
}

private int
registry_type(int ac, char **av)
{
	unless (av[1]) usage();
	printf("%s\n", reg_typestr(reg_type(av[1], av[2])));
	return (0);
}

private int
registry_values(int ac, char **av)
{
	char	**result;
	int	i;

	unless (av[1]) usage();
	unless (result = reg_values(av[1])) {
		fprintf(stderr, "no values\n");
		return (1);
	}
	EACH(result) {
		printf("%s\n", result[i]);
	}
	return (0);
}

private void
printData(long type, void *data, int len)
{
	int i;

	unless(data) return;
	switch (type) {
	    case REG_BINARY:
		for (i = 0; i < len; i++)
			printf("%.2x ", ((BYTE *)data)[i]);
		break;
	    case REG_DWORD:
		printf("%l", (long *)data);
		break;
	    case REG_EXPAND_SZ:
	    case REG_SZ:
		printf("%s", (char *)data);
		break;
	    default:
		printf("can not print type \"%s\"", reg_typestr(type));
		break;
	}
	printf("\n");
}

private void
usage(void)
{
	fprintf(stderr, "bk _registry is undocumented\n");
	exit(1);
}

#endif
