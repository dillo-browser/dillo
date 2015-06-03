 June 2000, --Jcid
 Last update: Jul 09

                              -------
                               CACHE
                              -------

   The  cache  module  is  the  main  abstraction  layer  between
rendering and networking.

   The  capi module acts as a discriminating wrapper which either
calls  the  cache  or  the  dpi routines depending on the type of
request.

   Every  URL  must be requested using a_Capi_open_url, which
sends the request to the cache if the data is cached, to dillo's
http module for http: URLs, and through dillo's DPI system for
other URLs.

   Here we'll document non dpi requests.


                         ----------------
                         CACHE PHILOSOPHY
                         ----------------

   Dillo's  cache  is  very  simple; every single resource that's
retrieved  (URL)  is  kept  in  memory. NOTHING is saved to disk.
This is mainly for three reasons:

   - Dillo encourages personal privacy and it assures there'll be
no recorded tracks of the sites you visited.

   -  The Network is full of intermediate transparent proxys that
serve as caches.

   -  If  you still want to have cached stuff, you can install an
external cache server (such as WWWOFFLE), and benefit from it.


                         ---------------
                         CACHE STRUCTURE
                         ---------------

   Currently, dillo's cache code is spread in different sources:
mainly  in  cache.[ch],  dicache.[ch]  and  it  uses  some  other
functions from mime.c and web.cc.

   Cache.c  is  the  principal  source,  and  it also is the one
responsible  for  processing  cache-clients  (held  in  a queue).
Dicache.c  is  the interface to the decompressed RGB representations
of currently-displayed images held in DW's imgbuf.

   mime.c  and  web.cc  are  used  for secondary tasks such as
assigning the right "viewer" or "decoder" for a given URL.


----------------
A bit of history
----------------

   Some  time  ago,  the  cache  functions,  URL  retrieval  and
external  protocols  were  a whole mess of mixed code, and it was
getting  REALLY hard to fix, improve or extend the functionality.
The  main  idea  of  this  "layering" is to make code-portions as
independent  as  possible  so  they  can  be  understood,  fixed,
improved or replaced without affecting the rest of the browser.

   An  interesting  part of the process is that, as resources are
retrieved,  the  client  (dillo  in  this  case) doesn't know the
Content-Type  of the resource at request-time. It only becomes known
when  the  resource  header  is retrieved (think of http). This
happens  when  the  cache  has control, so the cache sets the
proper  viewer for it (unless the Callback function was already
specified with the URL request).

   You'll find a good example in http.c.

   Note: All resources received by the cache have HTTP-style headers.
   The file/data/ftp DPIs generate these headers when sending their
   non-HTTP resources. Most importantly, a Content-Type header is
   generated based on file extension or file contents.


-------------
Cache clients
-------------

   Cache clients MUST use a_Capi_open_url to request an URL. The
client structure and the callback-function prototype are defined,
in cache.h, as follows:

struct _CacheClient {
   int Key;                 /* Primary Key for this client */
   const DilloUrl *Url;     /* Pointer to a cache entry Url */
   int Version;             /* Dicache version of this Url (0 if not used) */
   void *Buf;               /* Pointer to cache-data */
   uint_t BufSize;          /* Valid size of cache-data */
   CA_Callback_t Callback;  /* Client function */
   void *CbData;            /* Client function data */
   void *Web;               /* Pointer to the Web structure of our client */
};

typedef void (*CA_Callback_t)(int Op, CacheClient_t *Client);


   Notes:

   * Op is the operation that the callback is asked to perform
   by the cache. { CA_Send | CA_Close | CA_Abort }.

   * Client: The Client structure that originated the request.



--------------------------
Key-functions descriptions
--------------------------

································································
int a_Cache_open_url(void *Web, CA_Callback_t Call, void *CbData)

   if Web->url is not cached
      Create a cache-entry for that URL
      Send client to cache queue
   else
      Feed our client with cached data

································································

----------------------
Redirections mechanism
 (HTTP 30x answers)
----------------------

  This is by no means complete. It's a work in progress.

  Whenever  an  URL is served under an HTTP 30x header, its cache
entry  is  flagged  with 'CA_Redirect'. If it's a 301 answer, the
additional  'CA_ForceRedirect'  flag  is  also set, if it's a 302
answer,  'CA_TempRedirect'  is  also set (this happens inside the
Cache_parse_header() function).

  Later  on,  in Cache_process_queue(), when the entry is flagged
with 'CA_Redirect' Cache_redirect() is called.







-----------
Notes
-----------

   The  whole  process is asynchronous and very complex. I'll try
to document it in more detail later (source is commented).
   Currently  I  have  a drawing to understand it; hope the ASCII
translation serves the same as the original.
   If  you're  planning to understand the cache process thoroughly,
write  me  a  note and I will assign higher priority to further
improvement of this doc.
   Hope this helps!


