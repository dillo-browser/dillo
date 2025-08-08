dnl Process this file with aclocal, autoconf and automake.

AC_INIT([dillo], [3.2.0])

dnl Detect the canonical target build environment
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/dillo.cc])
AC_CONFIG_HEADERS([config.h])

sysconfdir=${sysconfdir}/${PACKAGE}

dnl Options

AC_ARG_WITH([jpeg-lib],
  [AS_HELP_STRING([--with-jpeg-lib=DIR], [Specify where to find libjpeg])],
  LIBJPEG_LIBDIR=$withval)

AC_ARG_WITH([jpeg-inc],
  [AS_HELP_STRING([--with-jpeg-inc=DIR], [Specify where to find libjpeg headers])],
  LIBJPEG_INCDIR=$withval)

AC_ARG_WITH([mbedtls-lib],
  [AS_HELP_STRING([--with-mbedtls-lib=DIR], [Specify where to find MbedTLS libraries])],
  [MBEDTLS_LIBDIR=$withval])

AC_ARG_WITH([mbedtls-inc],
  [AS_HELP_STRING([--with-mbedtls-inc=DIR], [Specify where to find MbedTLS headers])],
  [MBEDTLS_INCDIR=$withval])

AC_ARG_ENABLE([efence],
  [AS_HELP_STRING([--enable-efence], [Try to compile and run with Electric Fence])],
  [enable_efence=$enableval],
  [enable_efence=no])

AC_ARG_ENABLE([gprof],
  [AS_HELP_STRING([--enable-gprof], [Try to compile and run with profiling enabled])],
  [enable_gprof=$enableval],
  [enable_gprof=no])

AC_ARG_ENABLE([insure],
  [AS_HELP_STRING([--enable-insure], [Try to compile and run with Insure++])],
  [enable_insure=$enableval],
  [enable_insure=no])

AC_ARG_ENABLE([ipv6],
  [AS_HELP_STRING([--enable-ipv6], [Build with support for IPv6 (default: auto)])],
  [enable_ipv6=$enableval],
  [enable_ipv6=auto])

AC_ARG_ENABLE([cookies],
  [AS_HELP_STRING([--disable-cookies], [Dont compile support for cookies])],
  [enable_cookies=$enableval],
  [enable_cookies=yes])

AC_ARG_ENABLE([png],
  [AS_HELP_STRING([--disable-png], [Disable support for PNG images])],
  [enable_png=$enableval],
  [enable_png=yes])

AC_ARG_ENABLE([webp],
  [AS_HELP_STRING([--disable-webp], [Disable support for WEBP images])],
  [enable_webp=$enableval],
  [enable_webp=yes])

AC_ARG_ENABLE([jpeg],
  [AS_HELP_STRING([--disable-jpeg], [Disable support for JPEG images])],
  [enable_jpeg=$enableval],
  [enable_jpeg=yes])

AC_ARG_ENABLE([gif],
  [AS_HELP_STRING([--disable-gif], [Disable support for GIF images])],
  [enable_gif=$enableval],
  [enable_gif=yes])

AC_ARG_ENABLE([svg],
  [AS_HELP_STRING([--disable-svg], [Disable support for SVG images])],
  [enable_svg=$enableval],
  [enable_svg=yes])

AC_ARG_ENABLE([threaded-dns],
  [AS_HELP_STRING([--disable-threaded-dns], [Disable the advantage of a reentrant resolver library])],
  [enable_threaded_dns=$enableval],
  [enable_threaded_dns=yes])

AC_ARG_ENABLE([xembed],
  [AS_HELP_STRING([--disable-xembed], [Disable support for X11 XEmbed])],
  [enable_xembed=$enableval],
  [enable_xembed=yes])

AC_ARG_ENABLE([tls],
  [AS_HELP_STRING([--disable-tls], [Disable TLS support (for HTTPS)])],
  [enable_tls=$enableval],
  [enable_tls=yes])

dnl Deprecated, if given it will cause an error
AC_ARG_ENABLE([ssl],
  [AS_HELP_STRING([--disable-ssl], [Disable TLS support (deprecated, use --disable-tls)])],
  [enable_ssl=$enableval],
  [enable_ssl=])

AC_ARG_ENABLE([openssl],
  [AS_HELP_STRING([--disable-openssl], [Disable support for OpenSSL])],
  [enable_openssl=$enableval],
  [enable_openssl=yes])

AC_ARG_ENABLE([mbedtls],
  [AS_HELP_STRING([--disable-mbedtls], [Disable support for mbedTLS])],
  [enable_mbedtls=$enableval],
  [enable_mbedtls=yes])

