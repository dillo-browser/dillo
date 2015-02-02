#ifndef __DW_UI_HH__
#define __DW_UI_HH__

#ifndef __INCLUDED_FROM_DW_CORE_HH__
#   error Do not include this file directly, use "core.hh" instead.
#endif

namespace dw {
namespace core {

/**
 * \brief Anything related to embedded UI widgets is defined here.
 *
 * UI resources are another abstraction for Dw widgets, which are not
 * fully implemented in a platform-independent way. Typically, they
 * involve creating widgets, which the underlying UI toolkit provides.
 *
 * As you see in this diagram:
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
 *       label="dw::core";
 *
 *       subgraph cluster_ui {
 *          style="dashed"; color="#000080"; fontname=Helvetica; fontsize=10;
 *          label="dw::core::ui";
 *
 *          Embed [URL="\ref dw::core::ui::Embed"];
 *          Resource [color="#a0a0a0", URL="\ref dw::core::ui::Resource"];
 *          LabelButtonResource [color="#a0a0a0",
 *                              URL="\ref dw::core::ui::LabelButtonResource"];
 *          EntryResource [color="#a0a0a0",
 *                        URL="\ref dw::core::ui::EntryResource"];
 *          etc [color="#a0a0a0", label="..."];
 *       }
 *
 *       Widget [URL="\ref dw::core::Widget", color="#a0a0a0"];
 *    }
 *
 *    subgraph cluster_fltk {
 *       style="dashed"; color="#000080"; fontname=Helvetica; fontsize=10;
 *       label="dw::fltk::ui";
 *
 *       FltkLabelButtonResource
 *          [URL="\ref dw::fltk::ui::FltkLabelButtonResource"];
 *       FltkEntryResource [URL="\ref dw::fltk::ui::FltkEntryResource"];
 *    }
 *
 *    Widget -> Embed;
 *    Embed -> Resource [arrowhead="open", arrowtail="none",
 *                       headlabel="1", taillabel="1"];
 *    Resource -> LabelButtonResource;
 *    Resource -> EntryResource;
 *    Resource -> etc;
 *    LabelButtonResource -> FltkLabelButtonResource;
 *    EntryResource -> FltkEntryResource;
 * }
 * \enddot
 *
 * <center>[\ref uml-legend "legend"]</center>
 *
 * there are several levels:
 *
 * <ol>
 * <li> The Dw widget is dw::core::ui::Embed. It delegates most to
 *      dw::core::ui::Resource, which has similar methods like
 *      dw::core::Widget.
 *
 * <li> There are several sub interfaces of dw::core::ui::Resource, which
 *      may provide methods, as e.g. dw::core::ui::ListResource::addItem. In a
 *      platform independent context, you can cast the result of
 *      dw::core::ui::Embed::getResource to a specific sub class, if you
 *      know, which one is used. E.g., if you know, that a given instance
 *      dw::core::ui::Embed refers to a dw::core::ui::ListResource, you can
 *      write something like:
 *
 * \code
 * dw::core::ui::Embed *embed;
 * //...
 * ((dw::core::ui::ListResource*)embed->getResource ())->addItem ("Hello!");
 * \endcode
 *
 * <li> These sub classes are then fully implemented in a platform specific
 *      way. For an example, look at dw::fltk::ui.
 * </ol>
 *
 * There is a factory interface, dw::core::ui::ResourceFactory, which
 * provides methods for creating common resources. By calling
 * dw::core::Layout::getResourceFactory, which calls
 * dw::core::Platform::getResourceFactory, you get the factory for the used
 * platform.
 *
 * It is possible to define additional sub classes of
 * dw::core::ui::Resource, but since they are not provided by
 * dw::core::ui::ResourceFactory, you have to define some other
 * abstractions, if you want to remain platform independent.
 *
 *
 * <h3>...</h3>
 *
 *
 * <h3>Resouces needed for HTML</h3>
 *
 * This chapter describes, how the form controls defined by HTML are
 * implemented in Dw. Some of them do not refer to UI resources, but to
 * other widgets, links to the respective documentations are provided
 * here.
 *
 * <h4>Resouces created with \<INPUT\></h4>
 *
 * The HTML \<INPUT\> is always implemented by using UI
 * resources. \<INPUT\> element has the following attributes:
 *
 * <table>
 * <tr><th>Attribute <th>Implementation
 * <tr><td>type      <td>This defines the resource you have to instantiate.
 * <tr><td>name      <td>Not needed within Dw.
 * <tr><td>value     <td>The initial value is treated differently by different
 *                       resources.
 * <tr><td>checked   <td>Parameter to
 *                     dw::core::ui::ResourceFactory::createCheckButtonResource
 *                and dw::core::ui::ResourceFactory::createRadioButtonResource.
 * <tr><td>disabled  <td>This is provided for all resources by
 *                       dw::core::ui::Resource::setEnabled.
 * <tr><td>readonly  <td>This is provided by
 *                       dw::core::ui::TextResource::setEditable.
 * <tr><td>size      <td>This is handled by styles.
 * <tr><td>maxlength <td>Parameter of
 *                       dw::core::ui::ResourceFactory::createEntryResource.
 * <tr><td>src       <td>Handled by the caller (HTML parser).
 * <tr><td>alt       <td>Handled by the caller (HTML parser).
 * <tr><td>usemap    <td>Handled by the caller (HTML parser).
 * <tr><td>ismap     <td>Handled by the caller (HTML parser).
 * <tr><td>tabindex  <td>Not supported currently.
 * <tr><td>accesskey <td>Not supported currently.
 * <tr><td>onfocus   <td>Not supported currently.
 * <tr><td>onblur    <td>Not supported currently.
 * <tr><td>onselect  <td>Not supported currently.
 * <tr><td>onchange  <td>Not supported currently.
 * <tr><td>accept    <td>Not supported currently.
 * </table>
 *
 * For the different values of \em type, the following resources can be
 * used:
 *
 * <table>
 * <tr><th>Type     <th>Resource
 *                  <th>Factory Method
 * <tr><td>text     <td>dw::core::ui::EntryResource
 *                  <td>dw::core::ui::ResourceFactory::createEntryResource
 * <tr><td>password <td>dw::core::ui::EntryResource
 *                  <td>dw::core::ui::ResourceFactory::createEntryResource
 * <tr><td>checkbox <td>dw::core::ui::CheckButtonResource
 *                 <td>dw::core::ui::ResourceFactory::createCheckButtonResource
 * <tr><td>radio    <td>dw::core::ui::RadioButtonResource
 *                 <td>dw::core::ui::ResourceFactory::createRadioButtonResource
 * <tr><td>submit   <td>dw::core::ui::LabelButtonResource
 *                 <td>dw::core::ui::ResourceFactory::createLabelButtonResource
 * <tr><td>image    <td>dw::core::ui::ComplexButtonResource
 *              <td>dw::core::ui::ResourceFactory::createComplexButtonResource,
 *                     width a dw::Image inside and relief = false.
 * <tr><td>reset    <td>dw::core::ui::LabelButtonResource
 *                 <td>dw::core::ui::ResourceFactory::createLabelButtonResource
 * <tr><td>button   <td>dw::core::ui::LabelButtonResource
 *                 <td>dw::core::ui::ResourceFactory::createLabelButtonResource
 * <tr><td>hidden   <td>No rendering necessary.
 *                  <td>-
 * <tr><td>file     <td>Not supported currently.
 *                  <td>-
 * </table>
 *
 * <h4>\<SELECT\>, \<OPTGROUP\>, and \<OPTION\></h4>
 *
 * \<SELECT\> is implemented either by dw::core::ui::OptionMenuResource
 * (better suitable for \em size = 1 and single selection) or
 * dw::core::ui::ListResource, which have a common base,
 * dw::core::ui::SelectionResource. In the latter case, \em size must be
 * specified via dw::core::style::Style.
 *
 * Factory methods are dw::core::ui::ResourceFactory::createListResource and
 * dw::core::ui::ResourceFactory::createOptionMenuResource.
 *
 * \<OPTION\>'s are added via dw::core::ui::SelectionResource::addItem.
 *
 * \<OPTGROUP\> are created by using dw::core::ui::SelectionResource::pushGroup
 * and dw::core::ui::SelectionResource::popGroup.
 *
 * For lists, the selection mode must be set in
 * dw::core::ui::ResourceFactory::createListResource.
 *
 * <h4>\<TEXTAREA\></h4>
 *
 * \<TEXTAREA\> is implemented by dw::core::ui::MultiLineTextResource,
 * the factory method is
 * dw::core::ui::ResourceFactory::createMultiLineTextResource.
 * dw::core::ui::TextResource::setEditable can be used, as for entries.
 *
 * <h4>\<BUTTON\></h4>
 *
 * For handling \<BUTTON\>, dw::core::ui::ComplexButtonResource should be used,
 * with a dw::Textblock inside, and relief = true. The contents of \<BUTTON\>
 * is then added to the dw::Textblock.
 *
 * \todo describe activation signal
 */
namespace ui {

class Resource;

/**
 * \brief A widget for embedding UI widgets.
 *
 * \sa dw::core::ui
 */
class Embed: public Widget
{
   friend class Resource;

private:
   Resource *resource;

protected:
   void sizeRequestImpl (Requisition *requisition);
   void getExtremesImpl (Extremes *extremes);
   void sizeAllocateImpl (Allocation *allocation);
   void enterNotifyImpl (core::EventCrossing *event);
   void leaveNotifyImpl (core::EventCrossing *event);
   bool buttonPressImpl (core::EventButton *event);

public:
   static int CLASS_ID;

