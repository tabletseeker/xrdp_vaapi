# Process this file with autoconf to produce a configure script

AC_PREREQ(2.59)
AC_INIT([yami_inf], [0.1.0], [jay.sorg@gmail.com])
AC_CONFIG_HEADERS(config_ac.h:config_ac-h.in)
AM_INIT_AUTOMAKE([1.6 foreign])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])
AC_PROG_CC
AC_C_CONST
AC_PROG_LIBTOOL

AC_ARG_ENABLE(x11, AS_HELP_STRING([--enable-x11],
              [Build va_x11 (default: no)]),
              [], [enable-x11=no])
AM_CONDITIONAL(YAMI_INF_X11, [test x$enable_x11 = xyes])
AC_ARG_ENABLE(xcb, AS_HELP_STRING([--enable-xcb],
              [Build xcb (default: no)]),
              [], [enable-xcb=no])
AM_CONDITIONAL(YAMI_INF_XCB, [test x$enable_xcb = xyes])

PKG_CHECK_MODULES([LIBVA], [libva >= 1.3.0], [], [AC_MSG_ERROR([can not find libva])])
PKG_CHECK_MODULES([LIBYAMI], [libyami >= 0.3.0], [], [AC_MSG_ERROR([can not find libyami])])

# checking for libva_x11
if test "x$enable_x11" = "xyes"
then
    PKG_CHECK_MODULES([LIBVAX11], [libva-x11 >= 1.3.0], [], [AC_MSG_ERROR([can not find libva-x11])])
fi

# checking for libxcb
if test "x$enable_xcb" = "xyes"
then
    PKG_CHECK_MODULES([LIBXCB], [xcb >= 0.1.0], [], [AC_MSG_ERROR([can not find xcb])])
fi

AM_CONDITIONAL(GOT_PREFIX, test "x${prefix}" != "xNONE"])

AC_CONFIG_FILES([Makefile])

AC_OUTPUT
