# DPI (Dillo Plugin Interface) programs

# Common DPI utility sources
set(DPI_UTIL_SOURCES
    dpiutil.c
    dpiutil.h
)

# bookmarks.dpi
add_executable(bookmarks.dpi 
    bookmarks.c 
    ${DPI_UTIL_SOURCES}
)
target_link_libraries(bookmarks.dpi Dpip Dlib)
set_target_properties(bookmarks.dpi PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# downloads.dpi (C++)
add_executable(downloads.dpi 
    downloads.cc 
    ${DPI_UTIL_SOURCES}
)
target_link_libraries(downloads.dpi 
    Dpip 
    Dlib 
    ${FLTK_LIBRARIES}
)
target_compile_options(downloads.dpi PRIVATE ${FLTK_CXXFLAGS})
set_target_properties(downloads.dpi PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# ftp.filter.dpi
add_executable(ftp.filter.dpi 
    ftp.c 
    ${DPI_UTIL_SOURCES}
)
target_link_libraries(ftp.filter.dpi Dpip Dlib)
set_target_properties(ftp.filter.dpi PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# hello.filter.dpi
add_executable(hello.filter.dpi 
    hello.c 
    ${DPI_UTIL_SOURCES}
)
target_link_libraries(hello.filter.dpi Dpip Dlib)
set_target_properties(hello.filter.dpi PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# vsource.filter.dpi
add_executable(vsource.filter.dpi 
    vsource.c 
    ${DPI_UTIL_SOURCES}
)
target_link_libraries(vsource.filter.dpi Dpip Dlib)
set_target_properties(vsource.filter.dpi PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# file.dpi
add_executable(file.dpi 
    file.c 
    ${DPI_UTIL_SOURCES}
)
target_link_libraries(file.dpi Dpip Dlib)
set_target_properties(file.dpi PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# cookies.dpi
add_executable(cookies.dpi 
    cookies.c 
    ${DPI_UTIL_SOURCES}
)
target_link_libraries(cookies.dpi Dpip Dlib)
set_target_properties(cookies.dpi PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# datauri.filter.dpi
add_executable(datauri.filter.dpi 
    datauri.c 
    ${DPI_UTIL_SOURCES}
)
target_link_libraries(datauri.filter.dpi Dpip Dlib)
set_target_properties(datauri.filter.dpi PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Installation directories for DPI programs
set(DPI_LIBDIR "${CMAKE_INSTALL_FULL_LIBDIR}/dillo/dpi")

install(TARGETS bookmarks.dpi DESTINATION "${DPI_LIBDIR}/bookmarks")
install(TARGETS downloads.dpi DESTINATION "${DPI_LIBDIR}/downloads")
install(TARGETS ftp.filter.dpi DESTINATION "${DPI_LIBDIR}/ftp")
install(TARGETS hello.filter.dpi DESTINATION "${DPI_LIBDIR}/hello")
install(TARGETS vsource.filter.dpi DESTINATION "${DPI_LIBDIR}/vsource")
install(TARGETS file.dpi DESTINATION "${DPI_LIBDIR}/file")
install(TARGETS cookies.dpi DESTINATION "${DPI_LIBDIR}/cookies")
install(TARGETS datauri.filter.dpi DESTINATION "${DPI_LIBDIR}/datauri")
