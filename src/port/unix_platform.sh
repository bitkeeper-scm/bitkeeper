#! @SH@
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
	if type wish8.2 >/dev/null 2>&1
	then wish=wish8.2
	elif type wish8.0 >/dev/null 2>&1
	then wish=wish8.0
	else wish=wish
	fi
	if [ "X$EDITOR" = X ]
	then EDITOR=vi
	fi
	if [ "X$PAGER" = X ]
	then PAGER=more
	fi
}
