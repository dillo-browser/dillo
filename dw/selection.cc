/*
 * Dillo Widget
 *
 * Copyright 2005-2007 Sebastian Geerken <sgeerken@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */



#include "core.hh"
#include "../lout/debug.hh"

#include <string.h>

using namespace lout;

/*
 * strndup() is a GNU extension.
 */
extern "C" char *strndup(const char *s, size_t size)
{
   char *r = (char *) malloc (size + 1);

   if (r) {
      strncpy (r, s, size);
      r[size] = 0;
   }

   return r;
}

namespace dw {
namespace core {

SelectionState::SelectionState ()
{
   DBG_OBJ_CREATE ("dw::core::SelectionState");

   layout = NULL;

   selectionState = NONE;
   from = NULL;
   to = NULL;

   linkState = LINK_NONE;
   link = NULL;
}

SelectionState::~SelectionState ()
{
   reset ();
   DBG_OBJ_DELETE ();
}

void SelectionState::reset ()
{
   resetSelection ();
   resetLink ();
}

void SelectionState::resetSelection ()
{
   if (from)
      delete from;
   from = NULL;
   if (to)
      delete to;
   to = NULL;
   selectionState = NONE;
}


void SelectionState::resetLink ()
{
   if (link)
      delete link;
   link = NULL;
   linkState = LINK_NONE;
}

bool SelectionState::buttonPress (Iterator *it, int charPos, int linkNo,
                                  EventButton *event)
{
   Widget *itWidget = it->getWidget ();
   bool ret = false;

   if (!event) return ret;

   if (event->button == 3) {
      // menu popup
      layout->emitLinkPress (itWidget, linkNo, -1, -1, -1, event);
      ret = true;
   } else if (linkNo != -1) {
      // link handling
      (void) layout->emitLinkPress (itWidget, linkNo, -1, -1, -1, event);
      resetLink ();
      linkState = LINK_PRESSED;
      linkButton = event->button;
      DeepIterator *newLink = new DeepIterator (it);
      if (newLink->isEmpty ()) {
         delete newLink;
         resetLink ();
      } else {
         link = newLink;
         // It may be that the user has pressed on something activatable
         // (linkNo != -1), but there is no contents, e.g. with images
         // without ALTernative text.
         if (link) {
            linkChar = correctCharPos (link, charPos);
            linkNumber = linkNo;
         }
      }
      // We do not return the value of the signal method,
      // but we do actually process this event.
      ret = true;
   } else if (event->button == 1) {
      // normal selection handling
      highlight (false, 0);
      resetSelection ();

      selectionState = SELECTING;
      DeepIterator *newFrom = new DeepIterator (it);
      if (newFrom->isEmpty ()) {
         delete newFrom;
         resetSelection ();
      } else {
         from = newFrom;
         fromChar = correctCharPos (from, charPos);
         to = from->cloneDeepIterator ();
         toChar = correctCharPos (to, charPos);
      }
      ret = true;
   }

   return ret;
}

bool SelectionState::buttonRelease (Iterator *it, int charPos, int linkNo,
                                    EventButton *event)
{
   Widget *itWidget = it->getWidget ();
   bool ret = false;

   if (linkState == LINK_PRESSED && event && event->button == linkButton) {
      // link handling
      ret = true;
      if (linkNo != -1)
         (void) layout->emitLinkRelease (itWidget, linkNo, -1, -1, -1, event);

      // The link where the user clicked the mouse button?
      if (linkNo == linkNumber) {
         resetLink ();
         (void) layout->emitLinkClick (itWidget, linkNo, -1, -1, -1, event);
      } else {
         if (event->button == 1)
            // Reset links and switch to selection mode. The selection
            // state will be set to SELECTING, which is handled some lines
            // below.
            switchLinkToSelection (it, charPos);
      }
   }

   if (selectionState == SELECTING && event && event->button == 1) {
      // normal selection
      ret = true;
      adjustSelection (it, charPos);

      if (from->compareTo (to) == 0 && fromChar == toChar)
         // nothing selected
         resetSelection ();
      else {
         copy ();
         selectionState = SELECTED;
      }
   }

   return ret;
}

bool SelectionState::buttonMotion (Iterator *it, int charPos, int linkNo,
                                   EventMotion *event)
{
   if (linkState == LINK_PRESSED) {
      //link handling
      if (linkNo != linkNumber)
         // No longer the link where the user clicked the mouse button.
         // Reset links and switch to selection mode.
         switchLinkToSelection (it, charPos);
      // Still in link: do nothing.
   } else if (selectionState == SELECTING) {
      // selection
      adjustSelection (it, charPos);
   }

   return true;
}

/**
 * \brief General form of dw::core::SelectionState::buttonPress,
 *    dw::core::SelectionState::buttonRelease and
 *    dw::core::SelectionState::buttonMotion.
 */
bool SelectionState::handleEvent (EventType eventType, Iterator *it,
                                  int charPos, int linkNo,
                                  MousePositionEvent *event)
{
   switch (eventType) {
   case BUTTON_PRESS:
      return buttonPress (it, charPos, linkNo, (EventButton*)event);

   case BUTTON_RELEASE:
      return buttonRelease (it, charPos, linkNo, (EventButton*)event);

   case BUTTON_MOTION:
      return buttonMotion (it, charPos, linkNo, (EventMotion*)event);


   default:
      misc::assertNotReached ();
   }

   return false;
}


/**
 * \brief This method is called when the user decides not to activate a link,
 *    but instead select text.
 */
void SelectionState::switchLinkToSelection (Iterator *it, int charPos)
{
   // It may be that selection->link is NULL, see a_Selection_button_press.
   if (link) {
      // Reset old selection.
      highlight (false, 0);
      resetSelection ();

      // Transfer link state into selection state.
      from = link->cloneDeepIterator ();
      fromChar = linkChar;
      to = from->createVariant (it);
      toChar = correctCharPos (to, charPos);
      selectionState = SELECTING;

      // Reset link status.
      resetLink ();

      highlight (true, 0);

   } else {
      // A link was pressed on, but there is nothing to select. Reset
      // everything.
      resetSelection ();
      resetLink ();
   }
}

/**
 * \brief This method is used by core::dw::SelectionState::buttonMotion and
 *    core::dw::SelectionState::buttonRelease, and changes the second limit of
 *    the already existing selection region.
 */
void SelectionState::adjustSelection (Iterator *it, int charPos)
{
   DeepIterator *newTo;
   int newToChar, cmpOld, cmpNew, cmpDiff, len;
   bool bruteHighlighting = false;

   newTo = to->createVariant (it);
   newToChar = correctCharPos (newTo, charPos);

   cmpOld = to->compareTo (from);
   cmpNew = newTo->compareTo (from);

   if (cmpOld == 0 || cmpNew == 0) {
      // Either before, or now, the limits differ only by the character
      // position.
      bruteHighlighting = true;
   } else if (cmpOld * cmpNew < 0) {
      // The selection order has changed, i.e. the user moved the selection
      // end again beyond the position he started.
      bruteHighlighting = true;
   } else {
      // Here, cmpOld and cmpNew are equivalent and != 0.
      cmpDiff = newTo->compareTo (to);

      if (cmpOld * cmpDiff > 0) {
         // The user has enlarged the selection. Highlight the difference.
         if (cmpDiff < 0) {
            len = correctCharPos (to, END_OF_WORD);
            highlight0 (true, newTo, newToChar, to, len + 1, 1);
         } else {
            highlight0 (true, to, 0, newTo, newToChar, -1);
         }
      } else {
         if (cmpOld * cmpDiff < 0) {
            // The user has reduced the selection. Unhighlight the difference.
            highlight0 (false, to, 0, newTo, 0, cmpDiff);
         }

         // Otherwise, the user has changed the position only slightly.
         // In both cases, re-highlight the new position.
         if (cmpOld < 0) {
            len = correctCharPos (newTo, END_OF_WORD);
            newTo->highlight (newToChar, len + 1, HIGHLIGHT_SELECTION);
         } else
            newTo->highlight (0, newToChar, HIGHLIGHT_SELECTION);
      }
   }

   if (bruteHighlighting)
      highlight (false, 0);

   delete to;
   to = newTo;
   toChar = newToChar;

   if (bruteHighlighting)
      highlight (true, 0);
}

/**
 * \brief This method deals especially with the case that a widget passes
 *    dw::core::SelectionState::END_OF_WORD.
 */
int SelectionState::correctCharPos (DeepIterator *it, int charPos)
{
   Iterator *top = it->getTopIterator ();
   int len;

   if (top->getContent()->type == Content::TEXT)
      len = strlen(top->getContent()->text);
   else
      len = 1;

   return misc::min(charPos, len);
}

void SelectionState::highlight0 (bool fl, DeepIterator *from, int fromChar,
                                 DeepIterator *to, int toChar, int dir)
{
   DeepIterator *a, *b, *i;
   int cmp, aChar, bChar;
   bool start;

   if (from && to) {
      cmp = from->compareTo (to);
      if (cmp == 0) {
         if (fl) {
            if (fromChar < toChar)
               from->highlight (fromChar, toChar, HIGHLIGHT_SELECTION);
            else
               from->highlight (toChar, fromChar, HIGHLIGHT_SELECTION);
         } else
            from->unhighlight (0, HIGHLIGHT_SELECTION);
         return;
      }

      if (cmp < 0) {
         a = from;
         aChar = fromChar;
         b = to;
         bChar = toChar;
      } else {
         a = to;
         aChar = toChar;
         b = from;
         bChar = fromChar;
      }

      for (i = a->cloneDeepIterator (), start = true;
           (cmp = i->compareTo (b)) <= 0;
           i->next (), start = false) {
         if (i->getContent()->type == Content::TEXT) {
            if (fl) {
               if (start) {
                  i->highlight (aChar, strlen (i->getContent()->text) + 1,
                                HIGHLIGHT_SELECTION);
               } else if (cmp == 0) {
                  // the end
                  i->highlight (0, bChar, HIGHLIGHT_SELECTION);
               } else {
                  i->highlight (0, strlen (i->getContent()->text) + 1,
                                HIGHLIGHT_SELECTION);
               }
            } else {
               i->unhighlight (dir, HIGHLIGHT_SELECTION);
            }
         }
      }
      delete i;
   }
}

void SelectionState::copy()
{
   if (from && to) {
      Iterator *si;
      DeepIterator *a, *b, *i;
      int cmp, aChar, bChar;
      bool start;
      char *tmp;
      misc::StringBuffer strbuf;

      cmp = from->compareTo (to);
      if (cmp == 0) {
         if (from->getContent()->type == Content::TEXT) {
            si = from->getTopIterator ();
            if (fromChar < toChar)
               tmp = strndup (si->getContent()->text + fromChar,
                              toChar - fromChar);
            else
               tmp = strndup (si->getContent()->text + toChar,
                              fromChar - toChar);
            strbuf.appendNoCopy (tmp);
         }
      } else {
         if (cmp < 0) {
            a = from;
            aChar = fromChar;
            b = to;
            bChar = toChar;
         } else {
            a = to;
            aChar = toChar;
            b = from;
            bChar = fromChar;
         }

         for (i = a->cloneDeepIterator (), start = true;
              (cmp = i->compareTo (b)) <= 0;
              i->next (), start = false) {
            si = i->getTopIterator ();
            switch (si->getContent()->type) {
            case Content::TEXT:
               if (start) {
                  tmp = strndup (si->getContent()->text + aChar,
                                 strlen (i->getContent()->text) - aChar);
                  strbuf.appendNoCopy (tmp);
               } else if (cmp == 0) {
                  // the end
                  tmp = strndup (si->getContent()->text, bChar);
                  strbuf.appendNoCopy (tmp);
               } else
                  strbuf.append (si->getContent()->text);

               if (si->getContent()->space && cmp != 0)
                  strbuf.append (" ");

               break;

            case Content::BREAK:
               if (si->getContent()->breakSpace > 0)
                  strbuf.append ("\n\n");
               else
                  strbuf.append ("\n");
               break;
            default:
               // Make pedantic compilers happy. Especially
               // DW_CONTENT_WIDGET is never returned by a DwDeepIterator.
               break;
            }
         }
         delete i;
      }

      layout->copySelection(strbuf.getChars());
   }
}

} // namespace core
} // namespace dw
