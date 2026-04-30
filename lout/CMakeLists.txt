# lout library
add_definitions(-DCUR_WORKING_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

set(LOUT_SOURCES
    container.cc
    container.hh
    debug.hh
    debug_rtfl.hh
    identity.cc
    identity.hh
    misc.cc
    misc.hh
    object.cc
    object.hh
    signal.cc
    signal.hh
    unicode.cc
    unicode.hh
    msg.h
)

add_library(lout STATIC ${LOUT_SOURCES})
target_include_directories(lout PUBLIC ${CMAKE_SOURCE_DIR})
