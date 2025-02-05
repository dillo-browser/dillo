libdio_sources = files(
    'IO.c',
    'Url.h',
    'about.c',
    'dpi.c',
    'http.c',
    'iowatch.cc',
    'mime.c',
    'tls.c',
)

# TODO: What's supposed to happen here if tls isn't enabled? What does tls.c do then?
if get_option('tls').enabled()
    if get_option('mbedtls')
        libdio_sources += files('tls_mbedtls.c')
    elif get_option('openssl')
        libdio_sources += files('tls_openssl.c')
    endif
endif

libdio = static_library(
    'libdio',
    sources: libdio_sources,
    dependencies: fltk,
    include_directories: incdir,
    link_with: dpip,
)

dillo_libs += libdio
