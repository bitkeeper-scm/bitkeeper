# The purpose of this file is to check out all the sources in the proper
# order so Make will not think any of the autoconf scripts need to be
# regenerated or rerun.
# bk clean m4
# bk clean aclocal.m4 configure.in configure config.hin
# bk get -Sq m4
# bk get -Sq aclocal.m4 configure.in configure config.hin
# bk get -Sq

configure: aclocal.m4 config.hin
	-bk clean $@
	-bk get -q $@

config.hin: configure.in
	-bk clean $@
	-bk get -q $@

aclocal.m4: configure.in
	-bk clean $@
	-bk get -q $@
