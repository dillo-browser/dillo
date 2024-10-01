subdir('IO')

dillo_files = files(
    'auth.c',
    'actions.c',
    'bitvec.c',
    'bookmark.c',
    'bw.c',
    'cache.c',
    'capi.c',
    'chain.c',
    'colors.c',
    'cookies.c',
    'css.cc',
    'cssparser.cc',
    'decode.c',
    'dialog.cc',
    'dicache.c',
    'digest.c',
    'dillo.cc',
    'dns.c',
    'domain.c',
    'dpiapi.c',
    'findbar.cc',
    'form.cc',
    'gif.c',
    'history.c',
    'hsts.c',
    'html.cc',
    'image.cc',
    'imgbuf.cc',
    'jpeg.c',
    'keys.cc',
    'klist.c',
    'md5.c',
    'menu.cc',
    'misc.c',
    'nav.c',
    'paths.cc',
    'plain.cc',
    'png.c',
    'prefs.c',
    'prefsparser.cc',
    'styleengine.cc',
    'svg.c',
    'table.cc',
    'timeout.cc',
    'tipwin.cc',
    'ui.cc',
    'uicmd.cc',
    'url.c',
    'utf8.cc',
    'version.cc',
    'web.cc',
    'webp.c',
    'xembed.cc',
)

commit_h = vcs_tag(
    fallback: meson.project_version(),
    input: 'commit.h.in',
    output: 'commit.h'
)

dillo_files += [commit_h]

dillo = executable(
    'dillo',
    sources: dillo_files,
    dependencies: dillo_deps,
    include_directories: incdir,
    install: true,
    link_with: dillo_libs,
)

sysconf_files = files(
    'domainrc',
    'hsts_preload',
    'keysrc',
)

install_data(
    sysconf_files,
    install_dir: get_option('sysconfdir') / meson.project_name(),
    install_tag: 'runtime',
)
