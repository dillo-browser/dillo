dpi_link_libs = [dlib, dpip]

bookmarks_dpi_sources = files(
    'bookmarks.c',
    'dpiutil.c',
)

cookies_dpi_sources = files(
    'cookies.c',
    'dpiutil.c',
)

datauri_filter_dpi_sources = files(
    'datauri.c',
    'dpiutil.c',
)

downloads_dpi_sources = files(
    'downloads.cc',
    'dpiutil.c',
)

file_dpi_sources = files(
    'dpiutil.c',
    'file.c',
)

ftp_filter_dpi_sources = files(
    'dpiutil.c',
    'ftp.c',
)

hello_filter_dpi_sources = files(
    'dpiutil.c',
    'hello.c',
)

vsource_filter_dpi_sources = files(
    'dpiutil.c',
    'vsource.c',
)

dpis = {
    'bookmarks': bookmarks_dpi_sources,
    'cookies': cookies_dpi_sources,
    'datauri.filter': datauri_filter_dpi_sources,
    'downloads': downloads_dpi_sources,
    'file': file_dpi_sources,
    'ftp.filter': ftp_filter_dpi_sources,
    'hello.filter': hello_filter_dpi_sources,
    'vsource.filter': vsource_filter_dpi_sources,
}

# These are all so similar that it's trivial to iterate over
foreach dpi_name, dpi : dpis
    installdir = (
        get_option('libdir') / meson.project_name() / 'dpi' / dpi_name.split(
            '.',
        )[0]
    )
    executable(
        f'@dpi_name@.dpi',
        sources: dpi,
        dependencies: fltk,
        link_with: dpi_link_libs,
        install: true,
        install_dir: installdir,
        include_directories: incdir,
    )
endforeach
