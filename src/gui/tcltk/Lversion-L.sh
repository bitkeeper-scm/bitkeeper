#!/bin/sh
cat <<END
#lang L
void
Lversion()
{
	string  tag = "`bk prs -hr+ -d:TAG: ChangeSet`";
	string  utc = "`bk prs -hr+ -d:UTC: ChangeSet`";
	string  platform = "`../../../utils/bk_version`";
	string  build_time = "`date`";
	string  user = "`bk getuser`@`bk gethost -r`";
	string  dir = "`pwd`";

	if (tag != "") {
		puts("L version is \${tag} \${utc} for \${platform}");
	} else {
		puts("L version is \${utc} for \${platform}");
	}
	puts("Built by: \${user} in \${dir}");
	puts("Built on: \${build_time}");
}
END
