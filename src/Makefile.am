AM_CPPFLAGS=-DDILLORC_SYS='"$(sysconfdir)/dillorc"' @LIBJPEG_CPPFLAGS@
AM_CFLAGS = -I../../dw-testbed @LIBPNG_CFLAGS@
AM_CXXFLAGS = -I../../dw-testbed @LIBPNG_CFLAGS@ @LIBFLTK_CXXFLAGS@

SUBDIRS = IO

bin_PROGRAMS = dillo-fltk

dillo_fltk_LDADD = \
	../dlib/libDlib.a \
	../dpip/libDpip.a \
	IO/libDiof.a \
	../../dw-testbed/dw/libDw-widgets.a \
	../../dw-testbed/dw/libDw-fltk.a \
	../../dw-testbed/dw/libDw-core.a \
	../../dw-testbed/lout/liblout.a \
	@LIBJPEG_LIBS@ @LIBPNG_LIBS@ @LIBFLTK_LIBS@ @LIBZ_LIBS@

dillo_fltk_SOURCES = \
	dillo.cc \
	dir.c \
	dir.h \
	ui.cc \
	ui.hh \
	uicmd.cc \
	uicmd.hh \
	bw.h \
	bw.c \
	cookies.c \
	cookies.h \
	colors.c \
	colors.h \
	binaryconst.h \
	misc.c \
	misc.h \
	history.h \
	history.c \
	prefs.c \
	prefs.h \
	debug.h \
	msg.h \
	list.h \
	url.c \
	url.h \
	bitvec.c \
	bitvec.h \
	klist.c \
	klist.h \
	chain.c \
	chain.h \
	timeout.cc \
	timeout.hh \
	dialog.cc \
	dialog.hh \
	\
	\
	web.cc \
	web.hh \
	nav.c \
	nav.h \
	cache.c \
	cache.h \
	dicache.c \
	dicache.h \
	capi.c \
	capi.h \
	plain.cc \
	form.cc \
	form.hh \
	html.cc \
	html.hh \
	bookmark.c \
	bookmark.h \
	dns.c \
	dns.h \
	gif.c \
	jpeg.c \
	png.c \
	image.cc \
	image.hh \
	menu.hh \
	menu.cc \
	dpiapi.c \
	dpiapi.h \
	pixmaps.h

EXTRA_DIST = chg srch
