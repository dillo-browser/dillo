AM_CPPFLAGS=-DDPIDRC_SYS='"$(sysconfdir)/dpidrc"'

bin_PROGRAMS = dpid
dpid_LDADD = ../dpip/libDpip.a ../dlib/libDlib.a

EXTRA_DIST = dpidc
bin_SCRIPTS = dpidc

dpid_SOURCES = \
	dpi.h \
	dpi_service.h \
	dpi_socket_dir.h \
	dpid.h \
	dpid_common.h \
	misc_new.h \
	dpi.c \
	dpi_service.c \
	dpi_socket_dir.c \
	dpid.c \
	dpid_common.c \
	main.c \
	misc_new.c

install-data-local :
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)
	echo dpi_dir=$(libdir)/dillo/dpi > $(DESTDIR)$(sysconfdir)/dpidrc
