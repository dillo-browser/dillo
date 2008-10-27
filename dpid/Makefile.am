AM_CPPFLAGS=-DDPIDRC_SYS='"$(sysconfdir)/dpidrc"'

bin_PROGRAMS = dpid
dpid_LDADD = ../dpip/libDpip.a ../dlib/libDlib.a

EXTRA_DIST = dpidc dpidrc.in
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

sysconf_DATA = dpidrc
CLEANFILES = $(sysconf_DATA)

dpidrc: dpidrc.in
	sed -e 's|[@]libdir[@]|$(libdir)|' dpidrc.in > dpidrc