AC_ARG_ENABLE([brotli],
  [AS_HELP_STRING([--disable-brotli], [Disable support for brotli encoding])],
  [enable_brotli=$enableval],
  [enable_brotli=yes])

AC_ARG_WITH([ca-certs-file],
  [AS_HELP_STRING([--with-ca-certs-file=FILE], [Specify where to find a bundle of trusted CA certificates for TLS])],
  CA_CERTS_FILE=$withval)

AC_ARG_WITH([ca-certs-dir],
  [AS_HELP_STRING([--with-ca-certs-dir=DIR], [Specify where to find a directory containing trusted CA certificates for TLS])],
  CA_CERTS_DIR=$withval)

dnl Developer options

AC_ARG_ENABLE([html-tests],
  [AS_HELP_STRING([--enable-html-tests], [Enable HTML rendering tests, requires xvfb, xwd and imagemagick (for developers only)])],
  [enable_html_tests=$enableval],
  [enable_html_tests=no])

AC_ARG_ENABLE([rtfl],
  [AS_HELP_STRING([--enable-rtfl], [Print low-level RTFL messages for debugging the renderer. Adds very large slowdown (for developers only)])],
  [enable_rtfl=$enableval],
  [enable_rtfl=no])

if test "x$enable_ssl" = "xno"; then
  AC_MSG_ERROR([The flag --disable-ssl is deprecated, use --disable-tls])
fi
dnl Also catch --enable-ssl=maybe
if test "x$enable_ssl" != "x"; then
  AC_MSG_ERROR([The flag --enable-ssl is deprecated, use --enable-tls])
fi

AC_PROG_CC
AC_PROG_CXX
AC_PROG_RANLIB
AC_PROG_CPP

AC_MSG_CHECKING(if C++ compiler '$CXX' works)
AC_LANG_PUSH([C++])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
  #ifndef __cplusplus
  #error "No C++ support, AC_PROG_CXX failed"
  #endif
  ]])],
  [AC_MSG_RESULT([yes])],
  [AC_MSG_RESULT(no)
  AC_MSG_FAILURE([C++ compiler doesn't work])]
)
AC_LANG_POP([C++])

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

dnl -----------------------------------------------------------------
dnl Check for absolute path of working directory.
dnl This is needed for RTFL, to get full the full paths of the source
dnl file names
dnl -----------------------------------------------------------------
dnl
BASE_CUR_WORKING_DIR=`pwd`

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
AC_PATH_PROGS(FLTK_CONFIG,[fltk-config1.3 fltk-config])
AC_MSG_CHECKING([FLTK 1.3])
fltk_version="`$FLTK_CONFIG --version 2>/dev/null`"
case $fltk_version in
  1.3.*) AC_MSG_RESULT(yes)
         LIBFLTK_CXXFLAGS=`$FLTK_CONFIG --cxxflags`
         LIBFLTK_CFLAGS=`$FLTK_CONFIG --cflags`
         LIBFLTK_LIBS=`$FLTK_CONFIG --ldflags`;;
  1.4.*) AC_MSG_RESULT(no)
         AC_MSG_ERROR([FLTK $fltk_version not supported yet; use FLTK 1.3]);;
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
jpeg_ok=no
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

dnl ----------------
dnl Test for libwebp
dnl ----------------
dnl
if test "x$enable_webp" = "xyes"; then
  AC_CHECK_HEADER(webp/decode.h, webp_ok=yes, webp_ok=no)

  if test "x$webp_ok" = "xyes"; then
    old_libs="$LIBS"
    AC_CHECK_LIB(webp, WebPGetInfo, webp_ok=yes, webp_ok=no)
    LIBS="$old_libs"
  fi

  if test "x$webp_ok" = "xyes"; then
    LIBWEBP_LIBS="-lwebp"
  else
    AC_MSG_WARN([*** No libwebp found. Disabling webp images.***])
  fi
fi

if test "x$webp_ok" = "xyes"; then
  AC_DEFINE([ENABLE_WEBP], [1], [Enable webp images])
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
dnl Test for brotli
dnl ---------------
dnl
if test "x$enable_brotli" = "xyes"; then
  AC_CHECK_HEADER(brotli/decode.h, brotli_ok=yes, brotli_ok=no)

  if test "x$brotli_ok" = "xyes"; then
    old_libs="$LIBS"
    AC_CHECK_LIB(brotlidec, BrotliDecoderVersion, brotli_ok=yes, brotli_ok=no)
    LIBS="$old_libs"

    if test "x$brotli_ok" = "xyes"; then
      BROTLI_LIBS="-lbrotlidec"
    else
      AC_MSG_WARN([*** libbrotlidec not found. Disabling brotli encoding.***])
    fi
  else
    AC_MSG_WARN([*** brotli/decode.h not found. Disabling brotli encoding.***])
  fi

