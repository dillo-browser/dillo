AM_CPPFLAGS= \
	-I$(top_srcdir) \
	-DDILLO_SYSCONF='"$(sysconfdir)/"' \
	-DDILLO_DOCDIR='"$(docdir)/"' \
	@LIBJPEG_CPPFLAGS@
AM_CFLAGS = @LIBPNG_CFLAGS@
AM_CXXFLAGS = @LIBPNG_CFLAGS@ @LIBFLTK_CXXFLAGS@

SUBDIRS = IO

bin_PROGRAMS = dillo

dillo_LDADD = \
	$(top_builddir)/dlib/libDlib.a \
	$(top_builddir)/dpip/libDpip.a \
	IO/libDiof.a \
	$(top_builddir)/dw/libDw-widgets.a \
	$(top_builddir)/dw/libDw-fltk.a \
	$(top_builddir)/dw/libDw-core.a \
	$(top_builddir)/lout/liblout.a \
	@LIBJPEG_LIBS@ @LIBPNG_LIBS@ @LIBFLTK_LIBS@ @LIBZ_LIBS@ \
	@LIBICONV_LIBS@ @LIBPTHREAD_LIBS@ @LIBX11_LIBS@

dillo_SOURCES = \
	dillo.cc \
	paths.cc \
	paths.hh \
	tipwin.cc \
	tipwin.hh \
	ui.cc \
	ui.hh \
	uicmd.cc \
	uicmd.hh \
	bw.h \
	bw.c \
	cookies.c \
	cookies.h \
	auth.c \
	auth.h \
	md5.c \
	md5.h \
	digest.c \
	digest.h \
	colors.c \
	colors.h \
	binaryconst.h \
	misc.c \
	misc.h \
	history.h \
	history.c \
	prefs.c \
	prefs.h \
	prefsparser.cc \
	prefsparser.hh \
	keys.cc \
	keys.hh \
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
	utf8.cc \
	utf8.hh \
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
	decode.c \
	decode.h \
	dicache.c \
	dicache.h \
	capi.c \
	capi.h \
	domain.c \
	domain.h \
	css.cc \
	css.hh \
	cssparser.cc \
	cssparser.hh \
	doctree.hh \
	styleengine.cc \
	styleengine.hh \
	plain.cc \
	html.cc \
	html.hh \
	html_common.hh \
	form.cc \
	form.hh \
	table.cc \
	table.hh \
	bookmark.c \
	bookmark.h \
	dns.c \
	dns.h \
	gif.c \
	dgif.h \
	jpeg.c \
	djpeg.h \
	png.c \
	dpng.h \
	imgbuf.cc \
	imgbuf.hh \
	image.cc \
	image.hh \
	menu.hh \
	menu.cc \
	dpiapi.c \
	dpiapi.h \
	pixmaps.h \
	findbar.cc \
	findbar.hh \
	xembed.cc \
	xembed.hh

dist_sysconf_DATA = domainrc keysrc
EXTRA_DIST = chg srch
