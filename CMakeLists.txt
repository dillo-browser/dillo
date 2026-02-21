cmake_minimum_required(VERSION 3.15)

project(dillo VERSION 3.1.1
        LANGUAGES C CXX)

option(ENABLE_FLTK_1_4 "Enable FLTK 1.4" OFF)
option(ENABLE_JPEG  "Enable JPEG support"  ON)
option(ENABLE_PNG   "Enable PNG support"   ON)
option(ENABLE_WEBP  "Enable WebP support"  ON)
option(ENABLE_GIF   "Enable GIF support"   ON)
option(ENABLE_SVG   "Enable SVG support"   ON)
option(ENABLE_TLS   "Enable TLS (HTTPS)"   ON)
option(ENABLE_OPENSSL   "Use OpenSSL if available" ON)
option(ENABLE_MBEDTLS   "Use mbedTLS if OpenSSL unavailable/disabled" ON)
option(ENABLE_IPV6  "Enable IPv6 support"  OFF)
option(ENABLE_EFENCE "Enable ElectricFence" OFF)
option(ENABLE_GPROF "Enable gprof profiling" OFF)
option(ENABLE_INSURE "Enable Insure++"      OFF)
option(ENABLE_COOKIES  "Enable cookie support" ON)
option(ENABLE_THREADED_DNS "Enable threaded DNS" ON)
option(ENABLE_RTFL  "Enable RTFL debugging messages" OFF)
option(ENABLE_XEMBED "Enable XEmbed" ON) 
option(ENABLE_HTML_TESTS "Enable HTML tests" OFF)
option(DILLO_BUILD_TESTS "Build Dillo tests" OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CA_CERTS_FILE "" CACHE STRING
    "Path to a bundle of trusted CA certificates (equivalent to --with-ca-certs-file)")

set(CA_CERTS_DIR "" CACHE STRING
    "Path to a directory containing trusted CA certificates (equivalent to --with-ca-certs-dir)")

set(DILLO_COMPILE_DEFINITIONS "")
set(BASE_CUR_WORKING_DIR ${CMAKE_SOURCE_DIR})

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
include(CheckIncludeFile)
include(GNUInstallDirs)

set(CMAKE_INSTALL_SYSCONFDIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_SYSCONFDIR}/dillo/")

check_include_file("stdint.h" HAVE_STDINT_H)
check_include_file("stdint.h" HAVE_INTTYPES_H)

if(NOT ENABLE_COOKIES)
    list(APPEND DILLO_COMPILE_DEFINITIONS DISABLE_COOKIES)
endif()

if(ENABLE_IPV6)
    list(APPEND DILLO_COMPILE_DEFINITIONS ENABLE_IPV6)
endif()

if(ENABLE_EFENCE)
    set(EFENCE_LIB "efence")
endif()

if(ENABLE_GPROF)
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -pg")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
endif()

if(ENABLE_INSURE)
    message(WARNING "Insure++ usage is not trivial in CMake. Consider a custom toolchain.")
endif()

# Uncomment if threaded DNS is needed
# if(ENABLE_THREADED_DNS)
#     list(APPEND DILLO_COMPILE_DEFINITIONS D_DNS_THREADED)
# endif()

if(ENABLE_RTFL)
    list(APPEND DILLO_COMPILE_DEFINITIONS DBG_RTFL)
endif()

if(NOT ENABLE_XEMBED)
    list(APPEND DILLO_COMPILE_DEFINITIONS DISABLE_XEMBED)
endif()

set(DILLO_DEPS)
set(DILLO_FLTK_LIBS)
set(DILLO_FLTK_INCLUDE_DIR)

if (ENABLE_FLTK_1_4)
    find_package(FLTK 1.4 CONFIG)
    if(NOT FLTK_FOUND)
        set(FLTK_BUILD_TEST OFF CACHE BOOL " " FORCE)
        set(FLTK_BUILD_FLUID OFF CACHE BOOL " " FORCE)
        set(FLTK_BUILD_FLTK_OPTIONS OFF CACHE BOOL " " FORCE)
        include(FetchContent)
        FetchContent_Declare( 
            FLTK
            GIT_REPOSITORY https://github.com/FLTK/FLTK
            GIT_SHALLOW    TRUE
            OVERRIDE_FIND_PACKAGE
        )
        FetchContent_MakeAvailable(FLTK)
        find_package(FLTK 1.4 CONFIG REQUIRED)
    endif()
    message(WARNING "Dillo will attempt to link against FLTK 1.4. This might cause rendering issues!")
    list(APPEND DILLO_FLTK_LIBS fltk::fltk)
else()
    find_package(FLTK REQUIRED) # 1.3
    list(APPEND DILLO_FLTK_INCLUDE_DIR ${FLTK_INCLUDE_DIR})
    list(APPEND DILLO_FLTK_LIBS fltk)
    if (APPLE)
        list(APPEND DILLO_FLTK_LIBS "-framework Cocoa")
    elseif(WIN32)
        list(APPEND DILLO_FLTK_LIBS ws2_32 comctl32 gdi32 gdiplus oleaut32 ole32 uuid shell32 advapi32 comdlg32 winspool user32 kernel32 odbc32)
    else()
        list(APPEND DILLO_FLTK_LIBS X11)
    endif()