fi

if test "x$brotli_ok" = "xyes"; then
  AC_DEFINE([ENABLE_BROTLI], [1], [Enable brotli encoding])
fi

dnl ---------------
dnl Test for libpng
dnl ---------------
dnl
png_ok="no"
if test "x$enable_png" = "xyes"; then
  AC_MSG_CHECKING([for libpng-config])

dnl Check if the user hasn't set the variable $PNG_CONFIG
  if test -z "$PNG_CONFIG"; then
    PNG_CONFIG=`command -v libpng16-config`
    if test -z "$PNG_CONFIG"; then
      PNG_CONFIG=`command -v libpng14-config`
    fi
    if test -z "$PNG_CONFIG"; then
      PNG_CONFIG=`command -v libpng12-config`
    fi
    if test -z "$PNG_CONFIG"; then
      PNG_CONFIG=`command -v libpng-config`
    fi
    if test -z "$PNG_CONFIG"; then
      PNG_CONFIG=`command -v libpng10-config`
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

dnl Check if support for SVG images should be compiled in
if test "x$enable_svg" = "xyes"; then
  AC_DEFINE([ENABLE_SVG], [1], [Enable SVG images])
else
  AC_MSG_NOTICE([Disabling SVG])
fi

dnl --------------------------
dnl  Test for support for SSL/TLS
dnl --------------------------
dnl

