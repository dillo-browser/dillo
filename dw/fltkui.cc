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
   delete label;
}

Fl_Widget *FltkLabelButtonResource::createNewWidget (core::Allocation
                                                     *allocation)
{
   Fl_Button *button =
        new Fl_Button (allocation->x, allocation->y, allocation->width,
                       allocation->ascent + allocation->descent, label);
// button->clear_flag (SHORTCUT_LABEL);
   button->callback (widgetCallback, this);
   button->when (FL_WHEN_RELEASE);
   return button;
}

void FltkLabelButtonResource::sizeRequest (core::Requisition *requisition)
{
   if (style) {
      FltkFont *font = (FltkFont*)style->font;
#if 0
PORT1.3
      ::fltk::setfont(font->font,font->size);
#endif
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
      delete label;
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

void FltkEntryResource::setDisplayed(bool displayed)
{
   FltkResource::setDisplayed(displayed);
   queueResize(true);
}

void FltkEntryResource::sizeRequest (core::Requisition *requisition)
{
   if (displayed() && style) {
      FltkFont *font = (FltkFont*)style->font;
#if 0
PORT1.3
      ::fltk::setfont(font->font,font->size);
#endif
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

void FltkMultiLineTextResource::sizeRequest (core::Requisition *requisition)
{
   if (style) {
      FltkFont *font = (FltkFont*)style->font;
#if 0
PORT1.3
      ::fltk::setfont(font->font,font->size);
#endif
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
void FltkToggleButtonResource<I>::sizeRequest (core::Requisition *requisition)
{
   FltkFont *font = (FltkFont *)
      (this->FltkResource::style ? this->FltkResource::style->font : NULL);

   if (font) {
#if 0
PORT1.3
      ::fltk::setfont(font->font, font->size);
#endif
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
// cb->clear_flag (SHORTCUT_LABEL);
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
// button->clear_flag (SHORTCUT_LABEL);
   button->when (FL_WHEN_CHANGED);
   button->callback (widgetCallback, this);
   button->type (FL_TOGGLE_BUTTON);

   return button;
}

/*

FLTK 1.3's Browser doesn't seem to permit a hierarchy, so I think we'll
use Choice and Tree. There is no Item or ItemGroup, but I see they both
have an add("menu/sub/sub/leaf") interface (not based on a common
ancestor, though), so maybe SelectionResource's job will be to keep
track of our menu-adding string or something. This will be experimental
enough that I don't want to write the code until dw's in good enough shape
to test whether I'm going about it sensibly.

*/
// ----------------------------------------------------------------------

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

template <class I> int FltkSelectionResource<I>::getMaxStringWidth ()
{
   return 100;
}

// ----------------------------------------------------------------------

FltkOptionMenuResource::FltkOptionMenuResource (FltkPlatform *platform):
   FltkSelectionResource <dw::core::ui::OptionMenuResource> (platform)
{
   init (platform);
}

FltkOptionMenuResource::~FltkOptionMenuResource ()
{
}


Fl_Widget *FltkOptionMenuResource::createNewWidget (core::Allocation
                                                     *allocation)
{
   Fl_Choice *choice =
      new Fl_Choice (allocation->x, allocation->y,
                          allocation->width,
                          allocation->ascent + allocation->descent);
// choice->clear_flag (SHORTCUT_LABEL);
   choice->callback(widgetCallback,this);
   return choice;
}

void FltkOptionMenuResource::widgetCallback (Fl_Widget *widget,
                                             void *data)
{
#if 0
   ((FltkOptionMenuResource *) data)->selection =
      (long) (((Fl_Menu *) widget)->item()->user_data());
#endif
}

void FltkOptionMenuResource::sizeRequest (core::Requisition *requisition)
{
#if 0
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
#else
 requisition->width = 100;
 requisition->ascent = 11 + RELIEF_Y_THICKNESS;
 requisition->descent = 3 + RELIEF_Y_THICKNESS;
#endif
}

void FltkOptionMenuResource::addItem (const char *str,
                                      bool enabled, bool selected)
{
   int flags = enabled ? 0 : FL_MENU_INACTIVE;
   int index = ((Fl_Choice *)widget)->add(str, 0, 0, 0, flags);

   if (selected)
      ((Fl_Choice *)widget)->value(index);

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
// tree->clear_flag (SHORTCUT_LABEL);
   tree->callback(widgetCallback,this);
   tree->when(FL_WHEN_CHANGED);
   return tree;
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

void FltkListResource::addItem (const char *str, bool enabled, bool selected)
{
   Fl_Tree *tree = (Fl_Tree *) widget;
   Fl_Tree_Item *item = tree->add(tree->root(), str);
   int index = itemsSelected.size ();

   item->activate(enabled);

   itemsSelected.increase ();
   itemsSelected.set (index,selected);

   if (selected) {
      if (mode == SELECTION_MULTIPLE)
         item->select(selected);
      else
         ((Fl_Tree *)widget)->select_only(item, 0);
   }

   queueResize (true);
}

void FltkListResource::sizeRequest (core::Requisition *requisition)
{
#if 0
PORT1.3
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
#else
 requisition->width = 100;
 requisition->ascent = 14 * showRows;
 requisition->descent = 0;
#endif
}

bool FltkListResource::isSelected (int index)
{
   return itemsSelected.get (index);
}

} // namespace ui
} // namespace fltk
} // namespace dw

