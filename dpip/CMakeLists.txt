add_library(Dpip STATIC
    dpip.c
    dpip.h
)

target_include_directories(Dpip
    PUBLIC
        ${PROJECT_SOURCE_DIR} ${CMAKE_BINARY_DIR}
)