tls_ok="no"
tls_impl="none"
if test "x$enable_tls" = "xyes"; then
  if test "x$enable_openssl" = "xyes"; then
    dnl Search for OpenSSL headers first
    AC_CHECK_HEADER(openssl/ssl.h, openssl_ok=yes, openssl_ok=no)

    dnl If the headers are found, try to link with -lssl and -lcrypto
    if test "x$openssl_ok" = "xyes"; then
      old_libs="$LIBS"
      AC_CHECK_LIB(ssl, SSL_write, openssl_ok=yes, openssl_ok=no, -lcrypto)
      LIBS="$old_libs"
    fi

    dnl If all went good, set OpenSSL
    if test "x$openssl_ok" = "xyes"; then
      AC_MSG_NOTICE([Using OpenSSL as TLS library.])
      tls_impl="OpenSSL"
      AC_DEFINE([HAVE_OPENSSL], [1], [OpenSSL works])
      LIBSSL_LIBS="-lcrypto -lssl"
    else
      AC_MSG_NOTICE([Cannot find OpenSSL, trying mbedTLS...])
    fi
  else
    AC_MSG_NOTICE([Skipping OpenSSL search, as it is disabled])
  fi

  dnl Try to find mbedTLS if OpenSSL failed or is disabled
  if test "x$enable_mbedtls" = "xyes"; then
    if test "x$openssl_ok" != "xyes"; then

      if test -n "$MBEDTLS_LIBDIR"; then
        MBEDTLS_LDFLAGS="-L$MBEDTLS_LIBDIR"
      fi
      if test -n "$MBEDTLS_INCDIR"; then
        MBEDTLS_CPPFLAGS="-I$MBEDTLS_INCDIR"
      fi

      old_cppflags="$CPPFLAGS"
      CPPFLAGS="$CPPFLAGS $MBEDTLS_CPPFLAGS"
      dnl In mbed TLS 2.3.0, ssl.h needs platform.h but fails to include it.
      AC_CHECK_HEADER(mbedtls/ssl.h, mbedtls_ok=yes, mbedtls_ok=no, [#include <mbedtls/platform.h>])
      CPPFLAGS="$old_cppflags"

      dnl If the headers are found, try to link with mbedTLS
      if test "x$mbedtls_ok" = "xyes"; then
        old_libs="$LIBS"
        old_ldflags="$LDFLAGS"
        LDFLAGS="$LDFLAGS $MBEDTLS_LDFLAGS"
        AC_SEARCH_LIBS(mbedtls_ssl_init, [mbedtls-3 mbedtls], mbedtls_ok=yes, mbedtls_ok=no)
        AC_SEARCH_LIBS(mbedtls_pk_get_name, [mbedcrypto-3 mbedcrypto], mbedcrypto_ok=yes, mbedcrypto_ok=no)
        AC_SEARCH_LIBS(mbedtls_x509_crt_init, [mbedx509-3 mbedx509], mbedx509_ok=yes, mbedx509_ok=no)
        LDFLAGS="$old_ldflags"
        LIBS="$old_libs"
      fi

      dnl If it went good, use it, otherwise disable TLS support
      if test "x$mbedtls_ok" = "xyes" && test "x$mbedcrypto_ok" = "xyes" && test "x$mbedx509_ok" = "xyes" ; then
        AC_MSG_NOTICE([Using mbedTLS as TLS library.])
        tls_impl="mbedTLS"
        AC_DEFINE([HAVE_MBEDTLS], [1], [mbedTLS works])
        LIBSSL_LIBS="$ac_cv_search_mbedtls_ssl_init $ac_cv_search_mbedtls_pk_get_name $ac_cv_search_mbedtls_x509_crt_init"
        LIBSSL_LDFLAGS="$MBEDTLS_LDFLAGS"
        LIBSSL_CPPFLAGS="$MBEDTLS_CPPFLAGS"
      else
        mbedtls_ok=no
        AC_MSG_NOTICE([Cannot find mbedTLS])
      fi
    fi
  else
    AC_MSG_NOTICE([Skipping mbedTLS search, as it is disabled])
  fi

  dnl Only need one that works
  if test "x$openssl_ok" = "xyes" -o "x$mbedtls_ok" = "xyes"; then
    tls_ok="yes"
    AC_DEFINE([ENABLE_TLS], [1], [Enable TLS support])
  else
    AC_MSG_ERROR([No TLS library available])
  fi
fi

AM_CONDITIONAL([USE_OPENSSL],  [test "x$openssl_ok" = "xyes"])
AM_CONDITIONAL([USE_MBEDTLS],  [test "x$mbedtls_ok" = "xyes"])

dnl ----------------------------------------------------------------
dnl Test for iconv implementation first in libiconv and then in libc
dnl ----------------------------------------------------------------
AC_CHECK_HEADER(iconv.h, iconv_ok=yes, iconv_ok=no)
if test "x$iconv_ok" = "xyes"; then
  dnl First try to link with the libiconv library if available, otherwise
  dnl fall back to the implementation of libc. It is required to be done
  dnl in this order because FreeBSD distributes libiconv headers in
  dnl /usr/include/local/iconv.h, in the same place the headers for FLTK
  dnl are placed. If we select to link with libc, the headers provided by
  dnl libiconv will be used while linking with libc. This causes a linkage
  dnl error.
  AC_CHECK_LIB(iconv, iconv_open, LIBICONV_LIBS="-liconv",
    AC_CHECK_LIB(c, iconv_open, LIBICONV_LIBS="", iconv_ok=no))
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

dnl ------------
dnl IPv6 support
dnl ------------
dnl
html_tests_ok=no
if test "x$enable_ipv6" = "xauto" ; then
  dnl Detect support for IPv6 in libc
  AC_MSG_CHECKING([for IPv6 support])
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <sys/socket.h>
#include <netinet/in.h>
]],[[
struct sockaddr_in6 sa;
size_t len = sizeof(struct sockaddr_in6);
sa.sin6_family = AF_INET6;
int fd;
connect(fd, (struct sockaddr *)&sa, len);
]])],
  enable_ipv6=yes,
  enable_ipv6=no)

  AC_MSG_RESULT([$enable_ipv6])
fi

if test "x$enable_ipv6" = "xyes"; then
  AC_DEFINE([ENABLE_IPV6], [1], [Enable IPv6 support])
fi

dnl ----------
dnl HTML tests
dnl ----------
dnl
html_tests_ok=no
if test "x$enable_html_tests" = "xyes" ; then
  html_tests_ok=yes
  AC_CHECK_PROG(xvfb_ok, Xvfb, yes, no)
  AC_CHECK_PROG(xwd_ok, xwd, yes, no)
  AC_CHECK_PROG(xwininfo_ok, xwininfo, yes, no)
  AC_CHECK_PROG(convert_ok, convert, yes, no)

  if test "x$xvfb_ok" != "xyes"; then
    html_tests_ok=no
  fi
  if test "x$xwd_ok" != "xyes"; then
    html_tests_ok=no
  fi
  if test "x$xwininfo_ok" != "xyes"; then
    html_tests_ok=no
  fi
  if test "x$convert_ok" != "xyes"; then
    html_tests_ok=no
  fi
  if test "x$html_tests_ok" != "xyes"; then
    AC_MSG_ERROR([Cannot find all tools to enable HTML tests])
  fi
fi

AM_CONDITIONAL([ENABLE_HTML_TESTS],  [test "x$html_tests_ok" = "xyes"])

