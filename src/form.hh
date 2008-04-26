#ifndef __FORM_HH__
#define __FORM_HH__

#include "dw/core.hh"
#include "dw/ui.hh"

namespace form {

/**
 * \brief Handles HTML form data.
 *
 * Add resources by calling the respective add...Resource method. Furthermore,
 * this class implements dw::core::ui::ButtonResource::ClickedReceiver and
 * dw::core::ui::Resource::ActivateReceiver.
 */
class Form: public dw::core::ui::ButtonResource::ClickedReceiver,
            public dw::core::ui::Resource::ActivateReceiver
{
private:
   /**
    * \brief Decorates instances of dw::core::ui::Resource.
    *
    * This is the abstract base class, sub classes have to be defined to
    * decorate specific sub interfaces of dw::core::ui::Resource.
    */
   class ResourceDecorator: public lout::object::Object
   {
   private:
      const char *name;

   protected:
      ResourceDecorator (const char *name);
      ~ResourceDecorator ();

   public:
      inline const char *getName () { return name; }
      virtual const char *getValue () = 0;
   };

   /**
    * \brief Decorates instances of dw::core::ui::TextResource.
    */
   class TextResourceDecorator: public ResourceDecorator
   {
   private:
      dw::core::ui::TextResource *resource;

   public:
      TextResourceDecorator (const char *name,
                            dw::core::ui::TextResource *resource);
      const char *getValue ();
   };

   /**
    * \brief Decorates instances of dw::core::ui::RadioButtonResource.
    *
    * This class has to be instanciated only once for a group of radio
    * buttons.
    */
   class RadioButtonResourceDecorator: public ResourceDecorator
   {
   private:
      dw::core::ui::RadioButtonResource *resource;
      const char **values;

   public:
      RadioButtonResourceDecorator (const char *name,
                                   dw::core::ui::RadioButtonResource
                                   *resource,
                                   const char **values);
      ~RadioButtonResourceDecorator ();
      const char *getValue ();
   };

   lout::container::typed::List <ResourceDecorator> *resources;

   void *ext_data;  // external data pointer

public:
   Form (void *p);
   ~Form ();
   void clicked (dw::core::ui::ButtonResource *resource, int buttonNo,
                 int x, int y);
   void activate (dw::core::ui::Resource *resource);

};

} // namespace form

#endif // __FORM_HH__
