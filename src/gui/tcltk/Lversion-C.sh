#!/bin/sh
cat <<END
#define L_VER_TAG	"`bk prs -hr+ -d:TAG: ChangeSet`"
#define L_VER_UTC	"`bk prs -hr+ -d:UTC: ChangeSet`"
#define L_VER_PLATFORM	"`../../../utils/bk_version`"
#define L_VER_BUILD_TIME "`date`"
#define L_VER_USER	"`bk getuser`@`bk gethost -r`"
#define L_VER_PWD	"`pwd`"
END