dnl --------------------
dnl Command line options
dnl --------------------
dnl
if test "x$enable_cookies" = "xno" ; then
  CFLAGS="$CFLAGS -DDISABLE_COOKIES"
  CXXFLAGS="$CXXFLAGS -DDISABLE_COOKIES"
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
if test "x$enable_xembed" = "xno" ; then
  CXXFLAGS="$CFLAGS -DDISABLE_XEMBED"
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
  CFLAGS="$CFLAGS -pedantic -std=c99 -D_POSIX_C_SOURCE=200112L"
fi

dnl -----------
dnl CXX options
dnl -----------
dnl

if eval "test x$GCC = xyes"; then
  CXXFLAGS="$CXXFLAGS -Wall -W -Wno-unused-parameter -fno-rtti -fno-exceptions -pedantic -D_POSIX_C_SOURCE=200112L"
fi

dnl ----------------------------------
dnl Check if we can use the git commit
dnl ----------------------------------
git_ok=no
AC_CHECK_PROG(git_found, git, yes, no)
if test "x$git_found" = "xyes" ; then
  AC_MSG_CHECKING([.git directory exists])
  if test -d "$srcdir/.git"; then
    AC_MSG_RESULT([yes])
    AC_MSG_CHECKING(git describe works)
    git_version=`git --work-tree="$srcdir" describe --dirty`
    if test "x$git_version" = "x"; then
      AC_MSG_RESULT(no)
    else
      git_ok=yes
      AC_MSG_RESULT([yes ($git_version)])
    fi
  else
    AC_MSG_RESULT([no])
  fi
fi
AM_CONDITIONAL([GIT_AVAILABLE],  [test "x$git_ok" = "xyes"])

AC_SUBST(BASE_CUR_WORKING_DIR)
AC_SUBST(LIBJPEG_LIBS)
AC_SUBST(LIBJPEG_LDFLAGS)
AC_SUBST(LIBJPEG_CPPFLAGS)
AC_SUBST(LIBPNG_LIBS)
AC_SUBST(LIBPNG_CFLAGS)
AC_SUBST(LIBWEBP_LIBS)
AC_SUBST(LIBZ_LIBS)
AC_SUBST(BROTLI_LIBS)
AC_SUBST(LIBSSL_LIBS)
AC_SUBST(LIBSSL_LDFLAGS)
AC_SUBST(LIBSSL_CPPFLAGS)
AC_SUBST(LIBPTHREAD_LIBS)
AC_SUBST(LIBPTHREAD_LDFLAGS)
AC_SUBST(LIBFLTK_CXXFLAGS)
AC_SUBST(LIBFLTK_CFLAGS)
AC_SUBST(LIBFLTK_LIBS)
AC_SUBST(LIBICONV_LIBS)
AC_SUBST(LIBX11_LIBS)
AC_SUBST(CA_CERTS_FILE)
AC_SUBST(CA_CERTS_DIR)
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
    test/unit/Makefile
    test/dw/Makefile
    test/html/Makefile
])

AC_OUTPUT

_AS_ECHO([])
_AS_ECHO([Developer options:])
_AS_ECHO([])
_AS_ECHO([  RTFL enabled : ${enable_rtfl}])
_AS_ECHO([  HTML tests   : ${html_tests_ok}])
_AS_ECHO([])
_AS_ECHO([Configuration summary:])
_AS_ECHO([])
_AS_ECHO([  CC      : ${CC}])
_AS_ECHO([  CFLAGS  : ${CFLAGS}])
_AS_ECHO([  CXX     : ${CXX}])
_AS_ECHO([  CXXFLAGS: ${CXXFLAGS}])
_AS_ECHO([])
_AS_ECHO([  TLS enabled: ${tls_ok}])
_AS_ECHO([  TLS library: ${tls_impl}])
_AS_ECHO([  TLS flags  : ${LIBSSL_LIBS}])
_AS_ECHO([])
_AS_ECHO([Features:])
_AS_ECHO([])
_AS_ECHO([  Cookies enabled: ${enable_cookies}])
_AS_ECHO([  XEmbed enabled : ${enable_xembed}])
_AS_ECHO([  JPEG enabled   : ${jpeg_ok}])
_AS_ECHO([  PNG enabled    : ${png_ok}])
_AS_ECHO([  GIF enabled    : ${enable_gif}])
_AS_ECHO([  SVG enabled    : ${enable_svg}])
_AS_ECHO([  WEBP enabled   : ${webp_ok}])
_AS_ECHO([  Brotli enabled : ${brotli_ok}])
_AS_ECHO([  IPv6 enabled   : ${enable_ipv6}])
_AS_ECHO([])