   Embed(Resource *resource);
   ~Embed();

   void setWidth (int width);
   void setAscent (int ascent);
   void setDescent (int descent);
   void setDisplayed (bool displayed);
   void setEnabled (bool enabled);
   void draw (View *view, Rectangle *area);
   Iterator *iterator (Content::Type mask, bool atEnd);
   void setStyle (style::Style *style);

   inline void setUsesHints () { setFlags (USES_HINTS); }

   inline Resource *getResource () { return resource; }
};

/**
 * \brief Basic interface for all resources.
 *
 * \sa dw::core::ui
 */
class Resource
{
   friend class Embed;

public:
   /**
    * \brief Receiver interface for the "activate" signal.
    */
   class ActivateReceiver: public lout::signal::Receiver
   {
   public:
      virtual void activate (Resource *resource) = 0;
      virtual void enter (Resource *resource) = 0;
      virtual void leave (Resource *resource) = 0;
   };
   /**
    * \brief Receiver interface for the "clicked" signal.
    */
   class ClickedReceiver: public lout::signal::Receiver
   {
   public:
      virtual void clicked (Resource *resource, EventButton *event) = 0;
   };

private:
   class ActivateEmitter: public lout::signal::Emitter
   {
   protected:
      bool emitToReceiver (lout::signal::Receiver *receiver, int signalNo,
                           int argc, Object **argv);
   public:
      inline void connectActivate (ActivateReceiver *receiver) {
         connect (receiver); }
      void emitActivate (Resource *resource);
      void emitEnter (Resource *resource);
      void emitLeave (Resource *resource);
   };

