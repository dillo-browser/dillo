dist_doc_DATA = user_help.html
man_MANS = dillo.1
EXTRA_DIST = \
	index.doc \
	lout.doc \
	dw-map.doc \
	dw-overview.doc \
	dw-usage.doc \
	dw-layout-views.doc \
	dw-layout-widgets.doc \
	dw-widget-sizes.doc \
	dw-changes.doc \
	dw-images-and-backgrounds.doc \
	fltk-problems.doc \
	rounding-errors.doc \
	uml-legend.doc \
	dw-line-breaking.doc \
	dw-example-screenshot.png \
	dw-viewport-without-scrollbar.png \
	dw-viewport-with-scrollbar.png \
	dw-size-of-widget.png \
	dw-style-box-model.png \
	dw-style-length-absolute.png \
	dw-style-length-percentage.png \
	dw-style-length-relative.png \
	dw-textblock-collapsing-spaces-1-1.png \
	dw-textblock-collapsing-spaces-1-2.png \
	dw-textblock-collapsing-spaces-2-1.png \
	dw-textblock-collapsing-spaces-2-2.png \
	not-so-simple-container.png \
	Cache.txt \
	Cookies.txt \
	Dillo.txt \
	Dw.txt \
	HtmlParser.txt \
	IO.txt \
	Images.txt \
	Imgbuf.txt \
	NC_design.txt \
	Selection.txt \
	Dpid.txt \
	CCCwork.txt \
	README \
	dillo.1.in

dillo.1: $(srcdir)/dillo.1.in Makefile
	sed 's%/usr/local%${prefix}%g' < $(srcdir)/dillo.1.in > dillo.1
