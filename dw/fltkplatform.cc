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

#include "../lout/msg.h"
#include "../lout/debug.hh"
#include "fltkcore.hh"

#include <FL/fl_draw.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Tooltip.H>
#include <FL/Fl_Menu_Window.H>
#include <FL/Fl_Paged_Device.H>

/*
 * Local data
 */

/* Tooltips */
static Fl_Menu_Window *tt_window = NULL;
static int in_tooltip = 0, req_tooltip = 0;

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
                             FltkFont::FontFamily> *FltkFont::systemFonts =
                             NULL;

FltkFont::FontFamily FltkFont::standardFontFamily (FL_HELVETICA,
                                                   FL_HELVETICA_BOLD,
                                                   FL_HELVETICA_ITALIC,
                                                   FL_HELVETICA_BOLD_ITALIC);

FltkFont::FontFamily::FontFamily (Fl_Font fontNormal, Fl_Font fontBold,
                                  Fl_Font fontItalic, Fl_Font fontBoldItalic)
{
   font[0] = fontNormal;
   font[1] = fontBold;
   font[2] = fontItalic;
   font[3] = fontBoldItalic;
}

void FltkFont::FontFamily::set (Fl_Font f, int attrs)
{
   int idx = 0;
   if (attrs & FL_BOLD)
      idx += 1;
   if (attrs & FL_ITALIC)
      idx += 2;
   font[idx] = f;
}

Fl_Font FltkFont::FontFamily::get (int attrs)
{
   int idx = 0;
   if (attrs & FL_BOLD)
      idx += 1;
   if (attrs & FL_ITALIC)
      idx += 2;

   // should the desired font style not exist, we
   // return the normal font of the fontFamily
   return font[idx] >= 0 ? font[idx] : font[0];
}



FltkFont::FltkFont (core::style::FontAttrs *attrs)
{
   if (!systemFonts)
      initSystemFonts ();

   copyAttrs (attrs);

   int fa = 0;
   if (weight >= 500)
      fa |= FL_BOLD;
   if (style != core::style::FONT_STYLE_NORMAL)
      fa |= FL_ITALIC;

   object::ConstString nameString (name);
   FontFamily *family = systemFonts->get (&nameString);
   if (!family)
      family = &standardFontFamily;

   font = family->get (fa);

   fl_font(font, size);
   // WORKAROUND: A bug with fl_width(uint_t) on non-xft X was present in
   // 1.3.0 (STR #2688).
   spaceWidth = misc::max(0, (int)fl_width(" ") + letterSpacing);
   int xx, xy, xw, xh;
   fl_text_extents("x", xx, xy, xw, xh);
   xHeight = xh;
   descent = fl_descent();
   ascent = fl_height() - descent;
}

FltkFont::~FltkFont ()
{
   fontsTable->remove (this);
}

static void strstrip(char *big, const char *little)
{
   if (strlen(big) >= strlen(little) &&
      misc::AsciiStrcasecmp(big + strlen(big) - strlen(little), little) == 0)
      *(big + strlen(big) - strlen(little)) = '\0';
}

void FltkFont::initSystemFonts ()
{
   systemFonts = new container::typed::HashTable
      <lout::object::ConstString, FontFamily> (true, true);

   int k = Fl::set_fonts ("-*-iso10646-1");
   for (int i = 0; i < k; i++) {
      int t;
      char *name = strdup (Fl::get_font_name ((Fl_Font) i, &t));

      // normalize font family names (strip off "bold", "italic")
      if (t & FL_ITALIC)
         strstrip(name, " italic");
      if (t & FL_BOLD)
         strstrip(name, " bold");

      _MSG("Found font: %s%s%s\n", name, t & FL_BOLD ? " bold" : "",
                                  t & FL_ITALIC ? " italic" : "");

      object::String *familyName = new object::String(name);
      free (name);
      FontFamily *family = systemFonts->get (familyName);

      if (family) {
         family->set ((Fl_Font) i, t);
         delete familyName;
      } else {
         // set first font of family also as normal font in case there
         // is no normal (non-bold, non-italic) font
         family = new FontFamily ((Fl_Font) i, -1, -1, -1);
         family->set ((Fl_Font) i, t);
         systemFonts->put (familyName, family);
      }
   }
}

