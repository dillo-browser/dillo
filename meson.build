project(
    'dillo',
    'cpp',
    'c',
    version: '3.1.1',
    license: 'GPL-3.0-or-later',
    meson_version: '>= 1.4.0', #file.full_path()
    default_options: [
        'c_std=c11',
        'cpp_std=c++11',
        'warning_level=everything',
        'werror=false',
    ],
)

host = host_machine.system()
cxx = meson.get_compiler('cpp')
cc = meson.get_compiler('c')
exe_suffix = host_machine.system() == 'windows' ? 'exe' : ''

incdir = [include_directories('.')]

if cxx.get_id() == 'gcc' or cxx.get_id() == 'clang'
    add_project_arguments(
        '-Wno-unused-parameter',
        '-fno-exceptions',
        '-D_POSIX_C_SOURCE=200112L',
        language: 'c',
    )
    add_project_arguments(
        '-Wno-unused-parameter',
        '-fno-rtti',
        '-fno-exceptions',
        '-D_POSIX_C_SOURCE=200112L',
        language: 'cpp',
    )
endif

conf = configuration_data()

# Some basic defines upfront, we'll do the rest right before we configure_file
conf.set_quoted('VERSION', meson.project_version())
# TODO: This is -DDILLO_BINDIR why?
conf.set_quoted('DILLO_BINDIR', get_option('prefix') / get_option('bindir'))
conf.set_quoted(
    'DILLO_DOCDIR',
    get_option('prefix') / get_option('datadir') / 'doc' / meson.project_name(),
)
conf.set_quoted(
    'DILLO_LIBDIR',
    get_option('prefix') / get_option('libdir') / meson.project_name(),
)
# This one needs a trailing slash in the dillo source; for 1:1 autotools compat we'll add it here for now
conf.set_quoted(
    'DILLO_SYSCONF',
    get_option('prefix') / get_option('sysconfdir') / meson.project_name()
    + '/',
)
conf.set_quoted('DPIDRC_SYS', get_option('sysconfdir') / meson.project_name() / 'dpidrc')
conf.set_quoted('EXEEXT', exe_suffix)
conf.set_quoted('PACKAGE', meson.project_name())

###################
# Function checks #
###################

functions = []

# 'aix' isn't actually supported by meson, but we'll check for it anyway
if host == 'sunos' or host == 'aix'
    functions = ['gethostbyname', 'setsockopt']
    foreach func : functions
        if not cc.has_function(func)
            error(f'Function @func@ not found')
        endif
    endforeach
endif

###########
# Headers #
###########

# Check for socklen_t (in Unix98)
if host_machine.system() == 'windows'
    socket_header = 'ws2tcpip.h'
else
    socket_header = 'sys/socket.h'
endif

if not cc.has_header_symbol(socket_header, 'socklen_t')
    code = f'''
#include <sys/types.h>
#include <@socket_header@>
int accept(int, struct sockaddr *, size_t *);
int main() {}
'''
    if cc.compiles(code, name: 'socklen_t is size_t')
        conf.set('socklen_t', 'size_t')
    else
        conf.set('socklen_t', 'int')
    endif
endif

################
# Dependencies #
################

all_deps = []
dillo_deps = []

# FLTK is a bit of a mess; we'll try to use CMake to find it first.
# (Not using 'method: 'cmake' because if pkg-config ever does get implemented we'll want to use that)
# FLTK is painful. Since 2009 they've been 'considering' pkg-config support
# but even though they have most of the pieces in place nobody has bothered to implement
fltk = dependency('FLTK', required: false) # I'm pretty sure cmake falls back to fltk-config anyway.
if not fltk.found()
    if meson.is_cross_build()
        error('Did not find FLTK using CMake; cross-compilation is not possible')
    else
        # Fall back to a legacy fltk-config check implemented directly in meson
        # Thanks to Eli Schwartz for the following:
        fltk_config = find_program('fltk-config')
        fltk_include_dirs = run_command(
            fltk_config,
            '--includedir',
            check: true,
        ).stdout().strip().split()
        fltk_cxxflags = run_command(
            fltk_config,
            '--cxxflags',
            check: true,
        ).stdout().strip().split()
        #fltk_link_args = run_command(fltk_config, '--ldflags', '--libs', check:
        #true).stdout().strip().split()
        # --libs _always_ reports the .a file which means if you're using a shared library
        # you'll get a link error. We'll just use --ldflags for now.
        fltk_link_args = run_command(
            fltk_config,
            '--ldflags',
            check: true,
        ).stdout().strip().split()
        fltk = declare_dependency(
            compile_args: fltk_cxxflags,
            include_directories: fltk_include_dirs,
            link_args: fltk_link_args,
        )
    endif
