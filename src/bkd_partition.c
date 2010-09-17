#include "sccs.h"

#define PARTITION_KV	"BitKeeper/log/partition"

int
cat_partition_main(int ac, char **av)
{
	return (cat(PARTITION_KV) ? 1 : 0);
}
