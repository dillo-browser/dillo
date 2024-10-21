dpid_sources = files(
    'dpi.c',
    'dpi_socket_dir.c',
    'dpid.c',
    'dpid_common.c',
    'main.c',
    'misc_new.c',
)

link_libs = [
    dlib,
    dpip,
]

executable(
    'dpid',
    sources: dpid_sources,
    include_directories: incdir,
    link_with: link_libs,
    install: true,
)

executable(
    'dpidc',
    sources: 'dpidc.c',
    include_directories: incdir,
    link_with: link_libs,
    install: true,
)

sed = find_program('sed', required: true)
dpid_libdir = get_option('libdir') # TODO: Can we reuse this?
configure_file(
    input: 'dpidrc.in',
    output: 'dpidrc',
    command: [
        sed,
        '-e', f's|[@]libdir[@]|@dpid_libdir@|;s|[@]EXEEXT[@]|@exe_suffix@|g',
        '@INPUT@',
    ],
    capture: true,
    install_dir: get_option('sysconfdir'),
    install: true,
    install_tag: 'runtime',
)
