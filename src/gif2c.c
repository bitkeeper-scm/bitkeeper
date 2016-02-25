/*
 * Copyright 2000,2016 BitMover, Inc
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
