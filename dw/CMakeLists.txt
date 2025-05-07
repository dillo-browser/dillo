set(DILLO_DEFINES
    DILLO_LIBDIR="${CMAKE_INSTALL_LIBDIR}"
    CUR_WORKING_DIR="${BASE_CUR_WORKING_DIR}/dw"
)

add_library(Dw-core STATIC
    findtext.cc
    imgrenderer.cc
    iterator.cc
    layout.cc
    selection.cc
    stackingcontextmgr.cc
    style.cc
    tools.cc
    types.cc
    ui.cc
    view.hh
    widget.cc
    core.hh
    events.hh
    findtext.hh
    imgbuf.hh
    imgrenderer.hh
    iterator.hh
    layout.hh
    platform.hh
    selection.hh
    stackingcontextmgr.hh
    style.hh
    tools.hh
    types.hh
    ui.hh
    widget.hh
)
target_include_directories(Dw-core PRIVATE ${PROJECT_SOURCE_DIR} ${CMAKE_BINARY_DIR})
target_compile_definitions(Dw-core PRIVATE ${DILLO_DEFINES})
target_link_libraries(Dw-core PUBLIC lout)

add_library(Dw-fltk STATIC
    fltkcomplexbutton.cc
    fltkflatview.cc
    fltkimgbuf.cc
    fltkmisc.cc
    fltkplatform.cc
    fltkpreview.cc
    fltkui.cc
    fltkviewbase.cc
    fltkviewport.cc
    fltkcomplexbutton.hh
    fltkcore.hh
    fltkflatview.hh
    fltkimgbuf.hh
    fltkmisc.hh
    fltkplatform.hh
    fltkpreview.hh
    fltkui.hh
    fltkviewbase.hh
    fltkviewport.hh
)

target_include_directories(Dw-fltk PRIVATE ${PROJECT_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${DILLO_FLTK_INCLUDE_DIR})
target_link_libraries(Dw-fltk PUBLIC ${DILLO_FLTK_LIBS})
target_compile_definitions(Dw-fltk PRIVATE ${DILLO_DEFINES})

add_library(Dw-widgets STATIC
    alignedtablecell.cc
    alignedtextblock.cc
    bullet.cc
    hyphenator.cc
    image.cc
    listitem.cc
    oofawarewidget.cc
    oofawarewidget_iterator.cc
    ooffloatsmgr.cc
    oofposabslikemgr.cc
    oofposabsmgr.cc
    oofposfixedmgr.cc
    oofpositionedmgr.cc
    oofposrelmgr.cc
    outofflowmgr.cc
    regardingborder.cc
    ruler.cc
    simpletablecell.cc
    table.cc
    table_iterator.cc
    tablecell.cc
    textblock.cc
    textblock_iterator.cc
    textblock_linebreaking.cc
    alignedtablecell.hh
    alignedtextblock.hh
    bullet.hh
    hyphenator.hh
    image.hh
    listitem.hh
    oofawarewidget.hh
    ooffloatsmgr.hh
    oofposabslikemgr.hh
    oofposabsmgr.hh
    oofposfixedmgr.hh
    oofpositionedmgr.hh
    oofposrelmgr.hh
    outofflowmgr.hh
    regardingborder.hh
    ruler.hh
    simpletablecell.hh
    table.hh
    tablecell.hh
    textblock.hh
)

target_compile_definitions(Dw-widgets PRIVATE ${DILLO_DEFINES})
target_include_directories(Dw-widgets PRIVATE ${PROJECT_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${DILLO_FLTK_INCLUDE_DIR})
target_link_libraries(Dw-widgets PUBLIC ${DILLO_FLTK_LIBS})

# install(FILES preview.xbm DESTINATION share/dillo/...)