endif

# iconv has custom lookup functionality; Meson will check for iconv.h and libiconv
iconv = dependency('iconv', required: true)
jpeg = dependency('libjpeg', required: get_option('jpeg'))
png = dependency('libpng', required: get_option('png'))
pthread = dependency('threads', required: get_option('posix-threads'))
webp = dependency('libwebp', required: get_option('webp'))
x11 = dependency('x11', required: false) # This is not needed on some platforms; we should gate or otherwise handle this
zlib = dependency('zlib', required: true)

all_deps = [iconv, fltk]

foreach dep : [jpeg, png, pthread, webp, x11, zlib]
    if dep.found()
        all_deps += [dep]
    endif
endforeach

use_tls = get_option(
    'tls',
).enable_if(get_option('mbedtls') or get_option('openssl'))
if use_tls.enabled()
    if get_option('mbedtls') and get_option('openssl')
        error('Cannot enable both mbedtls and openssl')
    endif

    conf.set10('ENABLE_TLS', true, description: 'Enable TLS support')

    if get_option('openssl')
        openssl = dependency('openssl', required: true)
        all_deps += [openssl]
        conf.set10('HAVE_OPENSSL', true)
    elif get_option('mbedtls')
        mbedtls = dependency('mbedtls', required: false)
        mbedx509 = dependency('mbedx509', required: false)
        mbedcrypto = dependency('mbedcrypto', required: false)
        if mbedtls.found() and mbedx509.found() and mbedcrypto.found()
            all_deps += [mbedtls, mbedx509, mbedcrypto]
        else
            # We're probably on an OS with an outdated mbedtls package that doesn't include a pkgconfig (etc).
            # Try and find mbedtls3
            mbedtls = cc.find_library('mbedtls', has_headers: 'mbedtls/net.h', required: false)
            if mbedtls.found()
                # Assume that we can find the other two libraries in the same way; die if we don't
                mbedx509 = cc.find_library('mbedx509', required: true)
                mbedcrypto = cc.find_library('mbedcrypto', required: true)
                all_deps += [mbedtls, mbedx509, mbedcrypto]
            else
                # If all else fails assume that the user knows what they're doing and this is a very old mbedtls2
                message('Using fallback mbedtls2 definition')
                mbedtls = declare_dependency(
                    include_directories: ['/usr/include/mbedtls2/'],
                    link_args: [
                        '-L/usr/lib/mbedtls2',
                        '-lmbedtls',
                        '-lmbedx509',
                        '-lmbedcrypto',
                    ],
                )
                all_deps += mbedtls
            endif
        endif
        conf.set10('HAVE_MBEDTLS', true)
    endif
endif

### Test Deps:

efence = dependency('efence', required: get_option('efence'))
gprof = dependency('gprof', required: get_option('gprof'))

all_deps += [efence, gprof]

# Rely on meson to die if a required dependency is not found
# Add all 'found' deps to dillo_deps to link against.
foreach dep : all_deps
    if dep.found()
        dillo_deps += dep
    endif
endforeach

##############
# Misc Logic #
##############

# Since we tried to keep finding dependencies as tidy as possible we'll explicitly
# do any other checks (and config setting) in one place: here.

# Legacy autotools would check for old style libiconf and define inbuf_t accordingly; we'll only support new style
conf.set('inbuf_t', 'char')

if get_option('cookies').disabled()
    add_project_arguments('-DDISABLE_COOKIES', language: 'c')
    add_project_arguments('-DDISABLE_COOKIES', language: 'cpp')
endif

if get_option('gif').enabled()
    conf.set10('ENABLE_GIF', true, description: 'Enable GIF support')
endif

if get_option('ipv6').enabled()
    # I guess this is C only?
    add_project_arguments('-DENABLE_IPV6', language: 'c')
endif

if jpeg.found()
    conf.set10('ENABLE_JPEG', true, description: 'Enable JPEG support')
