#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

#AC_PREREQ(2.61)
AC_INIT(RubyHoldem, 0.1, time.in.world@gmail.com)

AC_CONFIG_SRCDIR([rubyholdem.cpp])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])

AC_PROG_CC(wineg++)
AC_C_CONST

AC_PATH_PROG([RUBY],[ruby],[ruby1.9])
#AX_PROG_RUBY_VERSION([1.9],,[AC_MSG_ERROR([Ruby version 1.9 required])])
AX_RUBY_DEVEL()

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
