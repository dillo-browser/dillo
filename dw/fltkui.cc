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



#include "fltkcore.hh"
#include "fltkflatview.hh"
#include "fltkcomplexbutton.hh"
#include "../lout/msg.h"
#include "../lout/misc.hh"

#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Round_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Tree.H>

#include <stdio.h>

namespace dw {
namespace fltk {
namespace ui {

enum { RELIEF_X_THICKNESS = 3, RELIEF_Y_THICKNESS = 3 };

using namespace lout::object;
using namespace lout::container::typed;

FltkResource::FltkResource (FltkPlatform *platform)
{
   this->platform = platform;

   allocation.x = 0;
   allocation.y = 0;
   allocation.width = 1;
   allocation.ascent = 1;
   allocation.descent = 0;

   style = NULL;

   enabled = true;
}

/**
 * This is not a constructor, since it calls some virtual methods, which
 * should not be done in a C++ base constructor.
 */
void FltkResource::init (FltkPlatform *platform)
{
   view = NULL;
   widget = NULL;
   platform->attachResource (this);
}

FltkResource::~FltkResource ()
{
   platform->detachResource (this);
   if (widget) {
      if (view) {
         view->removeFltkWidget(widget);
      }
      delete widget;
   }
   if (style)
      style->unref ();
}

void FltkResource::attachView (FltkView *view)
{
   if (this->view)
      MSG_ERR("FltkResource::attachView: multiple views!\n");

   if (view->usesFltkWidgets ()) {
      this->view = view;

      widget = createNewWidget (&allocation);
      view->addFltkWidget (widget, &allocation);
      if (style)
         setWidgetStyle (widget, style);
      if (! enabled)
         widget->deactivate ();
   }
}

void FltkResource::detachView (FltkView *view)
{
   if (this->view != view)
      MSG_ERR("FltkResource::detachView: this->view: %p view: %p\n",
              this->view, view);
   this->view = NULL;
}

void FltkResource::sizeAllocate (core::Allocation *allocation)
{
   this->allocation = *allocation;
   view->allocateFltkWidget (widget, allocation);
}

void FltkResource::draw (core::View *view, core::Rectangle *area)
{
   FltkView *fltkView = (FltkView*)view;
   if (fltkView->usesFltkWidgets () && this->view == fltkView) {
      fltkView->drawFltkWidget (widget, area);
   }
}

void FltkResource::setStyle (core::style::Style *style)
{
   if (this->style)
      this->style->unref ();

   this->style = style;
   style->ref ();

   setWidgetStyle (widget, style);
}

void FltkResource::setWidgetStyle (Fl_Widget *widget,
                                   core::style::Style *style)
{
   FltkFont *font = (FltkFont*)style->font;
   widget->labelsize (font->size);
   widget->labelfont (font->font);

   FltkColor *bg = (FltkColor*)style->backgroundColor;
   if (bg) {
      int normal_bg = bg->colors[FltkColor::SHADING_NORMAL];

      if (style->color) {
         int style_fg = ((FltkColor*)style->color)->colors
                                                   [FltkColor::SHADING_NORMAL];
         Fl_Color fg = fl_contrast(style_fg, normal_bg);

         widget->labelcolor(fg);
         widget->selection_color(fg);
      }

      widget->color(normal_bg);
   }
}

void FltkResource::setDisplayed(bool displayed)
{
   if (displayed)
      widget->show();
   else
      widget->hide();
}

bool FltkResource::displayed()
{
   bool ret = false;

   if (widget) {
      // visible() is not the same thing as being show()n exactly, but
      // show()/hide() set it appropriately for our purposes.
      ret = widget->visible();
   }
   return ret;
}

bool FltkResource::isEnabled ()
{
   return enabled;
}

void FltkResource::setEnabled (bool enabled)
{
   this->enabled = enabled;

   if (enabled)
      widget->activate ();
   else
      widget->deactivate ();
}

// ----------------------------------------------------------------------

template <class I> void FltkSpecificResource<I>::sizeAllocate (core::Allocation
                                                               *allocation)
{
   FltkResource::sizeAllocate (allocation);
}

template <class I> void FltkSpecificResource<I>::draw (core::View *view,
                                                       core::Rectangle *area)
{
   FltkResource::draw (view, area);
}

template <class I> void FltkSpecificResource<I>::setStyle (core::style::Style
                                                           *style)
{
   FltkResource::setStyle (style);
}

template <class I> bool FltkSpecificResource<I>::isEnabled ()
{
   return FltkResource::isEnabled ();
}

template <class I> void FltkSpecificResource<I>::setEnabled (bool enabled)
{
   FltkResource::setEnabled (enabled);
}

// ----------------------------------------------------------------------

FltkLabelButtonResource::FltkLabelButtonResource (FltkPlatform *platform,
                                                  const char *label):
   FltkSpecificResource <dw::core::ui::LabelButtonResource> (platform)
{
   this->label = strdup (label);
   init (platform);
}

FltkLabelButtonResource::~FltkLabelButtonResource ()
{
   free((char *)label);
}

Fl_Widget *FltkLabelButtonResource::createNewWidget (core::Allocation
                                                     *allocation)
{
   Fl_Button *button =
        new Fl_Button (allocation->x, allocation->y, allocation->width,
                       allocation->ascent + allocation->descent, label);
   button->callback (widgetCallback, this);
   button->when (FL_WHEN_RELEASE);
   return button;
}

void FltkLabelButtonResource::sizeRequest (core::Requisition *requisition)
{
   if (style) {
      FltkFont *font = (FltkFont*)style->font;
      fl_font(font->font,font->size);
      requisition->width =
         (int)fl_width (label, strlen (label))
         + 2 * RELIEF_X_THICKNESS;
      requisition->ascent = font->ascent + RELIEF_Y_THICKNESS;
      requisition->descent = font->descent + RELIEF_Y_THICKNESS;
   } else {
      requisition->width = 1;
      requisition->ascent = 1;
      requisition->descent = 0;
   }
}

/*
 * Get FLTK state and translate to dw
 *
 * TODO: find a good home for this and the fltkviewbase.cc original.
 */
static core::ButtonState getDwButtonState ()
{
   int s1 = Fl::event_state ();
   int s2 = (core::ButtonState)0;

   if (s1 & FL_SHIFT)   s2 |= core::SHIFT_MASK;
   if (s1 & FL_CTRL)    s2 |= core::CONTROL_MASK;
   if (s1 & FL_ALT)     s2 |= core::META_MASK;
   if (s1 & FL_BUTTON1) s2 |= core::BUTTON1_MASK;
   if (s1 & FL_BUTTON2) s2 |= core::BUTTON2_MASK;
   if (s1 & FL_BUTTON3) s2 |= core::BUTTON3_MASK;

   return (core::ButtonState)s2;
}

static void setButtonEvent(dw::core::EventButton *event)
{
   event->xCanvas = Fl::event_x();
   event->yCanvas = Fl::event_y();
   event->state = getDwButtonState();
   event->button = Fl::event_button();
   event->numPressed = Fl::event_clicks() + 1;
}

void FltkLabelButtonResource::widgetCallback (Fl_Widget *widget,
                                              void *data)
{
   if ((widget->when () & FL_WHEN_RELEASE) &&
       ((Fl::event_key() == FL_Enter) ||
        (Fl::event_button() == FL_LEFT_MOUSE ||
         Fl::event_button() == FL_MIDDLE_MOUSE))) {
      FltkLabelButtonResource *lbr = (FltkLabelButtonResource*) data;
      dw::core::EventButton event;
      setButtonEvent(&event);
      lbr->emitClicked(&event);
   }
}

const char *FltkLabelButtonResource::getLabel ()
{
   return label;
}


void FltkLabelButtonResource::setLabel (const char *label)
{
   delete this->label;
   this->label = strdup (label);

   widget->label (this->label);
   queueResize (true);
}

// ----------------------------------------------------------------------

FltkComplexButtonResource::FltkComplexButtonResource (FltkPlatform *platform,
                                                      dw::core::Widget
                                                      *widget, bool relief):
   FltkSpecificResource <dw::core::ui::ComplexButtonResource> (platform)
{
   flatView = topView = NULL;
   this->relief = relief;
   FltkResource::init (platform);
   ComplexButtonResource::init (widget);
}

FltkComplexButtonResource::~FltkComplexButtonResource ()
{
}

void FltkComplexButtonResource::widgetCallback (Fl_Widget *widget,
                                                void *data)
{
   FltkComplexButtonResource *res = (FltkComplexButtonResource*)data;

   if (widget->when() == FL_WHEN_RELEASE &&
       ((Fl::event_key() == FL_Enter) ||
        (Fl::event_button() == FL_LEFT_MOUSE ||
         Fl::event_button() == FL_MIDDLE_MOUSE))) {
      res->click_x = Fl::event_x();
      res->click_y = Fl::event_y();
      dw::core::EventButton event;
      setButtonEvent(&event);
      res->emitClicked(&event);
   } else {
      ((FltkViewBase*)res->flatView)->handle(Fl::event());
   }
}

dw::core::Platform *FltkComplexButtonResource::createPlatform ()
{
   return new FltkPlatform ();
}

void FltkComplexButtonResource::attachView (FltkView *view)
{
   FltkResource::attachView (view);

   if (view->usesFltkWidgets ())
      topView = view;
}

void FltkComplexButtonResource::detachView (FltkView *view)
{
   FltkResource::detachView (view);
}

void FltkComplexButtonResource::sizeAllocate (core::Allocation *allocation)
{
   FltkResource::sizeAllocate (allocation);

   ((FltkFlatView*)flatView)->resize (
      reliefXThickness (), reliefYThickness (),
      allocation->width - 2 * reliefXThickness (),
      allocation->ascent + allocation->descent - 2 * reliefYThickness ());

   ((FltkFlatView*)flatView)->parent ()->init_sizes ();
}

void FltkComplexButtonResource::setLayout (dw::core::Layout *layout)
{
   layout->attachView (flatView);
}

int FltkComplexButtonResource::reliefXThickness ()
{
   return relief ? RELIEF_X_THICKNESS : 0;
}

int FltkComplexButtonResource::reliefYThickness ()
{
   return relief ? RELIEF_Y_THICKNESS : 0;
}


Fl_Widget *FltkComplexButtonResource::createNewWidget (core::Allocation
                                                            *allocation)
{
   ComplexButton *button =
      new ComplexButton (allocation->x, allocation->y, allocation->width,
                         allocation->ascent + allocation->descent);
   button->callback (widgetCallback, this);
   button->when (FL_WHEN_RELEASE);
   if (!relief)
      button->box(FL_FLAT_BOX);

   flatView = new FltkFlatView (allocation->x + reliefXThickness (),
                                allocation->y + reliefYThickness (),
                                allocation->width - 2 * reliefXThickness (),
                                allocation->ascent + allocation->descent
                                   - 2 * reliefYThickness ());
   button->add ((FltkFlatView *)flatView);

   if (layout)
      layout->attachView (flatView);
   return button;
}

// ----------------------------------------------------------------------

FltkEntryResource::FltkEntryResource (FltkPlatform *platform, int maxLength,
                                      bool password, const char *label):
   FltkSpecificResource <dw::core::ui::EntryResource> (platform)
{
   this->maxLength = maxLength;
   this->password = password;
   this->label = label ? strdup(label) : NULL;

   initText = NULL;
   editable = false;

   init (platform);
}

FltkEntryResource::~FltkEntryResource ()
{
   if (initText)
      delete initText;
   if (label)
      free((char *)label);
}

Fl_Widget *FltkEntryResource::createNewWidget (core::Allocation
                                                    *allocation)
{
   Fl_Input *input =
        new Fl_Input (allocation->x, allocation->y, allocation->width,
                      allocation->ascent + allocation->descent);
   if (password)
      input->type(FL_SECRET_INPUT);
   input->callback (widgetCallback, this);
   input->when (FL_WHEN_ENTER_KEY_ALWAYS);

   if (label) {
      input->label(label);
      input->align(FL_ALIGN_INSIDE);
   }
   if (initText)
      input->value (initText);

   return input;
}

void FltkEntryResource::setWidgetStyle (Fl_Widget *widget,
                                        core::style::Style *style)
{
   Fl_Input *in = (Fl_Input *)widget;

   FltkResource::setWidgetStyle(widget, style);

   in->textcolor(widget->labelcolor());
   in->cursor_color(in->textcolor());
   in->textsize(in->labelsize());
   in->textfont(in->labelfont());
}

void FltkEntryResource::setDisplayed(bool displayed)
{
   FltkResource::setDisplayed(displayed);
   queueResize(true);
}

void FltkEntryResource::sizeRequest (core::Requisition *requisition)
{
   if (displayed() && style) {
      FltkFont *font = (FltkFont*)style->font;
      fl_font(font->font,font->size);
      requisition->width =
         (int)fl_width ('n')
         * (maxLength == UNLIMITED_MAX_LENGTH ? 10 : maxLength)
         + 2 * RELIEF_X_THICKNESS;
      requisition->ascent = font->ascent + RELIEF_Y_THICKNESS;
      requisition->descent = font->descent + RELIEF_Y_THICKNESS;
   } else {
      requisition->width = 0;
      requisition->ascent = 0;
      requisition->descent = 0;
   }
}

void FltkEntryResource::widgetCallback (Fl_Widget *widget, void *data)
{
   /* The (::fltk::event_key() == FL_Enter) test
    * is necessary because WHEN_ENTER_KEY also includes
    * other events we're not interested in. For instance pressing
    * The Back or Forward, buttons, or the first click on a rendered
    * page. BUG: this must be investigated and reported to FLTK2 team
    */
   _MSG("when = %d\n", widget->when ());
   if ((widget->when () & FL_WHEN_ENTER_KEY_ALWAYS) &&
       (Fl::event_key() == FL_Enter))
      ((FltkEntryResource*)data)->emitActivate ();
}

const char *FltkEntryResource::getText ()
{
   return ((Fl_Input*)widget)->value ();
}

void FltkEntryResource::setText (const char *text)
{
   if (initText)
      delete initText;
   initText = strdup (text);

   ((Fl_Input*)widget)->value (initText);
}

bool FltkEntryResource::isEditable ()
{
   return editable;
}

void FltkEntryResource::setEditable (bool editable)
{
   this->editable = editable;
}

// ----------------------------------------------------------------------

FltkMultiLineTextResource::FltkMultiLineTextResource (FltkPlatform *platform,
                                                      int cols, int rows):
   FltkSpecificResource <dw::core::ui::MultiLineTextResource> (platform)
{
   buffer = new Fl_Text_Buffer;
   editable = false;

   numCols = cols;
   numRows = rows;

   // Check values. Upper bound check is left to the caller.
   if (numCols < 1) {
      MSG_WARN("numCols = %d is set to 1.\n", numCols);
      numCols = 1;
   }
   if (numRows < 1) {
      MSG_WARN("numRows = %d is set to 1.\n", numRows);
      numRows = 1;
   }

   init (platform);
}

FltkMultiLineTextResource::~FltkMultiLineTextResource ()
{
   /* Free memory avoiding a double-free of text buffers */
   ((Fl_Text_Editor *) widget)->buffer (0);
   delete buffer;
}

Fl_Widget *FltkMultiLineTextResource::createNewWidget (core::Allocation
                                                            *allocation)
{
   Fl_Text_Editor *text =
      new Fl_Text_Editor (allocation->x, allocation->y, allocation->width,
                          allocation->ascent + allocation->descent);
   text->buffer (buffer);
   return text;
}

void FltkMultiLineTextResource::setWidgetStyle (Fl_Widget *widget,
                                                core::style::Style *style)
{
   Fl_Text_Editor *ed = (Fl_Text_Editor *)widget;

   FltkResource::setWidgetStyle(widget, style);

   ed->textcolor(widget->labelcolor());
   ed->cursor_color(ed->textcolor());
   ed->textsize(ed->labelsize());
   ed->textfont(ed->labelfont());
}

void FltkMultiLineTextResource::sizeRequest (core::Requisition *requisition)
{
   if (style) {
      FltkFont *font = (FltkFont*)style->font;
      fl_font(font->font,font->size);
      requisition->width =
         (int)fl_width ('n') * numCols + 2 * RELIEF_X_THICKNESS;
      requisition->ascent =
         RELIEF_Y_THICKNESS + font->ascent +
         (font->ascent + font->descent) * (numRows - 1);
      requisition->descent =
         font->descent +
         RELIEF_Y_THICKNESS;
   } else {
      requisition->width = 1;
      requisition->ascent = 1;
      requisition->descent = 0;
   }
}

const char *FltkMultiLineTextResource::getText ()
{
   return buffer->text ();
}

void FltkMultiLineTextResource::setText (const char *text)
{
   buffer->text (text);
}

bool FltkMultiLineTextResource::isEditable ()
{
   return editable;
}

void FltkMultiLineTextResource::setEditable (bool editable)
{
   this->editable = editable;
}

// ----------------------------------------------------------------------

template <class I>
FltkToggleButtonResource<I>::FltkToggleButtonResource (FltkPlatform *platform,
                                                       bool activated):
   FltkSpecificResource <I> (platform)
{
   initActivated = activated;
}


template <class I>
FltkToggleButtonResource<I>::~FltkToggleButtonResource ()
{
}


template <class I>
Fl_Widget *FltkToggleButtonResource<I>::createNewWidget (core::Allocation
                                                              *allocation)
{
   Fl_Button *button = createNewButton (allocation);
   button->value (initActivated);
   return button;
}

template <class I>
void FltkToggleButtonResource<I>::setWidgetStyle (Fl_Widget *widget,
                                                  core::style::Style *style)
{
   FltkResource::setWidgetStyle(widget, style);

   widget->selection_color(FL_BLACK);
}


template <class I>
void FltkToggleButtonResource<I>::sizeRequest (core::Requisition *requisition)
{
   FltkFont *font = (FltkFont *)
      (this->FltkResource::style ? this->FltkResource::style->font : NULL);

   if (font) {
      fl_font(font->font, font->size);
      requisition->width = font->ascent + font->descent + 2*RELIEF_X_THICKNESS;
      requisition->ascent = font->ascent + RELIEF_Y_THICKNESS;
      requisition->descent = font->descent + RELIEF_Y_THICKNESS;
   } else {
      requisition->width = 1;
      requisition->ascent = 1;
      requisition->descent = 0;
   }
}


template <class I>
bool FltkToggleButtonResource<I>::FltkToggleButtonResource::isActivated ()
{
   return ((Fl_Button*)this->widget)->value ();
}


template <class I>
void FltkToggleButtonResource<I>::setActivated (bool activated)
{
   initActivated = activated;
   ((Fl_Button*)this->widget)->value (initActivated);
}

// ----------------------------------------------------------------------

FltkCheckButtonResource::FltkCheckButtonResource (FltkPlatform *platform,
                                                  bool activated):
   FltkToggleButtonResource<dw::core::ui::CheckButtonResource> (platform,
                                                                activated)
{
   init (platform);
}


FltkCheckButtonResource::~FltkCheckButtonResource ()
{
}


Fl_Button *FltkCheckButtonResource::createNewButton (core::Allocation
                                                          *allocation)
{
   Fl_Check_Button *cb =
      new Fl_Check_Button (allocation->x, allocation->y, allocation->width,
                           allocation->ascent + allocation->descent);
   return cb;
}

// ----------------------------------------------------------------------

bool FltkRadioButtonResource::Group::FltkGroupIterator::hasNext ()
{
   return it.hasNext ();
}

dw::core::ui::RadioButtonResource
*FltkRadioButtonResource::Group::FltkGroupIterator::getNext ()
{
   return (dw::core::ui::RadioButtonResource*)it.getNext ();
}

void FltkRadioButtonResource::Group::FltkGroupIterator::unref ()
{
   delete this;
}


FltkRadioButtonResource::Group::Group (FltkRadioButtonResource
                                       *radioButtonResource)
{
   list = new lout::container::typed::List <FltkRadioButtonResource> (false);
   connect (radioButtonResource);
}

FltkRadioButtonResource::Group::~Group ()
{
   delete list;
}

void FltkRadioButtonResource::Group::connect (FltkRadioButtonResource
                                              *radioButtonResource)
{
   list->append (radioButtonResource);
}

void FltkRadioButtonResource::Group::unconnect (FltkRadioButtonResource
                                                *radioButtonResource)
{
   list->removeRef (radioButtonResource);
   if (list->isEmpty ())
      delete this;
}


FltkRadioButtonResource::FltkRadioButtonResource (FltkPlatform *platform,
                                                  FltkRadioButtonResource
                                                  *groupedWith,
                                                  bool activated):
   FltkToggleButtonResource<dw::core::ui::RadioButtonResource> (platform,
                                                                activated)
{
   init (platform);

   if (groupedWith) {
      group = groupedWith->group;
      group->connect (this);
   } else
      group = new Group (this);
}


FltkRadioButtonResource::~FltkRadioButtonResource ()
{
   group->unconnect (this);
}

dw::core::ui::RadioButtonResource::GroupIterator
*FltkRadioButtonResource::groupIterator ()
{
   return group->groupIterator ();
}

void FltkRadioButtonResource::widgetCallback (Fl_Widget *widget,
                                              void *data)
{
   if (widget->when () & FL_WHEN_CHANGED)
      ((FltkRadioButtonResource*)data)->buttonClicked ();
}

void FltkRadioButtonResource::buttonClicked ()
{
   for (Iterator <FltkRadioButtonResource> it = group->iterator ();
        it.hasNext (); ) {
      FltkRadioButtonResource *other = it.getNext ();
      other->setActivated (other == this);
   }
}

Fl_Button *FltkRadioButtonResource::createNewButton (core::Allocation
                                                     *allocation)
{
   /*
    * Groups of Fl_Radio_Button must be added to one Fl_Group, which is
    * not possible in this context. For this, we do the grouping ourself,
    * based on FltkRadioButtonResource::Group.
    *
    * What we actually need for this, is a widget, which behaves like a
    * check button, but looks like a radio button. The first depends on the
    * type, the second on the style. Since the type is simpler to change
    * than the style, we create a radio button, and then change the type
    * (instead of creating a check button, and changing the style).
    */

   Fl_Button *button =
      new Fl_Round_Button (allocation->x, allocation->y, allocation->width,
                           allocation->ascent + allocation->descent);
   button->when (FL_WHEN_CHANGED);
   button->callback (widgetCallback, this);
   button->type (FL_TOGGLE_BUTTON);

   return button;
}

// ----------------------------------------------------------------------

template <class I> dw::core::Iterator *
FltkSelectionResource<I>::iterator (dw::core::Content::Type mask, bool atEnd)
{
   /** \bug Implementation. */
   return new core::EmptyIterator (this->getEmbed (), mask, atEnd);
}

// ----------------------------------------------------------------------

FltkOptionMenuResource::FltkOptionMenuResource (FltkPlatform *platform):
   FltkSelectionResource <dw::core::ui::OptionMenuResource> (platform)
{
   /* Fl_Menu_ does not like multiple menu items with the same label, and
    * insert() treats some characters specially unless escaped, so let's
    * do our own menu handling.
    */
   itemsAllocated = 0x10;
   menu = new Fl_Menu_Item[itemsAllocated];
   memset(menu, 0, itemsAllocated * sizeof(Fl_Menu_Item));
   itemsUsed = 1; // menu[0].text == NULL, which is an end-of-menu marker.

   init (platform);
}

FltkOptionMenuResource::~FltkOptionMenuResource ()
{
   for (int i = 0; i < itemsUsed; i++) {
      if (menu[i].text)
         free((char *) menu[i].text);
   }
   delete[] menu;
}

void FltkOptionMenuResource::setWidgetStyle (Fl_Widget *widget,
                                             core::style::Style *style)
{
   Fl_Choice *ch = (Fl_Choice *)widget;

   FltkResource::setWidgetStyle(widget, style);

   ch->textcolor(widget->labelcolor());
   ch->textfont(ch->labelfont());
   ch->textsize(ch->labelsize());
}

Fl_Widget *FltkOptionMenuResource::createNewWidget (core::Allocation
                                                     *allocation)
{
   Fl_Choice *choice =
      new Fl_Choice (allocation->x, allocation->y,
                          allocation->width,
                          allocation->ascent + allocation->descent);
   choice->menu(menu);
   return choice;
}

void FltkOptionMenuResource::widgetCallback (Fl_Widget *widget,
                                             void *data)
{
}

int FltkOptionMenuResource::getMaxItemWidth()
{
   int i, max = 0;

   for (i = 0; i < itemsUsed; i++) {
      int width = 0;
      const char *str = menu[i].text;

      if (str) {
         width = fl_width(str);
         if (width > max)
            max = width;
      }
   }
   return max;
}

void FltkOptionMenuResource::sizeRequest (core::Requisition *requisition)
{
   if (style) {
      FltkFont *font = (FltkFont*)style->font;
      fl_font(font->font, font->size);
      int maxItemWidth = getMaxItemWidth ();
      requisition->ascent = font->ascent + RELIEF_Y_THICKNESS;
      requisition->descent = font->descent + RELIEF_Y_THICKNESS;
      requisition->width = maxItemWidth
         + (requisition->ascent + requisition->descent)
         + 2 * RELIEF_X_THICKNESS;
   } else {
      requisition->width = 1;
      requisition->ascent = 1;
      requisition->descent = 0;
   }
}

void FltkOptionMenuResource::enlargeMenu ()
{
   Fl_Choice *ch = (Fl_Choice *)widget;
   int selected = ch->value();
   Fl_Menu_Item *newMenu;

   itemsAllocated += 0x10;
   newMenu = new Fl_Menu_Item[itemsAllocated];
   memcpy(newMenu, menu, itemsUsed * sizeof(Fl_Menu_Item));
   memset(newMenu + itemsUsed, 0, 0x10 * sizeof(Fl_Menu_Item));
   delete[] menu;
   menu = newMenu;
   ch->menu(menu);
   ch->value(selected);
}

Fl_Menu_Item *FltkOptionMenuResource::newItem()
{
   Fl_Menu_Item *item;

   if (itemsUsed == itemsAllocated)
      enlargeMenu();

   item = menu + itemsUsed - 1;
   itemsUsed++;

   return item;
}
   
void FltkOptionMenuResource::addItem (const char *str,
                                      bool enabled, bool selected)
{
   Fl_Menu_Item *item = newItem();

   item->text = strdup(str);

   if (enabled == false)
      item->flags = FL_MENU_INACTIVE;

   // TODO: verify that an array index is exactly what value() wants,
   // even when there are submenus.
   if (selected)
      ((Fl_Choice *)widget)->value(item);

   queueResize (true);
}

void FltkOptionMenuResource::pushGroup (const char *name, bool enabled)
{
   Fl_Menu_Item *item = newItem();

   item->text = strdup(name);

   if (enabled == false)
      item->flags = FL_MENU_INACTIVE;

   item->flags |= FL_SUBMENU;

   queueResize (true);
}

void FltkOptionMenuResource::popGroup ()
{
   /* Item with NULL text field closes the submenu */
   newItem();
   queueResize (true);
}

bool FltkOptionMenuResource::isSelected (int index)
{
   return index == ((Fl_Choice *)widget)->value();
}

int FltkOptionMenuResource::getNumberOfItems()
{
   return ((Fl_Choice*)widget)->size();
}

// ----------------------------------------------------------------------

FltkListResource::FltkListResource (FltkPlatform *platform,
                                    core::ui::ListResource::SelectionMode
                                    selectionMode, int rowCount):
   FltkSelectionResource <dw::core::ui::ListResource> (platform),
   itemsSelected(8)
{
   mode = selectionMode;
   showRows = rowCount;
   init (platform);
}

FltkListResource::~FltkListResource ()
{
}


Fl_Widget *FltkListResource::createNewWidget (core::Allocation *allocation)
{
   Fl_Tree *tree =
      new Fl_Tree (allocation->x, allocation->y, allocation->width,
                           allocation->ascent + allocation->descent);

   tree->selectmode((mode == SELECTION_MULTIPLE) ? FL_TREE_SELECT_MULTI
                                                 : FL_TREE_SELECT_SINGLE);
   tree->showroot(0);
   tree->connectorstyle(FL_TREE_CONNECTOR_NONE);
   tree->marginleft(-14);
   tree->callback(widgetCallback,this);
   tree->when(FL_WHEN_CHANGED);

   currParent = tree->root();
   return tree;
}

void FltkListResource::setWidgetStyle (Fl_Widget *widget,
                                       core::style::Style *style)
{
   Fl_Tree *t = (Fl_Tree *)widget;

   FltkResource::setWidgetStyle(widget, style);

   t->item_labelfont(widget->labelfont());
   t->item_labelsize(widget->labelsize());
   t->item_labelfgcolor(widget->labelcolor());
   t->item_labelbgcolor(widget->color());
}

void FltkListResource::widgetCallback (Fl_Widget *widget, void *data)
{
   Fl_Tree_Item *fltkItem = ((Fl_Tree *) widget)->callback_item ();
   int index = -1;
   if (fltkItem)
      index = (long) (fltkItem->user_data ());
   if (index > -1) {
      FltkListResource *res = (FltkListResource *) data;
      bool selected = fltkItem->is_selected ();
      res->itemsSelected.set (index, selected);
   }
}

void *FltkListResource::newItem (const char *str, bool enabled)
{
   Fl_Tree *tree = (Fl_Tree *) widget;
   Fl_Tree_Item *parent = (Fl_Tree_Item *)currParent;
   Fl_Tree_Item *item = tree->add(parent, str);

   enabled &= parent->is_active();
   item->activate(enabled);
   itemsSelected.increase ();

   return item;
}

void FltkListResource::addItem (const char *str, bool enabled, bool selected)
{
   Fl_Tree *tree = (Fl_Tree *) widget;
   Fl_Tree_Item *item = (Fl_Tree_Item *) newItem(str, enabled);

   itemsSelected.set (itemsSelected.size() - 1, selected);

   if (selected) {
      if (mode == SELECTION_MULTIPLE)
         item->select(selected);
      else
         tree->select_only(item, 0);
   }
   queueResize (true);
}

void FltkListResource::pushGroup (const char *name, bool enabled)
{
   currParent = (Fl_Tree_Item *) newItem(name, enabled);
   queueResize (true);
}

void FltkListResource::popGroup ()
{
   Fl_Tree_Item *p = (Fl_Tree_Item *)currParent;

   if (p->parent())
      currParent = p->parent();
}

int FltkListResource::getMaxItemWidth()
{
   Fl_Tree *tree = (Fl_Tree *)widget;
   int max = 0;

   for (Fl_Tree_Item *i = tree->first(); i; i = tree->next(i)) {
      int width = 0;

      if (i == tree->root())
         continue;

      for (Fl_Tree_Item *p = i->parent(); p != tree->root(); p = p->parent())
         width += tree->connectorwidth();

      if (i->label())
         width += fl_width(i->label());

      if (width > max)
         max = width;
   }
   return max;
}

void FltkListResource::sizeRequest (core::Requisition *requisition)
{
   if (style) {
      FltkFont *font = (FltkFont*)style->font;
      fl_font(font->font,font->size);
      int rows = getNumberOfItems();
      if (showRows < rows) {
         rows = showRows;
      }
      requisition->width = getMaxItemWidth() + 5 + Fl::scrollbar_size();;
      requisition->ascent = font->ascent + 5 +
                            (rows - 1) * (font->ascent + font->descent + 1);
      requisition->descent = font->descent + 3;
   } else {
      requisition->width = 1;
      requisition->ascent = 1;
      requisition->descent = 0;
   }
}

bool FltkListResource::isSelected (int index)
{
   return itemsSelected.get (index);
}

} // namespace ui
} // namespace fltk
} // namespace dw

