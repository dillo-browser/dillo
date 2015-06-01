Aug 2003, Jorge Arellano Cid,
           Ferdi Franceschini --
Last update: Nov 2009


                               ------
                                dpid
                               ------

-------------
Nomenclature:
-------------

  dpi:
    generic term referring to dillo's plugin system (version1).

  dpi1:
    specific term for dillo's plugin spec version 1.
    at: http://www.dillo.org/dpi1.html

  dpi program:
    any plugin program itself.

  dpi framework:
    the code base inside and outside dillo that makes dpi1
    working possible (it doesn't include dpi programs).

  dpip:
    dillo plugin protocol. The HTML/XML like set of command tags
    and information that goes inside the communication sockets.
    Note: not yet fully defined, but functional.
    Note2: it was designed to be extensible.

  dpid:
    dillo plugin daemon.

  server plugin:
    A plugin that is capable of accepting connections on a socket.  Dpid will
    never run more than one instance of a server plugin at a time.

  filter plugin:
    A dpi program that reads from stdin and writes to stdout, and that
    exits after its task is done (they don't remain as server plugins).
    Warning, dpid will run multiple instances of filter plugins if requested.

-----------
About dpid:
-----------

  * dpid is a program which manages dpi connections.
  * dpid is a daemon that serves dillo using IDS sockets.
  * dpid launches dpi programs and arranges socket communication
    between the dpi program and dillo.

 The  concept  and  motivation  is  similar to that of inetd. The
plugin  manager  (dpid) listens for a service request on a socket
and  returns  the  socket/port  pair of a plugin that handles the
service.  It  also watches sockets of inactive plugins and starts
them when a connection is requested.


-----------------------------------------------------------
What's the problem with managing dpi programs inside dillo?
-----------------------------------------------------------

  That's the other way to handle it, but it started to show some
problems (briefly outlined here):

  * When having two or more running instances of Dillo, one
    should prevail, and take control of dpi managing (but all
    dillos carry the managing code).
  * If the managing-dillo exits, it must pass control to another
    instance, or leave it void if there's no other dillo running!
  * The need to synchronize all the running instances of
    dillo arises.
  * If the controlling instance finishes and quits, all the
    dpi-program PIDs are lost.
  * Terminating hanged dpis is hard if it's not done with signals
    (PIDs)
  * Forks can be expensive (Dillo had to fork its dpis).
  * When a managing dillo exits, the new one is no longer the
    parent of the forked dpis.
  * If Unix domain sockets for the dpis were to be named
    randomly, it gets very hard to recover their names if the
    controlling instance of dillo exits and another must "take
    over" the managing.
  * It increments dillo's core size.
  * If dillo hangs/crashes, dpi activity is lost (e.g. downloads)
  * ...

  That's why the managing daemon scheme was chosen.


----------------------
What does dpid handle?
----------------------

  It solves all the above mentioned shortcomings and also can do:

  *  Multiple dillos:
     dpid can communicate and serve more than one instance
     of dillo.

  *  Multiple dillo windows:
     two or more windows of the same dillo instance accessing dpis
     at the same time.

  *  Different implementations of the same service
     dpi  programs  ("dpis")  are  just  an  implementation  of a
     service.  There's no problem in implementing a different one
     for the same service (e.g. downloads).

  *  Upgrading a service:
     to   a  new  version  or  implementation  without  requiring
     patching dillo's core or even bringing down the dpid.


  And  finally,  being  aware  that  this  design can support the
following functions is very helpful:

             SCHEME                      Example
  ------------------------------------------------------------
  * "one demand/one response"         man, preferences, ...
  * "resident while working"          downloads, mp3, ...
  * "resident until exit request"     bookmarks, ...

  * "one client only"                 cd burner, ...
  * "one client per instance"         man, ...
  * "multiple clients/one instance"   downloads, cookies ...


--------
Features
--------
  * Dpi programs go in: "EPREFIX/dillo/dpi" or "~/.dillo/dpi". The binaries
    are named <name>.dpi as "bookmarks.dpi" and <name>.filter.dpi as in
    "hello.filter.dpi".  The  ".filter"  plugins simply read from stdin
    and write to stdout.
  * Register/update/remove dpis from list of available dpis when a
    'register_all' command is received.
  * dpid terminates when it receives a 'DpiBye' command.
  * dpis can be terminated with a 'DpiBye' command.
  * dpidc control program for dpid, currently allows register and stop.


-----
todo:
-----

 These features are already designed, waiting for implementation:

  * dpidc remove     // May be not necessary after all...


-----------------
How does it work?
-----------------

o    on startup dpid reads dpidrc for the path to the dpi directory
     (usually EPREFIX/lib/dillo/dpi). ~/.dillo/dpi is scanned first.

o    both directories are scanned for the list of available plugins.
     ~/.dillo/dpi overrides system-wide dpis.

o    next it creates internet domain sockets for the available plugins and
     then listens for service requests on its own socket,
     and for connections to the sockets of inactive plugins.

o    dpid returns the port of a plugin's socket when a client (dillo)
     requests a service.

o    if the requested plugin is a 'server' then
     1) dpid stops watching the socket for activity
     2) forks and starts the plugin
     3) resumes watching the socket when the plugin exits

