
# No #!, it's done with shell() in bk.c

#
# %W%  Copyright (c) Andrew Chang
# platform specific stuff for bk.sh
#
__platformInit()
{
	# Unix specific stuff
	GUI_BIN=$BIN
	RM=/bin/rm
	ECHO=echo
	TMP=/tmp/
	DEV_NULL=/dev/null
	if [ -x /usr/bin/mailx ]
	then	MAIL_CMD=mailx
	else	MAIL_CMD=mail
	fi
	if [ -x /usr/bin/nawk ]
	then	AWK=nawk
	else	AWK=awk
	fi
	ext=""	# Unlike win32, Unix binary does not have .exe extension
	tcl=""
	test "X$EDITOR" = X && EDITOR=vi
	test "X$PAGER" = X && PAGER=more
	WINDOWS=NO
	export PAGER EDITOR GUI_BIN RM ECHO TMP DEV_NULL MAIL_CMD AWK WINDOWS
}
