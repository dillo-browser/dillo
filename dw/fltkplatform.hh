#ifndef __DW_FLTKPLATFORM_HH__
#define __DW_FLTKPLATFORM_HH__

#ifndef __INCLUDED_FROM_DW_FLTK_CORE_HH__
#   error Do not include this file directly, use "fltkcore.hh" instead.
#endif

namespace dw {

/**
 * \brief This namespace contains FLTK implementations of Dw interfaces.
 */
namespace fltk {

class FltkFont: public core::style::Font
{
   class FontFamily: public lout::object::Object {
         Fl_Font font[4];
      public:
         FontFamily (Fl_Font fontNormal, Fl_Font fontBold,
                     Fl_Font fontItalic, Fl_Font fontBoldItalic);
         void set (Fl_Font, int attrs);
         Fl_Font get (int attrs);
   };

   static FontFamily standardFontFamily;

   static lout::container::typed::HashTable <lout::object::ConstString,
                                             FontFamily> *systemFonts;
   static lout::container::typed::HashTable <dw::core::style::FontAttrs,
                                       FltkFont> *fontsTable;

   FltkFont (core::style::FontAttrs *attrs);
   ~FltkFont ();

   static void initSystemFonts ();

public:
   Fl_Font font;

   static FltkFont *create (core::style::FontAttrs *attrs);
   static bool fontExists (const char *name);
   static Fl_Font get (const char *name, int attrs);
};


class FltkColor: public core::style::Color
{
   static lout::container::typed::HashTable <dw::core::style::ColorAttrs,
                                       FltkColor> *colorsTable;

   FltkColor (int color);
   ~FltkColor ();

public:
   int colors[SHADING_NUM];

   static FltkColor *create(int color);
};

class FltkTooltip: public core::style::Tooltip
{
private:
   FltkTooltip (const char *text);
   ~FltkTooltip ();
public:
   static FltkTooltip *create(const char *text);
   static void cancel();
   void onEnter();
   void onLeave();
   void onMotion();
};


/**
 * \brief This interface adds some more methods for all flkt-based views.
 */
class FltkView: public core::View
{
public:
   virtual bool usesFltkWidgets () = 0;

   virtual void addFltkWidget (Fl_Widget *widget,
                               core::Allocation *allocation);
   virtual void removeFltkWidget (Fl_Widget *widget);
   virtual void allocateFltkWidget (Fl_Widget *widget,
                                    core::Allocation *allocation);
   virtual void drawFltkWidget (Fl_Widget *widget, core::Rectangle *area);
};


class FltkPlatform: public core::Platform
{
private:
   class FltkResourceFactory: public core::ui::ResourceFactory
   {
   private:
      FltkPlatform *platform;

   public:
      inline void setPlatform (FltkPlatform *platform) {
         this->platform = platform; }

      core::ui::LabelButtonResource *createLabelButtonResource (const char
                                                                *label);
      core::ui::ComplexButtonResource *
      createComplexButtonResource (core::Widget *widget, bool relief);
      core::ui::ListResource *
      createListResource (core::ui::ListResource::SelectionMode selectionMode,
                          int rows);
      core::ui::OptionMenuResource *createOptionMenuResource ();
      core::ui::EntryResource *createEntryResource (int size, bool password,
                                                    const char *label,
                                                    const char *placeholder);
      core::ui::MultiLineTextResource *createMultiLineTextResource (int cols,
                                                                    int rows,
                                                      const char *placeholder);
      core::ui::CheckButtonResource *createCheckButtonResource (bool
                                                                activated);
      core::ui::RadioButtonResource *
      createRadioButtonResource (core::ui::RadioButtonResource
                                  *groupedWith, bool activated);
   };

   FltkResourceFactory resourceFactory;

   class IdleFunc: public lout::object::Object
   {
   public:
      int id;
      void (core::Layout::*func) ();
   };

   core::Layout *layout;

   lout::container::typed::List <IdleFunc> *idleQueue;
   bool idleFuncRunning;
   int idleFuncId;

   static void generalStaticIdle(void *data);
   void generalIdle();

   FltkView *view;
   lout::container::typed::List <ui::FltkResource> *resources;

public:
   FltkPlatform ();
   ~FltkPlatform ();

   void setLayout (core::Layout *layout);

   void attachView (core::View *view);

   void detachView  (core::View *view);

   int textWidth (core::style::Font *font, const char *text, int len);
   char *textToUpper (const char *text, int len);
   char *textToLower (const char *text, int len);
   int nextGlyph (const char *text, int idx);
   int prevGlyph (const char *text, int idx);
   float dpiX ();
   float dpiY ();

   int addIdle (void (core::Layout::*func) ());
   void removeIdle (int idleId);

   core::style::Font *createFont (core::style::FontAttrs *attrs,
                                      bool tryEverything);
   bool fontExists (const char *name);
   core::style::Color *createColor (int color);
   core::style::Tooltip *createTooltip (const char *text);
   void cancelTooltip();

   core::Imgbuf *createImgbuf (core::Imgbuf::Type type, int width, int height,
                               double gamma);

   void copySelection(const char *text);

   core::ui::ResourceFactory *getResourceFactory ();

   void attachResource (ui::FltkResource *resource);
   void detachResource (ui::FltkResource *resource);
};

} // namespace fltk
} // namespace dw

#endif // __DW_FLTKPLATFORM_HH__
