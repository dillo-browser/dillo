#include "form.hh"
#include "html.hh"

namespace form {

using namespace dw::core::ui;

Form::ResourceDecorator::ResourceDecorator (const char *name)
{
   this->name = strdup (name);
}

Form::ResourceDecorator::~ResourceDecorator ()
{
   delete name;
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
   this->values = new const char*[n];
   for(int i = 0; i < n; i++)
      this->values[i] = strdup (values[i]);
   values[n] = 0;
}

Form::RadioButtonResourceDecorator::~RadioButtonResourceDecorator ()
{
   for(int i = 0; values[i]; i++)
      delete values[i];
   delete values;
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


Form::Form (void *p)
{
   ext_data = p;
   resources = new container::typed::List <ResourceDecorator> (true);
}

Form::~Form ()
{
   delete resources;
}

void Form::clicked (ButtonResource *resource, int buttonNo, int x, int y)
{
/*
   for (container::typed::Iterator <ResourceDecorator> it =
           resources->iterator ();
        it.hasNext (); ) {
      ResourceDecorator *resource = it.getNext ();
      const char *value = resource->getValue ();
      if (value)
         printf ("%s = %s\n", resource->getName (), value);
   }
*/
   printf ("Form::clicked:: Button was clicked\n");
   // Let html.cc handle the event
   a_Html_form_event_handler(ext_data, this, (Resource*)resource, x, y);
}

void Form::activate (Resource *resource)
{
   a_Html_form_event_handler(ext_data, this, (Resource*)resource, -1, -1);
}

} // namespace form
