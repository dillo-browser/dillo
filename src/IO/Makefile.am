AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DDILLO_BINDIR='"$(bindir)/"' \
	-DCA_CERTS_FILE='"@CA_CERTS_FILE@"' \
	-DCA_CERTS_DIR='"@CA_CERTS_DIR@"' \
	@LIBSSL_CPPFLAGS@

AM_CFLAGS = @LIBFLTK_CFLAGS@
AM_CXXFLAGS = @LIBFLTK_CXXFLAGS@

noinst_LIBRARIES = libDiof.a

if USE_OPENSSL
TLS_OPENSSL = tls_openssl.c tls_openssl.h
else
TLS_OPENSSL =
endif

if USE_MBEDTLS
TLS_MBEDTLS = tls_mbedtls.c tls_mbedtls.h
else
TLS_MBEDTLS =
endif


libDiof_a_SOURCES = \
	mime.c \
	mime.h \
	about.c \
	Url.h \
	http.c \
	tls.h \
	tls.c \
	$(TLS_OPENSSL) \
	$(TLS_MBEDTLS) \
	dpi.c \
	IO.c \
	iowatch.cc \
	iowatch.hh \
	IO.h
