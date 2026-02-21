add_subdirectory(IO)

set(DILLO_SOURCES
    dillo.cc
    version.cc
    version.hh
    paths.cc
    paths.hh
    tipwin.cc
    tipwin.hh
    ui.cc
    ui.hh
    uicmd.cc
    uicmd.hh
    bw.h
    bw.c
    cookies.c
    cookies.h
    actions.c
    actions.h
    hsts.c
    hsts.h
    auth.c
    auth.h
    md5.c
    md5.h
    digest.c
    digest.h
    colors.c
    colors.h
    binaryconst.h
    misc.c
    misc.h
    history.h
    history.c
    prefs.c
    prefs.h
    prefsparser.cc
    prefsparser.hh
    keys.cc
    keys.hh
    msg.h
    list.h
    url.c
    url.h
    bitvec.c
    bitvec.h
    klist.c
    klist.h
    chain.c
    chain.h
    utf8.cc
    utf8.hh
    timeout.cc
    timeout.hh
    dialog.cc
    dialog.hh
    web.cc
    web.hh
    nav.c
    nav.h
    cache.c
    cache.h
    decode.c
    decode.h
    dicache.c
    dicache.h
    capi.c
    capi.h
    domain.c
    domain.h
    css.cc
    css.hh
    cssparser.cc
    cssparser.hh
    doctree.hh
    styleengine.cc
    styleengine.hh
    plain.cc
    html.cc
    html.hh
    html_charrefs.h
    html_common.hh
    form.cc
    form.hh
    table.cc
    table.hh
    bookmark.c
    bookmark.h
    dns.c
    dns.h
    gif.c
    dgif.h
    jpeg.c
    djpeg.h
    png.c
    dpng.h
    webp.c
    dwebp.h
    svg.c
    nanosvg.h
    nanosvgrast.h
    dsvg.h
    imgbuf.cc
    imgbuf.hh
    image.cc
    image.hh
    menu.hh
    menu.cc
    dpiapi.c
    dpiapi.h
    pixmaps.h
    findbar.cc
    findbar.hh
    xembed.cc
    xembed.hh
)

set(COMMIT_H   "${CMAKE_BINARY_DIR}/commit.h")
set(COMMIT_TMP "${CMAKE_BINARY_DIR}/commit.tmp.h")

if(GIT_AVAILABLE)
    add_custom_command(
        OUTPUT  "${COMMIT_H}"
        COMMAND ${CMAKE_COMMAND} -E echo "Generating commit.tmp.h from git describe"
        COMMAND git --work-tree="${PROJECT_SOURCE_DIR}" describe --dirty > commit_ver.txt
        COMMAND ${CMAKE_COMMAND} -E echo "#define GIT_COMMIT `cat commit_ver.txt`" > "${COMMIT_TMP}"
        COMMAND ${CMAKE_COMMAND} -E remove -f commit_ver.txt

        COMMAND ${CMAKE_COMMAND} -E touch commit.h

        COMMAND ${CMAKE_COMMAND} -E compare_files "${COMMIT_TMP}" "${COMMIT_H}" 
            || ${CMAKE_COMMAND} -E copy "${COMMIT_TMP}" "${COMMIT_H}"
        COMMAND ${CMAKE_COMMAND} -E remove -f "${COMMIT_TMP}"

        DEPENDS "${PROJECT_SOURCE_DIR}/.git"
        COMMENT "Generating commit.h (from git describe)"
        VERBATIM
    )
else()
    add_custom_command(
        OUTPUT  "${COMMIT_H}"
        COMMAND ${CMAKE_COMMAND} -E echo "/* no git commit info */" > "${COMMIT_H}"
        COMMENT "Creating empty commit.h (no GIT)"
        VERBATIM
    )
endif()

list(APPEND DILLO_SOURCES "${COMMIT_H}")

add_executable(dillo
    ${DILLO_SOURCES}
)

target_compile_definitions(dillo PRIVATE
    DILLO_SYSCONF="${CMAKE_INSTALL_SYSCONFDIR}"
    DILLO_DOCDIR="${CMAKE_INSTALL_DOCDIR}"
    CUR_WORKING_DIR="${BASE_CUR_WORKING_DIR}/src"
    inbuf_t=char
)

if(ENABLE_THREADED_DNS)
    target_compile_definitions(dillo PRIVATE D_DNS_THREADED)
endif()

if(ENABLE_WEBP)
    target_include_directories(dillo PRIVATE ${WEBP_CFLAGS})
endif()

target_include_directories(dillo PRIVATE ${DILLO_FLTK_INCLUDE_DIR})

target_link_libraries(dillo PRIVATE
    Dlib
    Dpip
    Diof
    Dw-widgets
    Dw-fltk
    Dw-core
    lout
    ${DILLO_FLTK_LIBS}    # required
    ZLIB::ZLIB    # required
    Iconv::Iconv # required
    Threads::Threads # required
    ${DILLO_DEPS}
)

install(TARGETS dillo
    RUNTIME DESTINATION bin
)

install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/domainrc"
    "${CMAKE_CURRENT_SOURCE_DIR}/keysrc"
    "${CMAKE_CURRENT_SOURCE_DIR}/hsts_preload"
    DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}"
)

install(FILES chg srch DESTINATION share/dillo/src)