endif()


find_package(ZLIB REQUIRED)

if(ENABLE_PNG)
    find_package(PNG)
    if(PNG_FOUND)
        list(APPEND DILLO_COMPILE_DEFINITIONS ENABLE_PNG)
        list(APPEND DILLO_DEPS PNG::PNG)
    else()
        message(WARNING "libpng not found, disabling PNG support.")
        set(ENABLE_PNG OFF)
    endif()
endif()

if(ENABLE_JPEG)
    find_package(JPEG)
    if(JPEG_FOUND)
        list(APPEND DILLO_COMPILE_DEFINITIONS ENABLE_JPEG)
        list(APPEND DILLO_DEPS JPEG::JPEG)
    else()
        message(WARNING "libjpeg not found, disabling JPEG support.")
        set(ENABLE_JPEG OFF)
    endif()
endif()

set(WEBP_CFLAGS)
set(WEBP_LIBS)

if(ENABLE_WEBP)
    # find_package(WebP QUIET)
    # if(WebP_FOUND)
    #     list(APPEND DILLO_COMPILE_DEFINITIONS ENABLE_WEBP)
    #     list(APPEND WEBP_CFLAGS ${WebP_INCLUDE_DIRS})
    #     list(APPEND WEBP_LIBS ${WebP_LIBRARIES})
    # else()
    find_package(PkgConfig)
    if (PkgConfig_FOUND)
        pkg_search_module(LIBWEBP QUIET libwebp)
        if (LIBWEBP_FOUND)
            list(APPEND DILLO_COMPILE_DEFINITIONS ENABLE_WEBP)
            list(APPEND WEBP_CFLAGS ${LIBWEBP_INCLUDE_DIRS})
            list(APPEND WEBP_LIBS ${LIBWEBP_LIBRARIES})
            list(APPEND DILLO_DEPS ${WEBP_LIBS})
        else()
            message(WARNING "libwebp not found")
            set(ENABLE_WEBP OFF)
        endif()
    endif()
endif()

if(ENABLE_GIF)
    list(APPEND DILLO_COMPILE_DEFINITIONS ENABLE_GIF)
endif()

if(ENABLE_SVG)
    list(APPEND DILLO_COMPILE_DEFINITIONS ENABLE_SVG)
endif()

set(TLS_OK OFF)
set(TLS_IMPL "none")
set(SSL_LIBS)

if(ENABLE_TLS)
    if(ENABLE_OPENSSL)
        find_package(OpenSSL QUIET)
        if(OPENSSL_FOUND)
            message(STATUS "Using OpenSSL as TLS library.")
            set(TLS_OK ON)
            set(TLS_IMPL "OpenSSL")
            set(ENABLE_MBEDTLS OFF)
            list(APPEND DILLO_COMPILE_DEFINITIONS ENABLE_TLS HAVE_OPENSSL)
            set(SSL_LIBS OpenSSL::SSL OpenSSL::Crypto)
        else()
            message(STATUS "Cannot find OpenSSL, trying mbedTLS.")
        endif()
    else()
        message(STATUS "Skipping OpenSSL search, as it is disabled.")
    endif()

    if(NOT TLS_OK AND ENABLE_MBEDTLS)
        find_path(MBEDTLS_INCLUDE_DIR mbedtls/ssl.h)
        find_library(MBEDTLS_LIB mbedtls)
        find_library(MBEDX509_LIB mbedx509)
        find_library(MBEDCRYPTO_LIB mbedcrypto)

        if(MBEDTLS_INCLUDE_DIR AND MBEDTLS_LIB AND MBEDX509_LIB AND MBEDCRYPTO_LIB)
            message(STATUS "Using mbedTLS as TLS library.")
            set(TLS_OK ON)
            set(TLS_IMPL "mbedTLS")
            list(APPEND DILLO_COMPILE_DEFINITIONS ENABLE_TLS HAVE_MBEDTLS)
            set(SSL_LIBS ${MBEDTLS_LIB} ${MBEDX509_LIB} ${MBEDCRYPTO_LIB})
        else()
            message(STATUS "Cannot find mbedTLS.")
        endif()
    else()
        message(STATUS "Skipping mbedTLS search, as it is disabled.")
    endif()

    if(NOT TLS_OK)
        message(FATAL_ERROR "No TLS library available")
    endif()

    list(APPEND DILLO_DEPS ${SSL_LIBS})
endif()

find_package(Iconv REQUIRED)
find_package(Threads REQUIRED)

set(DILLO_DEFINES_CONTENT "")
foreach(def IN LISTS DILLO_COMPILE_DEFINITIONS)
    set(DILLO_DEFINES_CONTENT "${DILLO_DEFINES_CONTENT}\n#define ${def}")
endforeach()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wall -W -Wno-unused-parameter -pedantic -std=c99 -D_POSIX_C_SOURCE=200112L -D_REENTRANT -D_THREAD_SAFE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -W -Wno-unused-parameter -fno-rtti -fno-exceptions -pedantic -std=c++11 -D_POSIX_C_SOURCE=200112L")
endif()

