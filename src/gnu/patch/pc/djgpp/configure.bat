@echo off
Rem	Configure patch for DJGPP v2.
Rem	$Id: configure.bat,v 1.4 1997/06/17 06:52:12 eggert Exp $

Rem	The DOS shell has fixed-size environment storage.
Rem	When the environment is full, the shell prints
Rem	"Out of environment space" and truncates the string at will.
Rem	Since people often ignore these messages,
Rem	test whether the environment variable got the correct value.

Rem	Where is our source directory?
set srcdir=.
if not "%srcdir%" == "." goto SmallEnv
if not "%1" == "" set srcdir=%1
if not "%1" == "" if not "%srcdir%" == "%1" goto SmallEnv

Rem	Create Makefile
sed -f %srcdir%/pc/djgpp/configure.sed -e "s,@srcdir@,%srcdir%,g" %srcdir%/Makefile.in >Makefile
sed -n -e "/^VERSION/p" %srcdir%/configure.in >>Makefile

goto Exit

:SmallEnv
echo Your environment size is too small.  Please enlarge it and run me again.

:Exit
set srcdir=
