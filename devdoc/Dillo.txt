"Eliminate the guesswork and quality goes up."


                             -------
                              DILLO
                             -------

   These  notes are written with a view to make it less hard, not
easier yet ;), to get into Dillo development.
   When I first got into it, I was totally unaware of the browser
internals.  Now  that  I've made my way deep into the core of it,
(we  rewrote it 90% and modified the rest), is time to write some
documentation,  just  to make a less steep learning curve for new
developers.

   --Jcid



                            --------
                            OVERVIEW
                            --------

   Dillo can be viewed as the sum of five main parts:

   1.- Dillo Widget: A custom widget, FLTK-based, that holds the
necessary data structures and mechanisms for graphical rendering.
(Described in Dw*.txt, dw*.c files among the sources.)

   2.-  Dillo Cache: Integrated with a signal driven Input/Output
engine  that  handles file descriptor activity, the cache acts as
the  main  abstraction  layer  between  rendering and networking.
   Every  URL,  whether  cached  or  not, must be retrieved using
a_Capi_open_url   (Described   briefly   in   Cache.txt,  source
contained in capi.c).
   IO is described in IO.txt (recommended), source in src/IO/.

   3.-  The  HTML  parser: A streamed parser that joins the Dillo
Widget  and  the  Cache  functionality  to make browsing possible
(Described in HtmlParser.txt, source mainly inside html.cc).

   4.-  Image  processing  code:  The  part  that  handles  image
retrieval,  decoding,  caching  and  displaying.  (Described  in
Images.txt.   Sources:  image.c,  dw/image.cc,  dicache.c,  gif.c,
jpeg.c and png.c)

   5.- The dpi framework: a gateway to interface the browser with
external programs (Example: the bookmarks server plugin).
Dpi spec: http://www.dillo.org/dpi1.html


                      -------------------------
                      HOW IS THE PAGE RENDERED?
                      -------------------------

(A short description of the internal function calling process)

   When  the  user requests a new URL, a_UIcmd_open_url
is  queried to do the job; it calls a_Nav_push (The highest level
URL  dispatcher); a_Nav_push updates current browsing history and
calls  Nav_open_url.  Nav_open_url closes all open connections by
calling  a_Bw_stop_clients,  and then calls
a_Capi_open_url which calls a_Cache_open_url (or the dpi module if
this gateway is used).

   If  Cache_entry_search  hits  (due to a cached url :), the client is
fed  with cached data, but if the URL isn't cached yet, a new CCC
(Concomitant  Control Chain) is created and committed to fetch the
URL.

   The  next  CCC  link  is dynamically assigned by examining the
URL's protocol. It can be a_Http_ccc or a_Dpi_ccc.

   If  we  have an HTTP URL, a_Http_ccc will succeed, and the http
module  will  be linked; it will create the proper HTTP query and
link the IO module to submit and deliver the answer.

   Note  that  as the Content-Type of the URL is not always known
in  advance, the answering branch decides where to dispatch it to
upon HTTP header arrival.


   What happens then?

   Well,  the  html  parser  gets  fed,  and proper functions are
called  for  each tag (to parse and call the appropriate methods)
and the whole page is contructed in a streamed way.
   Somewhere  in  the  middle of it, resize and repaint functions
are  activated  and  idle  functions draw to screen what has been
processed.

   (The process for images is described in Images.txt)




