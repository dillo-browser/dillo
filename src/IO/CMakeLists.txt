# IO library
add_definitions(-DDILLO_BINDIR="${CMAKE_INSTALL_BINDIR}/")

# Define CA certificates paths
if(NOT DEFINED CA_CERTS_FILE)
    set(CA_CERTS_FILE "/etc/ssl/certs/ca-certificates.crt")
endif()
if(NOT DEFINED CA_CERTS_DIR)
    set(CA_CERTS_DIR "/etc/ssl/certs/")
endif()

add_definitions(-DCA_CERTS_FILE="${CA_CERTS_FILE}")
add_definitions(-DCA_CERTS_DIR="${CA_CERTS_DIR}")

# Check for TLS implementation
if(OPENSSL_FOUND)
    set(TLS_SOURCES
        tls_openssl.c
        tls_openssl.h
    )
    add_definitions(-DUSE_OPENSSL)
elseif(HAVE_MBEDTLS)
    set(TLS_SOURCES
        tls_mbedtls.c
        tls_mbedtls.h
    )
    add_definitions(-DUSE_MBEDTLS)
else()
    set(TLS_SOURCES)
endif()

set(IO_SOURCES
    mime.c
    mime.h
    about.c
    control.c
    control.h
    Url.h
    http.c
    tls.h
    tls.c
    ${TLS_SOURCES}
    dpi.c
    IO.c
    iowatch.cc
    iowatch.hh
    IO.h
)

add_library(Diof STATIC ${IO_SOURCES})
target_include_directories(Diof PUBLIC 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

if(OPENSSL_FOUND)
    target_include_directories(Diof PRIVATE ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(Diof ${OPENSSL_LIBRARIES})
endif()

target_compile_options(Diof PRIVATE ${FLTK_CXXFLAGS})