endif

if png.found()
    conf.set10('ENABLE_PNG', true, description: 'Enable PNG support')
endif

if get_option('svg').enabled()
    conf.set10('ENABLE_SVG', true, description: 'Enable SVG support')
endif

if get_option('threaded-dns').enabled()
    add_project_arguments('-DD_DNS_THREADED', language: 'c')
endif

if get_option('rtfl').enabled()
    add_project_arguments('-DDBG_RTFL', language: 'cpp')
endif

if get_option('webp').enabled()
    conf.set10('ENABLE_WEBP', true, description: 'Enable WebP support')
endif

if get_option('xembed').enabled()
    add_project_arguments('-DDISABLE_XEMBED', language: 'cpp')
endif

# This is a bit more complex; TODO:
# I suspect that those wishing to use insure should set CC and LIBS themselves outside of the build system.
# if test "x$enable_insure" = "xyes" ; then
#   CC="insure -Zoi \"compiler $CC\""
#   LIBS="$LIBS -lstdc++-2-libc6.1-1-2.9.0"
# fi

# These autotools macros existed previously
# check for these and defines them if they're not present
# TODO: For now we'll just jam @include stdint.h into config.h
# AC_TYPE_INT16_T
# AC_TYPE_UINT16_T
# AC_TYPE_INT32_T
# AC_TYPE_UINT32_T
# We may not need this at all; these types have all been mandatory in posix since 2001!

conf.set('CA_CERTS_DIR', get_option('ca-certs-dir'))
conf.set('CA_CERTS_FILE', get_option('ca-certs-file'))

configure_file(
    output: 'config.h',
    configuration: conf,
)

install_data(
    files('dillorc'),
    install_dir: get_option('sysconfdir') / meson.project_name(),
    install_tag: 'runtime',
)

############
# Subdirs! #
############

# We store internal static libs here to link against in the main executable
dillo_libs = []

# We could subdir some of these small libs, but honestly it's so small it's not worth it.
dlib = static_library('dlib', sources: files('dlib/dlib.c'))

liblout_files = files(
    'lout/container.cc',
    'lout/debug.hh',
    'lout/identity.cc',
    'lout/misc.cc',
    'lout/object.cc',
    'lout/signal.cc',
    'lout/unicode.cc',
)

liblout = static_library(
    'liblout',
    sources: liblout_files,
    include_directories: incdir,
)

dpip = static_library(
    'dpip',
    sources: files('dpip/dpip.c'),
    include_directories: incdir,
)

dillo_libs += [dlib, dpip, liblout]

subdir('dpi')
subdir('dw')
subdir('src')
subdir('dpid')
subdir('test')

##################
# Config Summary #
##################

summary(
    {
        'Build': build_machine.system(),
        'Build CPU Family': build_machine.cpu_family(),
        'Host CPU Family': host_machine.cpu_family(),
        'Cross-compiling': meson.is_cross_build(),
        'Build Endianness': build_machine.endian(),
        'Host Endianness': host_machine.endian(),
        'Target': host,
        'C Compiler': cc.get_id(),
        'C Compiler Version': cc.version(),
        'Linker': cc.get_linker_id(),
    },
    section: 'Environment',
)

featurevals = {
    'TLS': use_tls.enabled(),
}

if use_tls.enabled()
    if get_option('openssl')
        featurevals += {'OpenSSL': openssl}
    elif get_option('mbedtls')
        featurevals += {'Mbed TLS': mbedtls}
    endif
endif

featurevals += {
    'Cookies': get_option('cookies').enabled(),
    'GIF': get_option('gif').enabled(),
    'JPEG': jpeg.found() ? jpeg : false,
    'PNG': png.found() ? png : false,
    'SVG': get_option('svg').enabled(),
    'XEmbed': get_option('xembed').enabled(),
    'WebP': get_option('webp').enabled() ? webp : false,
}

summary(
    featurevals,
    bool_yn: true,
    section: 'Features',
)

summary(
    {
        'Electric Fence': efence.found() ? efence : false,
        'gprof': gprof.found() ? gprof : false,
        'RTFL': get_option('rtfl').enabled(),
        'HTML tests': get_option('html-tests').enabled(),
    },
    bool_yn: true,
    section: 'Test and Debugging',
)
