add_library(Dlib STATIC
    dlib.c
    dlib.h
)

target_include_directories(Dlib
    PUBLIC
        ${PROJECT_SOURCE_DIR} ${CMAKE_BINARY_DIR}
)