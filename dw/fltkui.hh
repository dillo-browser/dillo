#ifndef __DW_FLTK_UI_HH__
#define __DW_FLTK_UI_HH__

#ifndef __INCLUDED_FROM_DW_FLTK_CORE_HH__
#   error Do not include this file directly, use "fltkcore.hh" instead.
#endif

#include <FL/Fl_Button.H>
#include <FL/Fl_Menu.H>
#include <FL/Fl_Text_Buffer.H>

namespace dw {
namespace fltk {

/**
 * \brief FLTK implementation of dw::core::ui.
 *
 * <div style="border: 2px solid #ff0000; margin-top: 0.5em;
 * margin-bottom: 0.5em; padding: 0.5em 1em;
 * background-color: #ffefe0"><b>Update:</b> The complicated design
 * results from my insufficient knowledge of C++ some years ago; since
 * then, I've learned how to deal with "diamond inheritance", as the
 * (ideal, not actually implemented) design in the first diagram
 * shows. It should be possible to implement this ideal design in a
 * straightforward way, and so get rid of templates. --SG</div>
 *
 * The design should be like this:
 *
 * \dot
 * digraph G {
 *    node [shape=record, fontname=Helvetica, fontsize=10];
 *    edge [arrowhead="none", arrowtail="empty", dir="both",
 *          labelfontname=Helvetica, labelfontsize=10, color="#404040",
 *          labelfontcolor="#000080"];
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
 * To solve this, we have to remove the dependency between
 * dw::fltk::ui::FltkResource and dw::core::ui::Resource, instead, the part
 * of dw::core::ui::Resource, which is implemented in
 * dw::fltk::ui::FltkResource, must be explicitly delegated from
 * dw::fltk::ui::FltkLabelButtonResourceto dw::fltk::ui::FltkResource:
 *
 * \dot
 * digraph G {
 *    node [shape=record, fontname=Helvetica, fontsize=10];
 *    edge [arrowhead="none", arrowtail="empty", dir="both",
 *          labelfontname=Helvetica, labelfontsize=10, color="#404040",
 *          labelfontcolor="#000080"];
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
 *    edge [arrowhead="none", arrowtail="empty", dir="both",
 *          labelfontname=Helvetica, labelfontsize=10, color="#404040",
 *          labelfontcolor="#000080"];
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
 *                                                         dir="both",
 *                                                         style="dashed",
 *                                                         color="#808000"];
 *    FltkSpecificResource -> FltkSpecificResource_entry [arrowhead="open",
 *                                                        arrowtail="none",
 *                                                        dir="both",
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
class FltkResource: public lout::object::Object
{
private:
   bool enabled;

protected:
   FltkView *view;
   Fl_Widget *widget;
   core::Allocation allocation;
   FltkPlatform *platform;

   core::style::Style *style;

   FltkResource (FltkPlatform *platform);
   void init (FltkPlatform *platform);
   virtual Fl_Widget *createNewWidget (core::Allocation *allocation) = 0;

   virtual void setWidgetStyle (Fl_Widget *widget, core::style::Style *style);
   void setDisplayed (bool displayed);
   bool displayed();
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

   static void widgetCallback (Fl_Widget *widget, void *data);

protected:
   Fl_Widget *createNewWidget (core::Allocation *allocation);

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

   static void widgetCallback (Fl_Widget *widget, void *data);

protected:
   FltkView *topView, *flatView;

   void attachView (FltkView *view);
   void detachView (FltkView *view);

   void sizeAllocate (core::Allocation *allocation);

   dw::core::Platform *createPlatform ();
   void setLayout (dw::core::Layout *layout);

   int reliefXThickness ();
   int reliefYThickness ();

   Fl_Widget *createNewWidget (core::Allocation *allocation);

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
   int size;
   bool password;
   const char *initText;
   char *label;
   int label_w;
   char *placeholder;
   bool editable;

   static void widgetCallback (Fl_Widget *widget, void *data);
   void setDisplayed (bool displayed);

protected:
   Fl_Widget *createNewWidget (core::Allocation *allocation);
   void setWidgetStyle (Fl_Widget *widget, core::style::Style *style);

public:
   FltkEntryResource (FltkPlatform *platform, int size, bool password,
                      const char *label, const char *placeholder);
   ~FltkEntryResource ();

   void sizeRequest (core::Requisition *requisition);
   void sizeAllocate (core::Allocation *allocation);

   const char *getText ();
   void setText (const char *text);
   bool isEditable ();
   void setEditable (bool editable);
   void setMaxLength (int maxlen);
};


class FltkMultiLineTextResource:
   public FltkSpecificResource <dw::core::ui::MultiLineTextResource>
{
private:
   bool editable;
   int numCols, numRows;
   char *placeholder;
protected:
   Fl_Widget *createNewWidget (core::Allocation *allocation);
   void setWidgetStyle (Fl_Widget *widget, core::style::Style *style);

public:
   FltkMultiLineTextResource (FltkPlatform *platform, int cols, int rows,
                              const char *placeholder);
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
   virtual Fl_Button *createNewButton (core::Allocation *allocation) = 0;
   Fl_Widget *createNewWidget (core::Allocation *allocation);
   void setWidgetStyle (Fl_Widget *widget, core::style::Style *style);

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
protected:
   Fl_Button *createNewButton (core::Allocation *allocation);

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
         lout::container::typed::Iterator <FltkRadioButtonResource> it;

      public:
         inline FltkGroupIterator (lout::container::typed::List
                                   <FltkRadioButtonResource>
                                   *list)
            { it = list->iterator (); }

         bool hasNext ();
         dw::core::ui::RadioButtonResource *getNext ();
         void unref ();
      };

      lout::container::typed::List <FltkRadioButtonResource> *list;

   protected:
      ~Group ();

   public:
      Group (FltkRadioButtonResource *radioButtonResource);

      inline lout::container::typed::Iterator <FltkRadioButtonResource>
                                              iterator ()
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

   static void widgetCallback (Fl_Widget *widget, void *data);
   void buttonClicked ();

protected:
   Fl_Button *createNewButton (core::Allocation *allocation);

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
   virtual bool setSelectedItems() { return false; }
   virtual void addItem (const char *str, bool enabled, bool selected) = 0;
   virtual void setItem (int index, bool selected) = 0;
   virtual void pushGroup (const char *name, bool enabled) = 0;
   virtual void popGroup () = 0;
public:
   FltkSelectionResource (FltkPlatform *platform) :
      FltkSpecificResource<I> (platform) {};
   dw::core::Iterator *iterator (dw::core::Content::Type mask, bool atEnd);
};


class FltkOptionMenuResource:
   public FltkSelectionResource <dw::core::ui::OptionMenuResource>
{
protected:
   Fl_Widget *createNewWidget (core::Allocation *allocation);
   virtual bool setSelectedItems() { return true; }
   void setWidgetStyle (Fl_Widget *widget, core::style::Style *style);
   int getNumberOfItems();
   int getMaxItemWidth ();
private:
   static void widgetCallback (Fl_Widget *widget, void *data);
   void enlargeMenu();
   Fl_Menu_Item *newItem();
   Fl_Menu_Item *menu;
   int itemsAllocated, itemsUsed;
public:
   FltkOptionMenuResource (FltkPlatform *platform);
   ~FltkOptionMenuResource ();

   void addItem (const char *str, bool enabled, bool selected);
   void setItem (int index, bool selected);
   void pushGroup (const char *name, bool enabled);
   void popGroup ();

   void sizeRequest (core::Requisition *requisition);
   bool isSelected (int index);
};

class FltkListResource:
   public FltkSelectionResource <dw::core::ui::ListResource>
{
protected:
   Fl_Widget *createNewWidget (core::Allocation *allocation);
   void setWidgetStyle (Fl_Widget *widget, core::style::Style *style);
   int getNumberOfItems();
   int getMaxItemWidth ();
private:
   static void widgetCallback (Fl_Widget *widget, void *data);
   void *newItem (const char *str, bool enabled, bool selected);
   int currDepth;
   int colWidths[4];
   int showRows;
   ListResource::SelectionMode mode;
public:
   FltkListResource (FltkPlatform *platform,
                     core::ui::ListResource::SelectionMode selectionMode,
                     int rows);
   ~FltkListResource ();

   void addItem (const char *str, bool enabled, bool selected);
   void setItem (int index, bool selected);
   void pushGroup (const char *name, bool enabled);
   void popGroup ();

   void sizeRequest (core::Requisition *requisition);
   bool isSelected (int index);
};


} // namespace ui
} // namespace fltk
} // namespace dw


#endif // __DW_FLTK_UI_HH__