   class ClickedEmitter: public lout::signal::Emitter
   {
   protected:
      bool emitToReceiver (lout::signal::Receiver *receiver, int signalNo,
                           int argc, Object **argv);
   public:
      inline void connectClicked (ClickedReceiver *receiver) {
         connect (receiver); }
      void emitClicked (Resource *resource, EventButton *event);
   };

   Embed *embed;
   ActivateEmitter activateEmitter;
   ClickedEmitter clickedEmitter;

   void emitEnter ();
   void emitLeave ();
protected:
   inline void queueResize (bool extremesChanged) {
      if (embed) embed->queueResize (0, extremesChanged);
   }

   virtual Embed *getEmbed () { return embed; }
   virtual void setEmbed (Embed *embed);

   inline void emitActivate () {
      return activateEmitter.emitActivate (this); }
   inline void emitClicked (EventButton *event) {
      clickedEmitter.emitClicked (this, event); }

public:
   inline Resource () { embed = NULL; }

   virtual ~Resource ();

   virtual void sizeRequest (Requisition *requisition) = 0;
   virtual void getExtremes (Extremes *extremes);
   virtual void sizeAllocate (Allocation *allocation);
   virtual void setWidth (int width);
   virtual void setAscent (int ascent);
   virtual void setDescent (int descent);
   virtual void setDisplayed (bool displayed);
   virtual void draw (View *view, Rectangle *area);
   virtual Iterator *iterator (Content::Type mask, bool atEnd) = 0;
   virtual void setStyle (style::Style *style);

   virtual bool isEnabled () = 0;
   virtual void setEnabled (bool enabled) = 0;

   inline void connectActivate (ActivateReceiver *receiver) {
      activateEmitter.connectActivate (receiver); }
   inline void connectClicked (ClickedReceiver *receiver) {
      clickedEmitter.connectClicked (receiver); }
};


class ButtonResource: public Resource
{};

/**
 * \brief Interface for labelled buttons resources.
 */
class LabelButtonResource: public ButtonResource
{
public:
   Iterator *iterator (Content::Type mask, bool atEnd);

   virtual const char *getLabel () = 0;
   virtual void setLabel (const char *label) = 0;
};

class ComplexButtonResource: public ButtonResource
{
private:
   class LayoutReceiver: public Layout::Receiver
   {
   public:
      ComplexButtonResource *resource;

      void canvasSizeChanged (int width, int ascent, int descent);
   };

   friend class LayoutReceiver;
   LayoutReceiver layoutReceiver;

