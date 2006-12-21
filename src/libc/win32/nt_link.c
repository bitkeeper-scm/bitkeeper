#include "system.h"

int
nt_link(const char *file1, const char *file2)
{
	unless (CreateHardLink(file2, file1, 0)) {
		switch (GetLastError()) {
			case ERROR_FILE_NOT_FOUND: errno = ENOENT; break;
			case ERROR_ACCESS_DENIED: errno = EACCES; break;
			case ERROR_NOT_SAME_DEVICE: errno = EXDEV; break;
			default: errno = EINVAL;
		}
		return (-1);
	}
	return (0);
}
