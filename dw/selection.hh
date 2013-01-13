#ifndef __DW_SELECTION_H__
#define __DW_SELECTION_H__

#ifndef __INCLUDED_FROM_DW_CORE_HH__
#   error Do not include this file directly, use "core.hh" instead.
#endif

namespace dw {
namespace core {

/**
 * \brief This class handles selections, as well as activation of links,
 *    which is closely related.
 *
 * <h3>General Overview</h3>
 *
 * dw::core::SelectionState is associated with dw::core::Layout. The selection
 * state is controlled by "abstract events", which are sent by single
 * widgets by calling one of the following methods:
 *
 * <ul>
 * <li> dw::core::SelectionState::buttonPress for button press events,
 * <li> dw::core::SelectionState::buttonRelease for button release events, and
 * <li> dw::core::SelectionState::buttonMotion for motion events (with pressed
 *      mouse button).
 * </ul>
 *
 * The widget must construct simple iterators (dw::core::Iterator), which will
 * be transferred to deep iterators (dw::core::DeepIterator), see below for
 * more details. All event handling methods have the same signature, the
 * arguments in detail are:
 *
 * <table>
 * <tr><td>dw::core::Iterator *it       <td>the iterator pointing on the item
 *                                          under the mouse pointer; this
 *                                          iterator \em must be created with
 *                                         dw::core::Content::SELECTION_CONTENT
 *                                          as mask
 * <tr><td>int charPos                  <td>the exact (character) position
 *                                          within the iterator,
 * <tr><td>int linkNo                   <td>if this item is associated with a
 *                                          link, its number (see
 *                                          dw::core::Layout::LinkReceiver),
 *                                          otherwise -1
 * <tr><td>dw::core::EventButton *event <td>the event itself; only the button
 *                                          is used
 * </table>
 *
 * Look also at dw::core::SelectionState::handleEvent, which may be useful
 * in some circumstances.
 *
 * In some cases, \em charPos would be difficult to determine. E.g., when
 * the dw::Textblock widget decides that the user is pointing on a position
 * <i>at the end</i> of an image (DwImage), it constructs a simple iterator
 * pointing on this image widget. In a simple iterator, that fact that
 * the pointer is at the end, would be represented by \em charPos == 1. But
 * when transferring this simple iterator into an deep iterator, this
 * simple iterator is discarded and instead the stack has an iterator
 * pointing to text at the top. As a result, only the first letter of the
 * ALT text would be copied.
 *
 * To avoid this problem, widgets should in this case pass
 * dw::core::SelectionState::END_OF_WORD as \em charPos, which is then
 * automatically reduced to the actual length of the deep(!) iterator.
 *
 * The return value is the same as in DwWidget event handling methods.
 * I.e., in most cases, they should simply return it. The events
 * dw::core::Layout::LinkReceiver::press,
 * dw::core::Layout::LinkReceiver::release and
 * dw::core::Layout::LinkReceiver::click (but not
 * dw::core::Layout::LinkReceiver::enter) are emitted by these methods, so
 * that widgets which let dw::core::SelectionState handle links, should only
 * emit dw::core::Layout::LinkReceiver::enter for themselves.
 *
 * <h3>Selection State</h3>
 *
 * Selection interferes with handling the activation of links, so the
 * latter is also handled by the dw::core::SelectionState. Details are based on
 * following guidelines:
 *
 * <ol>
 * <li> It should be simple to select links and to start selection in
 *      links. The rule to distinguish between link activation and
 *      selection is that the selection starts as soon as the user leaves
 *      the link. (This is, IMO, a useful feature. Even after drag and
 *      drop has been implemented in dillo, this should be somehow
 *      preserved.)
 *
 * <li> The selection should stay as long as possible, i.e., the old
 *      selection is only cleared when a new selection is started.
 * </ol>
 *
 * The latter leads to a model with two states: the selection state and
 * the link handling state.
 *
 * The general selection works, for events not pointing on links, like
 * this (numbers in parantheses after the event denote the button, "n"
 * means arbitrary button):
 *
 * \dot
 * digraph G {
 *    node [shape=ellipse, fontname=Helvetica, fontsize=10];
 *    edge [arrowhead="open", labelfontname=Helvetica, labelfontsize=10,
 *          color="#404040", labelfontcolor="#000080",
 *          fontname=Helvetica, fontsize=10, fontcolor="#000080"];
 *    fontname=Helvetica; fontsize=10;
 *
 *    NONE;
 *    SELECTING;
 *    q [label="Anything selected?", shape=plaintext];
 *    SELECTED;
 *
 *    NONE -> SELECTING [label="press(1)\non non-link"];
 *    SELECTING -> SELECTING [label="motion(1)"];
 *    SELECTING -> q [label="release(1)"];
 *    q -> SELECTED [label="yes"];
 *    q -> NONE [label="no"];
 *    SELECTED -> SELECTING [label="press(1)"];
 *
 * }
 * \enddot
 *
 * The selected region is represented by two instances of
 * dw::core::DeepIterator.
 *
 * Links are handled by a different state machine:
 *
 * \dot
 * digraph G {
 *    node [shape=ellipse, fontname=Helvetica, fontsize=10];
 *    edge [arrowhead="open", labelfontname=Helvetica, labelfontsize=10,
 *          color="#404040", labelfontcolor="#000080",
 *          fontname=Helvetica, fontsize=10, fontcolor="#000080"];
 *    fontname=Helvetica; fontsize=10;
 *
 *    LINK_NONE;
 *    LINK_PRESSED;
 *    click [label="Emit \"click\" signal.", shape=record];
 *    q11 [label="Still the same link?", shape=plaintext];
 *    q21 [label="Still the same link?", shape=plaintext];
 *    q22 [label="n == 1?", shape=plaintext];
 *    SELECTED [label="Switch selection\nto SELECTED", shape=record];
 *    q12 [label="n == 1?", shape=plaintext];
 *    SELECTING [label="Switch selection\nto SELECTING", shape=record];
 *
 *    LINK_NONE -> LINK_PRESSED [label="press(n)\non link"];
 *    LINK_PRESSED -> q11 [label="motion(n)"];
 *    q11 -> LINK_PRESSED [label="yes"];
 *    q11 -> q12 [label="no"];
 *    q12 -> SELECTING [label="yes"];
 *    SELECTING -> LINK_NONE;
 *    q12 -> LINK_NONE [label="no"];
 *    LINK_PRESSED -> q21 [label="release(n)"];
 *    q21 -> click [label="yes"];
 *    click -> LINK_NONE;
 *    q21 -> q22 [label="no"];
 *    q22 -> SELECTED [label="yes"];
 *    SELECTED -> LINK_NONE;
 *    q22 -> LINK_NONE [label="no"];
 * }
 * \enddot
 *
 * Switching selection simply means that the selection state will
 * eventually be SELECTED/SELECTING, with the original and the current
 * position making up the selection region. This happens for button 1,
 * events with buttons other than 1 do not affect selection at all.
 *
 *
 * \todo dw::core::SelectionState::buttonMotion currently always assumes
 *    that button 1 has been pressed (since otherwise it would not do
 *    anything). This should be made a bit cleaner.
 *
 * \todo The selection should be cleared, when the user selects something
 *    somewhere else (perhaps switched into "non-active" mode, as e.g. Gtk+
 *    does).
 *
 */
class SelectionState
{
public:
   enum { END_OF_WORD =  1 << 30 };

private:
   Layout *layout;

