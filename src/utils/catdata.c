/*
 * %K%
 * Copyright (c) 1999 Larry McVoy
 */
extern unsigned int program_size;
extern unsigned char program_data[];

main()
{
	int	fd;

#ifdef	WIN32
	_fmode = _O_BINARY;
#endif

	fd = creat("_data", 0666);
	write(fd, program_data, program_size);
	close(fd);
	exit(0);
}
