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

#include <stdio.h>
#include <fltk/Widget.h>
#include <fltk/Group.h>
#include <fltk/Input.h>
#include <fltk/TextEditor.h>
#include <fltk/RadioButton.h>
#include <fltk/CheckButton.h>
#include <fltk/Choice.h>
#include <fltk/Browser.h>
#include <fltk/Font.h>
#include <fltk/draw.h>
#include <fltk/Symbol.h>
#include <fltk/Item.h>
#include <fltk/ItemGroup.h>
#include <fltk/events.h>

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

void FltkResource::setWidgetStyle (::fltk::Widget *widget,
                                   core::style::Style *style)
{
   FltkFont *font = (FltkFont*)style->font;
   widget->labelsize (font->size);
   widget->labelfont (font->font);
   widget->textsize (font->size);
   widget->textfont (font->font);

   FltkColor *bg = (FltkColor*)style->backgroundColor;
   if (bg) {
      int normal_bg = bg->colors[FltkColor::SHADING_NORMAL];

      if (style->color) {
         int style_fg = ((FltkColor*)style->color)->colors
                                                   [FltkColor::SHADING_NORMAL];
         ::fltk::Color fg = ::fltk::contrast(style_fg, normal_bg);

         widget->labelcolor(fg);
         widget->textcolor(fg);
         widget->selection_color(fg);
      }

      widget->color(normal_bg);
      widget->buttoncolor(normal_bg);
      widget->selection_textcolor(normal_bg);
      if (widget->type() != ::fltk::Widget::RADIO &&
          widget->type() != ::fltk::Widget::TOGGLE) {
         /* it looks awful to highlight the buttons */
         widget->highlight_color(bg->colors[FltkColor::SHADING_LIGHT]);
      }
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
   delete label;
}

::fltk::Widget *FltkLabelButtonResource::createNewWidget (core::Allocation
                                                          *allocation)
{
   ::fltk::Button *button =
        new ::fltk::Button (allocation->x, allocation->y, allocation->width,
                            allocation->ascent + allocation->descent,
                            label);
   button->set_flag (::fltk::RAW_LABEL);
   button->callback (widgetCallback, this);
   button->when (::fltk::WHEN_RELEASE);
   return button;
}

void FltkLabelButtonResource::sizeRequest (core::Requisition *requisition)
{
   if (style) {
      FltkFont *font = (FltkFont*)style->font;
      ::fltk::setfont(font->font,font->size);
      requisition->width =
         (int)::fltk::getwidth (label, strlen (label))
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
   int s1 = ::fltk::event_state ();
   int s2 = (core::ButtonState)0;

   if (s1 & ::fltk::SHIFT)   s2 |= core::SHIFT_MASK;
   if (s1 & ::fltk::CTRL)    s2 |= core::CONTROL_MASK;
   if (s1 & ::fltk::ALT)     s2 |= core::META_MASK;
   if (s1 & ::fltk::BUTTON1) s2 |= core::BUTTON1_MASK;
   if (s1 & ::fltk::BUTTON2) s2 |= core::BUTTON2_MASK;
   if (s1 & ::fltk::BUTTON3) s2 |= core::BUTTON3_MASK;

   return (core::ButtonState)s2;
}

static void setButtonEvent(dw::core::EventButton *event)
{
   event->xCanvas = ::fltk::event_x();
   event->yCanvas = ::fltk::event_y();
   event->state = getDwButtonState();
   event->button = ::fltk::event_button();
   event->numPressed = ::fltk::event_clicks() + 1;
}

void FltkLabelButtonResource::widgetCallback (::fltk::Widget *widget,
                                              void *data)
{
   if ((widget->when () & ::fltk::WHEN_RELEASE) &&
       ((::fltk::event_key() == FL_Enter) ||
        (::fltk::event_button() == FL_LEFT_MOUSE ||
         ::fltk::event_button() == FL_MIDDLE_MOUSE))) {
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

void FltkComplexButtonResource::widgetCallback (::fltk::Widget *widget,
                                                void *data)
{
   FltkComplexButtonResource *res = (FltkComplexButtonResource*)data;

   if (widget->when() == ::fltk::WHEN_RELEASE &&
       ((::fltk::event_key() == FL_Enter) ||
        (::fltk::event_button() == FL_LEFT_MOUSE ||
         ::fltk::event_button() == FL_MIDDLE_MOUSE))) {
      res->click_x = ::fltk::event_x();
      res->click_y = ::fltk::event_y();
      dw::core::EventButton event;
      setButtonEvent(&event);
      res->emitClicked(&event);
   } else {
      ((FltkViewBase*)res->flatView)->handle(::fltk::event());
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


::fltk::Widget *FltkComplexButtonResource::createNewWidget (core::Allocation
                                                            *allocation)
{
   ComplexButton *button =
      new ComplexButton (allocation->x, allocation->y, allocation->width,
                         allocation->ascent + allocation->descent);
   button->callback (widgetCallback, this);
   button->when (::fltk::WHEN_RELEASE);
   if (!relief)
      button->box(::fltk::FLAT_BOX);

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
      delete label;
}

::fltk::Widget *FltkEntryResource::createNewWidget (core::Allocation
                                                    *allocation)
{
   ::fltk::Input *input =
        new ::fltk::Input (allocation->x, allocation->y, allocation->width,
                           allocation->ascent + allocation->descent);
   if (password)
      input->type(::fltk::Input::SECRET);
   input->callback (widgetCallback, this);
   input->when (::fltk::WHEN_ENTER_KEY_ALWAYS);

   if (label) {
      input->label(label);
      input->set_flag(::fltk::ALIGN_INSIDE_LEFT);
   }
   if (initText)
      input->value (initText);

   return input;
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
      ::fltk::setfont(font->font,font->size);
      requisition->width =
         (int)::fltk::getwidth ("n", 1)
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

void FltkEntryResource::widgetCallback (::fltk::Widget *widget,
                                        void *data)
{
   /* The (::fltk::event_key() == FL_Enter) test
    * is necessary because WHEN_ENTER_KEY also includes
    * other events we're not interested in. For instance pressing
    * The Back or Forward, buttons, or the first click on a rendered
    * page. BUG: this must be investigated and reported to FLTK2 team
    */
   _MSG("when = %d\n", widget->when ());
   if ((widget->when () & ::fltk::WHEN_ENTER_KEY_ALWAYS) &&
       (::fltk::event_key() == FL_Enter))
      ((FltkEntryResource*)data)->emitActivate ();
}

const char *FltkEntryResource::getText ()
{
   return ((::fltk::Input*)widget)->value ();
}

void FltkEntryResource::setText (const char *text)
{
   if (initText)
      delete initText;
   initText = strdup (text);

   ((::fltk::Input*)widget)->value (initText);
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
   buffer = new ::fltk::TextBuffer;
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
   ((::fltk::TextEditor *) widget)->buffer (0);
   delete buffer;
}

::fltk::Widget *FltkMultiLineTextResource::createNewWidget (core::Allocation
                                                            *allocation)
{
   ::fltk::TextEditor *text =
      new ::fltk::TextEditor (allocation->x, allocation->y,
                              allocation->width,
                              allocation->ascent + allocation->descent);
   text->buffer (buffer);
   return text;
}

void FltkMultiLineTextResource::sizeRequest (core::Requisition *requisition)
{
   if (style) {
      FltkFont *font = (FltkFont*)style->font;
      ::fltk::setfont(font->font,font->size);
      requisition->width =
         (int)::fltk::getwidth ("n", 1) * numCols +
         2 * RELIEF_X_THICKNESS;
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
::fltk::Widget *FltkToggleButtonResource<I>::createNewWidget (core::Allocation
                                                              *allocation)
{
   ::fltk::Button *button = createNewButton (allocation);
   button->value (initActivated);
   return button;
}


template <class I>
void FltkToggleButtonResource<I>::sizeRequest (core::Requisition *requisition)
{
   FltkFont *font = (FltkFont *)
      (this->FltkResource::style ? this->FltkResource::style->font : NULL);

   if (font) {
      ::fltk::setfont(font->font, font->size);
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
   return ((::fltk::Button*)this->widget)->value ();
}


template <class I>
void FltkToggleButtonResource<I>::setActivated (bool activated)
{
   initActivated = activated;
   ((::fltk::Button*)this->widget)->value (initActivated);
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


::fltk::Button *FltkCheckButtonResource::createNewButton (core::Allocation
                                                          *allocation)
{
   ::fltk::CheckButton *cb =
      new ::fltk::CheckButton (allocation->x, allocation->y, allocation->width,
                               allocation->ascent + allocation->descent);
   cb->set_flag (::fltk::RAW_LABEL);
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

void FltkRadioButtonResource::widgetCallback (::fltk::Widget *widget,
                                              void *data)
{
   if (widget->when () & ::fltk::WHEN_CHANGED)
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

::fltk::Button *FltkRadioButtonResource::createNewButton (core::Allocation
                                                          *allocation)
{
   /*
    * Groups of fltk::RadioButton must be added to one fltk::Group, which is
    * not possible in this context. For this, we do the grouping ourself,
    * based on FltkRadioButtonResource::Group.
    *
    * What we actually need for this, is a widget, which behaves like a
    * check button, but looks like a radio button. The first depends on the
    * type, the second on the style. Since the type is simpler to change
    * than the style, we create a radio button, and then change the type
    * (instead of creating a check button, and changing the style).
    */

   ::fltk::Button *button =
      new ::fltk::RadioButton (allocation->x, allocation->y,
                               allocation->width,
                               allocation->ascent + allocation->descent);
   button->set_flag (::fltk::RAW_LABEL);
   button->when (::fltk::WHEN_CHANGED);
   button->callback (widgetCallback, this);
   button->type (::fltk::Button::TOGGLE);

   return button;
}

// ----------------------------------------------------------------------

template <class I> FltkSelectionResource<I>::Item::Item (Type type,
                                                         const char *name,
                                                         bool enabled,
                                                         bool selected)
{
   this->type = type;
   this->name = name ? strdup (name) : NULL;
   this->enabled = enabled;
   initSelected = selected;
}

template <class I> FltkSelectionResource<I>::Item::~Item ()
{
   if (name)
      delete name;
}

template <class I>
::fltk::Item *FltkSelectionResource<I>::Item::createNewWidget (int index)
{
   ::fltk::Item *item = new ::fltk::Item (name);
   item->set_flag (::fltk::RAW_LABEL);
   item->user_data ((void *) index);
   return item;
}

template <class I>
::fltk::ItemGroup *
FltkSelectionResource<I>::Item::createNewGroupWidget ()
{
   ::fltk::ItemGroup *itemGroup = new ::fltk::ItemGroup (name);
   itemGroup->set_flag (::fltk::RAW_LABEL);
   itemGroup->user_data ((void *) -1L);
   return itemGroup;
}


template <class I>
FltkSelectionResource<I>::WidgetStack::WidgetStack (::fltk::Menu *widget)
{
   this->widget = widget;
   this->stack = new Stack <TypedPointer < ::fltk::Menu> > (true);
}

template <class I> FltkSelectionResource<I>::WidgetStack::~WidgetStack ()
{
   delete stack;
}


template <class I>
FltkSelectionResource<I>::FltkSelectionResource (FltkPlatform *platform):
   FltkSpecificResource<I> (platform)
{
   widgetStacks = new List <WidgetStack> (true);
   allItems = new List <Item> (true);
   items = new Vector <Item> (16, false);
}

template <class I> FltkSelectionResource<I>::~FltkSelectionResource ()
{
   delete widgetStacks;
   delete allItems;
   delete items;
}

template <class I> dw::core::Iterator *
FltkSelectionResource<I>::iterator (dw::core::Content::Type mask, bool atEnd)
{
   /** \bug Implementation. */
   return new core::EmptyIterator (this->getEmbed (), mask, atEnd);
}

template <class I> ::fltk::Widget *
FltkSelectionResource<I>::createNewWidget (core::Allocation *allocation)
{
   /** \todo Attributes (enabled, selected). */

   ::fltk::Menu *menu = createNewMenu (allocation);
   WidgetStack *widgetStack = new WidgetStack (menu);
   widgetStack->stack->push (new TypedPointer < ::fltk::Menu> (menu));
   widgetStacks->append (widgetStack);


   ::fltk::Menu *itemGroup;
   ::fltk::Item *itemWidget;

   ::fltk::Group *currGroup = widgetStack->stack->getTop()->getTypedValue();

   int index = 0;
   for (Iterator <Item> it = allItems->iterator (); it.hasNext (); ) {
      Item *item = it.getNext ();
      switch (item->type) {
      case Item::ITEM:
         itemWidget = item->createNewWidget (index++);
         currGroup->add (itemWidget);
         break;

      case Item::START:
         itemGroup = item->createNewGroupWidget ();
         currGroup->add (itemGroup);
         widgetStack->stack->push (new TypedPointer < ::fltk::Menu> (menu));
         currGroup = itemGroup;
         break;

      case Item::END:
         widgetStack->stack->pop ();
         currGroup = widgetStack->stack->getTop()->getTypedValue();
         break;
      }
   }

   return menu;
}

template <class I>
typename FltkSelectionResource<I>::Item *
FltkSelectionResource<I>::createNewItem (typename Item::Type type,
                                         const char *name,
                                         bool enabled,
                                         bool selected) {
   return new Item(type,name,enabled,selected);
}

template <class I> void FltkSelectionResource<I>::addItem (const char *str,
                                                           bool enabled,
                                                           bool selected)
{
   int index = items->size ();
   Item *item = createNewItem (Item::ITEM, str, enabled, selected);
   items->put (item);
   allItems->append (item);

   for (Iterator <WidgetStack> it = widgetStacks->iterator ();
        it.hasNext(); ) {
      WidgetStack *widgetStack = it.getNext ();
      ::fltk::Item *itemWidget = item->createNewWidget (index);
      widgetStack->stack->getTop()->getTypedValue()->add (itemWidget);

      if (!enabled)
         itemWidget->deactivate ();

      if (selected) {
         itemWidget->set_selected();
         if (setSelectedItems ()) {
            // Handle multiple item selection.
            int *pos = new int[widgetStack->stack->size ()];
            int i;
            Iterator <TypedPointer < ::fltk::Menu> > it;
            for (it = widgetStack->stack->iterator (),
                    i = widgetStack->stack->size () - 1;
                 it.hasNext ();
                 i--) {
               TypedPointer < ::fltk::Menu> * p = it.getNext ();
               pos[i] =  p->getTypedValue()->children () - 1;
            }
            widgetStack->widget->set_item (pos, widgetStack->stack->size ());
            delete [] pos;
         }
      }
   }
}

template <class I> void FltkSelectionResource<I>::pushGroup (const char *name,
                                                             bool enabled)
{
   Item *item = createNewItem (Item::START, name, enabled);
   allItems->append (item);

   for (Iterator <WidgetStack> it = widgetStacks->iterator ();
        it.hasNext(); ) {
      WidgetStack *widgetStack = it.getNext ();
      ::fltk::ItemGroup *group = item->createNewGroupWidget ();
      widgetStack->stack->getTop()->getTypedValue()->add (group);
      widgetStack->stack->push (new TypedPointer < ::fltk::Menu> (group));
      if (!enabled)
         group->deactivate ();
   }
}

template <class I> void FltkSelectionResource<I>::popGroup ()
{
   Item *item = createNewItem (Item::END);
   allItems->append (item);

   for (Iterator <WidgetStack> it = widgetStacks->iterator ();
        it.hasNext(); ) {
      WidgetStack *widgetStack = it.getNext ();
      widgetStack->stack->pop ();
   }
}

template <class I> int FltkSelectionResource<I>::getNumberOfItems ()
{
   return items->size ();
}

template <class I> const char *FltkSelectionResource<I>::getItem (int index)
{
   return items->get(index)->name;
}

template <class I> int FltkSelectionResource<I>::getMaxStringWidth ()
{
   int width = 0, numberOfItems = getNumberOfItems ();
   for (int i = 0; i < numberOfItems; i++) {
      int len = (int)::fltk::getwidth (getItem(i));
      if (len > width) width = len;
   }
   return width;
}

// ----------------------------------------------------------------------

FltkOptionMenuResource::FltkOptionMenuResource (FltkPlatform *platform):
   FltkSelectionResource <dw::core::ui::OptionMenuResource> (platform),
   selection(-1)
{
   init (platform);
}

FltkOptionMenuResource::~FltkOptionMenuResource ()
{
}


::fltk::Menu *FltkOptionMenuResource::createNewMenu (core::Allocation
                                                     *allocation)
{
   ::fltk::Menu *menu =
      new ::fltk::Choice (allocation->x, allocation->y,
                          allocation->width,
                          allocation->ascent + allocation->descent);
   menu->set_flag (::fltk::RAW_LABEL);
   menu->callback(widgetCallback,this);
   return menu;
}

void FltkOptionMenuResource::widgetCallback (::fltk::Widget *widget,
                                             void *data)
{
   ((FltkOptionMenuResource *) data)->selection =
      (long) (((::fltk::Menu *) widget)->item()->user_data());
}

void FltkOptionMenuResource::sizeRequest (core::Requisition *requisition)
{
   if (style) {
      FltkFont *font = (FltkFont*)style->font;
      ::fltk::setfont(font->font,font->size);
      int maxStringWidth = getMaxStringWidth ();
      requisition->ascent = font->ascent + RELIEF_Y_THICKNESS;
      requisition->descent = font->descent + RELIEF_Y_THICKNESS;
      requisition->width = maxStringWidth
         + (requisition->ascent + requisition->descent) * 4 / 5
         + 2 * RELIEF_X_THICKNESS;
   } else {
      requisition->width = 1;
      requisition->ascent = 1;
      requisition->descent = 0;
   }
}

void FltkOptionMenuResource::addItem (const char *str,
                                      bool enabled, bool selected)
{
   FltkSelectionResource<dw::core::ui::OptionMenuResource>::addItem
      (str,enabled,selected);
   if (selected)
      selection = (items->size ()) - 1;

   queueResize (true);
}

bool FltkOptionMenuResource::isSelected (int index)
{
   return index == selection;
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


::fltk::Menu *FltkListResource::createNewMenu (core::Allocation *allocation)
{
   ::fltk::Menu *menu =
      new ::fltk::Browser (allocation->x, allocation->y, allocation->width,
                           allocation->ascent + allocation->descent);
   if (mode == SELECTION_MULTIPLE)
      menu->type(::fltk::Browser::MULTI);
   menu->set_flag (::fltk::RAW_LABEL);
   menu->callback(widgetCallback,this);
   menu->when(::fltk::WHEN_CHANGED);
   return menu;
}

void FltkListResource::widgetCallback (::fltk::Widget *widget, void *data)
{
   ::fltk::Widget *fltkItem = ((::fltk::Menu *) widget)->item ();
   int index = -1;
   if (fltkItem)
      index = (long) (fltkItem->user_data ());
   if (index > -1) {
      /* A MultiBrowser will trigger a callback for each item that is
       * selected and each item that is deselected, but a "plain"
       * Browser will only trigger the callback for the newly selected item
       * (for which selected() is false, incidentally).
       */
      FltkListResource *res = (FltkListResource *) data;
      if (res->mode == SELECTION_MULTIPLE) {
         bool selected = fltkItem->selected ();
         res->itemsSelected.set (index, selected);
      } else {
         int size = res->itemsSelected.size();
         for (int i = 0; i < size; i++)
            res->itemsSelected.set (i, false);
         res->itemsSelected.set (index, true);
      }
   }
}

void FltkListResource::addItem (const char *str, bool enabled, bool selected)
{
   FltkSelectionResource<dw::core::ui::ListResource>::addItem
      (str,enabled,selected);
   int index = itemsSelected.size ();
   itemsSelected.increase ();
   itemsSelected.set (index,selected);
   queueResize (true);
}

void FltkListResource::sizeRequest (core::Requisition *requisition)
{
   if (style) {
      FltkFont *font = (FltkFont*)style->font;
      ::fltk::setfont(font->font,font->size);
      int rows = getNumberOfItems();
      if (showRows < rows) {
         rows = showRows;
      }
      /*
       * The widget sometimes shows scrollbars when they are not required.
       * The following values try to keep any scrollbars from obscuring
       * options, at the cost of showing too much whitespace at times.
       */
      requisition->width = getMaxStringWidth() + 24;
      requisition->ascent = font->ascent + 2 +
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