bool
FltkFont::fontExists (const char *name)
{
   if (!systemFonts)
      initSystemFonts ();
   object::ConstString familyName (name);
   return systemFonts->get (&familyName) != NULL;
}

Fl_Font
FltkFont::get (const char *name, int attrs)
{
   if (!systemFonts)
      initSystemFonts ();
   object::ConstString familyName (name);
   FontFamily *family = systemFonts->get (&familyName);
   if (family)
      return family->get (attrs);
   else
      return FL_HELVETICA;
}

bool
FltkPlatform::fontExists (const char *name)
{
   return FltkFont::fontExists (name);
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

   if (!(colors[SHADING_NORMAL] = shadeColor (color, SHADING_NORMAL) << 8))
      colors[SHADING_NORMAL] = FL_BLACK;
   if (!(colors[SHADING_INVERSE] = shadeColor (color, SHADING_INVERSE) << 8))
      colors[SHADING_INVERSE] = FL_BLACK;
   if (!(colors[SHADING_DARK] = shadeColor (color, SHADING_DARK) << 8))
      colors[SHADING_DARK] = FL_BLACK;
   if (!(colors[SHADING_LIGHT] = shadeColor (color, SHADING_LIGHT) << 8))
      colors[SHADING_LIGHT] = FL_BLACK;
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
}

FltkTooltip::~FltkTooltip ()
{
   if (in_tooltip || req_tooltip)
      cancel(); /* cancel tooltip window */
}

FltkTooltip *FltkTooltip::create (const char *text)
{
   return new FltkTooltip(text);
}

/*
 * Tooltip callback: used to delay it a bit
 * INVARIANT: Only one instance of this function is requested.
 */
static void tooltip_tcb(void *data)
{
   req_tooltip = 2;
   ((FltkTooltip *)data)->onEnter();
   req_tooltip = 0;
}

