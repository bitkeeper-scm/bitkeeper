#!@SH@

# unuuwrap - the receiving side of a uuencode stream
# %W%

cd /tmp
TMP=unuu$$
cat > $TMP
uudecode $TMP
set `grep '^begin ' $TMP`
FILE=$3
cat $FILE
/bin/rm -f $FILE $TMP
exit 0
