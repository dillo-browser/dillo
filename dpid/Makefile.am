AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DDPIDRC_SYS='"$(sysconfdir)/dpidrc"'

bin_PROGRAMS = dpid dpidc
dpid_LDADD = \
	$(top_builddir)/dpip/libDpip.a \
	$(top_builddir)/dlib/libDlib.a
dpidc_LDADD = \
	$(top_builddir)/dpip/libDpip.a \
	$(top_builddir)/dlib/libDlib.a

EXTRA_DIST = dpidrc.in

dpid_SOURCES = \
	dpi.h \
	dpi_socket_dir.h \
	dpid.h \
	dpid_common.h \
	misc_new.h \
	dpi.c \
	dpi_socket_dir.c \
	dpid.c \
	dpid_common.c \
	main.c \
	misc_new.c

dpidc_SOURCES = dpidc.c

sysconf_DATA = dpidrc
CLEANFILES = $(sysconf_DATA)

dpidrc: $(srcdir)/dpidrc.in Makefile
	sed -e 's|[@]libdir[@]|$(libdir)|' $(srcdir)/dpidrc.in > dpidrc

