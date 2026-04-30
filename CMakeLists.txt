cmake_minimum_required(VERSION 3.10)
project(dillo VERSION 3.3.0 LANGUAGES C CXX)

# CMake policy settings
cmake_policy(SET CMP0072 NEW)  # OpenGL preference
cmake_policy(SET CMP0009 NEW)  # FILE GLOB_RECURSE behavior

# Include GNUInstallDirs for standard installation directories
include(GNUInstallDirs)

# Set output directories to build directory root
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Multi-config generators support
foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG} CONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR})
endforeach()

# Set C and C++ standards
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configuration options
option(ENABLE_COOKIES "Enable cookies support" ON)
option(ENABLE_PNG "Enable PNG images support" ON)
option(ENABLE_WEBP "Enable WebP images support" ON)
option(ENABLE_JPEG "Enable JPEG images support" ON)
option(ENABLE_GIF "Enable GIF images support" ON)
option(ENABLE_SVG "Enable SVG images support" ON)
option(ENABLE_IPV6 "Enable IPv6 support" ON)
option(ENABLE_TLS "Enable TLS support" ON)
option(ENABLE_XEMBED "Enable X11 XEmbed support" ON)
option(ENABLE_CONTROL_SOCKET "Enable control socket" ON)

# Find required packages
find_package(PkgConfig)

# Find FLTK
find_package(FLTK REQUIRED)

# Find X11
find_package(X11 REQUIRED)

# Find threading
find_package(Threads REQUIRED)

# Find iconv
find_path(ICONV_INCLUDE_DIR iconv.h)
find_library(ICONV_LIBRARY iconv)
if(ICONV_INCLUDE_DIR AND ICONV_LIBRARY)
    set(ICONV_FOUND TRUE)
else()
    set(ICONV_FOUND FALSE)
endif()

# Find zlib
find_package(ZLIB)

# Find OpenSSL for TLS support
if(ENABLE_TLS)
    find_package(OpenSSL)
    if(OPENSSL_FOUND)
        set(HAVE_OPENSSL TRUE)
        add_definitions(-DHAVE_OPENSSL)
    endif()
endif()

# Find libpng
if(ENABLE_PNG)
    find_package(PNG)
    if(PNG_FOUND)
        add_definitions(-DENABLE_PNG)
    endif()
endif()

# Find libjpeg
if(ENABLE_JPEG)
    find_package(JPEG)
    if(JPEG_FOUND)
        add_definitions(-DENABLE_JPEG)
    endif()
endif()

# Find libwebp
if(ENABLE_WEBP)
    pkg_check_modules(WEBP libwebp)
    if(WEBP_FOUND)
        add_definitions(-DENABLE_WEBP)
    endif()
endif()

# Compiler flags
# Suppress unused parameter warnings - many function parameters are required for API compatibility
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter")

# Debug flags for Debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
endif()

# Release flags for Release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -DNDEBUG")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -DNDEBUG")
endif()

# Add FLTK flags
add_compile_options(${FLTK_CXXFLAGS})

# Configuration definitions
add_definitions(-DDILLO_SYSCONF="${CMAKE_INSTALL_FULL_SYSCONFDIR}/dillo/")
add_definitions(-DDILLO_DOCDIR="${CMAKE_INSTALL_FULL_DOCDIR}/")
add_definitions(-DDILLO_LIBDIR="${CMAKE_INSTALL_FULL_LIBDIR}/dillo/")

# Development mode: use build directory for DPI plugins
add_definitions(-DDPI_LIBDIR="${CMAKE_BINARY_DIR}")
add_definitions(-DDPIDRC_SYS="${CMAKE_BINARY_DIR}/dpidrc")

# Enable features based on options
if(ENABLE_COOKIES)
    add_definitions(-DENABLE_COOKIES)
endif()

if(ENABLE_GIF)
    add_definitions(-DENABLE_GIF)
endif()

if(ENABLE_SVG)
    add_definitions(-DENABLE_SVG)
endif()

if(ENABLE_IPV6)
    add_definitions(-DENABLE_IPV6)
endif()

if(ENABLE_XEMBED)
    add_definitions(-DENABLE_XEMBED)
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${ICONV_INCLUDE_DIR})

# Subdirectories
add_subdirectory(lout)
add_subdirectory(dw)
add_subdirectory(dlib)
add_subdirectory(dpip)
add_subdirectory(src/IO)
add_subdirectory(dpid)
add_subdirectory(dpi)
add_subdirectory(src)

# Copy configuration files to build directory for development
configure_file(${CMAKE_SOURCE_DIR}/dillorc ${CMAKE_BINARY_DIR}/dillorc COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/src/domainrc ${CMAKE_BINARY_DIR}/domainrc COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/src/keysrc ${CMAKE_BINARY_DIR}/keysrc COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/src/hsts_preload ${CMAKE_BINARY_DIR}/hsts_preload COPYONLY)

# Installation configuration
install(FILES dillorc DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/dillo)
install(FILES dillo.desktop DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)

# Install icons if they exist
if(EXISTS "${CMAKE_SOURCE_DIR}/icons/48x48/dillo.png")
    install(FILES icons/48x48/dillo.png 
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/48x48/apps/)
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/icons/128x128/dillo.png")
    install(FILES icons/128x128/dillo.png 
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/128x128/apps/)
endif()

# Install scripts
install(PROGRAMS dillo-install-hyphenation DESTINATION ${CMAKE_INSTALL_BINDIR})

# Add uninstall target
if(NOT TARGET uninstall)
    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake_uninstall.cmake.in"
        "${CMAKE_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE @ONLY
    )

    add_custom_target(uninstall
        COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_BINARY_DIR}/cmake_uninstall.cmake"
        COMMENT "Uninstalling Dillo..."
    )
endif()

# Print configuration summary
message(STATUS "Dillo Configuration Summary:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Cookies: ${ENABLE_COOKIES}")
message(STATUS "  PNG: ${ENABLE_PNG}")
message(STATUS "  JPEG: ${ENABLE_JPEG}")
message(STATUS "  GIF: ${ENABLE_GIF}")
message(STATUS "  SVG: ${ENABLE_SVG}")
message(STATUS "  WebP: ${ENABLE_WEBP}")
message(STATUS "  IPv6: ${ENABLE_IPV6}")
message(STATUS "  TLS: ${ENABLE_TLS}")
message(STATUS "  XEmbed: ${ENABLE_XEMBED}")
message(STATUS "  Control Socket: ${ENABLE_CONTROL_SOCKET}")
