dnl Process this file with aclocal, autoconf and automake.

AC_INIT([dillo], [3.0.5])

dnl Detect the canonical target build environment
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/dillo.cc])
AC_CONFIG_HEADERS([config.h])

sysconfdir=${sysconfdir}/${PACKAGE}

dnl Options

AC_ARG_WITH(jpeg-lib, [  --with-jpeg-lib=DIR     Specify where to find libjpeg], LIBJPEG_LIBDIR=$withval)
AC_ARG_WITH(jpeg-inc, [  --with-jpeg-inc=DIR     Specify where to find libjpeg's headers], LIBJPEG_INCDIR=$withval)

AC_ARG_ENABLE(efence, [  --enable-efence         Try to compile and run with Electric Fence],
                    , enable_efence=no)
AC_ARG_ENABLE(gprof,  [  --enable-gprof          Try to compile and run with profiling enabled],
                    , enable_gprof=no)
AC_ARG_ENABLE(insure, [  --enable-insure         Try to compile and run with Insure++],
                    , enable_insure=no)
AC_ARG_ENABLE(ssl,    [  --enable-ssl            Enable ssl, https (ALPHA CODE)],
                    , enable_ssl=no)
AC_ARG_ENABLE(ipv6,   [  --enable-ipv6           Build with support for IPv6], , )
AC_ARG_ENABLE(cookies,[  --disable-cookies       Don't compile support for cookies],
                    , enable_cookies=yes)
AC_ARG_ENABLE(png,    [  --disable-png           Disable support for PNG images],
              enable_png=$enableval, enable_png=yes)
AC_ARG_ENABLE(jpeg,   [  --disable-jpeg          Disable support for JPEG images],
              enable_jpeg=$enableval, enable_jpeg=yes)
AC_ARG_ENABLE(gif,    [  --disable-gif           Disable support for GIF images],
              enable_gif=$enableval, enable_gif=yes)
AC_ARG_ENABLE(threaded-dns,[  --disable-threaded-dns  Disable the advantage of a reentrant resolver library],
              enable_threaded_dns=$enableval, enable_threaded_dns=yes)
AC_ARG_ENABLE(rtfl,   [  --enable-rtfl           Build with rtfl messages (for debugging rendering)])
AC_PROG_CC
AC_PROG_CXX
AC_PROG_RANLIB
AC_PROG_CPP

dnl ----------------------------
dnl Check our char and int types
dnl ----------------------------
dnl
AC_CHECK_SIZEOF(char)
AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(void *)

AC_TYPE_INT16_T
AC_TYPE_UINT16_T
AC_TYPE_INT32_T
AC_TYPE_UINT32_T

dnl --------------------------------------
dnl Check whether to add /usr/local or not
dnl (this is somewhat a religious problem)
dnl --------------------------------------
dnl
if test "`$CPP -v < /dev/null 2>&1 | grep '/usr/local/include' 2>&1`" = ""; then
  CPPFLAGS="$CPPFLAGS -I/usr/local/include"
  LDFLAGS="$LDFLAGS -L/usr/local/lib"
fi

dnl ------------------------------------
dnl Check for socket libs (AIX, Solaris)
dnl ------------------------------------
dnl
AC_CHECK_FUNCS(gethostbyname,,
  [AC_CHECK_LIB(nsl,gethostbyname,,[AC_CHECK_LIB(socket,gethostbyname)])])
AC_CHECK_FUNCS(setsockopt,,[AC_CHECK_LIB(socket,setsockopt)])

dnl --------------------
dnl Checks for socklen_t
dnl --------------------
dnl
AC_MSG_CHECKING([for socklen_t])
ac_cv_socklen_t=""
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <sys/types.h>
#include <sys/socket.h>
]],[[
socklen_t a=0;
getsockname(0,(struct sockaddr*)0, &a);
]])],
ac_cv_socklen_t="socklen_t",
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <sys/types.h>
#include <sys/socket.h>
]],[[
int a=0;
getsockname(0,(struct sockaddr*)0, &a);
]])],
ac_cv_socklen_t="int",
ac_cv_socklen_t="size_t"
)
)
AC_MSG_RESULT($ac_cv_socklen_t)
if test "$ac_cv_socklen_t" != "socklen_t"; then
  AC_DEFINE_UNQUOTED([socklen_t], [$ac_cv_socklen_t],
                     [Define the real type of socklen_t])
