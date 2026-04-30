# Main dillo browser executable
add_definitions(-DCUR_WORKING_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

# Generate commit.h if git is available
find_package(Git)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --dirty
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    set(GIT_COMMIT_DEFINE "#define GIT_COMMIT \"${GIT_COMMIT}\"")
else()
    set(GIT_COMMIT_DEFINE "#define GIT_COMMIT \"\"")
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/commit.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/commit.h"
    @ONLY
)

# Main dillo executable sources
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

# Add image format sources based on enabled features
if(ENABLE_GIF)
    list(APPEND DILLO_SOURCES
        gif.c
        dgif.h
    )
endif()

if(ENABLE_JPEG)
    list(APPEND DILLO_SOURCES
        jpeg.c
        djpeg.h
    )
endif()

if(ENABLE_PNG)
    list(APPEND DILLO_SOURCES
        png.c
        dpng.h
    )
endif()

if(ENABLE_WEBP AND WEBP_FOUND)
    list(APPEND DILLO_SOURCES
        webp.c
        dwebp.h
    )
endif()

if(ENABLE_SVG)
    list(APPEND DILLO_SOURCES
        svg.c
        nanosvg.h
        nanosvgrast.h
        dsvg.h
    )
endif()

# Create the main dillo executable
add_executable(dillo 
    ${DILLO_SOURCES}
    "${CMAKE_CURRENT_BINARY_DIR}/commit.h"
)

# Link libraries for dillo
set(DILLO_LIBS
    Dlib
    Dpip
    Diof
    Dw-core
    Dw-fltk
    Dw-widgets
    lout
    ${FLTK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${X11_LIBRARIES}
)

# Add image libraries
if(ENABLE_JPEG AND JPEG_FOUND)
    list(APPEND DILLO_LIBS ${JPEG_LIBRARIES})
endif()

if(ENABLE_PNG AND PNG_FOUND)
    list(APPEND DILLO_LIBS ${PNG_LIBRARIES})
endif()

if(ENABLE_WEBP AND WEBP_FOUND)
    list(APPEND DILLO_LIBS ${WEBP_LIBRARIES})
endif()

if(ZLIB_FOUND)
    list(APPEND DILLO_LIBS ${ZLIB_LIBRARIES})
endif()

if(ICONV_FOUND)
    list(APPEND DILLO_LIBS ${ICONV_LIBRARY})
endif()

if(OPENSSL_FOUND)
    list(APPEND DILLO_LIBS ${OPENSSL_LIBRARIES})
endif()

target_link_libraries(dillo ${DILLO_LIBS})
target_compile_options(dillo PRIVATE ${FLTK_CXXFLAGS})
target_include_directories(dillo PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
set_target_properties(dillo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Control socket client (optional)
if(ENABLE_CONTROL_SOCKET)
    add_executable(dilloc dilloc.c)
    target_link_libraries(dilloc Dlib)
    set_target_properties(dilloc PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()

# Install targets
install(TARGETS dillo DESTINATION ${CMAKE_INSTALL_BINDIR})
if(ENABLE_CONTROL_SOCKET)
    install(TARGETS dilloc DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# Install configuration files
install(FILES 
    domainrc 
    keysrc 
    hsts_preload
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/dillo
)
