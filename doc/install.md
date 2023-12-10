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

Then, to install Dillo:

### From a release

```sh
$ tar jxvf dillo-3.0.5.tar.bz2
$ cd dillo-3.0.5
$ ./configure; make
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

Dillo compiles on BSD systems; please report on this anyway, and note
that you'll need GNU make. If your dillo crashes or locks at times, use:

```
$ ./configure --disable-threaded-dns
```

So dillo uses a single thread for name resolving.

## Solaris

Dillo may compile and run OK on Solaris but (please report).
Use gmake (a symbolic link make -> gmake works OK).

Solaris is very inconsistent so you may need to add/remove:

```
-lrt -lposix4
```

at link time.