fi


dnl -------------------------
dnl Test for FLTK 1.3 library
dnl -------------------------
dnl
dnl For debugging and to be user friendly
AC_MSG_CHECKING([FLTK 1.3])
AC_PATH_PROG(FLTK_CONFIG,fltk-config)
fltk_version="`$FLTK_CONFIG --version 2>/dev/null`"
case $fltk_version in
  1.3.*) AC_MSG_RESULT(yes)
         LIBFLTK_CXXFLAGS=`$FLTK_CONFIG --cxxflags`
         LIBFLTK_CFLAGS=`$FLTK_CONFIG --cflags`
         LIBFLTK_LIBS=`$FLTK_CONFIG --ldflags`;;
  ?*)    AC_MSG_RESULT(no)
         AC_MSG_ERROR(FLTK 1.3 required; version found: $fltk_version);;
  *)     AC_MSG_RESULT(no)
         AC_MSG_ERROR(FLTK 1.3 required; fltk-config not found)
esac

dnl -----------------------------------
dnl Test for X11 (only on some systems)
dnl -----------------------------------
AC_MSG_CHECKING([whether to link to X11])
AC_LANG_PUSH([C++])
old_libs=$LIBS
old_cxxflags=$CXXFLAGS
LIBS=$LIBFLTK_LIBS
CXXFLAGS=$LIBFLTK_CXXFLAGS
AC_RUN_IFELSE([AC_LANG_PROGRAM([[
#define FL_INTERNALS
#include <FL/x.H>
]],[[
#ifdef X_PROTOCOL
   return 0;
#else
   return 1;
#endif
]])], [AC_MSG_RESULT(yes)
       LIBX11_LIBS="-lX11"],
      [AC_MSG_RESULT(no)],
      [AC_MSG_RESULT(no)
       AC_MSG_WARN([*** Test for X11 not possible when cross-compiling. ***])])
CXXFLAGS=$old_cxxflags
LIBS=$old_libs
AC_LANG_POP([C++])

dnl ----------------
dnl Test for libjpeg
dnl ----------------
dnl
if test "x$enable_jpeg" = "xyes"; then
  AC_CHECK_HEADER(jpeglib.h, jpeg_ok=yes, jpeg_ok=no)

  if test "x$jpeg_ok" = "xyes"; then
    old_libs="$LIBS"
    AC_CHECK_LIB(jpeg, jpeg_destroy_decompress, jpeg_ok=yes, jpeg_ok=no)
    LIBS="$old_libs"
  fi

  if test "x$jpeg_ok" = "xyes"; then
    LIBJPEG_LIBS="-ljpeg"
    if test -n "$LIBJPEG_LIBDIR"; then
      LIBJPEG_LDFLAGS="-L$LIBJPEG_LIBDIR"
    fi
    if test -n "$LIBJPEG_INCDIR"; then
      LIBJPEG_CPPFLAGS="-I$LIBJPEG_INCDIR"
    fi
  else
    AC_MSG_WARN([*** No libjpeg found. Disabling jpeg images.***])
  fi
fi

if test "x$jpeg_ok" = "xyes"; then
  AC_DEFINE([ENABLE_JPEG], [1], [Enable JPEG images])
fi

dnl -------------
dnl Test for zlib
dnl -------------
dnl
AC_CHECK_HEADER(zlib.h, libz_ok=yes, libz_ok=no)

if test "x$libz_ok" = "xyes"; then
  old_libs="$LIBS"
  AC_CHECK_LIB(z, zlibVersion, libz_ok=yes, libz_ok=no)
  LIBS="$old_libs"
fi

if test "x$libz_ok" = xyes; then
  LIBZ_LIBS="-lz"
else
  AC_MSG_ERROR(zlib must be installed!)
fi

