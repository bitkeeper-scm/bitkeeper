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
	MAIL_CMD=mail
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

