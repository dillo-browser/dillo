
     _________________________________________________________________

                             Naming&Coding design
     _________________________________________________________________

   Dillo's code is divided into modules. For instance: bookmark, cache,
   dicache, gif.

   Let's think of a module named "menu", then:
     * Every internal routine of the module, should start with "Menu_"
       prefix.
     * "Menu_" prefixed functions are not meant to be called from outside
       the module.
     * If the function is to be exported to other modules (i.e. it will
       be called from the outside), it should be wrapped with an "a_"
       prefix.

   For instance: if the function name is "Menu_create", then it's an
   internal function, but if we need to call it from the outside, then it
   should be renamed to "a_Menu_create".

   Why the "a_" prefix?
   Because of historical reasons.
   And "a_Menu_create" reads better than "d_Menu_create" because the
   first one suggests "a Menu create" function!

   Another way of understanding this is thinking of "a_" prefixed
   functions as Dillo's internal library, and the rest ("Menu_" prefixed
   in our example) as a private module-library.

   Indentation:

   Source code must be indented with 3 blank spaces, no Tabs.
   Why?
   Because different editors expand or treat tabs in several ways; 8
   spaces being the most common, but that makes code really wide and
   we'll try to keep it within the 80 columns bounds (printer friendly).

   You can use:   indent -kr -sc -i3 -bad -nbbo -nut -l79 myfile.c

   Function commenting:

   Every single function of the module should start with a short comment
   that explains its purpose; three lines must be enough, but if you
   think it requires more, enlarge it.

   /*
    * Try finding the url in the cache. If it hits, send the contents
    * to the caller. If it misses, set up a new connection.
    */
   int a_Cache_open_url(const char *url, void *Data)
   {
      ...
      ...
      ...
   }

   We also have the BUG:, TODO:, and WORKAROUND: tags.
   Use them within source code comments to spot hidden issues. For
   instance:

   /* BUG: this counter is not accurate */
   ++i;

   /* TODO: get color from the right place */
   a = color;

   /* WORKAROUND: the canonical way of doing it doesn't work yet. */
   ++a; ++a; ++a;

   Function length:

   Let's try to keep functions within the 45 lines boundary. This eases
   code reading, following, understanding and maintenance.

   Functions with a single exit:

   It's much easier to follow and maintain functions with a single exit
   point at the bottom (instead of multiple returns). The exception to
   the rule are calls like dReturn_if_fail() at its head.

   dlib functions:

     * Dillo uses dlib extensively in its C sources. Before starting
       to code something new, a good starting point is to check what
       this library has to offer (check dlib/dlib.h).
     * Memory management must be done using dNew, dNew0, dMalloc, dFree
       and their relatives.
     * For debugging purposes and error catching (not for normal flow):
       dReturn_if_fail, dReturn_val_if_fail etc. are encouraged.
     * The MSG macro is extensively used to output additional information
       to the calling terminal.

     _________________________________________________________________

  C++

   Source code in C++ should follow the same rules with these exceptions:

     * Class method names are camel-cased and start with lowercase
       e.g. appendInputMultipart
     * Classes and types start uppercased
       e.g. class DilloHtmlReceiver
     * Class methods don't need to prefix its module name
       e.g. links->get()

   We also try to keep the C++ relatively simple. Dillo does use
   inheritance and templates, but that's about all.

     _________________________________________________________________

  What do we get with this?

     * A clear module API for Dillo; every function prefixed "a_" is to
       be used outside the module.
     * A way to identify where the function came from (the
       capitalized word is the module name).
     * An inner ADT (Abstract data type) for the module that can be
       isolated, tested and replaced independently.
     * A two stage instance for bug-fixing. You can change the exported
       function algorithms while someone else fixes the internal
       module-ADT!
     * A coding standard ;)
     _________________________________________________________________

        Naming&Coding design by Jorge Arellano Cid
