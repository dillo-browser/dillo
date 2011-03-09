AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DDILLO_BINDIR='"$(bindir)/"'
AM_CFLAGS = @LIBFLTK_CFLAGS@
AM_CXXFLAGS = @LIBFLTK_CXXFLAGS@

noinst_LIBRARIES = libDiof.a

libDiof_a_SOURCES = \
	mime.c \
	mime.h \
	about.c \
	Url.h \
	http.c \
	dpi.c \
	IO.c \
	iowatch.cc \
	iowatch.hh \
	IO.h
