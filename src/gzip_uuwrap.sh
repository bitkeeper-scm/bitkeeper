#!@SH@

# gzip_uuwrap - the sending side of a gzip | uuencode stream
# %W%


PATH=$PATH:/usr/local/bin:/usr/freeware/bin

GZIP=NO
for i in `echo $PATH | sed 's/:/ /g'`
do	if [ -f $i/gzip -a -x $i/gzip ]
	then	GZIP=YES
	fi
done
if [ $GZIP = NO ]
then	exec bk uuwrap
	exit 1
fi
exec gzip | uuencode bkpatch$$
