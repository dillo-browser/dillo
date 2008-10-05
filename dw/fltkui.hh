#ifndef __DW_FLTK_UI_HH__
#define __DW_FLTK_UI_HH__

#ifndef __INCLUDED_FROM_DW_FLTK_CORE_HH__
#   error Do not include this file directly, use "fltkcore.hh" instead.
#endif

#include <fltk/Button.h>
#include <fltk/Menu.h>
#include <fltk/TextBuffer.h>
#include <fltk/Item.h>
#include <fltk/ItemGroup.h>

namespace dw {
namespace fltk {

using namespace lout;

/**
 * \brief FLTK implementation of dw::core::ui.
 *
 * The design should be like this:
 *
 * \dot
 * digraph G {
 *    node [shape=record, fontname=Helvetica, fontsize=10];
 *    edge [arrowhead="none", arrowtail="empty", labelfontname=Helvetica,
 *          labelfontsize=10, color="#404040", labelfontcolor="#000080"];
 *    fontname=Helvetica; fontsize=10;
 * 
 *    subgraph cluster_core {
 *       style="dashed"; color="#000080"; fontname=Helvetica; fontsize=10;
 *       label="dw::core::ui";
 *
 *       Resource [color="#a0a0a0", URL="\ref dw::core::ui::Resource"];
 *       LabelButtonResource [color="#a0a0a0",
 *                            URL="\ref dw::core::ui::LabelButtonResource"];
 *       EntryResource [color="#a0a0a0",
 *                      URL="\ref dw::core::ui::EntryResource"];
 *    }
 *
 *    subgraph cluster_fltk {
 *       style="dashed"; color="#000080"; fontname=Helvetica; fontsize=10;
 *       label="dw::fltk::ui";
 *
 *       FltkResource [color="#a0a0a0", URL="\ref dw::fltk::ui::FltkResource"];
 *       FltkLabelButtonResource
 *          [URL="\ref dw::fltk::ui::FltkLabelButtonResource"];
 *       FltkEntryResource [URL="\ref dw::fltk::ui::FltkEntryResource"];
 *    }
 *
 *    Resource -> LabelButtonResource;
 *    Resource -> EntryResource;
 *    FltkResource -> FltkLabelButtonResource;
 *    FltkResource -> FltkEntryResource;
 *    Resource -> FltkResource;
 *    LabelButtonResource -> FltkLabelButtonResource;
 *    EntryResource -> FltkEntryResource;
 * }
 * \enddot
 *
 * <center>[\ref uml-legend "legend"]</center>
 *
 * where dw::fltk::ui::FltkResource provides some base funtionality for all
 * conctrete FLTK implementations of sub-interfaces of dw::core::ui::Resource.
 * However, this is not directly possible in C++, since the base class
 * dw::core::ui::Resource is ambiguous for
 * dw::fltk::ui::FltkLabelButtonResource.
 *
 * To solve this, we have to remove the depencency between
 * dw::fltk::ui::FltkResource and dw::core::ui::Resource, instead, the part
 * of dw::core::ui::Resource, which is implemented in
 * dw::fltk::ui::FltkResource, must be explicitely delegated from
 * dw::fltk::ui::FltkLabelButtonResourceto dw::fltk::ui::FltkResource:
 *
 * \dot
 * digraph G {
 *    node [shape=record, fontname=Helvetica, fontsize=10];
 *    edge [arrowhead="none", arrowtail="empty", labelfontname=Helvetica,
 *          labelfontsize=10, color="#404040", labelfontcolor="#000080"];
 *    fontname=Helvetica; fontsize=10;
 * 
 *    subgraph cluster_core {
 *       style="dashed"; color="#000080"; fontname=Helvetica; fontsize=10;
 *       label="dw::core::ui";
 *
 *       Resource [color="#a0a0a0", URL="\ref dw::core::ui::Resource"];
 *       LabelButtonResource [color="#a0a0a0",
 *                           URL="\ref dw::core::ui::LabelButtonResource"];
 *       EntryResource [color="#a0a0a0",
 *                      URL="\ref dw::core::ui::EntryResource"];
 *    }
 *
 *    subgraph cluster_fltk {
 *       style="dashed"; color="#000080"; fontname=Helvetica; fontsize=10;
 *       label="dw::fltk::ui";
 *
 *       FltkResource [color="#a0a0a0", URL="\ref dw::fltk::ui::FltkResource"];
 *       FltkLabelButtonResource
 *          [URL="\ref dw::fltk::ui::FltkLabelButtonResource"];
 *       FltkEntryResource [URL="\ref dw::fltk::ui::FltkEntryResource"];
 *    }
 *
 *    Resource -> LabelButtonResource;
 *    Resource -> EntryResource;
 *    FltkResource -> FltkLabelButtonResource;
 *    FltkResource -> FltkEntryResource;
 *    LabelButtonResource -> FltkLabelButtonResource;
 *    EntryResource -> FltkEntryResource;
 * }
 * \enddot
 *
 * <center>[\ref uml-legend "legend"]</center>
 *
 * To make this a bit simpler, we use templates:
 *
 * \dot
 * digraph G {
 *    node [shape=record, fontname=Helvetica, fontsize=10];
 *    edge [arrowhead="none", arrowtail="empty", labelfontname=Helvetica,
 *          labelfontsize=10, color="#404040", labelfontcolor="#000080"];
 *    fontname=Helvetica; fontsize=10;
 * 
 *    subgraph cluster_core {
 *       style="dashed"; color="#000080"; fontname=Helvetica; fontsize=10;
 *       label="dw::core::ui";
 *
 *       Resource [color="#a0a0a0", URL="\ref dw::core::ui::Resource"];
 *       LabelButtonResource [color="#a0a0a0",
 *                            URL="\ref dw::core::ui::LabelButtonResource"];
 *       EntryResource [color="#a0a0a0",
 *                      URL="\ref dw::core::ui::EntryResource"];
 *    }
 *
 *    subgraph cluster_fltk {
 *       style="dashed"; color="#000080"; fontname=Helvetica; fontsize=10;
 *       label="dw::fltk::ui";
 *
 *       FltkResource [color="#a0a0a0", URL="\ref dw::fltk::ui::FltkResource"];
 *       FltkSpecificResource [color="#a0a0a0",
 *                             fillcolor="#ffffc0", style="filled"
 *                             URL="\ref dw::fltk::ui::FltkSpecificResource"];
 *       FltkSpecificResource_button [color="#a0a0a0",
 *                       label="FltkSpecificResource \<LabelButtonResource\>"];
 *       FltkSpecificResource_entry [color="#a0a0a0",
 *                             label="FltkSpecificResource \<EntryResource\>"];
 *       FltkEntryResource [URL="\ref dw::fltk::ui::FltkEntryResource"];
 *       FltkLabelButtonResource
 *          [URL="\ref dw::fltk::ui::FltkLabelButtonResource"];
 *    }
 *
 *    Resource -> LabelButtonResource;
 *    Resource -> EntryResource;
 *    FltkResource -> FltkSpecificResource;
 *    FltkSpecificResource -> FltkSpecificResource_button [arrowhead="open",
 *                                                         arrowtail="none",
 *                                                         style="dashed",
 *                                                         color="#808000"];
 *    FltkSpecificResource -> FltkSpecificResource_entry [arrowhead="open",
 *                                                        arrowtail="none",
 *                                                        style="dashed",
 *                                                        color="#808000"];
 *    LabelButtonResource -> FltkSpecificResource_button;
 *    EntryResource -> FltkSpecificResource_entry;
 *    FltkSpecificResource_button -> FltkLabelButtonResource;
 *    FltkSpecificResource_entry -> FltkEntryResource;
 * }
 * \enddot
 *
 * <center>[\ref uml-legend "legend"]</center>
 */
namespace ui {

/**
 * ...
 */
class FltkResource: public object::Object
{
protected:
   class ViewAndWidget: public object::Object
   {
   public:
      FltkView *view;
      ::fltk::Widget *widget;
   };

