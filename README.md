# Dillo web browser

Dillo is a multi-platform graphical web browser, known for its
speed and small footprint, that is developed with a focus on
personal security and privacy.

The dillo3 series uses version 1.3.x of the FLTK GUI toolkit
(http://fltk.org).

The core team currently plans to focus on implementing the CSS
feature of floating elements.  This will greatly improve
dillo's web page rendering since many sites have adopted floats
instead of tables. 

The core team welcomes developers willing to join our workforce. 


NOTE:  With  FLTK-1.3,  when  running  on X with Xft enabled (the
default),  font naming is more restricted than it was with FLTK2
as used by dillo2.  If your font_* preferences are no longer
working well, please try the fc-list command as it is shown in
dillorc.


  Here's a list of some old well-known problems of dillo:

         * no FRAMES rendering
         * https code not yet fully trusted
           (enable it with: ./configure --enable-ssl ).


--------
FLTK-1.3
--------

  If you don't have FLTK-1.3 (try 'fltk-config --version' to check),
  you can get it from:

     http://fltk.org/software.php

  and build it like:

     tar zxvf fltk-1.3.3-source.tar.gz      [or latest 1.3.x version]
     cd fltk-1.3.3
     less README.Unix.txt
     make
     sudo make install
     cd ..

------
Dillo3
------

     tar jxvf dillo-3.0.5.tar.bz2
     cd dillo-3.0.5
     ./configure; make
     sudo make install-strip

  In order to use the hyphenation feature, pattern files from CTAN need
  to be installed.
  This can be done with the script dillo-install-hyphenation.
  Call it with ISO-639-1 language codes as arguments, or without arguments
  to get more help.


------------
Dpi programs
------------

  These  are  installed by "make install". If you don't have root
access,  copy  "dillo"  and "dpid" to some directory in your path
and  install  the  dpis by running "./install-dpi-local" from the
top directory (they will be installed under ~/.dillo).


----
*BSD
----

  Dillo   compiles on *BSD systems; please report on this anyway,
and note that you'll need GNU make.

  If your dillo crashes or locks at times, just use:

    ./configure --disable-threaded-dns

  so dillo uses a single thread for name resolving.


-------
Solaris
-------

  Dillo may compile and run OK on Solaris but (please report):

    * use gmake (a symbolic link make -> gmake works OK)

  Solaris is very inconsistent so you may need to add/remove:

  -lrt -lposix4

  at link time.


Jorge.-
(jcid@dillo.org)
April, 2014