string(REGEX REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL}")
string(REGEX REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
string(REGEX REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
string(REGEX REPLACE "-DNDEBUG" "" CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL}")
string(REGEX REPLACE "-DNDEBUG" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
string(REGEX REPLACE "-DNDEBUG" "" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/config.h"
    @ONLY
)

message(STATUS "")
message(STATUS "==== Configuration summary ====")
message(STATUS "  C Compiler     : ${CMAKE_C_COMPILER}")
message(STATUS "  C Flags        : ${CMAKE_C_FLAGS}")
message(STATUS "  CXX Compiler   : ${CMAKE_CXX_COMPILER}")
message(STATUS "  CXX Flags      : ${CMAKE_CXX_FLAGS}")
message(STATUS "")
message(STATUS "  TLS enabled    : ${ENABLE_TLS}")
message(STATUS "  TLS library    : ${TLS_IMPL}")
if(TLS_OK)
    if(TLS_IMPL STREQUAL "OpenSSL")
        message(STATUS "  TLS flags      : ${OpenSSL_LIBRARIES}")
    elseif(TLS_IMPL STREQUAL "mbedTLS")
        message(STATUS "  TLS flags      : ${MBEDTLS_LIB} ${MBEDX509_LIB} ${MBEDCRYPTO_LIB}")
    endif()
endif()
message(STATUS "")
message(STATUS "  Cookies        : ${ENABLE_COOKIES}")
message(STATUS "  XEmbed         : ${ENABLE_XEMBED}")
message(STATUS "  RTFL           : ${ENABLE_RTFL}")
message(STATUS "  JPEG           : ${ENABLE_JPEG}")
message(STATUS "  PNG            : ${ENABLE_PNG}")
message(STATUS "  GIF            : ${ENABLE_GIF}")
message(STATUS "  SVG            : ${ENABLE_SVG}")
message(STATUS "  WEBP           : ${ENABLE_WEBP}")
message(STATUS "")
message(STATUS "  HTML tests     : ${ENABLE_HTML_TESTS}")
message(STATUS "====================================")
message(STATUS "")

add_subdirectory(lout)
add_subdirectory(dw)
add_subdirectory(dlib)
add_subdirectory(dpip)
add_subdirectory(dpid)
add_subdirectory(src)
add_subdirectory(dpi)
add_subdirectory(doc)

if (DILLO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
    if(ENABLE_HTML_TESTS)
        find_program(XVFB_EXECUTABLE xvfb-run)
        find_program(XWD_EXECUTABLE xwd)
        find_program(XWININFO_EXECUTABLE xwininfo)
        find_program(CONVERT_EXECUTABLE convert)
        if(NOT (XVFB_EXECUTABLE AND XWD_EXECUTABLE AND XWININFO_EXECUTABLE AND CONVERT_EXECUTABLE))
            message(FATAL_ERROR "Cannot find all tools to enable HTML tests!")
        endif()
    endif()
endif()

include(CPack)

install(PROGRAMS "${CMAKE_SOURCE_DIR}/dillo-install-hyphenation"
        DESTINATION "${CMAKE_INSTALL_BINDIR}")

install(FILES "${CMAKE_SOURCE_DIR}/dillorc"
        DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}")

install(FILES "${CMAKE_SOURCE_DIR}/dillo.desktop"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")

install(FILES "${CMAKE_SOURCE_DIR}/icons/48x48/dillo.png"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/48x48/apps")

install(FILES "${CMAKE_SOURCE_DIR}/icons/128x128/dillo.png"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/128x128/apps")

set(CPACK_SOURCE_FILES
    "${CMAKE_SOURCE_DIR}/README.md"
    "${CMAKE_SOURCE_DIR}/Doxyfile"
    "${CMAKE_SOURCE_DIR}/dillorc"
    "${CMAKE_SOURCE_DIR}/install-dpi-local"
    "${CMAKE_SOURCE_DIR}/d_size.h"
    "${CMAKE_SOURCE_DIR}/dillo-install-hyphenation"
    "${CMAKE_SOURCE_DIR}/desktop_DATA/dillo.desktop"
    "${CMAKE_SOURCE_DIR}/appicon48_DATA/icons/48x48/dillo.png"
    "${CMAKE_SOURCE_DIR}/appicon128_DATA/icons/128x128/dillo.png"
)

set(CPACK_SOURCE_IGNORE_FILES
    "/\\.git/"
    "/build/"
    "CMakeFiles/"
    "CMakeCache.txt"
    "cmake_install.cmake"
    "Makefile"
)

set(CPACK_SOURCE_INCLUDE_TOPLEVEL_DIRECTORY OFF)

set(CPACK_GENERATOR "TGZ;ZIP")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Dillo Web Browser")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")

foreach(file IN LISTS CPACK_SOURCE_FILES)
    list(APPEND CPACK_SOURCE_FILES_LIST "${file}")
endforeach()

set(CPACK_SOURCE_FILES "${CPACK_SOURCE_FILES_LIST}")

