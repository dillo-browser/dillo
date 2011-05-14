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



#include "form.hh"

namespace form {

using namespace dw::core::ui;

Form::ResourceDecorator::ResourceDecorator (const char *name)
{
   this->name = strdup (name);
}

Form::ResourceDecorator::~ResourceDecorator ()
{
   free((char *)name);
}

Form::TextResourceDecorator::TextResourceDecorator (const char *name,
                                                  TextResource *resource):
   Form::ResourceDecorator (name)
{
   this->resource = resource;
}

const char *Form::TextResourceDecorator::getValue ()
{
   return resource->getText ();
}

Form::RadioButtonResourceDecorator::RadioButtonResourceDecorator
   (const char *name, RadioButtonResource *resource, const char **values):
      Form::ResourceDecorator (name)
{
   this->resource = resource;

   int n = 0;
   while (values[n])
      n++;
   this->values = new const char*[n + 1];
   for (int i = 0; i < n; i++)
      this->values[i] = strdup (values[i]);
   this->values[n] = 0;
}

Form::RadioButtonResourceDecorator::~RadioButtonResourceDecorator ()
{
   for (int i = 0; values[i]; i++)
      free((char *)values[i]);
   delete[] values;
}

const char *Form::RadioButtonResourceDecorator::getValue ()
{
   RadioButtonResource::GroupIterator *it;
   int i;
   for (it = resource->groupIterator (), i = 0; it->hasNext (); i++) {
      RadioButtonResource *resource = it->getNext ();
      if(resource->isActivated ()) {
         it->unref ();
         return values[i];
      }
   }

   it->unref ();
   return NULL;
}

Form::CheckButtonResourceDecorator::CheckButtonResourceDecorator
   (const char *name, CheckButtonResource *resource):
   Form::ResourceDecorator (name)
{
   this->resource = resource;
}

const char *Form::CheckButtonResourceDecorator::getValue ()
{
   return resource->isActivated () ? "true" : NULL;
}

Form::SelectionResourceDecorator::SelectionResourceDecorator
   (const char *name, SelectionResource *resource, const char **values):
      Form::ResourceDecorator (name)
{
   this->resource = resource;

   int n = 0;
   while (values[n])
      n++;
   this->values = new const char*[n + 1];
   for(int i = 0; i < n; i++)
      this->values[i] = strdup (values[i]);
   this->values[n] = 0;
}

Form::SelectionResourceDecorator::~SelectionResourceDecorator ()
{
   for(int i = 0; values[i]; i++)
      free((char *)values[i]);
   delete[] values;
}

const char *Form::SelectionResourceDecorator::getValue ()
{
   valueBuf.clear();
   int n = resource->getNumberOfItems ();
   bool first = true;
   for (int i = 0; i < n; i++) {
      if (resource->isSelected (i)) {
         if (!first)
            valueBuf.append (", ");
         valueBuf.append (values[i]);
         first = false;
      }
   }

   return valueBuf.getChars ();
}

void Form::FormActivateReceiver::activate (Resource *resource)
{
   form->send (NULL, NULL, -1, -1);
}

void Form::FormActivateReceiver::enter (Resource *resource)
{
}

void Form::FormActivateReceiver::leave (Resource *resource)
{
}

Form::FormClickedReceiver::FormClickedReceiver (Form *form, const char *name,
                                          const char *value)
{
   this->form = form;
   this->name = strdup (name);
   this->value = strdup (value);
}

Form::FormClickedReceiver::~FormClickedReceiver ()
{
   free((char *)name);
   free((char *)value);
}

void Form::FormClickedReceiver::clicked (Resource *resource,
                                         dw::core::EventButton *event)
{
   form->send (name, value, event->xCanvas, event->yCanvas);
}

Form::Form ()
{
   resources = new lout::container::typed::List <ResourceDecorator> (true);
   activateReceiver = new FormActivateReceiver (this);
   clickedReceivers =
      new lout::container::typed::List <FormClickedReceiver> (true);
}

Form::~Form ()
{
   delete resources;
   delete activateReceiver;
   delete clickedReceivers;
}

/**
 * \brief Adds an instance of dw::core::ui::TextResource.
 */
void Form::addTextResource (const char *name,
                            dw::core::ui::TextResource *resource)
{
   resources->append (new TextResourceDecorator (name, resource));
   resource->connectActivate (activateReceiver);
}

/**
 * \brief Adds an instance of dw::core::ui::RadioButtonResource.
 *
 * This method has to be called only once for a group of radio buttons.
 */
void Form::addRadioButtonResource (const char *name,
                                   dw::core::ui::RadioButtonResource *resource,
                                   const char **values)
{
   resources->append (new RadioButtonResourceDecorator (name, resource,
                                                        values));
   resource->connectActivate (activateReceiver);
}

/**
 * \brief Adds an instance of dw::core::ui::CheckButtonResource.
 */
void Form::addCheckButtonResource (const char *name,
                                   dw::core::ui::CheckButtonResource *resource)
{
   resources->append (new CheckButtonResourceDecorator (name, resource));
   resource->connectActivate (activateReceiver);
}

/**
 * \brief Adds an instance of dw::core::ui::SelectionResource.
 */
void Form::addSelectionResource (const char *name,
                                 dw::core::ui::SelectionResource *resource,
                                 const char **values)
{
   resources->append (new SelectionResourceDecorator (name, resource, values));
   resource->connectActivate (activateReceiver);
}

/**
 * \todo Comment this;
 */
void Form::addButtonResource (const char *name,
                              dw::core::ui::ButtonResource *resource,
                              const char *value)
{
   FormClickedReceiver *receiver =
      new FormClickedReceiver (this, name, value);
   resource->connectClicked (receiver);
   clickedReceivers->append (receiver);
}

/**
 * \todo Comment this;
 */
void Form::send (const char *buttonName, const char *buttonValue, int x, int y)
{
   for (lout::container::typed::Iterator <ResourceDecorator> it =
           resources->iterator ();
        it.hasNext (); ) {
      ResourceDecorator *resource = it.getNext ();
      const char *value = resource->getValue ();
      if (value)
         printf ("%s = %s; x=%d y=%d\n", resource->getName (), value, x, y);
   }

   if(buttonName && buttonValue)
      printf ("%s = %s\n", buttonName, buttonValue);
}

} // namespace form
