#include <stdio.h>
#include <ctype.h>

main(int ac, char **av)
{
	int	c;
	int	len = 0;

	printf("unsigned char %s_gif[] = { ", av[1]);
	while ((c = getchar()) != -1) {
		printf("0x%x,", c);
		len++;
	}
	printf("};\n");
	printf("int %s_len = %d;\n", av[1], len);
}