   container::typed::List <ViewAndWidget> *viewsAndWidgets;
   core::Allocation allocation;
   FltkPlatform *platform;

   core::style::Style *style;

   FltkResource (FltkPlatform *platform);
   void init (FltkPlatform *platform);
   virtual ::fltk::Widget *createNewWidget (core::Allocation *allocation) = 0;

   void setWidgetStyle (::fltk::Widget *widget, core::style::Style *style);
public:
   ~FltkResource ();

   virtual void attachView (FltkView *view);
   virtual void detachView (FltkView *view);
   
   void sizeAllocate (core::Allocation *allocation);
   void draw (core::View *view, core::Rectangle *area);

   void setStyle (core::style::Style *style);

   bool isEnabled ();
   void setEnabled (bool enabled);
};


template <class I> class FltkSpecificResource: public I, public FltkResource
{
public:
   inline FltkSpecificResource (FltkPlatform *platform) :
      FltkResource (platform) { }

   void sizeAllocate (core::Allocation *allocation);
   void draw (core::View *view, core::Rectangle *area);
   void setStyle (core::style::Style *style);

   bool isEnabled ();
   void setEnabled (bool enabled);
};


class FltkLabelButtonResource:
   public FltkSpecificResource <dw::core::ui::LabelButtonResource>
{
private:
   const char *label;

   static void widgetCallback (::fltk::Widget *widget, void *data);

protected:
   ::fltk::Widget *createNewWidget (core::Allocation *allocation);

public:
   FltkLabelButtonResource (FltkPlatform *platform, const char *label);
   ~FltkLabelButtonResource ();

   void sizeRequest (core::Requisition *requisition);

   const char *getLabel ();
   void setLabel (const char *label);
};


class FltkComplexButtonResource:
   public FltkSpecificResource <dw::core::ui::ComplexButtonResource>
{
private:
   bool relief;

   static void widgetCallback (::fltk::Widget *widget, void *data);

protected:
   class ViewAndView: public object::Object
   {
   public:
      FltkView *topView, *flatView;
   };

   FltkView *lastFlatView;

   container::typed::List <ViewAndView> *viewsAndViews;

   void attachView (FltkView *view);
   void detachView (FltkView *view);

   void sizeAllocate (core::Allocation *allocation);

   dw::core::Platform *createPlatform ();
   void setLayout (dw::core::Layout *layout);
   
   int reliefXThickness ();
   int reliefYThickness ();

   ::fltk::Widget *createNewWidget (core::Allocation *allocation);

public:
   FltkComplexButtonResource (FltkPlatform *platform, dw::core::Widget *widget,
                              bool relief);
   ~FltkComplexButtonResource ();
};


/**
 * \bug Maximal length not supported yet.
 * \todo Text values are not synchronized (not needed in dillo).
 */
class FltkEntryResource:
   public FltkSpecificResource <dw::core::ui::EntryResource>
{
private:
   int maxLength;
   bool password;
   const char *initText;
   bool editable;

   static void widgetCallback (::fltk::Widget *widget, void *data);

protected:
   ::fltk::Widget *createNewWidget (core::Allocation *allocation);

public:
   FltkEntryResource (FltkPlatform *platform, int maxLength, bool password);
   ~FltkEntryResource ();

   void sizeRequest (core::Requisition *requisition);

   const char *getText ();
   void setText (const char *text);
   bool isEditable ();
   void setEditable (bool editable);
};


class FltkMultiLineTextResource:
   public FltkSpecificResource <dw::core::ui::MultiLineTextResource>
{
private:
   ::fltk::TextBuffer *buffer;
   bool editable;
   int numCols, numRows;

protected:
   ::fltk::Widget *createNewWidget (core::Allocation *allocation);

public:
   FltkMultiLineTextResource (FltkPlatform *platform, int cols, int rows);
   ~FltkMultiLineTextResource ();

   void sizeRequest (core::Requisition *requisition);

   const char *getText ();
   void setText (const char *text);
   bool isEditable ();
   void setEditable (bool editable);
};


template <class I> class FltkToggleButtonResource:
   public FltkSpecificResource <I>
{
private:
   bool initActivated;

protected:
   virtual ::fltk::Button *createNewButton (core::Allocation *allocation) = 0;
   // TODO: maybe getFont() is not the best way to do it...
   virtual FltkFont *getFont() = 0;
   ::fltk::Widget *createNewWidget (core::Allocation *allocation);
   
public:
   FltkToggleButtonResource (FltkPlatform *platform,
                             bool activated);
   ~FltkToggleButtonResource ();

   void sizeRequest (core::Requisition *requisition);

   bool isActivated ();
   void setActivated (bool activated);
};


class FltkCheckButtonResource:
   public FltkToggleButtonResource <dw::core::ui::CheckButtonResource>
{
private:
   FltkFont *getFont() {return style ? (FltkFont *)style->font : NULL;}
protected:
   ::fltk::Button *createNewButton (core::Allocation *allocation);

public:
   FltkCheckButtonResource (FltkPlatform *platform,
                            bool activated);
   ~FltkCheckButtonResource ();
};


class FltkRadioButtonResource:
   public FltkToggleButtonResource <dw::core::ui::RadioButtonResource>
{
private:
   class Group
   {
   private:
      class FltkGroupIterator:
         public dw::core::ui::RadioButtonResource::GroupIterator
      {
      private:
         container::typed::Iterator <FltkRadioButtonResource> it;

      public:
         inline FltkGroupIterator (container::typed::List
                                   <FltkRadioButtonResource>
                                   *list)
            { it = list->iterator (); }

         bool hasNext ();
         dw::core::ui::RadioButtonResource *getNext ();
         void unref ();
      };

      container::typed::List <FltkRadioButtonResource> *list;

   protected:
      ~Group ();

   public:
      Group (FltkRadioButtonResource *radioButtonResource);
      
      inline container::typed::Iterator <FltkRadioButtonResource> iterator ()
      {
         return list->iterator ();
      }

      inline dw::core::ui::RadioButtonResource::GroupIterator
         *groupIterator ()
      {
         return new FltkGroupIterator (list);
      }

      void connect (FltkRadioButtonResource *radioButtonResource);
      void unconnect (FltkRadioButtonResource *radioButtonResource);
   };

   Group *group;

   static void widgetCallback (::fltk::Widget *widget, void *data);
   FltkFont *getFont() {return style ? (FltkFont *)style->font : NULL;}
   void buttonClicked ();

protected:
   ::fltk::Button *createNewButton (core::Allocation *allocation);

public:
   FltkRadioButtonResource (FltkPlatform *platform,
                            FltkRadioButtonResource *groupedWith,
                            bool activated);
   ~FltkRadioButtonResource ();

   GroupIterator *groupIterator ();
};


template <class I> class FltkSelectionResource:
   public FltkSpecificResource <I>
{
protected:
   class Item: public object::Object
   {
   public:
      enum Type { ITEM, START, END } type;

      const char *name;
      bool enabled, initSelected;

      Item (Type type, const char *name = NULL, bool enabled = true,
            bool selected = false);
      ~Item ();

      ::fltk::Item *createNewWidget (int index);
      ::fltk::ItemGroup *createNewGroupWidget ();
   };

   class WidgetStack: public object::Object
   {
   public:
      ::fltk::Menu *widget;
      container::typed::Stack <object::TypedPointer < ::fltk::Menu> > *stack;

      WidgetStack (::fltk::Menu *widget);
      ~WidgetStack ();
   };

   container::typed::List <WidgetStack> *widgetStacks;
   container::typed::List <Item> *allItems;
   container::typed::Vector <Item> *items;

   Item *createNewItem (typename Item::Type type,
                        const char *name = NULL,
                        bool enabled = true,
                        bool selected = false);

   ::fltk::Widget *createNewWidget (core::Allocation *allocation);
   virtual ::fltk::Menu *createNewMenu (core::Allocation *allocation) = 0;
   virtual bool setSelectedItems() { return false; }

   int getMaxStringWidth ();

public:
   FltkSelectionResource (FltkPlatform *platform);
   ~FltkSelectionResource ();

   dw::core::Iterator *iterator (dw::core::Content::Type mask, bool atEnd); 

   void addItem (const char *str, bool enabled, bool selected);
   
   void pushGroup (const char *name, bool enabled);
   void popGroup ();

   int getNumberOfItems ();
   const char *getItem (int index);
};


class FltkOptionMenuResource:
   public FltkSelectionResource <dw::core::ui::OptionMenuResource>
{
protected:
   ::fltk::Menu *createNewMenu (core::Allocation *allocation);
   virtual bool setSelectedItems() { return true; }

private:
   static void widgetCallback (::fltk::Widget *widget, void *data);
   int selection;

public:
   FltkOptionMenuResource (FltkPlatform *platform);
   ~FltkOptionMenuResource ();

   void addItem (const char *str, bool enabled, bool selected);

   void sizeRequest (core::Requisition *requisition);
   bool isSelected (int index);
};

class FltkListResource:
   public FltkSelectionResource <dw::core::ui::ListResource>
{
protected:
   ::fltk::Menu *createNewMenu (core::Allocation *allocation);

private:
   static void widgetCallback (::fltk::Widget *widget, void *data);
   misc::SimpleVector <bool> itemsSelected;

public:
   FltkListResource (FltkPlatform *platform,
                     core::ui::ListResource::SelectionMode selectionMode);
   ~FltkListResource ();

   void addItem (const char *str, bool enabled, bool selected);

   void sizeRequest (core::Requisition *requisition);
   bool isSelected (int index);
};


} // namespace ui
} // namespace fltk
} // namespace dw


#endif // __DW_FLTK_UI_HH__
