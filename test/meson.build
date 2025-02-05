# Unit tests

unittestdir = meson.project_source_root() / 'test' / 'unit'

containers = executable(
    'containers',
    'unit/containers.cc',
    link_with: [liblout, dlib],
    include_directories: incdir,
)
cookies = executable(
    'cookies',
    'unit/cookies.c',
    link_with: [dpip, dlib],
    include_directories: incdir,
)
identity = executable(
    'identity',
    'unit/identity.cc',
    link_with: [liblout, dlib],
    include_directories: incdir,
)
liang = executable(
    'liang',
    'unit/liang.cc',
    dependencies: [fltk, x11],
    link_with: [dlib, liblout, dwcore, dw_fltk, dw_widgets],
    include_directories: incdir,
    cpp_args: f'-DCUR_SRC_DIR="@unittestdir@"',
)
notsosimplevector = executable(
    'notsosimplevector',
    'unit/notsosimplevector.cc',
    include_directories: incdir,
)
shapes = executable(
    'shapes',
    'unit/shapes.cc',
    link_with: [liblout, dlib, dwcore],
    include_directories: incdir,
)
trie = executable(
    'trie',
    'unit/trie.cc',
    dependencies: [fltk, x11],
    link_with: [dlib, liblout, dwcore, dw_fltk, dw_widgets],
    include_directories: incdir,
)
unicode_test = executable(
    'unicode_test',
    'unit/unicode_test.cc',
    dependencies: [fltk, x11],
    link_with: liblout,
    include_directories: incdir,
)

test('containers', containers)
test('cookies', cookies, should_fail: true)
test('identity', identity)
test('liang', liang)
test('notsosimplevector', notsosimplevector)
test('shapes', shapes)
test('trie', trie, should_fail: true)

# dw
# According to the old Makefile.am, most of these tests are supposed to be run manually as they require
# user interaction.

dw_test_deps = [fltk, x11]
dw_link_with = [dlib, liblout, dwcore, dw_fltk, dw_widgets]

dw_anchors_test = executable(
    'dw_anchors_test',
    'dw/dw_anchors_test.cc',
    dependencies: dw_test_deps,
    link_with: dw_link_with,
    include_directories: incdir,
)
dw_border_test = executable(
    'dw_border_test',
    'dw/dw_border_test.cc',
    dependencies: dw_test_deps,
    link_with: dw_link_with,
    include_directories: incdir,
)
dw_example = executable(
    'dw_example',
    'dw/dw_example.cc',
    dependencies: dw_test_deps,
    link_with: dw_link_with,
    include_directories: incdir,
)
dw_find_test = executable(
    'dw_find_test',
    'dw/dw_find_test.cc',
    dependencies: dw_test_deps,
    link_with: dw_link_with,
    include_directories: incdir,
)
dw_float_test = executable(
    'dw_float_test',
    'dw/dw_float_test.cc',
    dependencies: dw_test_deps,
    link_with: dw_link_with,
    include_directories: incdir,
)
dw_image_background = executable(
    'dw_image_background',
    'dw/dw_image_background.cc',
    dependencies: dw_test_deps,
    link_with: dw_link_with,
    include_directories: incdir,
)
dw_images_scaled = executable(
    'dw_images_scaled',
    'dw/dw_images_scaled.cc',
    dependencies: dw_test_deps,
    link_with: dw_link_with,
    include_directories: incdir,
)
dw_images_scaled2 = executable(
    'dw_images_scaled2',
    'dw/dw_images_scaled2.cc',
    dependencies: dw_test_deps,
    link_with: dw_link_with,
    include_directories: incdir,
)
dw_images_simple = executable(
    'dw_images_simple',
    'dw/dw_images_simple.cc',
    dependencies: dw_test_deps,
    link_with: dw_link_with,
    include_directories: incdir,
)
dw_imgbuf_mem_test = executable(
    'dw_imgbuf_mem_test',
    'dw/dw_imgbuf_mem_test.cc',
    dependencies: dw_test_deps,
    link_with: dw_link_with,
    include_directories: incdir,
)
dw_links = executable(
    'dw_links',
    'dw/dw_links.cc',
    dependencies: dw_test_deps,
    link_with: dw_link_with,
    include_directories: incdir,
)
dw_links2 = executable(
    'dw_links2',
    'dw/dw_links2.cc',
    dependencies: dw_test_deps,
    link_with: dw_link_with,
    include_directories: incdir,
)
dw_lists = executable(
    'dw_lists',
    'dw/dw_lists.cc',
    dependencies: dw_test_deps,
    link_with: dw_link_with,
    include_directories: incdir,
)
dw_resource_test = executable(
    'dw_resource_test',
    'dw/dw_resource_test.cc',
    dependencies: dw_test_deps,
    link_with: dw_link_with,
    include_directories: incdir,
)
dw_simple_container_sources = files('dw/dw_simple_container.cc', 'dw/dw_simple_container_test.cc')
dw_simple_container_test = executable(
    'dw_simple_container_test',
    dw_simple_container_sources,
    dependencies: dw_test_deps,
    link_with: dw_link_with,
    include_directories: incdir,
)
dw_table = executable(
    'dw_table',
    'dw/dw_table.cc',
    dependencies: dw_test_deps,
    link_with: dw_link_with,
    include_directories: incdir,
)
dw_table_aligned = executable(
    'dw_table_aligned',
    'dw/dw_table_aligned.cc',
    dependencies: dw_test_deps,
    link_with: dw_link_with,
    include_directories: incdir,
)
dw_ui_test_sources = files('dw/dw_ui_test.cc', 'dw/form.cc')
dw_ui_test = executable(
    'dw_ui_test',
    dw_ui_test_sources,
    dependencies: dw_test_deps,
    link_with: dw_link_with,
    include_directories: incdir,
)

