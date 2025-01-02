target_include_directories(dillo INTERFACE
    ${PROJECT_SOURCE_DIR} ${CMAKE_BINARY_DIR}
)

add_executable(bookmarks_dpi
    bookmarks.c
    dpiutil.c
    dpiutil.h
)

set_target_properties(bookmarks_dpi PROPERTIES OUTPUT_NAME "bookmarks.dpi")

target_link_libraries(bookmarks_dpi PRIVATE
    Dpip
    Dlib
)

install(TARGETS bookmarks_dpi
    RUNTIME DESTINATION lib/dillo/dpi/bookmarks
)

add_executable(downloads_dpi
    downloads.cc
    dpiutil.c
    dpiutil.h
)
set_target_properties(downloads_dpi PROPERTIES OUTPUT_NAME "downloads.dpi")

target_include_directories(downloads_dpi PRIVATE ${DILLO_FLTK_INCLUDE_DIR})

target_link_libraries(downloads_dpi PRIVATE
    ${DILLO_FLTK_LIBS}
    Dpip
    Dlib
)

install(TARGETS downloads_dpi
    RUNTIME DESTINATION lib/dillo/dpi/downloads
)

add_executable(ftp_filter_dpi
    ftp.c
    dpiutil.c
    dpiutil.h
)
set_target_properties(ftp_filter_dpi PROPERTIES OUTPUT_NAME "ftp.filter.dpi")

target_link_libraries(ftp_filter_dpi PRIVATE
    Dpip
    Dlib
)

install(TARGETS ftp_filter_dpi
    RUNTIME DESTINATION lib/dillo/dpi/ftp
)

add_executable(hello_filter_dpi
    hello.c
    dpiutil.c
    dpiutil.h
)
set_target_properties(hello_filter_dpi PROPERTIES OUTPUT_NAME "hello.filter.dpi")

target_link_libraries(hello_filter_dpi PRIVATE
    Dpip
    Dlib
)

install(TARGETS hello_filter_dpi
    RUNTIME DESTINATION lib/dillo/dpi/hello
)

add_executable(vsource_filter_dpi
    vsource.c
    dpiutil.c
    dpiutil.h
)
set_target_properties(vsource_filter_dpi PROPERTIES OUTPUT_NAME "vsource.filter.dpi")

target_link_libraries(vsource_filter_dpi PRIVATE
    Dpip
    Dlib
)

install(TARGETS vsource_filter_dpi
    RUNTIME DESTINATION lib/dillo/dpi/vsource
)

add_executable(file_dpi
    file.c
    dpiutil.c
    dpiutil.h
)
set_target_properties(file_dpi PROPERTIES OUTPUT_NAME "file.dpi")

target_link_libraries(file_dpi PRIVATE
    Dpip
    Dlib
)

install(TARGETS file_dpi
    RUNTIME DESTINATION lib/dillo/dpi/file
)

add_executable(cookies_dpi
    cookies.c
    dpiutil.c
    dpiutil.h
)
set_target_properties(cookies_dpi PROPERTIES OUTPUT_NAME "cookies.dpi")

target_link_libraries(cookies_dpi PRIVATE
    Dpip
    Dlib
)

install(TARGETS cookies_dpi
    RUNTIME DESTINATION lib/dillo/dpi/cookies
)

add_executable(datauri_filter_dpi
    datauri.c
    dpiutil.c
    dpiutil.h
)
set_target_properties(datauri_filter_dpi PROPERTIES OUTPUT_NAME "datauri.filter.dpi")

target_link_libraries(datauri_filter_dpi PRIVATE
    Dpip
    Dlib
)

install(TARGETS datauri_filter_dpi
    RUNTIME DESTINATION lib/dillo/dpi/datauri
)