dnl ---------------
dnl Test for libpng
dnl ---------------
dnl
if test "x$enable_png" = "xyes"; then
  AC_MSG_CHECKING([for libpng-config])

dnl Check if the user hasn't set the variable $PNG_CONFIG
  if test -z "$PNG_CONFIG"; then
    PNG_CONFIG=`which libpng16-config`
    if test -z "$PNG_CONFIG"; then
      PNG_CONFIG=`which libpng14-config`
    fi
    if test -z "$PNG_CONFIG"; then
      PNG_CONFIG=`which libpng12-config`
    fi
    if test -z "$PNG_CONFIG"; then
      PNG_CONFIG=`which libpng-config`
    fi
    if test -z "$PNG_CONFIG"; then
      PNG_CONFIG=`which libpng10-config`
    fi
  fi

dnl Check if the libpng-config script was found and is executable
  if test -n "$PNG_CONFIG" && test -x "$PNG_CONFIG"; then
    AC_MSG_RESULT([$PNG_CONFIG])
    png_ok="yes"
  else
    AC_MSG_RESULT([missing])
    png_ok="no"
  fi

  if test "x$png_ok" = "xyes"; then
dnl For debugging and to be user friendly
    AC_MSG_CHECKING([for libpng version])
    png_version=`$PNG_CONFIG --version`
    case $png_version in
      1.[[0246]].*) AC_MSG_RESULT([$png_version]) ;;
                *) AC_MSG_RESULT([$png_version (unrecognised version)]) ;;
    esac

dnl Try to use options that are supported by all libpng-config versions...
    LIBPNG_CFLAGS=`$PNG_CONFIG --cflags`
    LIBPNG_LIBS=`$PNG_CONFIG --ldflags`
    case $png_version in
      1.2.4*) LIBPNG_LIBS="$LIBPNG_LIBS `$PNG_CONFIG --libs`" ;;
    esac
  else
dnl Try to find libpng even though libpng-config wasn't found
    AC_CHECK_HEADERS(png.h libpng/png.h, png_ok=yes && break, png_ok=no)

    if test "x$png_ok" = "xyes"; then
      old_libs="$LIBS"
      AC_CHECK_LIB(png, png_sig_cmp, png_ok=yes, png_ok=no, $LIBZ_LIBS -lm)
      LIBS="$old_libs"

      if test "x$png_ok" = "xyes"; then
        LIBPNG_LIBS="-lpng -lm"
      fi
    fi

    if test "x$png_ok" = "xno"; then
      AC_MSG_WARN([*** No libpng found. Disabling PNG images ***])
    fi
  fi
fi

if test "x$png_ok" = "xyes"; then
  AC_DEFINE([ENABLE_PNG], [1], [Enable PNG images])
fi

dnl Check if support for GIF images should be compiled in
if test "x$enable_gif" = "xyes"; then
  AC_DEFINE([ENABLE_GIF], [1], [Enable GIF images])
fi

dnl --------------------------
dnl  Test for support for SSL
dnl --------------------------
dnl
if test "x$enable_ssl" = "xyes"; then
  AC_CHECK_HEADER(openssl/ssl.h, ssl_ok=yes, ssl_ok=no)

  if test "x$ssl_ok" = "xyes"; then
    old_libs="$LIBS"
    AC_CHECK_LIB(ssl, SSL_library_init, ssl_ok=yes, ssl_ok=no, -lcrypto)
    LIBS="$old_libs"
  fi

  if test "x$ssl_ok" = "xyes"; then
    LIBSSL_LIBS="-lcrypto -lssl"
    AC_MSG_WARN([*** Enabling ssl support. THIS IS ALPHA CODE!***])
  else
    AC_MSG_WARN([*** No libssl found. Disabling ssl support.***])
  fi
fi

if test "x$ssl_ok" = "xyes"; then
  AC_DEFINE([ENABLE_SSL], [1], [Enable SSL support])
fi

