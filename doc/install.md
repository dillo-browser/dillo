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
 - mbedTLS 2 or 3

If you don't want to use a TLS library, use the configure option
`--disable-tls` to disable TLS support. You can use `--disable-openssl`
and `--disable-mbedtls` to control the search. By default OpenSSL is
search first, then mbedTLS.

Then, to build and install Dillo:

### From a release

```sh
$ tar jxvf dillo-3.0.5.tar.bz2
$ cd dillo-3.0.5
$ ./configure
$ make
$ sudo make install-strip
```

### From git

```sh
$ git clone git@github.com:dillo-browser/dillo.git
$ cd dillo
$ ./autogen.sh
$ ./configure
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

To build Dillo on MacOS you would need to get FLTK as well as
autoconf and automake if you are building Dillo from the git repository.
They are available in the brew package manager:

```
$ brew install autoconf automake fltk
```

For OpenSSL you can use either 1.1 or 3.X (recommended):

```
$ brew install openssl@1.1
$ brew install openssl@3
```
