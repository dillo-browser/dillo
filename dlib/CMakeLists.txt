# dlib library
set(DLIB_SOURCES
    dlib.h
    dlib.c
)

add_library(Dlib STATIC ${DLIB_SOURCES})
target_include_directories(Dlib PUBLIC ${CMAKE_SOURCE_DIR})
