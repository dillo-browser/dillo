# Installing Dillo on Linux

Dillo is already packaged in many Linux distributions. To use the binary
from your distribution check your package manager. Example in Arch
Linux:

```
$ sudo pacman -S dillo
```

## Building from source

Dillo requires FLTK-1.3, if you don't have it (try `fltk-config
--version` to check), follow the steps in the FLTK documentation to
install it:

https://www.fltk.org/doc-1.3/intro.html

Additionally, it is **strongly recommended** that you install a TLS
library to browse HTTPS pages. Currently, Dillo supports any of the
following libraries:

 - OpenSSL 1.1 or 3
 - LibreSSL
 - mbedTLS 2 or 3 (TLSv1.3 is not supported yet)

If you don't want to use a TLS library, use the configure option
`--disable-tls` to disable TLS support. You can use `--disable-openssl`
and `--disable-mbedtls` to control the search. By default OpenSSL or
LibreSSL is search first, then mbedTLS.

For Debian, you can use the following command to install the required
packages to build Dillo:

```sh
$ sudo apt install gcc g++ autoconf automake make zlib1g-dev \
    libfltk1.3-dev libssl-dev libc6-dev \
    libpng-dev libjpeg-dev libwebp-dev libbrotli-dev
```

If you prefer to use mbedTLS, replace `libssl-dev` with
`libmbedtls-dev`.

To build and install Dillo follow the steps below.

### From a release

```sh
$ tar jxvf dillo-3.0.5.tar.bz2
$ cd dillo-3.0.5
$ mkdir build
$ cd build
$ ../configure --prefix=/usr/local
$ make
$ sudo make install
```

### From git

```sh
$ git clone https://github.com/dillo-browser/dillo.git
$ cd dillo
$ ./autogen.sh
$ mkdir build
$ cd build
$ ../configure --prefix=/usr/local
$ make
$ sudo make install
```

## Hyphenation database

In order to use the hyphenation feature, pattern files from CTAN need to
be installed. This can be done with the script
`dillo-install-hyphenation`. Call it with ISO-639-1 language codes
("en", "es", "de"...) as arguments, or without arguments to get more
help.

## Dpi programs

These are installed by `make install`. If you don't have root access,
copy "dillo" and "dpid" to some directory in your path and install
the dpis by running `./install-dpi-local` from the top directory (they
will be installed under ~/.dillo).

# Other systems

## BSD

On FreeBSD 14.0 you can install the required dependencies from ports as:

```
$ pkg install -y automake fltk
```

Notice that on BSD systems, some required dependencies like FLTK are
available as ports and are installed in /usr/local (instead of /usr).
Additionally, the compiler won't look for headers or libraries in
/usr/local by default (you can check with $CC -v), however the `$PATH`
may include /usr/local/bin, so the binaries may be available but not the
headers. Some dependencies like FLTK are searched invoking the
`fltk-config` command that inform where to find the headers and
libraries for that package.

Other libraries are searched by attempting to locate the headers and
libraries directly. By default, the configure script won't look in
non-default paths of the compiler, so to add /usr/local to the search
path, invoke the configure script with the following options:

```
$ ./configure CPPFLAGS='-I/usr/local/include' LDFLAGS='-L/usr/local/lib'
```

Note that you'll need GNU make to build Dillo.

If it crashes or locks at times, use the `--disable-threaded-dns`
option, so Dillo uses a single thread for name resolution.

## Solaris

Dillo may compile and run OK on Solaris but (please report).
Use gmake (a symbolic link make -> gmake works OK).

Solaris is very inconsistent so you may need to add/remove:

```
-lrt -lposix4
```

at link time.

## MacOS

Dillo is now packaged in [Homebrew](https://brew.sh/).

```
$ brew install dillo
```

### Building from source

To build Dillo on MacOS you would need to get FLTK as well as
autoconf and automake if you are building Dillo from the git repository.
They are available in the Homebrew package manager:

```
$ brew install autoconf automake fltk@1.3
```

For OpenSSL you can use either 1.1 or 3.X (recommended):

```
$ brew install openssl@1.1
$ brew install openssl@3
```

Homebrew installs libraries and headers in different folders depending on the
architecture (Intel vs ARM), so you will need to invoke the configure script
with the following options so it knows where to search:

```
$ ./configure LDFLAGS="-L`brew --prefix openssl`/lib" CPPFLAGS="-I`brew --prefix openssl`/include"
```

## Windows via Cygwin

Dillo can be built for Windows (tested on Windows 11) by using the
[Cygwin](https://www.cygwin.com/) POSIX portability layer and run with Xorg. You
will need the following dependencies to build Dillo with mbedTLS:

```
gcc-core gcc-g++ autoconf automake make zlib-devel mbedtls-devel libfltk-devel
libiconv-devel libpng-devel libjpeg-devel libwebp-devel libbrotli-devel
```

**Note**: Dillo can also be built with OpenSSL (libssl-devel) but there is a
[known problem with detached threads](https://github.com/dillo-browser/dillo/issues/172)
used by the DNS resolver and OpenSSL that causes a crash. If you use OpenSSL,
disable the threaded resolver with `--disable-threaded-dns`.

You will also need [Xorg](https://x.cygwin.com/docs/ug/cygwin-x-ug.html) to run
Dillo graphically:

```
xorg-server xinit
```

You can also install all the dependencies from the command line with:
```
setup-x86_64.exe -q -P gcc-core,gcc-g++,autoconf,automake,make,zlib-devel,mbedtls-devel,libfltk-devel,libiconv-devel,libpng-devel,libjpeg-devel,libwebp-devel,libbrotli-devel,xorg-server,xinit
```

To build Dillo, follow the usual steps from a Cygwin shell:

```sh
$ git clone https://github.com/dillo-browser/dillo.git
$ cd dillo
$ ./autogen.sh
$ mkdir build
$ cd build
$ ../configure --prefix=/usr/local
$ make
$ make install
```

You should be able to find Dillo in the `$PATH` as it should be installed in
`/usr/local/bin/dillo`. To run it, you first need to have an [Xorg server
running](https://x.cygwin.com/docs/ug/using.html#using-starting), which you can
do from a shell:

```sh
$ startxwin
```

And then, from another shell:

```sh
$ export DISPLAY=:0
$ dillo
```