o    if the requested plugin is a 'filter' then
     1) dpid accepts the connection
     2) maps the socket fd to stdin/stdout (with dup2)
     3) forks and starts the plugin
     4) continues to watch the socket for new connections




---------------------------
dpi service process diagram
---------------------------

  These drawings should be worth a thousand words! :)


(I)
     .--- s1 s2 s3 ... sn
     |
  [dpid]                     [dillo]
     |
     '--- srs

  The dpid is running listening on several sockets.


(II)
     .--- s1 s2 s3 ... sn
     |
  [dpid]                     [dillo]
     |                          |
     '--- srs ------------------'

  dillo needs a service so it connects to the service request
  socket of the dpid (srs) and asks for the socket name of the
  required plugin (using dpip).


(III)
     .--- s1 s2 s3 ... sn
     |          |
  [dpid]        |            [dillo]
     |          |               |
     '--- srs   '---------------'

  then it connects to that socket (s3, still serviced by dpid!)


(IV)
     .--- s1 s2 s3 ... sn
     |          |
 .[dpid]        |            [dillo]
 .   |          |               |
 .   '--- srs   '---------------'
 .
 .............[dpi program]

  when s3 has activity (incoming data), dpid forks the dpi
  program for it...


(V)
     .--- s1 s2 (s3) ... sn
     |
  [dpid]                     [dillo]
     |                          |
     '--- srs   .---------------'
                |
              [dpi program]

  ... and lets it "to take over" the socket.

  Once  there's  a  socket  channel  for dpi and dillo, the whole
communication  process  takes  place until the task is done. When
the dpi program exits, dpid resumes listening on the socket (s3).


--------------------------------
So, how do I make my own plugin?
--------------------------------

  Maybe  the  simplest  way to get started is to understand a few
concepts  and  then to use the hands-on method by using/modifying
the  hello  dpi.  It's  designed  as an example to get developers
started.

  ---------
  Concepts:
  ---------

    * Dillo plugins work by communicating two processes: dillo
      and the dpi.
    * The underlying protocol (DPIP) has a uniform API which is
      powerful  enough  for both blocking and nonblocking IO, and
      filter or server dpis.
    * The simplest example is one-request one-answer (for example
      dillo  asks  for  a  URL and the dpi sends it). You'll find
      this and more complex examples in hello.c

  First, you should get familiar with the hello dpi as a user:

    $dillo dpi:/hello/

  Once  you've  played  enough  with  it,  start reading the well
commented code in hello.c and start making changes!


  ---------------
  Debugging a dpi
  ---------------

  The  simplest way is to add printf-like feedback using the MSG*
macros.  You  can  start  the dpid by hand on a terminal to force
messages  to  go  there.  Filter  dpis use sdterr and server dpis
stdout. 

  Sometimes  more  complex dpis need more than MSG*. In this case
you can use gdb like this.

    1.- Add an sleep(20) statement just after the dpi starts.
    2.- Start  dillo and issue a request for your dpi. This will
        get your dpi started.
    3.- Standing in the dpi source directory:
        ps aux|grep dpi
    4.- Take note of the dpi's PID and start gdb, then:
        (gdb) attach <PID>
    5.- Continue from there...


  ------------
  Final Notes:
  ------------

  1.-  If  you  already  understand the hello dpi and want to try
something more advanced:

    * bookmarks.c is a good example of a blocking server
    * file.c is an advanced example of a server handling multiple
      non-blocking connections with select().

  2.-   Multiple   instances  of  a  filter  plugin  may  be  run
concurrently, this could be a problem if your plugin records data
in  a  file,  however  it  is safe if you simply write to stdout.
Alternatively  you  could write a 'server' plugin instead as they
are guaranteed not to run concurrently.

  3.- The hardest part is to try to modify the dpi framework code
inside  dillo; you have been warned! It already supports a lot of
functionality,  but if you need to do some very custom stuff, try
extending the "chat" command, or asking in dillo-dev.



          >>>>>>>>>>>>>>>>>>>>>       <<<<<<<<<<<<<<<<<<<<<

