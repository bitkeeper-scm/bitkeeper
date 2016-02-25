/*
 * Copyright 2006,2012,2016 BitMover, Inc
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
#include "cmd.h"

#ifndef WIN32
int
registry_main(int ac, char **av)
{
	fprintf(stderr, "bk _registry: Only supported on Windows\n");
	return (1);
}
#else

typedef enum {CLRBIT, SETBIT, GETBIT} bitop;

private	int	registry_broadcast(int ac, char **av);
private int	registry_delete(int ac, char **av);
private int	registry_dump(int ac, char **av);
private int	registry_get(int ac, char **av);
private int	registry_keys(int ac, char **av);
private	int	registry_set(int ac, char **av);
private int	registry_type(int ac, char **av);
private	int	registry_values(int ac, char **av);
private	int	registry_bitop(bitop op, int ac, char **av);
private void	printData(long type, void *data, int len);
private	void	recursiveDump(char *key);
private void	usage(void);

int
registry_main(int ac, char **av)
{
	char	*cmd = av[1];

	unless (cmd) usage();

	if (streq(cmd, "broadcast")) {
		return (registry_broadcast(ac-1, av+1));
	} else if (streq(cmd, "delete")) {
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
	} else if (streq(cmd, "getbit")) {
		return (registry_bitop(GETBIT, ac-1,av+1));
	} else if (streq(cmd, "setbit")) {
		return (registry_bitop(SETBIT, ac-1,av+1));
	} else if (streq(cmd, "clearbit")) {
		return (registry_bitop(CLRBIT, ac-1,av+1));
	} else {
		usage();
	}

	/* NOT REACHED */
	assert("Unreachable statement reached" == 0);
	return (0);
}

private	int
registry_broadcast(int ac, char **av)
{
	int	timeout = 0;

	unless(av[1]) usage();
	if (av[2]) timeout = atoi(av[2]);
	return (reg_broadcast(av[1], timeout));
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
	int	type = 0;
	char	*key = av[1];
	char	*value, *data = 0;
	u32	dwordval;

	unless (key) usage();
	if (value = av[2]) {
		unless (data = av[3]) usage();
		if (!strncasecmp(data, "dword:", 6)) {
			type = REG_DWORD;
			dwordval = strtoul(data+6, 0, 0);
			data = (char *)&dwordval;
		} else if (!strncasecmp(data, "sz:", 3)) {
			type = REG_SZ;
			data += 3;
		} else if (!strncasecmp(data, "expand_sz:", 10)) {
			type = REG_EXPAND_SZ;
			data += 10;
		} else {
			type = REG_SZ;
		}
	}
	if (err = reg_set(key, value, type, data, 0)) {
		fprintf(stderr, "registery_set: ");
		if (value) {
			fprintf(stderr, "Could not set entry %s %s\n",
			    key, value);
		} else {
			fprintf(stderr, "Could not create key %s\n", key);
		}
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
		printf("%u", *(u32 *)data);
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

private int
registry_bitop(bitop op, int ac, char **av)
{
	u8	*val;
	u8	mask;
	int	bit;
	long	len;

	unless (av[1] && av[2] && av[3]) usage();
	unless (val = reg_get(av[1], av[2], &len)) {
		fprintf(stderr, "entry not found\n");
		return (1);
	}
	unless (reg_type(av[1], av[2]) == REG_BINARY) {
		fprintf(stderr, "entry not binary\n");
		return (1);
	}
	bit = strtoul(av[3], 0, 0);
	if (bit >= (8 * len)) {
		fprintf(stderr, "bit %d too high (max %d)\n", bit, 8 * len);
		return (1);
	}
	mask = 1<<(bit % 8);
	/* print current value */
	printf("%d\n", (val[bit/8] & mask) ? 1 : 0);

	switch (op) {
	    case GETBIT: break;
	    case SETBIT:
		val[bit/8] |= mask;
		break;
	    case CLRBIT:
		val[bit/8] &= ~mask;
		break;
	   default: assert(0);
	}
	unless (op == GETBIT) {
		if (reg_set(av[1], av[2], REG_BINARY, val, len)) {
			fprintf(stderr, "failed to set bit\n");
			return (1);
		}
	}
	free(val);
	return (0);
}

private void
usage(void)
{
	fprintf(stderr, "bk _registry is undocumented\n");
	exit(1);
}

#endif
