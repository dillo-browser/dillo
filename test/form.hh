#ifndef __TEST_FORM_HH__
#define __TEST_FORM_HH__

#include "../dw/core.hh"
#include "../dw/ui.hh"

namespace form {

/**
 * \brief Handles HTML form data.
 *
 * Add resources by calling the respective add...Resource method. Furtermore,
 * this class impelements dw::core::ui::ButtonResource::ClickedReceiver, the
 * form data is printed to stdout, when the "clicked" signal is received.
 *
 * \todo wrong comment
 */
class Form
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
    * This class has to be instantiated only once for a group of radio
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

   /**
    * \brief Decorates instances of dw::core::ui::CheckButtonResource.
    */
   class CheckButtonResourceDecorator: public ResourceDecorator
   {
   private:
      dw::core::ui::CheckButtonResource *resource;

   public:
      CheckButtonResourceDecorator (const char *name,
                                    dw::core::ui::CheckButtonResource
                                    *resource);
      const char *getValue ();
   };

   /**
    * \brief Decorates instances of dw::core::ui::SelectionResource.
    */
   class SelectionResourceDecorator: public ResourceDecorator
   {
   private:
      dw::core::ui::SelectionResource *resource;
      const char **values;
      lout::misc::StringBuffer valueBuf;

   public:
      SelectionResourceDecorator (const char *name,
                                  dw::core::ui::SelectionResource *resource,
                                  const char **values);
      ~SelectionResourceDecorator ();
      const char *getValue ();
   };

   class FormActivateReceiver: public dw::core::ui::Resource::ActivateReceiver
   {
   private:
      Form *form;

   public:
      inline FormActivateReceiver (Form *form) { this->form = form; }

      void activate (dw::core::ui::Resource *resource);
      void enter (dw::core::ui::Resource *resource);
      void leave (dw::core::ui::Resource *resource);
   };

   class FormClickedReceiver:
      public dw::core::ui::Resource::ClickedReceiver
   {
   private:
      Form *form;
      const char *name, *value;

   public:
      FormClickedReceiver (Form *form, const char *name, const char *value);
      ~FormClickedReceiver ();

      void clicked(dw::core::ui::Resource *resource,
                   dw::core::EventButton *event);
   };

   lout::container::typed::List <ResourceDecorator> *resources;
   FormActivateReceiver *activateReceiver;
   lout::container::typed::List <FormClickedReceiver> *clickedReceivers;

public:
   Form ();
   ~Form ();

   void addTextResource (const char *name,
                         dw::core::ui::TextResource *resource);
   void addRadioButtonResource (const char *name,
                                dw::core::ui::RadioButtonResource *resource,
                                const char **values);
   void addCheckButtonResource (const char *name,
                                dw::core::ui::CheckButtonResource *resource);
   void addSelectionResource (const char *name,
                              dw::core::ui::SelectionResource *resource,
                              const char **values);
   void addButtonResource (const char *name,
                           dw::core::ui::ButtonResource *resource,
                           const char *value);

   void send (const char *buttonName, const char *buttonValue, int x, int y);
};

} // namespace form

#endif // __TEST_FORM_HH__
