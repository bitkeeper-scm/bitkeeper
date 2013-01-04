# The purpose of this file is to check out all the sources in the proper
# order so Make will not think any of the autoconf scripts need to be
# regenerated or rerun.
# bk get -Sq configure.in configure config.hin
# bk get -Sq

configure: config.hin
	-bk clean $@
	-bk get -q $@

config.hin: stamp-h.in
	-bk clean $@
	-bk get -q $@

stamp-h.in: configure.in
	-bk clean $@
	-bk get -q $@
