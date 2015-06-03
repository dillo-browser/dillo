/** \page uml-legend UML Legend

This page describes the notation for several diagrams used in the
documentation, which is a slight variation of UML.


<h2>Classes</h2>

Classes are represented by boxes, containing there names:

\dot
digraph G {
   node [shape=record, fontname=Helvetica, fontsize=10];
   fontname=Helvetica; fontsize=8;
   "Concrete Class";
   "Abstract Class" [color="#a0a0a0"];
   Interface [color="#ff8080"];
}
\enddot

(In most cases, the attributes and operations are left away, for
better readibility. Just click on it, to get to the detailed
description.)

Of course, in C++, there are no interfaces, but here, we call a class,
which has only virtual abstract methods, and so does not provide any
functionality, an interface.

Templates get a yellow background color:

\dot
digraph G {
   node [shape=record, fontname=Helvetica, fontsize=10,
         fillcolor="#ffffc0", style="filled"];
   fontname=Helvetica; fontsize=8;
   "Concrete Class Template";
   "Abstract Class Template" [color="#a0a0a0"];
   "Interface Template" [color="#ff8080"];
}
\enddot


<h2>Objects</h2>

In some cases, an examle for a concrete constellation of objects is
shown. An object is represented by a box containing a name and the
class, separated by a colon.

\dot
digraph G {
   node [shape=record, fontname=Helvetica, fontsize=10];
   edge [arrowhead="open", labelfontname=Helvetica, labelfontsize=10,
         color="#404040", labelfontcolor="#000080"];
   fontname=Helvetica; fontsize=10;

   "x: A" -> "y1: B";
   "x: A" -> "y2: B";
}
\enddot

The names (\em x, \em y, and \em z) are only meant within the context
of the diagram, there needs not to be a relation to the actual names
in the program. They should be unique within the diagram.

Classes and objects may be mixed in one diagram.


<h2>Associations</h2>

\dot
digraph G {
   node [shape=record, fontname=Helvetica, fontsize=10];
   edge [arrowhead="open", labelfontname=Helvetica, labelfontsize=10,
         color="#404040", labelfontcolor="#000080",
         fontname=Helvetica, fontsize=10, fontcolor="#000080"];
   fontname=Helvetica; fontsize=10;
   A -> B [headlabel="*", taillabel="1", label="x"];
}
\enddot

In this example, one instance of A refers to an arbitrary number of B
instances (denoted by the "*"), and each instance of B is referred by
exactly one ("1") A. The label \em x is the name of the association,
in most cases the name of the field, e.g. A::x.

Possible other values for the \em multiplicity:

<ul>
<li> a concrete number, in most cases "1",
<li> a range, e.g. "0..1",
<li> "*", denoting an arbitrary number.
</ul>


<h2>Implementations and Inheritance</h2>

\dot
digraph G {
   node [shape=record, fontname=Helvetica, fontsize=10];
   edge [arrowhead="none", dir="both", arrowtail="empty",
         labelfontname=Helvetica, labelfontsize=10, color="#404040",
         labelfontcolor="#000080"];
   fontname=Helvetica; fontsize=10;
   A[color="#ff8080"];
   B[color="#ff8080"];
   C;
   D;
   A -> B;
   A -> C [style="dashed"];
   C -> D;
}
\enddot

In this example,

<ul>
<li> the interface B extends the interface A,
<li> the class C implements the interface A, and
<li> the class D extends the class C.
</ul>


<h2>Template Instantiations</h2>

Template instantiations are shown as own classes/interfaces, the
instantiation by the template is shown by a yellow dashed arrow:

\dot
digraph G {
   node [shape=record, fontname=Helvetica, fontsize=10];
   edge [arrowhead="none", arrowtail="empty", dir="both",
         labelfontname=Helvetica, labelfontsize=10, color="#404040",
         labelfontcolor="#000080"];
   fontname=Helvetica; fontsize=10;

   A[color="#ff8080"];
   B[color="#ff8080"];
   C[color="#ff8080", fillcolor="#ffffc0", style="filled"];
   C_A[color="#ff8080", label="C \<A\>"];
   C_B[color="#ff8080", label="C \<A\>"];
   D;

   C -> C_A [arrowhead="open", arrowtail="none", style="dashed",
             color="#808000"];
   C -> C_B [arrowhead="open", arrowtail="none", style="dashed",
             color="#808000"];
   A -> C_A;
   B -> C_B;
   C_A -> D [style="dashed"];
}
\enddot

In this example, the interface template C uses the template argument
as super interface.


<h2>Packages</h2>

Packages are presented by dashed rectangles:

\dot
digraph G {
   node [shape=record, fontname=Helvetica, fontsize=10];
   edge [arrowhead="none", arrowtail="empty", dir="both",
         labelfontname=Helvetica, labelfontsize=10, color="#404040",
         labelfontcolor="#000080"];
   fontname=Helvetica; fontsize=10;

   subgraph cluster_1 {
      style="dashed"; color="#000080"; fontname=Helvetica; fontsize=10;
      label="package 1";

      A;
      B [color="#a0a0a0"];
   }

   subgraph cluster_2 {
      style="dashed"; color="#000080"; fontname=Helvetica; fontsize=10;
      label="package 2";

      C;
      D [color="#a0a0a0"];
      E
   }

   A -> C;
   B -> D;
   D -> E;
   E -> A [arrowhead="open", arrowtail="none"];
}
\enddot

Packages may be nested.

*/