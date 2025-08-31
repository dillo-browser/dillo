AM_TESTS_ENVIRONMENT = env \
	TOP_BUILDDIR='$(top_builddir)' \
	TOP_SRCDIR='$(top_srcdir)' \
	BUILDDIR='$(builddir)'

LOG_COMPILER = $(srcdir)/driver.sh

EXTRA_DIST = \
	driver.sh \
	leakfilter.awk \
	manual \
	render

TESTS = \
	render/b-div.html \
	render/div-100-percent-with-padding.html \
	render/float-img-justify.html \
	render/form-display-none.html \
	render/github-infinite-loop.html \
	render/hackernews.html \
	render/hr.html \
	render/img-aspect-ratio-absolute.html \
	render/img-aspect-ratio-div.html \
	render/img-aspect-ratio-mix-border.html \
	render/img-aspect-ratio-mix-margin.html \
	render/img-aspect-ratio-mix.html \
	render/img-aspect-ratio-relative-inside-fixed.html \
	render/img-aspect-ratio.html \
	render/margin-rounding.html \
	render/main-style.html \
	render/margin-auto.html \
	render/max-width-body.html \
	render/max-width-div-clamp.html \
	render/max-width-div.html \
	render/max-width-html.html \
	render/max-width-nested-div.html \
	render/meta-refresh-0-no-url.html \
	render/min-width-body.html \
	render/min-width-div-extend.html \
	render/min-width-div.html \
	render/min-width-html.html \
	render/min-width-nested-div.html \
	render/multiple-floats.html \
	render/span-padding.html \
	render/svg-current-color.html \
	render/table-max-width.html \
	render/table-missing-width-in-one-column.html \
	render/table-td-width-percent-img.html \
	render/table-td-width-percent.html \
	render/table-thead-tfoot.html \
	render/table-thead-tfoot-open-tag.html \
	render/text-align-center.html \
	render/white-space.html

# To be fixed
XFAIL_TESTS = \
	render/div-100-percent-with-padding.html \
	render/float-img-justify.html \
	render/margin-auto.html \
	render/margin-rounding.html \
	render/max-width-html.html \
	render/min-width-html.html \
	render/multiple-floats.html \
	render/span-padding.html \
	render/table-td-width-percent.html
