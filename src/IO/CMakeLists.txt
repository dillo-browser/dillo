set(DIOF_SOURCES
    mime.c
    mime.h
    about.c
    Url.h
    http.c
    tls.h
    tls.c
    dpi.c
    IO.c
    iowatch.cc
    iowatch.hh
    IO.h
)

if (ENABLE_TLS)
    if(ENABLE_OPENSSL)
        list(APPEND DIOF_SOURCES
            tls_openssl.c
            tls_openssl.h
        )
    endif()

    if(ENABLE_MBEDTLS)
        list(APPEND DIOF_SOURCES
            tls_mbedtls.c
            tls_mbedtls.h
        )
    endif()
endif()

add_library(Diof STATIC
    ${DIOF_SOURCES}
)

target_include_directories(Diof
    PUBLIC
        ${PROJECT_SOURCE_DIR} ${CMAKE_BINARY_DIR}
)

target_compile_definitions(Diof
    PRIVATE
        DILLO_BINDIR="${CMAKE_INSTALL_BINDIR}/"
        CA_CERTS_FILE="${CA_CERTS_FILE}"
        CA_CERTS_DIR="${CA_CERTS_DIR}"
)

if(DEFINED FLTK_CFLAGS)
    target_compile_options(Diof PRIVATE ${FLTK_CFLAGS})
endif()

if(DEFINED FLTK_CXXFLAGS)
    target_compile_options(Diof PRIVATE ${FLTK_CXXFLAGS})
endif()

if (ENABLE_TLS)
    if (ENABLE_OPENSSL)
        target_link_libraries(Diof PUBLIC OpenSSL::SSL OpenSSL::Crypto)
    elseif(ENABLE_MBEDTLS)
        target_link_libraries(Diof PUBLIC ${MBEDTLS_LIB} ${MBEDX509_LIB} ${MBEDCRYPTO_LIB})
    endif()
endif()

# install(TARGETS Diof DESTINATION lib)