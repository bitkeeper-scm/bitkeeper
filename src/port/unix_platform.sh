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
	if [ -x /usr/bin/mailx ]
	then	MAIL_CMD=mailx
	else	MAIL_CMD=mail
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


# Log whatever they wanted to run in the logfile if we can find the root
__logCommand() {
	DIR="BitKeeper/etc"
	PREFIX=""
	for i in 1 2 3 4 5 6 7 8 9 0
	do	if [ -d $PREFIX$DIR ]
		then	LDIR=${PREFIX}BitKeeper/log
			if [ ! -d $LDIR ]
			then	mkdir $LDIR
			fi
			if [ ! -f ${LDIR}/cmd ]
			then	touch ${LDIR}/cmd
				chmod 666 ${LDIR}/cmd
			fi
		       if [ ! -w ${LDIR} -a -f ${LDIR}/cmd -a ! -w ${LDIR}/cmd ]
		    	then	return
			fi
			if [ ! -w ${LDIR}/cmd ]
			then	mv ${LDIR}/cmd ${LDIR}/ocmd
				cp ${LDIR}/ocmd ${LDIR}/cmd
				chmod 666 ${LDIR}/cmd
				rm ${LDIR}/ocmd
			fi
			echo "${USER}: $@" >> ${LDIR}/cmd
			return
		fi
		PREFIX="../$PREFIX"
	done
}