   Widget *childWidget;

protected:
   Layout *layout;
   int click_x, click_y;

   void setEmbed (Embed *embed);

   virtual Platform *createPlatform () = 0;
   virtual void setLayout (Layout *layout) = 0;

   virtual int reliefXThickness () = 0;
   virtual int reliefYThickness () = 0;

   void init (Widget *widget);

public:
   ComplexButtonResource ();
   ~ComplexButtonResource ();

   void sizeRequest (Requisition *requisition);
   void getExtremes (Extremes *extremes);
   void sizeAllocate (Allocation *allocation);
   void setWidth (int width);
   void setAscent (int ascent);
   void setDescent (int descent);
   Iterator *iterator (Content::Type mask, bool atEnd);
   int getClickX () {return click_x;};
   int getClickY () {return click_y;};
};

/**
 * \brief Base interface for dw::core::ui::ListResource and
 *    dw::core::ui::OptionMenuResource.
 */
class SelectionResource: public Resource
{
public:
   virtual void addItem (const char *str, bool enabled, bool selected) = 0;
   virtual void setItem (int index, bool selected) = 0;
   virtual void pushGroup (const char *name, bool enabled) = 0;
   virtual void popGroup () = 0;

   virtual int getNumberOfItems () = 0;
   virtual bool isSelected (int index) = 0;
};

class ListResource: public SelectionResource
{
public:
   enum SelectionMode {
      /**
       * \brief Exactly one item is selected.
       *
       * If no item is selected initially, the first one is selected.
       */
      SELECTION_EXACTLY_ONE,

      /**
       * \brief Exactly one item is selected, except possibly at the beginning.
       *
       * If no item is selected initially, no one is selected automatically.
       * The user may not unselect the only selected item.
       */
      SELECTION_EXACTLY_ONE_BY_USER,

      /**
       * \brief At most one item is selected.
       *
       * If no item is selected initially, no one is selected automatically.
       * The user may unselect the only selected item.
       */
      SELECTION_AT_MOST_ONE,

      /**
       * \brief An arbitrary number of items may be selected.
       */
      SELECTION_MULTIPLE
   };
};

class OptionMenuResource: public SelectionResource
{
};

class TextResource: public Resource
{
public:
   Iterator *iterator (Content::Type mask, bool atEnd);

   virtual const char *getText () = 0;
   virtual void setText (const char *text) = 0;
   virtual bool isEditable () = 0;
   virtual void setEditable (bool editable) = 0;
};

class EntryResource: public TextResource
{
public:
   enum { UNLIMITED_SIZE = -1 };
   virtual void setMaxLength (int maxlen) = 0;
};

class MultiLineTextResource: public TextResource
{
};


class ToggleButtonResource: public Resource
{
public:
   virtual bool isActivated () = 0;
   virtual void setActivated (bool activated) = 0;
};

class CheckButtonResource: public ToggleButtonResource
{
public:
   Iterator *iterator (Content::Type mask, bool atEnd);
};

class RadioButtonResource: public ToggleButtonResource
{
public:
   class GroupIterator
   {
   protected:
      GroupIterator () { }
      virtual ~GroupIterator ();

   public:
      virtual bool hasNext () = 0;
      virtual RadioButtonResource *getNext () = 0;
      virtual void unref () = 0;
   };

   /**
    * \brief Return an iterator, to access all radio button resources
    *    within the group.
    */
   virtual GroupIterator *groupIterator () = 0;

   Iterator *iterator (Content::Type mask, bool atEnd);
};


/**
 * \brief A factory for the common resource.
 */
class ResourceFactory: public lout::object::Object
{
public:
   virtual LabelButtonResource *createLabelButtonResource (const char *label)
      = 0;
   virtual ComplexButtonResource *createComplexButtonResource (Widget *widget,
                                                               bool relief)
      = 0;
   virtual ListResource *createListResource (ListResource::SelectionMode
                                             selectionMode, int rows) = 0;
   virtual OptionMenuResource *createOptionMenuResource () = 0;
   virtual EntryResource *createEntryResource (int size, bool password,
                                               const char *label,
                                               const char *placeholder) = 0;
   virtual MultiLineTextResource *createMultiLineTextResource (int cols,
                                                               int rows,
                                                  const char *placeholder) = 0;
   virtual CheckButtonResource *createCheckButtonResource (bool activated) = 0;
   virtual RadioButtonResource *createRadioButtonResource (RadioButtonResource
                                                           *groupedWith,
                                                           bool activated) = 0;
};

} // namespace ui
} // namespace core
} // namespace dw

#endif // __DW_UI_HH__
