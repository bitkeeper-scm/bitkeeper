/*
 * Copyright 1999-2000,2016 BitMover, Inc
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
