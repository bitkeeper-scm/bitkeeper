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


# Log whatever they wanted to run in the logfile if we can find the root
_logCommand() {
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
		    	then	echo No write permission on BitKeeper/log/cmd
				return
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

