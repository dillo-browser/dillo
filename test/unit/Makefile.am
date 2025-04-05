AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DCUR_SRC_DIR="\"$(srcdir)\"" \
	-DCUR_WORKING_DIR='"@BASE_CUR_WORKING_DIR@/test/unit"'

AM_CFLAGS = @LIBFLTK_CFLAGS@
AM_CXXFLAGS = @LIBFLTK_CXXFLAGS@

LDADD = \
	$(top_builddir)/lout/liblout.a

TESTS = \
	containers \
	disposition \
	identity \
	liang \
	notsosimplevector \
	shapes \
	unicode_test

# Some test are broken, so only build them
check_PROGRAMS = $(TESTS) \
	cookies \
	trie

EXTRA_DIST = \
	hyph-en-us.pat \
	hyph-de.pat

containers_SOURCES = containers.cc
containers_LDADD = \
	$(top_builddir)/lout/liblout.a \
	$(top_builddir)/dlib/libDlib.a
disposition_SOURCES = \
	disposition.c
disposition_LDADD = \
	$(top_builddir)/dlib/libDlib.a
notsosimplevector_SOURCES = notsosimplevector.cc
identity_SOURCES = identity.cc
identity_LDADD = \
	$(top_builddir)/lout/liblout.a \
	$(top_builddir)/dlib/libDlib.a
cookies_SOURCES = cookies.c
cookies_LDADD = \
	$(top_builddir)/dpip/libDpip.a \
	$(top_builddir)/dlib/libDlib.a
shapes_SOURCES = shapes.cc
shapes_LDADD = \
	$(top_builddir)/dw/libDw-core.a \
	$(top_builddir)/dlib/libDlib.a \
	$(top_builddir)/lout/liblout.a
unicode_test_SOURCES = unicode_test.cc
unicode_test_LDADD = \
	$(top_builddir)/lout/liblout.a \
	@LIBFLTK_LIBS@ @LIBX11_LIBS@
liang_SOURCES = liang.cc
liang_LDADD = \
	$(top_builddir)/dw/libDw-widgets.a \
	$(top_builddir)/dw/libDw-fltk.a \
	$(top_builddir)/dw/libDw-core.a \
	$(top_builddir)/lout/liblout.a \
	$(top_builddir)/dlib/libDlib.a \
	@LIBFLTK_LIBS@ @LIBX11_LIBS@
trie_SOURCES = trie.cc
trie_LDADD = \
	$(top_builddir)/dw/libDw-widgets.a \
	$(top_builddir)/dw/libDw-fltk.a \
	$(top_builddir)/dw/libDw-core.a \
	$(top_builddir)/lout/liblout.a \
	$(top_builddir)/dlib/libDlib.a \
	@LIBFLTK_LIBS@ @LIBX11_LIBS@
