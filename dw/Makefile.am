AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DDILLO_LIBDIR='"$(pkglibdir)/"'


noinst_LIBRARIES = \
	libDw-core.a \
	libDw-fltk.a \
	libDw-widgets.a

libDw_core_a_SOURCES = \
	core.hh \
	events.hh \
	findtext.cc \
	findtext.hh \
	imgbuf.hh \
	imgrenderer.hh \
	imgrenderer.cc \
	iterator.cc \
	iterator.hh \
	layout.cc \
	layout.hh \
	platform.hh \
	selection.hh \
	selection.cc \
	style.cc \
	style.hh \
	types.cc \
	types.hh \
	ui.cc \
	ui.hh \
	view.hh \
	widget.cc \
	widget.hh

libDw_fltk_a_SOURCES = \
	fltkcomplexbutton.cc \
	fltkcomplexbutton.hh \
	fltkcore.hh \
	fltkflatview.cc \
	fltkflatview.hh \
	fltkimgbuf.cc \
	fltkimgbuf.hh \
	fltkmisc.cc \
	fltkmisc.hh \
	fltkplatform.cc \
	fltkplatform.hh \
	fltkpreview.hh \
	fltkpreview.cc \
	fltkui.cc \
	fltkui.hh \
	fltkviewbase.cc \
	fltkviewbase.hh \
	fltkviewport.cc \
	fltkviewport.hh

libDw_fltk_a_CXXFLAGS = @LIBFLTK_CXXFLAGS@

libDw_widgets_a_SOURCES = \
	alignedtextblock.cc \
	alignedtextblock.hh \
	bullet.cc \
	bullet.hh \
	hyphenator.cc \
	hyphenator.hh \
	image.cc \
	image.hh \
	listitem.cc \
	listitem.hh \
	ruler.cc \
	ruler.hh \
	table.cc \
	table.hh \
	tablecell.cc \
	tablecell.hh \
	textblock.cc \
	textblock_iterator.cc \
	textblock_linebreaking.cc \
	textblock.hh

EXTRA_DIST = preview.xbm
