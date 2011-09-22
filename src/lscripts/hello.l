void
main(int ac, string av[])
{
	int i;

	printf("Hello world\n");
	printf("ac = %d\n", ac);
	for (i = 0; defined(av[i]); i++) {
		puts(av[i]);
	}
}
