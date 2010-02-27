Last review: August 04, 2009 --jcid


----------------------------
Internal working for the CCC
----------------------------


HTTP protocol
-------------


      Query:                              |
                                          .
          1B --> 1B  1B --> 1B      -->   |  -------------.
      .----.     .----.     .----.        .               |
I     |Capi|     |http|     | IO |        |               |
      '----'     '----'     '----'        .               |
          1F <-- 1F  1F <-- 1F            |               V
                                          .
                                          |           [Server]
      Answer:                             .

          2B --> 2B  2B --> 2B            |               |
      .----.     .----.     .----.        .               |
II    |Capi|     |Dpi |     | IO |        |               |
      '----'     '----'     '----'        .               |
          2F <-- 2F  2F <-- 2F      <--   |  <------------'
                                          .
                                          |

*  a_Capi_open_url()  builds  both the Answer and Query chains at
once  (Answer  first  then  Query), to ensure a uniform structure
that avoids complexity (e.g. race conditions).

*  Http_get()  sets  a  callback  for  the  DNS hostname resolve.
Normally  it  comes  later, but may also by issued immediately if
the hostname is cached.

*  The  socket FD is passed by means of OpSend by the http module
once the remote IP is known and the socket is connected.



Function calls for HTTP CCC
---------------------------

  a_Capi_open_url
    if (reload)
      Capi OpStart 2B (answer)    [Capi] --> [dpi]  --> [IO]
      Capi OpStart 1B (query)     [Capi] --> [http] --> [IO]
        Http_get
    a_Cache_open_url
      if URL_E2EReload -> prepare reload
      if cached
        client enqueue
        delayed process queue
      else
        Cache_entry_add
        client enqueue

    -//->
    a_Http_dns_cb
      Http_connect_socket
        OpSend FD, BCK
        OpSend FD, FWD
        Http_send_query
          a_Http_make_query_str
            OpSend, BCK
              IO_submit
                a_IOwatch_add_fd (DIO_WRITE, ...)


  Note about 'web' structures. They're created using a_Web_new().
The  web.c  module  keeps a list of valid webs, so anytime you're
unsure  of  a  weak  reference  to  'web', it can be checked with
a_Web_valid(web).



------------
Dpi protocol
------------


      Query:                              |
                                          .
          1B --> 1B  1B --> 1B      -->   |  -------------.
      .----.     .----.     .----.        .               |
I     |Capi|     |Dpi |     | IO |        |               |
      '----'     '----'     '----'        .               |
          1F <-- 1F  1F <-- 1F            |               V
                                          .
                                          |           [Server]
                                          .
      Answer (same as HTTP):              |               |
                                          .               |
          2B --> 2B  2B --> 2B            |               |
      .----.     .----.     .----.        .               |
II    |Capi|     |Dpi |     | IO |        |               |
      '----'     '----'     '----'        .               |
          2F <-- 2F  2F <-- 2F      <--   |  <------------'
                                          .
                                          |


CCC Construction:

  a_Capi_open_url()  calls  a_Capi_dpi_send_cmd()  when  the  URL
belongs to a dpi and it is not cached.

  a_Capi_dpi_send_cmd()  builds  both the Answer and Query chains
at  once (Answer first then Query), in the same way as HTTP does.
Note  that  the  answer chain is the same for both, and the query
chain only differs in the module in the middle ([http] or [dpi]).


Function calls for DPI CCC
--------------------------

  a_Capi_open_url
    a_Capi_dpi_send_cmd
      Capi OpStart 2B (answer)    [Capi] --> [dpi]  --> [IO]
      Capi OpStart 1B (query)     [Capi] --> [http] --> [IO]
      a_Cache_open_url
        [...]


Normal termination:

  When  the dpi server is done, it closes the FD, and OpEnd flows
from  IO  to  Capi (answer branch). When in Capi, capi propagates
OpEnd to the query branch.

Abnormal termination:

  The  transfer may be aborted by a_Capi_conn_abort_by_url(). The
OpAbort is not yet standardized and has an ad-hoc implementation.
One idea is to have OpAbort always propagate BCK and then FWD and
to jump into the other chain when it gets to [Capi].


Debugging CCC
-------------

  A  simple way to "look" inside it, is to "#define VERBOSE 1" in
chain.c,  and  then to follow its work with a printed copy of the
diagrams in this document.

  Each new data request generates a CCC, so if you want to debug,
it's  good  to refine the testcase to the minimum possible number
of connections.