dnl --------------------------------------------------------------
dnl Test for iconv functionality in libc or for libiconv usability
dnl --------------------------------------------------------------
AC_CHECK_HEADER(iconv.h, iconv_ok=yes, iconv_ok=no)
if test "x$iconv_ok" = "xyes"; then
  AC_CHECK_LIB(c, iconv_open, LIBICONV_LIBS="",
    AC_CHECK_LIB(iconv, iconv_open, LIBICONV_LIBS="-liconv", iconv_ok=no))
fi
if test "x$iconv_ok" = "xno"; then
  dnl Test for OpenBSD
  old_libs="$LIBS"
  LIBS="$old_libs -liconv"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <iconv.h>
]],[[
iconv_open("","");
]])],
iconv_ok=yes,iconv_ok=no)
  LIBS="$old_libs"
  if test "x$iconv_ok" = "xyes"; then
     LIBICONV_LIBS="-liconv"
  fi
fi

if test "x$iconv_ok" = "xno"; then
  AC_MSG_ERROR(libiconv must be installed!)
fi

dnl ----------------------
dnl Check if we need to
dnl support the old
dnl iconv interface
dnl ----------------------
if test "x$iconv_ok" = "xyes"; then
  old_libs="$LIBS"
  LIBS="$old_libs $LIBICONV_LIBS"
  old_cflags="$CFLAGS"
  CFLAGS="$CFLAGS -Werror"
  AC_LANG_PUSH([C++])
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <iconv.h>
]],[[
const char *inPtr;
char *outPtr;
size_t inLeft = 0, outRoom = 0;
iconv_t encoder = iconv_open("ASCII", "UTF-8");
iconv(encoder, &inPtr, &inLeft, &outPtr, &outRoom);
]])],
iconv_old=yes,iconv_old=no)
  AC_LANG_POP([C++])
  LIBS="$old_libs"
  CFLAGS="$old_cflags"

  if test "x$iconv_old" = "xyes"; then
    AC_DEFINE([inbuf_t], [const char], [Use const char pointers for older libiconv])
  else
    AC_DEFINE([inbuf_t], [char], [Use char pointers for newer libiconv])
  fi
fi

dnl ----------------------
dnl Test for POSIX threads
dnl ----------------------
dnl
if test -z "$LIBPTHREAD_LIBS"; then
case $target in
  *-*-linux*|*-*-solaris*)
    old_libs="$LIBS"
    AC_CHECK_LIB(pthread, pthread_create, LIBPTHREAD_LIBS="-lpthread")
    LIBS="$old_libs"
    ;;

  *-*-osf1*)
    AC_MSG_CHECKING(whether pthreads work)
    LIBPTHREAD_LIBS="-lpthread -lexc -ldb"
    AC_MSG_WARN([*** _Untested pthreads_ try setting LIBPTHREAD_LIBS manually if it doesn't work ***])
    ;;

  *-*-minix*)
    AC_MSG_NOTICE([Minix detected, skipping pthread detection])
    ;;

  *)
    AC_MSG_CHECKING(whether threads work with -pthread)
    LDSAVEFLAGS=$LDFLAGS
    LDFLAGS="$LDFLAGS -pthread"
    AC_LINK_IFELSE([AC_LANG_CALL([],[pthread_create])],
pthread_ok=yes, pthread_ok=no)
    LDFLAGS=$LDSAVEFLAGS

    if test "x$pthread_ok" = "xyes"; then
      AC_MSG_RESULT(yes)
      LIBPTHREAD_LDFLAGS="-pthread"
    else
      AC_MSG_RESULT(no. Now we will try some libraries.)

      AC_SEARCH_LIBS(pthread_create, pthread,
                     LIBPTHREADS_LIBS="-lpthread",
      AC_SEARCH_LIBS(pthread_create, pthreads,
                     LIBPTHREADS_LIBS="-lpthreads",
      AC_SEARCH_LIBS(pthread_create, c_r,
                     LIBPTHREADS_LIBS="-lc_r", thread_ok=no)))

      if test "x$thread_ok" = "xno"; then
        AC_MSG_WARN([*** No pthreads found. ***])
        AC_MSG_ERROR([*** Try setting LIBPTHREAD_LIBS manually to point to your pthreads library. ***])
        exit 1
      else
        AC_MSG_WARN([found a way to link threads, but it may not work...])
      fi
    fi
    ;;

