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

#include <stdio.h>
#include <wchar.h>
#include <wctype.h>

#include "../lout/msg.h"
#include "fltkcore.hh"

#include <FL/fl_draw.H>
#include <FL/Fl_Tooltip.H>

namespace dw {
namespace fltk {

using namespace lout;

/**
 * \todo Distinction between italics and oblique would be nice.
 */

container::typed::HashTable <dw::core::style::FontAttrs,
                             FltkFont> *FltkFont::fontsTable =
   new container::typed::HashTable <dw::core::style::FontAttrs,
                                    FltkFont> (false, false);

container::typed::HashTable <lout::object::ConstString,
                             lout::object::Integer> *FltkFont::systemFonts = NULL;

FltkFont::FltkFont (core::style::FontAttrs *attrs)
{
   if (!systemFonts) {
      systemFonts = new container::typed::HashTable
         <lout::object::ConstString, lout::object::Integer> (true, true);

      int k = Fl::set_fonts ("-*");
      for (int i = 0; i < k; i++) {
         int t;
         const char *name = Fl::get_font_name ((Fl_Font) i, &t);
         systemFonts->put(new object::ConstString (name),
                          new object::Integer (i));
      }
   }

   copyAttrs (attrs);

   int fa = 0;
   if (weight >= 500)
      fa |= FL_BOLD;
   if (style != core::style::FONT_STYLE_NORMAL)
      fa  |= FL_ITALIC;
#if 0
PORT1.3
   font = ::fltk::font(name, fa);
   if (font == NULL) {
      /*
       * If using xft, fltk::HELVETICA just means sans, fltk::COURIER
       * means mono, and fltk::TIMES means serif.
       */
      font = FL_HELVETICA->plus (fa);
   }
#else
   object::ConstString *nameString = new object::ConstString (name);
   object::Integer *fontIndex = systemFonts->get(nameString);
   delete nameString;
   if (fontIndex) {
      font = fontIndex->getValue ();
   } else {
      font = FL_HELVETICA;
   }
#endif

   fl_font(font, size);
   spaceWidth = misc::max(0, (int)fl_width(' ') + letterSpacing);
   int xh, xw = 0;
   fl_measure("x", xw, xh, 0);
   xHeight = xh;
   descent = (int)fl_descent();
   ascent = (int)fl_height() - descent;

   /**
    * \bug The code above does not seem to work, so this workaround.
    */
   xHeight = ascent * 3 / 5;
}

FltkFont::~FltkFont ()
{
   fontsTable->remove (this);
}

bool
FltkPlatform::fontExists (const char *name)
{
#if 0
PORT1.3
   return ::fltk::font(name) != NULL;
#else
   return true;
#endif
}

FltkFont*
FltkFont::create (core::style::FontAttrs *attrs)
{
   FltkFont *font = fontsTable->get (attrs);

   if (font == NULL) {
      font = new FltkFont (attrs);
      fontsTable->put (font, font);
   }

   return font;
}

container::typed::HashTable <dw::core::style::ColorAttrs,
                             FltkColor>
   *FltkColor::colorsTable =
      new container::typed::HashTable <dw::core::style::ColorAttrs,
                                       FltkColor> (false, false);

FltkColor::FltkColor (int color): Color (color)
{
   this->color = color;

   colors[SHADING_NORMAL] = shadeColor (color, SHADING_NORMAL) << 8;
   colors[SHADING_INVERSE] = shadeColor (color, SHADING_INVERSE) << 8;
   colors[SHADING_DARK] = shadeColor (color, SHADING_DARK) << 8;
   colors[SHADING_LIGHT] = shadeColor (color, SHADING_LIGHT) << 8;
}

FltkColor::~FltkColor ()
{
   colorsTable->remove (this);
}

FltkColor * FltkColor::create (int col)
{
   ColorAttrs attrs(col);
   FltkColor *color = colorsTable->get (&attrs);

   if (color == NULL) {
      color = new FltkColor (col);
      colorsTable->put (color, color);
   }

   return color;
}

FltkTooltip::FltkTooltip (const char *text) : Tooltip(text)
{
   shown = false;

   if (!text || !strpbrk(text, "&@")) {
      escaped_str = NULL;
   } else {
      /*
       * WORKAROUND: ::fltk::Tooltip::tooltip_timeout() makes instance_
       * if necessary, and immediately uses it. This means that we can't
       * get our hands on it to set RAW_LABEL until after it has been shown
       * once. So let's escape the special characters ourselves.
       */
      const char *src = text;
      char *dest = escaped_str = (char *) malloc(strlen(text) * 2 + 1);

      while (*src) {
         if (*src == '&' || *src == '@')
            *dest++ = *src;
         *dest++ = *src++;
      }
      *dest = '\0';
   }
}

FltkTooltip::~FltkTooltip ()
{
#if 0
PORT1.3
probably can remember the one from onEnter
   if (shown)
      Fl_Tooltip::exit();
#endif
   if (escaped_str)
      free(escaped_str);
}

FltkTooltip *FltkTooltip::create (const char *text)
{
   return new FltkTooltip(text);
}

void FltkTooltip::onEnter()
{
   Fl_Widget *widget = Fl::belowmouse();

   Fl_Tooltip::enter_area(widget, widget->x(), widget->y(), widget->w(),
                          widget->h(), escaped_str ? escaped_str : str);
   shown = true;
}

void FltkTooltip::onLeave()
{
   Fl_Tooltip::exit(NULL);
   shown = false;
}

void FltkTooltip::onMotion()
{
}

void FltkView::addFltkWidget (Fl_Widget *widget,
                              core::Allocation *allocation)
{
}

void FltkView::removeFltkWidget (Fl_Widget *widget)
{
}

void FltkView::allocateFltkWidget (Fl_Widget *widget,
                                   core::Allocation *allocation)
{
}

void FltkView::drawFltkWidget (Fl_Widget *widget, core::Rectangle *area)
{
}


core::ui::LabelButtonResource *
FltkPlatform::FltkResourceFactory::createLabelButtonResource (const char
                                                              *label)
{
   return new ui::FltkLabelButtonResource (platform, label);
}

core::ui::ComplexButtonResource *
FltkPlatform::FltkResourceFactory::createComplexButtonResource (core::Widget
                                                                *widget,
                                                                bool relief)
{
   return new ui::FltkComplexButtonResource (platform, widget, relief);
}

core::ui::ListResource *
FltkPlatform::FltkResourceFactory::createListResource (core::ui
                                                       ::ListResource
                                                       ::SelectionMode
                                                       selectionMode, int rows)
{
   return new ui::FltkListResource (platform, selectionMode, rows);
}

core::ui::OptionMenuResource *
FltkPlatform::FltkResourceFactory::createOptionMenuResource ()
{
   return new ui::FltkOptionMenuResource (platform);
}

core::ui::EntryResource *
FltkPlatform::FltkResourceFactory::createEntryResource (int maxLength,
                                                        bool password,
                                                        const char *label)
{
   return new ui::FltkEntryResource (platform, maxLength, password, label);
}

core::ui::MultiLineTextResource *
FltkPlatform::FltkResourceFactory::createMultiLineTextResource (int cols,
                                                                int rows)
{
   return new ui::FltkMultiLineTextResource (platform, cols, rows);
}

core::ui::CheckButtonResource *
FltkPlatform::FltkResourceFactory::createCheckButtonResource (bool activated)
{
   return new ui::FltkCheckButtonResource (platform, activated);
}

core::ui::RadioButtonResource
*FltkPlatform::FltkResourceFactory::createRadioButtonResource
(core::ui::RadioButtonResource *groupedWith, bool activated)
{
   return
      new ui::FltkRadioButtonResource (platform,
                                       (ui::FltkRadioButtonResource*)
                                       groupedWith,
                                       activated);
}

// ----------------------------------------------------------------------

FltkPlatform::FltkPlatform ()
{
   layout = NULL;
   idleQueue = new container::typed::List <IdleFunc> (true);
   idleFuncRunning = false;
   idleFuncId = 0;

   view = NULL;
   resources = new container::typed::List <ui::FltkResource> (false);

   resourceFactory.setPlatform (this);
}

FltkPlatform::~FltkPlatform ()
{
   if (idleFuncRunning)
      Fl::remove_idle (generalStaticIdle, (void*)this);
   delete idleQueue;
   delete resources;
}

void FltkPlatform::setLayout (core::Layout *layout)
{
   this->layout = layout;
}


void FltkPlatform::attachView (core::View *view)
{
   if (this->view)
      MSG_ERR("FltkPlatform::attachView: multiple views!\n");
   this->view = (FltkView*)view;

   for (container::typed::Iterator <ui::FltkResource> it =
           resources->iterator (); it.hasNext (); ) {
      ui::FltkResource *resource = it.getNext ();
      resource->attachView (this->view);
   }
}


void FltkPlatform::detachView  (core::View *view)
{
   if (this->view != view)
      MSG_ERR("FltkPlatform::detachView: this->view: %p view: %p\n",
              this->view, view);

   for (container::typed::Iterator <ui::FltkResource> it =
           resources->iterator (); it.hasNext (); ) {
      ui::FltkResource *resource = it.getNext ();
      resource->detachView ((FltkView*)view);
   }
   this->view = NULL;
}


int FltkPlatform::textWidth (core::style::Font *font, const char *text,
                             int len)
{
   char chbuf[4];
   wchar_t wc, wcu;
   int width = 0;
   FltkFont *ff = (FltkFont*) font;
   int curr = 0, next = 0, nb;

   if (font->fontVariant == 1) {
      int sc_fontsize = lout::misc::roundInt(ff->size * 0.78);
      for (curr = 0; next < len; curr = next) {
         next = nextGlyph(text, curr);
         wc = fl_utf8decode(text + curr, text + next, &nb);
         if ((wcu = towupper(wc)) == wc) {
            /* already uppercase, just draw the character */
            fl_font(ff->font, ff->size);
            width += font->letterSpacing;
            width += (int)fl_width(text + curr, next - curr);
         } else {
            /* make utf8 string for converted char */
            nb = fl_utf8encode(wcu, chbuf);
            fl_font(ff->font, sc_fontsize);
            width += font->letterSpacing;
            width += (int)fl_width(chbuf, nb);
         }
      }
   } else {
      fl_font (ff->font, ff->size);
      width = (int) fl_width (text, len);

      if (font->letterSpacing) {
         int curr = 0, next = 0;

         while (next < len) {
            next = nextGlyph(text, curr);
            width += font->letterSpacing;
            curr = next;
         }
      }
   }

   return width;
}

int FltkPlatform::nextGlyph (const char *text, int idx)
{
   return fl_utf8fwd (&text[idx + 1], text, &text[strlen (text)]) - text;
}

int FltkPlatform::prevGlyph (const char *text, int idx)
{
   return fl_utf8back (&text[idx - 1], text, &text[strlen (text)]) - text;
}

float FltkPlatform::dpiX ()
{
   float horizontal, vertical;

   Fl::screen_dpi(horizontal, vertical);
   return horizontal;
}

float FltkPlatform::dpiY ()
{
   float horizontal, vertical;

   Fl::screen_dpi(horizontal, vertical);
   return vertical;
}

void FltkPlatform::generalStaticIdle (void *data)
{
   ((FltkPlatform*)data)->generalIdle();
}

void FltkPlatform::generalIdle ()
{
   IdleFunc *idleFunc;

   if (!idleQueue->isEmpty ()) {
      /* Execute the first function in the list. */
      idleFunc = idleQueue->getFirst ();
      (layout->*(idleFunc->func)) ();

      /* Remove this function. */
      idleQueue->removeRef(idleFunc);
   }

   if (idleQueue->isEmpty()) {
      idleFuncRunning = false;
      Fl::remove_idle (generalStaticIdle, (void*)this);
   }
}

/**
 * \todo Incomplete comments.
 */
int FltkPlatform::addIdle (void (core::Layout::*func) ())
{
   /*
    * Since ... (todo) we have to wrap around fltk_add_idle. There is only one
    * idle function, the passed idle function is put into a queue.
    */
   if (!idleFuncRunning) {
      Fl::add_idle (generalStaticIdle, (void*)this);
      idleFuncRunning = true;
   }

   idleFuncId++;

   IdleFunc *idleFunc = new IdleFunc();
   idleFunc->id = idleFuncId;
   idleFunc->func = func;
   idleQueue->append (idleFunc);

   return idleFuncId;
}

void FltkPlatform::removeIdle (int idleId)
{
   bool found;
   container::typed::Iterator <IdleFunc> it;
   IdleFunc *idleFunc;

   for (found = false, it = idleQueue->iterator(); !found && it.hasNext(); ) {
      idleFunc = it.getNext();
      if (idleFunc->id == idleId) {
         idleQueue->removeRef (idleFunc);
         found = true;
      }
   }

   if (idleFuncRunning && idleQueue->isEmpty())
      Fl::remove_idle (generalStaticIdle, (void*)this);
}

core::style::Font *FltkPlatform::createFont (core::style::FontAttrs
                                             *attrs,
                                             bool tryEverything)
{
   return FltkFont::create (attrs);
}

core::style::Color *FltkPlatform::createColor (int color)
{
   return FltkColor::create (color);
}

core::style::Tooltip *FltkPlatform::createTooltip (const char *text)
{
   return FltkTooltip::create (text);
}

void FltkPlatform::copySelection(const char *text)
{
   Fl::copy(text, strlen(text), 0);
}

core::Imgbuf *FltkPlatform::createImgbuf (core::Imgbuf::Type type,
                                          int width, int height)
{
   return new FltkImgbuf (type, width, height);
}

core::ui::ResourceFactory *FltkPlatform::getResourceFactory ()
{
   return &resourceFactory;
}


void FltkPlatform::attachResource (ui::FltkResource *resource)
{
   resources->append (resource);
   resource->attachView (view);
}

void FltkPlatform::detachResource (ui::FltkResource *resource)
{
   resources->removeRef (resource);
}

} // namespace fltk
} // namespace dw
