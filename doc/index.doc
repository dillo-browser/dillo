/** \mainpage

<h2>Overview</h2>

This is a list of documents to start with:

<ul>
<li> \ref lout
<li> \ref dw-overview (map at \ref dw-map)
</ul>

Currently, a document \ref fltk-problems is maintained, ideally, it
will be removed soon.

<h2>Historical</h2>

<h3>Replacements for GTK+ and GLib</h3>

There are several classes etc., which are used for tasks formerly (in the GTK+
version of dillo) achieved by GtkObject (in 1.2.x, this is part of Gtk+) and
GLib. For an overview on all this, take a look at \ref lout.

GtkObject is replaced by the following:

<ul>
<li> lout::object::Object is a common base class for many classes used
     dillo. In the namespace lout::object, there are also some more common
     classes and interfaces.

<li> A sub class of lout::object::Object is
     lout::identity::IdentifiableObject, which allows to determine the
     class at run-time (equivalent to GTK_CHECK_CAST in GtkObject).

<li> For signals, there is the namespace lout::signal.
</ul>

Hash tables, linked lists etc. can be found in the lout::container namespace,
several useful macros from GLib have been implemented as inline functions
in the lout::misc namespace.

As an alternative to the macros defined in list.h, there is also a template
class, lout::misc::SimpleVector, which does the same.

<h3>Changes in Dw</h3>

If you have been familiar with Dw before, take a look at \ref dw-changes.

*/
