AM_TESTS_ENVIRONMENT = env \
	TOP_BUILDDIR='$(top_builddir)' \
	TOP_SRCDIR='$(top_srcdir)' \
	BUILDDIR='$(builddir)'

LOG_COMPILER = $(srcdir)/driver.sh

EXTRA_DIST = \
	driver.sh \
	manual \
	render

TESTS = \
	render/b-div.html \
	render/div-100-percent-with-padding.html \
	render/float-img-justify.html \
	render/hackernews.html \
	render/img-aspect-ratio.html \
	render/main-style.html \
	render/margin-auto.html \
	render/max-width-body.html \
	render/max-width-div.html \
	render/max-width-div-clamp.html \
	render/max-width-html.html \
	render/max-width-nested-div.html \
	render/min-width-body.html \
	render/min-width-div-extend.html \
	render/min-width-div.html \
	render/min-width-html.html \
	render/min-width-nested-div.html \
	render/span-padding.html \
	render/table-max-width.html \
	render/table-missing-width-in-one-column.html \
	render/table-td-width-percent-img.html \
	render/table-td-width-percent.html \
	render/table-thead-tfoot.html \
	render/white-space.html

# To be fixed
XFAIL_TESTS = \
	render/div-100-percent-with-padding.html \
	render/float-img-justify.html \
	render/img-aspect-ratio.html \
	render/margin-auto.html \
	render/max-width-html.html \
	render/min-width-html.html \
	render/span-padding.html \
	render/table-td-width-percent.html
