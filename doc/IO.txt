
This is the updated base of a paper I wrote with Horst.
It provides a good introduction to Dillo's internals.
(Highly recommended if you plan to patch or develop in Dillo)

It may not be exactly accurate (it's quite old), but it explains
the theory behind in some detail, so it's more than recommended
reading material.
--Jcid


-----------------------------------------------------
Parallel network programming of the Dillo web browser
-----------------------------------------------------

 Jorge Arellano-Cid <jcid@dillo.org>
 Horst H. von Brand <vonbrand@inf.utfsm.cl>


--------
Abstract
--------

   Network  programs  face  several delay sources when sending or
retrieving  data.  This  is  particularly problematic in programs
which interact directly with the user, most notably web browsers.
We  present  a hybrid approach using threads communicated through
pipes  and  signal  driven  I/O, which allows a non-blocking main
thread and overlapping waiting times.


------------
Introduction
------------

   The Dillo project didn't start from scratch but mainly working
on  the  code base of gzilla (a light web browser written by Raph
Levien). As the project went by, the code of the whole source was
standardized,  and the networking engine was replaced with a new,
faster  design.  Later,  the  parser  was  changed,  the streamed
handling  of data and its error control was put under the control
of the CCC (Concomitant Control Chain), and the old widget system
was  replaced  with  a new one (Dw). The source code is currently
regarded   as   "very   stable   beta",   and   is  available  at
<http://www.dillo.org>. Dillo is a project licensed under the GNU
General Public License.

   This  paper covers basic design aspects of the hybrid approach
that  the  Dillo  web  browser  uses  to  solve  several  latency
problems.  After  introducing  the  main  delay-sources, the main
points of the hybrid design will be addressed.


-------------
Delay sources
-------------

   Network  programs  face several delay-sources while sending or
retrieving  data.  In  the particular case of a web browser, they
are found in:

  DNS querying:
    The time required to solve a name.

  Initiating the TCP connection:
    The three way handshake of the TCP protocol.

  Sending the query:
    The time spent uploading queries to the remote server.

  Retrieving data:
    The time spent expecting and receiving the query answer.

  Closing the TCP connection:
    The four packet-sending closing sequence of the TCP protocol.

   In  a  WAN  context,  every  single  item  of this list has an
associated  delay that is non deterministic and often measured in
seconds. If we add several connections per browsed page (each one
requiring  at  least  the 4 last steps), the total latency can be
considerable.


-----------------------------------
The traditional (blocking) approach
-----------------------------------

   The main problems with the blocking approach are:

     When issuing an operation that can't be completed
     immediately, the process is put to sleep waiting for
     completion, and the program doesn't do any other
     processing in the meantime.

     When waiting for a specific socket operation to complete,
     packets that belong to other connections may be arriving,
     and have to wait for service.

     Web browsers handle many small transactions,
     if waiting times are not overlapped
     the latency perceived by the user can be very annoying.

     If the user interface is just put to sleep during network
     operations, the program becomes unresponsive, confusing
     and perhaps alarming the user.

     Not overlapping waiting times and processing makes
     graphical rendering (which is arguably the central function
     of a browser) unnecessarily slow.


---------------------
Dillo's hybrid design
---------------------

   Dillo  uses  threads  and  signal  driven  I/O  extensively to
overlap   waiting   times  and  computation.  Handling  the  user
interface  in a thread that never blocks gives a good interactive
``feel.''  The  use of GTK+, a sophisticated widget framework for
graphical  user  interfaces,  helped very much to accomplish this
goal.  All the interface, rendering and I/O engine was built upon
its facilities.

   The  design  is  said to be ``hybrid'' because it uses threads
for  DNS  querying and reading local files, and signal driven I/O
for  TCP  connections.  The  threaded  DNS  scheme is potentially
concurrent  (this  depends on underlying hardware), while the I/O
handling   (both   local   files   and   remote  connections)  is
definitively parallel.

   To  simplify  the  structure  of  the browser, local files are
encapsulated  into  HTTP streams and presented to the rest of the
browser  as  such, in exactly the same way a remote connection is
handled.  To  create  this  illusion,  a thread is launched. This
thread  opens  a  pipe  to  the  browser,  it then synthesizes an
appropriate  HTTP  header, sends it together with the file to the
browser  proper.  In  this way, all the browser sees is a handle,
the  data on it can come from a remote connection or from a local
file.

   To  handle  a remote connection is more complex. In this case,
the  browser  asks the cache manager for the URL. The name in the
URL  has  to  be  resolved  through  the DNS engine, a socket TCP
connection  must be established, the HTTP request has to be sent,
and  finally  the  result  retrieved. Each of the steps mentioned
could  give  rise to errors, which have to be handled and somehow
communicated to the rest of the program. For performance reasons,
it  is  critical that responses are cached locally, so the remote
connection  doesn't  directly  hand over the data to the browser;
the  response is passed to the cache manager which then relays it
to  the rest of the browser. The DNS engine caches DNS responses,
and  either  answers  them from the cache or by querying the DNS.
Querying  is  done  in a separate thread, so that the rest of the
browser isn't blocked by long waits here.

   The  activities  mentioned do not happen strictly in the order
stated  above.  It  is  even possible that several URLs are being
handled  at  the  same  time,  in  order  to  overlap waiting and
downloading.   The   functions  called  directly  from  the  user
interface   have   to  return  quickly  to  maintain  interactive
response.  Sometimes they return connection handlers that haven't
been completely set up yet. As stated, I/O is signal-driven, when
one  of  the  descriptors  is ready for data transfer (reading or
writing), it wakes up the I/O engine.

   Data transfer between threads inside the browser is handled by
pipes,  shared  memory  is  little used. This almost obviates the
need for explicit synchronization, which is one of the main areas
of  complexity and bugs in concurrent programs. Dillo handles its
threads  in  a way that its developers can think of it as running
on a single thread of control. This is accomplished by making the
DNS  engine  call-backs  happen  within  the  main thread, and by
isolating file loading with pipes.

   Using threads in this way has three big advantages:

     The browser doesn't block when one of its child threads
     blocks. In particular, the user interface is responsive
     even while resolving a name or downloading a file.

     Developers don't need to deal with complex concurrent
     concerns. Concurrency is hard to handle,  and few developers
     are adept at this. This gives access a much larger pool of
     potential developers, something which can be critical
     in an open-source development project.

     By making the code mostly sequential, debugging the code
     with traditional tools like gdb is possible. Debugging
     parallel programs is very hard, and appropriate tools are
     hard to come by.

   Because  of  simplicity and portability concerns, DNS querying
is  done  in  a  separate  thread. The standard C library doesn't
provide  a  function for making DNS queries that don't block. The
alternative  is  to implement a new, custom DNS querying function
that doesn't block. This is certainly a complex task, integrating
this  mechanism  into the thread structure of the program is much
simpler.

   Using  a  thread  and  a  pipe  to  read  a  local file adds a
buffering step to the process (and a certain latency), but it has
a couple of significative advantages:

     By handling local files in the same way as remote
     connections, a significant amount of code is reused.

     A preprocessing step of the file data can be added easily,
     if needed. In fact, the file is encapsulated into an HTTP
     data stream.


-----------
DNS queries
-----------

   Dillo handles DNS queries with threads, letting a child thread
wait  until  the  DNS server answers the request. When the answer
arrives,  a call-back function is called, and the program resumes
what  it  was doing at DNS-request time. The interesting thing is
that  the  call-back  happens in the main thread, while the child
thread  simply  exits  when  done.  This is implemented through a
server-channel design.


The server channel
------------------

   There  is  one  thread  for each channel, and each channel can
have  multiple  clients. When the program requests an IP address,
the server first looks for a cached match; if it hits, the client
call-back  is  invoked immediately, but if not, the client is put
into  a  queue,  a thread is spawned to query the DNS, and a GTK+
idle  client  is  set  to poll the channel 5~times per second for
completion,  and  when  it finally succeeds, every client of that
channel is serviced.

   This  scheme  allows all the further processing to continue on
the same thread it began: the main thread.


------------------------
Handling TCP connections
------------------------

   Establishing   a   TCP  connection  requires  the  well  known
three-way handshake packet-sending sequence. Depending on network
traffic  and several other issues, significant delay can occur at
this phase.

   Dillo  handles the connection by a non blocking socket scheme.
Basically,  a socket file descriptor of AF_INET type is requested
and set to non-blocking I/O. When the DNS server has resolved the
name, the socket connection process begins by calling connect(2);
  {We use the Unix convention of identifying the manual section
   where the concept is described, in this case
   section 2 (system calls).}
which  returns  immediately  with an EINPROGRESS error.

   After  the  connection  reaches the EINPROGRESS ``state,'' the
socket  waits in background until connection succeeds (or fails),
when  that  happens, a callback function is awaked to perform the
following  steps: set the I/O engine to send the query and expect
its answer (both in background).

   The  advantage  of  this scheme is that every required step is
quickly  done  without  blocking the browser. Finally, the socket
will generate a signal whenever I/O is possible.


----------------
Handling queries
----------------

   In  the case of a HTTP URL, queries typically translate into a
short  transmission  (the  HTTP  query)  and  a lengthy retrieval
process.  Queries  are  not  always  short though, specially when
requesting  forms  (all  the  form  data  is  attached within the
query), and also when requesting CGI programs.

   Regardless  of  query  length,  query  sending  is  handled in
background.  The thread that was initiated at TCP connecting time
has all the transmission framework already set up; at this point,
packet   sending   is   just   a   matter   of  waiting  for  the
write  signal  (G_IO_OUT) to come and then sending the data. When
the  socket  gets  ready for transmission, the data is sent using
IO_write.


--------------
Receiving data
--------------

   Although  conceptually  similar to sending queries, retrieving
data is very different as the data received can easily exceed the
size  of  the query by many orders of magnitude (for example when
downloading  images or files). This is one of the main sources of
latency,  the  retrieval can take several seconds or even minutes
when downloading large files.

   The  data  retrieving process for a single file, that began by
setting up the expecting framework at TCP connecting time, simply
waits  for  the  read  signal  (G_IO_IN).  When  it  happens, the
low-level   I/O  engine  gets  called,  the  data  is  read  into
pre-allocated   buffers   and   the  appropriate  call-backs  are
performed.  Technically,  whenever  a G_IO_IN event is generated,
data  is  received  from the socket file descriptor, by using the
IO_read function. This iterative process finishes upon EOF (or on
an error condition).


----------------------
Closing the connection
----------------------

   Closing  a  TCP connection requires four data segments, not an
impressive  amount  but  twice  the round trip time, which can be
substantial.  When  data  retrieval  finishes,  socket closing is
triggered. There's nothing but a IO_close_fd call on the socket's
file  descriptor.  This  process was originally designed to split
the  four  segment  close into two partial closes, one when query
sending is done and the other when all data is in. This scheme is
not currently used because the write close also stops the reading
part.


The low-level I/O engine
------------------------

   Dillo  I/O  is carried out in the background. This is achieved
by  using  low level file descriptors and signals. Anytime a file
descriptor  shows  activity,  a  signal  is raised and the signal
handler takes care of the I/O.

   The  low-level  I/O  engine  ("I/O  engine"  from here on) was
designed  as  an  internal  abstraction layer for background file
descriptor  activity.  It  is  intended  to  be used by the cache
module  only;  higher level routines should ask the cache for its
URLs.  Every  operation  that  is  meant  to  be  carried  out in
background  should  be  handled by the I/O engine. In the case of
TCP sockets, they are created and submitted to the I/O engine for
any further processing.

   The  submitting process (client) must fill a request structure
and let the I/O engine handle the file descriptor activity, until
it  receives a call-back for finally processing the data. This is
better understood by examining the request structure:

 typedef struct {
    gint Key;              /* Primary Key (for klist) */
    gint Op;               /* IORead | IOWrite | IOWrites */
    gint FD;               /* Current File Descriptor */
    gint Flags;            /* Flag array */
    glong Status;          /* Number of bytes read, or -errno code */

    void *Buf;             /* Buffer place */
    size_t BufSize;        /* Buffer length */
    void *BufStart;        /* PRIVATE: only used inside IO.c! */

    void *ExtData;         /* External data reference (not used by IO.c) */
    void *Info;            /* CCC Info structure for this IO */
    GIOChannel *GioCh;     /* IO channel */
    guint watch_id;        /* glib's event source id */
 } IOData_t;

   To request an I/O operation, this structure must be filled and
passed to the I/O engine.

  'Op' and 'Buf' and 'BufSize' MUST be provided.

  'ExtData' MAY be provided.

  'Status',  'FD'  and  'GioCh'  are  set  by I/O engine internal
routines.

   When  there  is new data in the file descriptor, 'IO_callback'
gets  called  (by  glib).  Only  after  the  I/O  engine finishes
processing the data are the upper layers notified.


The I/O engine transfer buffer
------------------------------

   The  'Buf' and  'BufSize'  fields  of  the  request  structure
provide  the transfer buffer for each operation. This buffer must
be set by the client (to increase performance by avoiding copying
data).

   On  reads,  the client specifies the amount and where to place
the retrieved data; on writes, it specifies the amount and source
of  the  data  segment  that  is to be sent. Although this scheme
increases  complexity,  it has proven very fast and powerful. For
instance,  when  the  size  of  a document is known in advance, a
buffer for all the data can be allocated at once, eliminating the
need for multiple memory reallocations. Even more, if the size is
known  and the data transfer is taking the form of multiple small
chunks  of  data,  the  client  only  needs  to  update 'Buf' and
BufSize'  to  point  to  the  next byte in its large preallocated
reception  buffer  (by  adding  the  chunk size to 'Buf'). On the
other  hand,  if the size of the transfer isn't known in advance,
the  reception  buffer  can remain untouched until the connection
closes,  but  the  client  must  then accomplish the usual buffer
copying and reallocation.

   The  I/O  engine  also  lets  the client specify a full length
transfer  buffer  when  sending data. It doesn't matter (from the
client's  point  of  view) if the data fits in a single packet or
not,  it's  the I/O engine's job to divide it into smaller chunks
if needed and to perform the operation accordingly.


------------------------------------------
Handling multiple simultaneous connections
------------------------------------------

   The  previous sections describe the internal work for a single
connection,  the  I/O engine handles several of them in parallel.
This  is the normal downloading behavior of a web page. Normally,
after   retrieving   the   main  document  (HTML  code),  several
references  to  other files (typically images) and sometimes even
to  other  sites  (mostly advertising today) are found inside the
page.  In  order  to parse and complete the page rendering, those
other  documents  must  be  fetched  and  displayed, so it is not
uncommon  to  have  multiple  downloading  connections (every one
requiring the whole fetching process) happening at the same time.

   Even  though  socket  activity  can  reach  a hectic pace, the
browser  never  blocks.  Note also that the I/O engine is the one
that  directs  the  execution flow of the program by triggering a
call-back  chain whenever a file descriptor operation succeeds or
fails.

   A  key point for this multiple call-back chained I/O engine is
that  every  single  function  in the chain must be guaranteed to
return  quickly.  Otherwise,  the  whole  system  blocks until it
returns.


-----------
Conclusions
-----------

   Dillo is currently in very stable beta state. It already shows
impressive  performance,  and  its  interactive  ``feel'' is much
better than that of other web browsers.

   The modular structure of Dillo, and its reliance on GTK1 allow
it  to  be  very  small.  Not every feature of HTML-4.01 has been
implemented  yet,  but  no  significant  problems are foreseen in
doing this.

   The  fact  that  Dillo's  central  I/O engine is written using
advanced  features  of  POSIX  and  TCP/IP  networking  makes its
performance  possible, but on the other hand this also means that
only a fraction of the interested hackers are able to work on it.

   A  simple code base is critical when trying to attract hackers
to  work  on  a  project  like this one. Using the GTK+ framework
helped  both  in  creating  the  graphical  user interface and in
handling  the  concurrency  inside the browser. By having threads
communicate  through  pipes the need for explicit synchronization
is  almost  completely  eliminated,  and  with  it  most  of  the
complexity of concurrent programming disappears.

   A  clean,  strictly  applied  layering approach based on clear
abstractions  is  vital  in  each  programming  project.  A good,
supportive framework is of much help here.


