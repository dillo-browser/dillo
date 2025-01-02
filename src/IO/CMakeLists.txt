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
        ${PROJECT_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${DILLO_FLTK_INCLUDE_DIR}
)

target_link_libraries(Diof PRIVATE ${DILLO_FLTK_LIBS})

target_compile_definitions(Diof
    PRIVATE
        DILLO_BINDIR="${CMAKE_INSTALL_BINDIR}/"
        CA_CERTS_FILE="${CA_CERTS_FILE}"
        CA_CERTS_DIR="${CA_CERTS_DIR}"
)

if (ENABLE_TLS)
    target_link_libraries(Diof PUBLIC ${SSL_LIBS})
endif()



# install(TARGETS Diof DESTINATION lib)