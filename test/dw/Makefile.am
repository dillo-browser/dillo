AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DCUR_WORKING_DIR='"@BASE_CUR_WORKING_DIR@/test"'

AM_CFLAGS = @LIBFLTK_CFLAGS@
AM_CXXFLAGS = @LIBFLTK_CXXFLAGS@

LDADD = \
	$(top_builddir)/dw/libDw-widgets.a \
	$(top_builddir)/dw/libDw-fltk.a \
	$(top_builddir)/dw/libDw-core.a \
	$(top_builddir)/lout/liblout.a \
	$(top_builddir)/dlib/libDlib.a \
	@LIBFLTK_LIBS@ @LIBX11_LIBS@

check_PROGRAMS = \
	dw-anchors-test \
	dw-border-test \
	dw-example \
	dw-find-test \
	dw-float-test \
	dw-image-background \
	dw-images-scaled \
	dw-images-scaled2 \
	dw-images-simple \
	dw-imgbuf-mem-test \
	dw-links \
	dw-links2 \
	dw-lists \
	dw-resource-test \
	dw-simple-container-test \
	dw-table \
	dw-table-aligned \
	dw-ui-test

# Don't run most tests yet, as they require graphical display and human
# intervention.
TESTS = \
	dw-imgbuf-mem-test

dw_anchors_test_SOURCES = dw_anchors_test.cc
dw_border_test_SOURCES = dw_border_test.cc
dw_example_SOURCES = dw_example.cc
dw_find_test_SOURCES = dw_find_test.cc
dw_float_test_SOURCES = dw_float_test.cc
dw_links_SOURCES = dw_links.cc
dw_links2_SOURCES = dw_links2.cc
dw_image_background_SOURCES = dw_image_background.cc
dw_images_simple_SOURCES = dw_images_simple.cc
dw_images_scaled_SOURCES = dw_images_scaled.cc
dw_images_scaled2_SOURCES = dw_images_scaled2.cc
dw_lists_SOURCES = dw_lists.cc
dw_simple_container_test_SOURCES = \
	dw_simple_container.hh \
	dw_simple_container.cc \
	dw_simple_container_test.cc
dw_table_aligned_SOURCES = dw_table_aligned.cc
dw_table_SOURCES = dw_table.cc
dw_imgbuf_mem_test_SOURCES = dw_imgbuf_mem_test.cc
dw_resource_test_SOURCES = dw_resource_test.cc
dw_ui_test_SOURCES = \
	dw_ui_test.cc \
	form.cc \
	form.hh
