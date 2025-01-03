add_library(lout STATIC
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

target_include_directories(lout
    PUBLIC
        ${PROJECT_SOURCE_DIR} ${CMAKE_BINARY_DIR}
)

target_compile_definitions(lout
    PRIVATE
        CUR_WORKING_DIR="${BASE_CUR_WORKING_DIR}/lout"
)