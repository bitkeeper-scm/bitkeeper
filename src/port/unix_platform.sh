#! @SH@
#
# %W%  Copyright (c) Andrew Chang
# platform specific stuff for bk.sh
#
_platformInit()
{
	# Unix specific stuff
	GUI_BIN=$BIN
	RM=/bin/rm
	ECHO=echo
	TMP=/tmp/
	if [ -x /usr/bin/mailx ]
	then	MAIL_CMD=mailx
	else	MAIL_CMD=mail
	fi
	ext=""	# Unlike win32, Unix binary does not have .exe extension
	tcl=""
	wish=wish
	if [ X$EDITOR = X ]
	then EDITOR=vi
	fi
	if [ X$PAGER = X ]
	then PAGER=more
	fi
}