   // selection
   enum {
      NONE,
      SELECTING,
      SELECTED
   } selectionState;

   DeepIterator *from, *to;
   int fromChar, toChar;

   // link handling
   enum {
      LINK_NONE,
      LINK_PRESSED
   } linkState;

   int linkButton;
   DeepIterator *link;
   int linkChar, linkNumber;

   void resetSelection ();
   void resetLink ();
   void switchLinkToSelection (Iterator *it, int charPos);
   void adjustSelection (Iterator *it, int charPos);
   static int correctCharPos (DeepIterator *it, int charPos);

   void highlight (bool fl, int dir)
   { highlight0 (fl, from, fromChar, to, toChar, dir); }

   void highlight0 (bool fl, DeepIterator *from, int fromChar,
                    DeepIterator *to, int toChar, int dir);
   void copy ();

public:
   enum EventType { BUTTON_PRESS, BUTTON_RELEASE, BUTTON_MOTION };

   SelectionState ();
   ~SelectionState ();

   inline void setLayout (Layout *layout) { this->layout = layout; }
   void reset ();
   bool buttonPress (Iterator *it, int charPos, int linkNo,
                     EventButton *event);
   bool buttonRelease (Iterator *it, int charPos, int linkNo,
                       EventButton *event);
   bool buttonMotion (Iterator *it, int charPos, int linkNo,
                      EventMotion *event);

   bool handleEvent (EventType eventType, Iterator *it, int charPos,
                     int linkNo, MousePositionEvent *event);
};

} // namespace core
} // namespace dw

#endif // __DW_SELECTION_H__
