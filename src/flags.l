// Verify, in a trigger, that sccs.h doesn't have dups in flags defines
// checks a file on stdin: bk tclsh flags.l < sccs.h
int
main(void)
{
	string	key, buf, hex, pre, symbol, group;
	string	plist[] = {
			"INIT",
			"GET",
			"CLEAN",
			"DELTA",
			"ADMIN",
			"PRS"
		};

	int	reserved[] = {1, 2, 4, 8};
	int	r;
	string	used{string};
	int	prefix{string};
	int	num;
	int	errors = 0;

	foreach (pre in plist) prefix{pre} = 1;

	while (buf = <stdin>) {
		unless (buf =~ /^#define\s+(([^_]+)_[A-Z0-9_]+)\s+(0x[0-8]+)/) {
			continue;
		}
		group = $2;	// INIT, GET, ...
		unless (prefix{group}) continue;
		symbol = $1;	// INIT_MUSTEXIST, ...
		num = (int)$3;	// 0x00000001 and 1 are same thing
		key = "${group} ${num}";
		foreach (r in reserved) {
			if (num == r) {
				warn("${symbol} may not use ${r}\n");
				errors++;
			}
		}
		if (used{key}) {
			hex = sprintf("0x%x", num);
			warn("${hex} used by ${symbol} and ${used{key}}\n");
			errors++;
		}
		used{key} = symbol;
	}
	return (errors);
}
