# Configure `patch'.
# Copyright 1993, 1997-1998, 1999 Free Software Foundation, Inc.
dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.12)
AC_INIT(patch.c)
AC_CONFIG_HEADER(config.h:config.hin)
AC_ARG_PROGRAM
AC_CANONICAL_HOST

PACKAGE=patch
VERSION=2.5.4
AC_SUBST(PACKAGE)
AC_SUBST(VERSION)

AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_MAKE_SET
# Use ed_PROGRAM, not ED_PROGRAM,
# because <errno.h> reserves symbols starting with `E'.
AC_PATH_PROG(ed_PROGRAM, ed, ed)

AC_SYS_LARGEFILE

AC_AIX
AC_MINIX
AC_ISC_POSIX
AC_EXEEXT

AM_C_PROTOTYPES
AC_C_CONST

AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h limits.h string.h unistd.h utime.h varargs.h)
jm_AC_HEADER_INTTYPES_H

AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIGNAL
AC_TYPE_SIZE_T
jm_STRUCT_UTIMBUF

AC_CHECK_FUNC(getopt_long, , [LIBOBJS="$LIBOBJS getopt1\$U.o getopt\$U.o"])
AC_SUBST(LIBOBJS)
AC_CHECK_FUNCS(_doprintf isascii fseeko memcmp mktemp \
  pathconf raise setmode sigaction sigprocmask sigsetmask strerror)
AC_REPLACE_FUNCS(memchr mkdir rename rmdir)
jm_FUNC_MALLOC
jm_FUNC_REALLOC
AC_FUNC_CLOSEDIR_VOID
AC_FUNC_VPRINTF

AC_SYS_LONG_FILE_NAMES
jm_STRUCT_DIRENT_D_INO

AC_OUTPUT(Makefile)
