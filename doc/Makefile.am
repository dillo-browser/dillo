doc_DATA = user_help.html
man_MANS = dillo.1
EXTRA_DIST = \
	README \
	Cookies.txt \
	dillo.1.in \
	user_help.in.html \
	install.md

dillo.1: $(srcdir)/dillo.1.in Makefile
	sed 's%/usr/local%${prefix}%g' < $(srcdir)/dillo.1.in > dillo.1

# Use .in.html instead of .html.in so it is recognized as HTML.
user_help.html: $(srcdir)/user_help.in.html Makefile
	sed 's/__VERSION__/${VERSION}/g' $(srcdir)/user_help.in.html > $@

DISTCLEANFILES = dillo.1 user_help.html
