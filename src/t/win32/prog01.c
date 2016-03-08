// Copyright 2005,2008,2015 BitMover, Inc
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/* 
 * Delete av[1] and exit in a pseudo-atomic operation. 	
 */

#include <win32.h>

int
main(int ac, char **av)
{
	HANDLE	h;
	char	*file;

	if (ac < 2) {
		fprintf(stderr, "usage: %s <file-to-delete>\n", av[0]);
		exit(1);
	}
	file = av[1];
	h = CreateFile(file, GENERIC_READ, FILE_SHARE_DELETE, 0, 
	    OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, 0);
	if (h == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "cannot open file %s. Error number %ld\n",
		    file, GetLastError());
		exit(1);
	}
	/* Note that we DO NOT close the handle on purpose. The handle will
	 * be closed by Windows when the process exists and then the file 
	 * will be deleted. This is to guarantee that once the file is 
	 * actually deleted, this process is gone.
	 */
	exit(0);
}
