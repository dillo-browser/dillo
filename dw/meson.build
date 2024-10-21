dwcore_sources = files(
    'findtext.cc',
    'imgrenderer.cc',
    'iterator.cc',
    'layout.cc',
    'selection.cc',
    'stackingcontextmgr.cc',
    'style.cc',
    'tools.cc',
    'types.cc',
    'ui.cc',
    'widget.cc',
)

dw_fltk_sources = files(
    'fltkcomplexbutton.cc',
    'fltkflatview.cc',
    'fltkimgbuf.cc',
    'fltkmisc.cc',
    'fltkplatform.cc',
    'fltkpreview.cc',
    'fltkui.cc',
    'fltkviewbase.cc',
    'fltkviewport.cc',
)

dw_widgets_sources = files(
    'alignedtablecell.cc',
    'alignedtextblock.cc',
    'bullet.cc',
    'hyphenator.cc',
    'image.cc',
    'listitem.cc',
    'oofawarewidget.cc',
    'oofawarewidget_iterator.cc',
    'ooffloatsmgr.cc',
    'oofposabslikemgr.cc',
    'oofposabsmgr.cc',
    'oofposfixedmgr.cc',
    'oofpositionedmgr.cc',
    'oofposrelmgr.cc',
    'outofflowmgr.cc',
    'regardingborder.cc',
    'ruler.cc',
    'simpletablecell.cc',
    'table.cc',
    'table_iterator.cc',
    'tablecell.cc',
    'textblock.cc',
    'textblock_iterator.cc',
    'textblock_linebreaking.cc',
)

dwcore = static_library(
    'dwcore',
    sources: dwcore_sources,
    include_directories: incdir,
)

dw_fltk = static_library(
    'dw_fltk',
    sources: dw_fltk_sources,
    dependencies: fltk,
    include_directories: incdir,
)

dw_widgets = static_library(
    'dw_widgets',
    sources: dw_widgets_sources,
    include_directories: incdir,
    link_with: liblout,
)

dillo_libs += [dwcore, dw_fltk, dw_widgets]
