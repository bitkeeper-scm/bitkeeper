#!@SH@

# unuuwrap - the receiving side of a uuencode stream
# @(#)unuuwrap.sh 1.3

cd /tmp
TMP=unuu$$
cat > $TMP
uudecode $TMP
set `grep '^begin ' $TMP`
FILE=$3
if [ ! -r $FILE ]
then	chmod 0664 $FILE
fi
gunzip < $FILE
/bin/rm -f $FILE $TMP
exit 0
