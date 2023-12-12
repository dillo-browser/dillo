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



#include <FL/Fl.H>
#include <FL/Fl_Window.H>

#include "../dw/core.hh"
#include "../dw/fltkcore.hh"
#include "../dw/fltkviewport.hh"
#include "../dw/table.hh"
#include "../dw/textblock.hh"
#include "../dw/ui.hh"
#include "form.hh"

using namespace lout::object;
using namespace lout::container::typed;
using namespace dw;
using namespace dw::core;
using namespace dw::core::style;
using namespace dw::core::ui;
using namespace dw::fltk;

int main(int argc, char **argv)
{
   FltkPlatform *platform = new FltkPlatform ();
   Layout *layout = new Layout (platform);

   Fl_Window *window = new Fl_Window(400, 400, "Dw UI Test");
   window->box(FL_NO_BOX);
   window->begin();

   FltkViewport *viewport = new FltkViewport (0, 0, 400, 400);
   layout->attachView (viewport);

   StyleAttrs styleAttrs;
   styleAttrs.initValues ();
   styleAttrs.margin.setVal (5);
   styleAttrs.color = Color::create (layout, 0x000000);
   styleAttrs.backgroundColor = Color::create (layout, 0xffffff);

   FontAttrs fontAttrs;
   fontAttrs.name = "Helvetica";
   fontAttrs.size = 14;
   fontAttrs.weight = 400;
   fontAttrs.style = FONT_STYLE_NORMAL;
   fontAttrs.letterSpacing = 0;
   fontAttrs.fontVariant = FONT_VARIANT_NORMAL;
   styleAttrs.font = dw::core::style::Font::create (layout, &fontAttrs);

   Style *tableStyle = Style::create (&styleAttrs);

   Table *table = new Table (false);
   table->setStyle (tableStyle);
   layout->setWidget (table);

   tableStyle->unref();

   styleAttrs.backgroundColor = NULL;
   styleAttrs.margin.setVal (0);

   Style *cellStyle = Style::create (&styleAttrs);

   // First of all, the resources. Later, they are embedded into the
   // widget tree.
   EntryResource *entryres1 =
      layout->getResourceFactory()->createEntryResource (10, false, NULL,NULL);
   entryres1->setText ("Hi!");
   EntryResource *entryres2 =
      layout->getResourceFactory()->createEntryResource (10, true, NULL,
                                                         "password field!");
   MultiLineTextResource *textres =
      layout->getResourceFactory()->createMultiLineTextResource (15,3,
                                                      "textarea placeholder!");
   RadioButtonResource *radiores1 =
      layout->getResourceFactory()->createRadioButtonResource (NULL, false);
   RadioButtonResource *radiores2 =
      layout->getResourceFactory()->createRadioButtonResource (radiores1,
                                                               false);
   CheckButtonResource *checkres =
      layout->getResourceFactory()->createCheckButtonResource (true);
   SelectionResource *selres[2];
   selres[0] = layout->getResourceFactory()->createOptionMenuResource ();
   selres[1] = layout->getResourceFactory()->createListResource
      (ListResource::SELECTION_AT_MOST_ONE, 4);
   LabelButtonResource *buttonres =
      layout->getResourceFactory()->createLabelButtonResource ("Run!");

   // Note on complex buttons: before any operations on the widget, which
   // need a layout, the complex button resource should be created, since
   // then, a layout and a platform are instantiated.
   Textblock *cbuttontext = new Textblock(false);
   ComplexButtonResource *cbuttonres =
      layout->getResourceFactory()->createComplexButtonResource (cbuttontext,
                                                                 true);
   cbuttontext->setStyle (cellStyle);
   cbuttontext->addText ("Run (complex)!", cellStyle);
   cbuttontext->flush ();

   // The entry resources are put into a special handler, which is
   // also a receiver for the button resources.
   form::Form *form = new form::Form();
   form->addTextResource ("val1", entryres1);
   form->addTextResource ("val2", entryres2);
   form->addTextResource ("text", textres);
   const char *radiovalues[] = { "radio1", "radio2", NULL };
   form->addRadioButtonResource ("val3", radiores1, radiovalues);
   form->addCheckButtonResource ("check", checkres);
   const char *selvalues[] = { "i1", "g1", "i11", "i12", "i13", "(pop)", "i2",
                               "g2", "i21", "i22", "i23", "(pop)", "i3", NULL};
   form->addSelectionResource ("val4", selres[0], selvalues);
   form->addSelectionResource ("val5", selres[1], selvalues);
   form->addButtonResource ("button", buttonres, "Run!");
   form->addButtonResource ("cbutton", cbuttonres, "cbuttonval");

   // Create the widgets.
   table->addRow (cellStyle);

   Textblock *label1 = new Textblock(false);
   label1->setStyle (cellStyle);
   table->addCell (label1, 1, 1);
   label1->addText ("val1 = ", cellStyle);
   label1->flush ();

   Embed *input1 = new Embed (entryres1);
   input1->setStyle (cellStyle);
   table->addCell (input1, 1, 1);

   table->addRow (cellStyle);

   Textblock *label2 = new Textblock(false);
   label2->setStyle (cellStyle);
   table->addCell (label2, 1, 1);
   label2->addText ("val2 = ", cellStyle);
   label2->flush ();

   Embed *input2 = new Embed (entryres2);
   input2->setStyle (cellStyle);
   table->addCell (input2, 1, 1);

   table->addRow (cellStyle);

   Textblock *label = new Textblock(false);
   label->setStyle (cellStyle);
   table->addCell (label, 1, 1);
   label->addText ("text = ", cellStyle);
   label->flush ();

   Embed *text = new Embed (textres);
   textres->setText("Hi textarea");
   text->setStyle (cellStyle);
   table->addCell (text, 1, 1);

   table->addRow (cellStyle);

   Textblock *radiolabel1 = new Textblock(false);
   radiolabel1->setStyle (cellStyle);
   table->addCell (radiolabel1, 2, 1);
   Embed *radio1 = new Embed (radiores1);
   radiolabel1->addWidget (radio1, cellStyle);
   radiolabel1->addText (" radio1", cellStyle);
   radiolabel1->flush ();

   table->addRow (cellStyle);
   Textblock *radiolabel2 = new Textblock(false);
   radiolabel2->setStyle (cellStyle);
   table->addCell (radiolabel2, 2, 1);
   Embed *radio2 = new Embed (radiores2);
   radiolabel2->addWidget (radio2, cellStyle);
   radiolabel2->addText (" radio2", cellStyle);
   radiolabel2->flush ();

   table->addRow (cellStyle);
   Textblock *checklabel = new Textblock(false);
   checklabel->setStyle (cellStyle);
   table->addCell (checklabel, 2, 1);
   Embed *check = new Embed (checkres);
   checklabel->addWidget (check, cellStyle);
   checklabel->addText (" check", cellStyle);
   checklabel->flush ();

   for(int i = 0; i < 2; i++) {
      table->addRow (cellStyle);

      Embed *sel = new Embed (selres[i]);
      sel->setStyle (cellStyle);
      table->addCell (sel, 2, 1);

      selres[i]->addItem("item 1", true, false);

      selres[i]->pushGroup("group 1", true);
      selres[i]->addItem("item 1/1", true, false);
      selres[i]->addItem("item 1/2", true, true);
      selres[i]->addItem("item 1/3", false, false);
      selres[i]->popGroup();

      selres[i]->addItem("item 2", false, i == 1);

      selres[i]->pushGroup("group 2", true);
      selres[i]->addItem("item 2/1", true, false);
      selres[i]->addItem("item 2/2", true, false);
      selres[i]->addItem("item 2/3", false, false);
      selres[i]->popGroup();

      selres[i]->addItem("item 3", false, false);
   }

   table->addRow (cellStyle);
   Embed *button = new Embed (buttonres);
   button->setStyle (cellStyle);
   table->addCell (button, 2, 1);

   table->addRow (cellStyle);
   Embed *cbutton = new Embed (cbuttonres);
   cbutton->setStyle (cellStyle);
   table->addCell (cbutton, 2, 1);

   cellStyle->unref();

   window->resizable(viewport);
   window->show();
   int errorCode = Fl::run();

   delete form;
   delete layout;

   return errorCode;
}