test('DW imgbuf mem test', dw_imgbuf_mem_test)

if get_option('html-tests').enabled()
    imgformats = ['png', 'svg']
    foreach imgformat : imgformats
        if not get_option(imgformat).enabled()
            error(f'Option \'@imgformat@\' is required for the HTML tests')
        endif
    endforeach
    html_tests = [
        'html/render/b-div.html',
        'html/render/div-100-percent-with-padding.html',
        'html/render/float-img-justify.html',
        'html/render/github-infinite-loop.html',
        'html/render/hackernews.html',
        'html/render/img-aspect-ratio.html',
        'html/render/main-style.html',
        'html/render/margin-auto.html',
        'html/render/max-width-body.html',
        'html/render/max-width-div-clamp.html',
        'html/render/max-width-div.html',
        'html/render/max-width-html.html',
        'html/render/max-width-nested-div.html',
        'html/render/meta-refresh-0-no-url.html',
        'html/render/min-width-body.html',
        'html/render/min-width-div-extend.html',
        'html/render/min-width-div.html',
        'html/render/min-width-html.html',
        'html/render/min-width-nested-div.html',
        'html/render/span-padding.html',
        'html/render/svg-current-color.html',
        'html/render/table-max-width.html',
        'html/render/table-missing-width-in-one-column.html',
        'html/render/table-td-width-percent-img.html',
        'html/render/table-td-width-percent.html',
        'html/render/table-thead-tfoot-open-tag.html',
        'html/render/table-thead-tfoot.html',
        'html/render/white-space.html',
    ]

    # To be fixed
    xfail_tests = [
        'html/render/div-100-percent-with-padding.html',
        'html/render/float-img-justify.html',
        'html/render/margin-auto.html',
        'html/render/max-width-html.html',
        'html/render/min-width-html.html',
        'html/render/span-padding.html',
        'html/render/table-td-width-percent.html',
    ]

    dillo_path = dillo.full_path()

    foreach file : html_tests
        test_name = file.substring(12, -5)
        test_file = files(file)
        test(
            test_name,
            files('html/driver.sh'),
            env: [f'DILLOBIN=@dillo_path@'],
            args: test_file[0].full_path(),
            is_parallel: false,
        )
    endforeach

    foreach file : xfail_tests
        test_name = file.substring(12, -5)
        test_file = files(file)
        test(
            test_name,
            files('html/driver.sh'),
            env: [f'DILLOBIN=@dillo_path@'],
            args: test_file[0].full_path(),
            should_fail: true,
            is_parallel: false,
        )
    endforeach
endif
