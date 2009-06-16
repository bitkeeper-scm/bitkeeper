# The purpose of this file is to check out all the sources in the proper
# order so Make will not think any of the autoconf scripts need to be
# regenerated or rerun.
# bk get -Sq configure.in configure config.hin
# bk get -Sq

configure: configure.in config.hin stamp-h.in
	touch configure

config.hin: stamp-h.in
	touch config.hin

stamp-h.in: configure.in
	touch stamp-h.in
