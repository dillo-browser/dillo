# dpip library
set(DPIP_SOURCES
    dpip.h
    dpip.c
)

add_library(Dpip STATIC ${DPIP_SOURCES})
target_include_directories(Dpip PUBLIC ${CMAKE_SOURCE_DIR})
