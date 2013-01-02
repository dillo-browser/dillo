/** \page lout Lots of Useful Tools

In the "lout" directory, there are some common base functionality for
C++. Most is described as doxygen comments, this text gives an
overview.

<h2>Common Base Class</h2>

Many classes are derived from lout::object::Object, which defines some
general methods. See there for more information.

For the case, that you need primitive C++ types, there are some
wrappers:

<table>
<tr><th>C++ Type         <th>Wrapper Class
<tr><td>void*            <td>lout::object::Pointer
<tr><td>specific pointer <td>lout::object::TypedPointer (template class)
<tr><td>int              <td>lout::object::Integer
<tr><td>const char*      <td>lout::object::ConstString
<tr><td>char*            <td>lout::object::String
</table>


<h2>Containers</h2>

In the namespace lout::container, several container classes are defined,
which all deal with instances of lout::object::Object.

<h3>Untyped Containers</h3>

In lout::container::untyped, there are the following containers:

<ul>
<li>lout::container::untyped::Vector, a dynamically increases array,
<li>lout::container::untyped::List, a linked list,
<li>lout::container::untyped::HashTable, a hash table, and
<li>lout::container::untyped::Stack, a stack.
</ul>

All provide specific methods, but since they have a common base class,
lout::container::untyped::Collection, they all provide iterators, by the
method lout::container::untyped::Collection::iterator.

<h3>Typed Containers</h3>

lout::container::typed provides wrappers for the container classes defined
in lout::container::untyped, which are more type safe, by using C++
templates.


<h2>Signals</h2>

For how to connect objects at run-time (to reduce dependencies), take a
look at the lout::signal namespace.

There is also a base class lout::signal::ObservedObject, which implements
signals for deletion.


<h2>Debugging</h2>

In debug.hh, there are some some useful macros for debugging messages,
see the file for mor informations.


<h2>Identifying Classes at Runtime</h2>

If the class of an object must be identified at runtime,
lout::identity::IdentifiableObject should be used as the base class,
see there for more details.


<h2>Miscellaneous</h2>

The lout::misc namespace provides several miscellaneous stuff:

<ul>
<li> In some contexts, it is necessary to compare objects
     (less/greater), for this, also lout::misc::Comparable must be
     implemented. For example., lout::container::untyped::Vector::sort and
     lout::container::typed::Vector::sort cast the elements to
     lout::misc::Comparable. This can be mixed with lout::object::Object.
<li> lout::misc::SimpleVector, a simple, template based vector class
     (not depending on lout::object::Object) (a variant for handling a
     special case in an efficient way is lout::misc::NotSoSimpleVector),
<li> lout::misc::StringBuffer, class for fast concatenation of a large number
     of strings,
<li> lout::misc::BitSet implements a bitset.
<li> useful (template) functions (lout::misc::min, lout::misc::max), and
<li> some functions useful for runtime checks (lout::misc::assert,
     lout::misc::assertNotReached).
</ul>

*/
