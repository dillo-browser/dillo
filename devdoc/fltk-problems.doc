/** \page fltk-problems Problems with FLTK

<h2>dw::fltk::FltkViewport</h2>

Current problems:

<ul>
<li> How should dw::fltk::FltkViewport::cancelQueueDraw be implemented?

<li> If the value of a scrollbar is changed by the program, not the user,
     the callback seems not to be called. Can this be assured?

<li> The same for dw::fltk::FltkViewport::layout?

<li> Also, the problems with the widgets seems to work. Also sure?

<li> When drawing, clipping of 32 bit values is not working properly.

<li> The item group within a selection widget (menu) should not be selectable.
</ul>


<h2>dw::fltk::FltkPlatform</h2>

<ul>
<li> There is the problem, that fltk::font always returns a font, the
     required one, or a replacements. The latter is not wanted in all
     cases, e.g. when several fonts are tested. Perhaps, this could be
     solved by searching in the font list. <i>[This was true of fltk2.
     What is the state of font handling now with fltk-1.3?]</i>

<li> Distinction between italics and oblique would be nice
     (dw::fltk::FltkFont::FltkFont).
</ul>


<h2>dw::fltk::ui::FltkCheckButtonResource</h2>

Groups of Fl_Radio_Button must be added to one Fl_Group, which is
not possible in this context. There are two alternatives:

<ol>
<li>there is a more flexible way to group radio buttons, or
<li>radio buttons are not grouped, instead, grouping (especially
    unchecking other buttons) is done by the application.
</ol>

(This is mostly solved.)

<h2>dw::fltk::FltkImgbuf</h2>

Alpha transparency should be best abstracted by FLTK itself. If not,
perhaps different implementations for different window systems could
be used. Then, it is for X necessary to use GCs with clipping masks.


<h2>dw::fltk::ui::ComplexButton</h2>

Unfortunately, FLTK does not provide a button with Fl_Group as parent, so
that children may be added to the button. dw::fltk::ui::ComplexButton does
exactly this, and is, in an ugly way, a modified copy of the FLTK
button.

It would be nice, if this is merged with the standard FLTK
button. Furthermore, setting the type is strange.

If the files do not compile, it may be useful to create a new one from
the FLTK source:

<ol>
<li> Copy Fl_Button.H from FLTK to dw/fltkcomplexbutton.hh and
     src/Button.cxx to dw/fltkcomplexbutton.cc.

<li> In both files, rename "Button" to "ComplexButton". Automatic replacing
     should work.

<li> Apply the changes below.
</ol>

The following changes should be applied manually.

<h3>Changes in fltkcomplexbutton.hh</h3>

First of all, the \#define's for avoiding multiple includes:

\code
-#ifndef fltk_ComplexButton_h // fltk_Button_h formerly
-#define fltk_ComplexButton_h
+#ifndef __FLTK_COMPLEX_BUTTON_HH__
+#define __FLTK_COMPLEX_BUTTON_HH__
\endcode

at the beginning and

\code
-#endif
+#endif // __FLTK_COMPLEX_BUTTON_HH__
\endcode

at the end. Then, the namespace is changed:

\code
-namespace fltk {
+namespace dw {
+namespace fltk {
+namespace ui {
\endcode

at the beginning and

\code
-}
+} // namespace ui
+} // namespace fltk
+} // namespace dw
\endcode

at the end. Most important, the base class is changed:

\code
-#include "FL/Fl_Widget.H"
+#include <FL/Fl_Group.H>
\endcode

and

\code
-class FL_API ComplexButton : public Fl_Widget {
+class ComplexButton: public Fl_Group
+{
\endcode

Finally, for dw::fltk::ui::ComplexButton::default_style, there is a
namespace conflict:

\code
-  static NamedStyle* default_style;
+  static ::fltk::NamedStyle* default_style;
\endcode

<h3>Changes in fltkcomplexbutton.cc</h3>

First, \#include's:

\code

 #include <FL/Fl.H>
-#include <FL/ComplexButton.h> // <FL/Fl_Button.H> formerly
 #include <FL/Fl_Group.H>
 #include <FL/Fl_Window.H>
+
+#include "fltkcomplexbutton.hh"
\endcode

Second, namespaces:

\code
+using namespace dw::fltk::ui;
\endcode

Since the base class is now Fl_Group, the constructor must be changed:

\code
-ComplexButton::ComplexButton(int x,int y,int w,int h, const char *l) : Fl_Widget(x,y,w,h,l) {
+ComplexButton::ComplexButton(int x,int y,int w,int h, const char *l) :
+   Fl_Group(x,y,w,h,l)
+{
\endcode

Finally, the button must draw its children (end of
dw::fltk::ui::ComplexButton::draw()):

\code
+
+  for (int i = children () - 1; i >= 0; i--)
+     draw_child (*child (i));
 }
\endcode

*/