esac
fi

dnl --------------------
dnl Command line options
dnl --------------------
dnl
if test "x$enable_cookies" = "xno" ; then
  CFLAGS="$CFLAGS -DDISABLE_COOKIES"
  CXXFLAGS="$CXXFLAGS -DDISABLE_COOKIES"
fi
if test "x$enable_ipv6" = "xyes" ; then
  CFLAGS="$CFLAGS -DENABLE_IPV6"
fi
if test "x$enable_efence" = "xyes" ; then
  LIBS="-lefence $LIBS"
fi
if test "x$enable_gprof" = "xyes" ; then
  CFLAGS="$CFLAGS -pg"
  CXXFLAGS="$CXXFLAGS -pg"
fi
if test "x$enable_insure" = "xyes" ; then
  CC="insure -Zoi \"compiler $CC\""
  LIBS="$LIBS -lstdc++-2-libc6.1-1-2.9.0"
fi
if test "x$enable_threaded_dns" = "xyes" ; then
  CFLAGS="$CFLAGS -DD_DNS_THREADED"
fi
if test "x$enable_rtfl" = "xyes" ; then
  CXXFLAGS="$CXXFLAGS -DDBG_RTFL"
fi

dnl -----------------------
dnl Checks for header files
dnl -----------------------
dnl
AC_CHECK_HEADERS(fcntl.h unistd.h sys/uio.h)

dnl --------------------------
dnl Check for compiler options
dnl --------------------------
dnl
if eval "test x$GCC = xyes"; then
  if test "`echo $CFLAGS | grep '\-D_REENTRANT' 2> /dev/null`" = ""; then
    CFLAGS="$CFLAGS -D_REENTRANT"
  fi
  if test "`echo $CFLAGS | grep '\-D_THREAD_SAFE' 2> /dev/null`" = ""; then
    CFLAGS="$CFLAGS -D_THREAD_SAFE"
  fi
  if test "`echo $CFLAGS | grep '\-Wall' 2> /dev/null`" = ""; then
    CFLAGS="$CFLAGS -Wall"
  fi
  if test "`echo $CFLAGS | grep -e '-W ' -e '-W$' 2> /dev/null`" = ""; then
    CFLAGS="$CFLAGS -W"
  fi
  if test "`echo $CFLAGS | grep '\-Wno-unused-parameter' 2> /dev/null`" = ""; then
    CFLAGS="$CFLAGS -Wno-unused-parameter"
  fi
  if test "`echo $CFLAGS | grep '\-Waggregate-return' 2> /dev/null`" = ""; then
    CFLAGS="$CFLAGS -Waggregate-return"
  fi
fi
dnl -----------
dnl CXX options
dnl -----------
dnl

if eval "test x$GCC = xyes"; then
  CXXFLAGS="$CXXFLAGS -Wall -W -Wno-unused-parameter -fno-rtti -fno-exceptions"
fi

AC_SUBST(LIBJPEG_LIBS)
AC_SUBST(LIBJPEG_LDFLAGS)
AC_SUBST(LIBJPEG_CPPFLAGS)
AC_SUBST(LIBPNG_LIBS)
AC_SUBST(LIBPNG_CFLAGS)
AC_SUBST(LIBZ_LIBS)
AC_SUBST(LIBSSL_LIBS)
AC_SUBST(LIBPTHREAD_LIBS)
AC_SUBST(LIBPTHREAD_LDFLAGS)
AC_SUBST(LIBFLTK_CXXFLAGS)
AC_SUBST(LIBFLTK_CFLAGS)
AC_SUBST(LIBFLTK_LIBS)
AC_SUBST(LIBICONV_LIBS)
AC_SUBST(LIBX11_LIBS)
AC_SUBST(datadir)

AC_CONFIG_FILES([
    Makefile
    dlib/Makefile
    dpip/Makefile
    dpid/Makefile
    dpi/Makefile
    doc/Makefile
    dw/Makefile
    lout/Makefile
    src/Makefile
    src/IO/Makefile
    test/Makefile
])

AC_OUTPUT
