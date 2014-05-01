Process used for importing netbsd libs

set -x

cd ~/bk/bk-stdio-hist
rm -f src/libc/SCCS/*.stdio.h
rsync -av work:/mnt/4bsd.SCCS/include/SCCS/s.stdio.h src/libc/SCCS/s.stdio.h
echo src/libc/SCCS/s.stdio.h | BK_HOST=4bsd.org bk sccs2bk -c`bk id` -
cd ~/bk/bk-stdio-hist/src/libc
../rcsimport.pl ~/bk/netbsd.src/include/stdio.h,v

rm -rf stdio
mkdir stdio
cd stdio
rsync -av work:/mnt/4bsd.SCCS/lib/libc/stdio/SCCS .
mv -f SCCS/S.Makefile SCCS/s.Makefile
mv -f SCCS/S.vfprintf.c SCCS/s.vfprintf.c

cd ~/bk/bk-stdio-hist
bk sfiles src/libc/stdio | BK_HOST=4bsd.org bk sccs2bk -c`bk id` -
cd ~/bk/bk-stdio-hist/src/libc/stdio
../../rcsimport.pl $HOME/bk/netbsd.src/src/lib/libc/stdio/*,v

bk -r. prs -hnd'$if(:HOST:!=netbsd.org){:GFILE:}' -r+ | bk rm -