void FltkTooltip::onEnter()
{
   _MSG("FltkTooltip::onEnter\n");
   if (!str || !*str)
      return;
   if (req_tooltip == 0) {
      Fl::remove_timeout(tooltip_tcb);
      Fl::add_timeout(1.0, tooltip_tcb, this);
      req_tooltip = 1;
      return;
   }

   if (!tt_window) {
      tt_window = new Fl_Menu_Window(0,0,100,24);
      tt_window->set_override();
      tt_window->box(FL_NO_BOX);
      Fl_Box *b = new Fl_Box(0,0,100,24);
      b->box(FL_BORDER_BOX);
      b->color(fl_color_cube(FL_NUM_RED-1, FL_NUM_GREEN-1, FL_NUM_BLUE-2));
      b->labelcolor(FL_BLACK);
      b->labelfont(FL_HELVETICA);
      b->labelsize(14);
      b->align(FL_ALIGN_WRAP|FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
      tt_window->resizable(b);
      tt_window->end();
   }

   /* prepare tooltip window */
   int x, y;
   Fl_Box *box = (Fl_Box*)tt_window->child(0);
   box->label(str);
   Fl::get_mouse(x,y); y += 6;
   /* calculate window size */
   int ww, hh;
   ww = 800; // max width;
   box->measure_label(ww, hh);
   ww += 6 + 2 * Fl::box_dx(box->box());
   hh += 6 + 2 * Fl::box_dy(box->box());
   tt_window->resize(x,y,ww,hh);
   tt_window->show();
   in_tooltip = 1;
}

/*
 * Leaving the widget cancels the tooltip
 */
void FltkTooltip::onLeave()
{
   _MSG(" FltkTooltip::onLeave  in_tooltip=%d\n", in_tooltip);
   cancel();
}

void FltkPlatform::cancelTooltip()
{
   FltkTooltip::cancel();
}

/*
 * Remove a shown tooltip or cancel a pending one
 */
void FltkTooltip::cancel()
{
   if (req_tooltip) {
      Fl::remove_timeout(tooltip_tcb);
      req_tooltip = 0;
   }
   if (!in_tooltip) return;
   in_tooltip = 0;
   tt_window->hide();

   /* WORKAROUND: (Black magic here)
    * Hiding a tooltip with the keyboard or mousewheel doesn't work.
    * The code below "fixes" the problem */
   Fl_Widget *widget = Fl::belowmouse();
   if (widget && widget->window()) {
      widget->window()->damage(FL_DAMAGE_EXPOSE,0,0,1,1);
   }
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
FltkPlatform::FltkResourceFactory::createEntryResource (int size,
                                                        bool password,
                                                        const char *label,
                                                       const char *placeholder)
{
   return new ui::FltkEntryResource (platform, size, password, label,
                                     placeholder);
}

core::ui::MultiLineTextResource *
FltkPlatform::FltkResourceFactory::createMultiLineTextResource (int cols,
                                                                int rows,
                                                       const char *placeholder)
{
   return new ui::FltkMultiLineTextResource (platform, cols, rows,placeholder);
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
   DBG_OBJ_CREATE ("dw::fltk::FltkPlatform");

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

   DBG_OBJ_DELETE ();
}

void FltkPlatform::setLayout (core::Layout *layout)
{
   this->layout = layout;
   DBG_OBJ_ASSOC_CHILD (layout);
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


void FltkPlatform::detachView (core::View *view)
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
   int c, cu;
   int width = 0;
   FltkFont *ff = (FltkFont*) font;
   int curr = 0, next = 0, nb;

   if (font->fontVariant == core::style::FONT_VARIANT_SMALL_CAPS) {
      int sc_fontsize = lout::misc::roundInt(ff->size * 0.78);
      for (curr = 0; next < len; curr = next) {
         next = nextGlyph(text, curr);
         c = fl_utf8decode(text + curr, text + next, &nb);
         if ((cu = fl_toupper(c)) == c) {
            /* already uppercase, just draw the character */
            fl_font(ff->font, ff->size);
            if (fl_nonspacing(cu) == 0) {
               width += font->letterSpacing;
               width += (int)fl_width(text + curr, next - curr);
            }
         } else {
            /* make utf8 string for converted char */
            nb = fl_utf8encode(cu, chbuf);
            fl_font(ff->font, sc_fontsize);
            if (fl_nonspacing(cu) == 0) {
               width += font->letterSpacing;
               width += (int)fl_width(chbuf, nb);
            }
         }
      }
   } else {
      fl_font (ff->font, ff->size);
      width = (int) fl_width (text, len);

      if (font->letterSpacing) {
         int curr = 0, next = 0;

         while (next < len) {
            next = nextGlyph(text, curr);
            c = fl_utf8decode(text + curr, text + next, &nb);
            if (fl_nonspacing(c) == 0)
               width += font->letterSpacing;
            curr = next;
         }
      }
   }

   return width;
}

char *FltkPlatform::textToUpper (const char *text, int len)
{
   char *newstr = NULL;

   if (len > 0) {
      int newlen;

      newstr = (char*) malloc(3 * len + 1);
      newlen = fl_utf_toupper((const unsigned char*)text, len, newstr);
      assert(newlen <= 3 * len);
      newstr[newlen] = '\0';
   }
   return newstr;
}

char *FltkPlatform::textToLower (const char *text, int len)
{
   char *newstr = NULL;

   if (len > 0) {
      int newlen;

      newstr = (char*) malloc(3 * len + 1);
      newlen = fl_utf_tolower((const unsigned char*)text, len, newstr);
      assert(newlen <= 3 * len);
      newstr[newlen] = '\0';
   }
   return newstr;
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
                                          int width, int height, double gamma)
{
   return new FltkImgbuf (type, width, height, gamma);
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
