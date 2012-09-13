#ifndef __LOUT_SIGNALS_HH__
#define __LOUT_SIGNALS_HH__

#include "object.hh"
#include "container.hh"

namespace lout {

/**
 * \brief This namespace provides base classes to define signals.
 *
 * By using signals, objects may be connected at run-time, e.g. a general
 * button widget may be connected to another application-specific object,
 * which reacts on the operations on the button by the user. In this case,
 * the button e.g. defines a signal "clicked", which is "emitted" each
 * time the user clicks on the button. After the application-specific
 * object has been connected to this signal, a specific method of it will
 * be called each time, this button emits the "clicked" signal.
 *
 * Below, we will call the level, on which signals are defined, the
 * "general level", and the level, on which the signals are connected,
 * the "caller level".
 *
 * <h3>Defining Signals</h3>
 *
 * Typically, signals are grouped. To define a signal group \em bar for your
 * class \em Foo, you have to define two classes, the emitter and the
 * receiver (BarEmitter and BarReceiver), and instantiate the emitter:
 *
 * \dot
 * digraph G {
 *    node [shape=record, fontname=Helvetica, fontsize=10];
 *    edge [arrowhead="none", arrowtail="empty", labelfontname=Helvetica,
 *          labelfontsize=10, color="#404040", labelfontcolor="#000080"];
 *    fontname=Helvetica; fontsize=10;
 *
 *    subgraph cluster_signal {
 *       style="dashed"; color="#000080"; fontname=Helvetica; fontsize=10;
 *       label="signal";
 *
 *       Emitter [color="#a0a0a0", URL="\ref signal::Emitter"];
 *       Receiver [color="#a0a0a0", URL="\ref signal::Receiver"];
 *    }
 *
 *    subgraph cluster_foo {
 *       style="dashed"; color="#000080"; fontname=Helvetica; fontsize=10;
 *       label="General (foo)";
 *
 *       Foo;
 *       BarEmitter;
 *       BarReceiver [color="#a0a0a0"];
 *    }
 *
 *    Emitter -> BarEmitter;
 *    Receiver -> BarReceiver;
 *    Foo -> BarEmitter [arrowhead="open", arrowtail="none",
 *                       headlabel="1", taillabel="1"];
 * }
 * \enddot
 *
 * <center>[\ref uml-legend "legend"]</center>
 *
 * BarEmitter (class and instance) may be kept private, but BarReceiver must
 * be public, since the caller of Foo must create a sub class of it. For
 * BarEmitter, several methods must be implemented, see signal::Emitter for
 * details. In BarReceiver, only some virtual abstract methods are defined,
 * which the caller must implement. In this case, it is recommended to define
 * a connectBar(BarReceiver*) method in Foo, which is delegated to the
 * BarEmitter.
 *
 * <h3>Connecting to Signals</h3>
 *
 * A caller, which wants to connect to a signal, must define a sub class of
 * the receiver, and implement the virtual methods. A typical design looks
 * like this:
 *
 * \dot
 * digraph G {
 *    node [shape=record, fontname=Helvetica, fontsize=10];
 *    edge [arrowhead="open", arrowtail="none", labelfontname=Helvetica,
 *          labelfontsize=10, color="#404040", labelfontcolor="#000080"];
 *    fontname=Helvetica; fontsize=10;
 *
 *    subgraph cluster_foo {
 *       style="dashed"; color="#000080"; fontname=Helvetica; fontsize=10;
 *       label="Generall (foo)";
 *
 *       BarReceiver [color="#a0a0a0"];
 *    }
 *
 *    subgraph cluster_qix {
 *       style="dashed"; color="#000080"; fontname=Helvetica; fontsize=10;
 *       label="Caller (qix)";
 *
 *       Qix;
 *       QixBarReceiver;
 *    }
 *
 *    BarReceiver -> QixBarReceiver [arrowhead="none", arrowtail="empty"];
 *    QixBarReceiver -> Qix [headlabel="1", taillabel="*"];
 * }
 * \enddot
 *
 * <center>[\ref uml-legend "legend"]</center>
 *
 * (We skip "baz" in the canon, for better readability.)
 *
 * Here, the QixBarReceiver is connected to the Qix, so that the signals can
 * be delegated to the Qix. Notice that the receiver gets automatically
 * disconnected, when deleted (see signal::Receiver::~Receiver).
 *
 * <h3>Void and Boolean Signals</h3>
 *
 * In the simplest case, signal emitting involves calling a list of
 * signal receivers (void signals). For boolean signals, the receivers return
 * a boolean value, and the result of the signal emission (the return value of
 * signal::Emitter::emitBool) returns the disjunction of the values returned
 * by the receivers. Typically, a receiver states with its return value,
 * whether the signal was used in any way, the resulting return value so
 * indicates, whether at least one receiver has used the signal.
 *
 * In Dw, events are processed this way. In the simplest case, they are
 * delegated to the parent widget, if the widget does not process them (by
 * returning false). As an addition, signals are emitted, and if a receiver
 * processes the event, this is handled the same way, as if the widget itself
 * would have processed it.
 *
 * Notice, that also for boolean signals, all receivers are called, even
 * after one receiver has already returned true.
 *
 * <h3>Memory Management</h3>
 *
 * <h4>Emitters</h4>
 *
 * Emitters are typically instantiated one, for one object emitting the
 * signals. In the example above, the class Foo will contain a field
 * "BarEmitter barEmitter" (not as a pointer, "BarEmitter *barEmitter").
 *
 * <h4>Receivers</h4>
 *
 * It is important, that a emitter never deletes a receiver, it just removes
 * them from the receivers list. Likewise, when a receiver is deleted, it
 * unconnects itself from all emitters. (The same receiver instance can indeed
 * be connected to multiple emitters.) So, the caller has to care about
 * deleting receivers.
 *
 * In the example above, something like that will work:
 *
 * \code
 * class Qix
 * {
 * private:
 *    class QixBarReceiver
 *    {
 *    public:
 *        Qix *qix;
 *        // ...
 *    };
 *
 *    QixBarReceiver barReceiver;
 *
 *    // ...
 * };
 * \endcode
 *
 * The constructor of Qix should then set \em qix:
 *
 * \code
 * Qix::Qix ()
 * {
 *    barReceiver.qix = this.
 *    // ...
 * }
 * \endcode
 *
 * After this, &\em barReceiver can be connected to all instances of
 * BarEmitter, also multiple times.
 */
namespace signal {

class Receiver;

/**
 * \brief The base class for signal emitters.
 *
 * If defining a signal group, a sub class of this class must be defined,
 * with
 *
 * <ul>
 * <li> a definition of the different signals (as enumeration),
 * <li> an implementation of signal::Emitter::emitToReceiver,
 * <li> wrappers for signal::Emitter::emitVoid and signal::Emitter::emitBool,
 *      respectively (one for each signal), and
 * <li> a wrapper for signal::Emitter::connect.
 * </ul>
 *
 * There are two representations of signals:
 *
 * <ul>
 * <li> In the \em unfolded representation, the signal itself is represented
 *      by the method itself (in the emitter or the receiver), and the
 *      arguments are represented as normal C++ types.
 *
 * <li> \em Folding signals means to represent the signal itself by an integer
 *      number (enumeration), and translate the arguments in an object::Object*
 *      array. (If a given argument is not an instance of object::Object*,
 *      the wrappers in ::object can be used.)
 * </ul>
 *
 * \sa ::signal
 */
class Emitter: public object::Object
{
   friend class Receiver;

private:
   container::typed::List <Receiver> *receivers;

   void unconnect (Receiver *receiver);

protected:
   void emitVoid (int signalNo, int argc, Object **argv);
   bool emitBool (int signalNo, int argc, Object **argv);
   void connect(Receiver *receiver);

   /**
    * \brief A sub class must implement this for a call to a single
    *    receiver.
    *
    * This methods gets the signal in a \em folded representation, it has
    * to unfold it, and pass it to a single receiver. For boolean signals,
    * the return value of the receiver must be returned, for void signals,
    * the return value is discarded.
    */
   virtual bool emitToReceiver (Receiver *receiver, int signalNo,
                                int argc, Object **argv) = 0;

public:
   Emitter();
   ~Emitter();

   void intoStringBuffer(misc::StringBuffer *sb);
};

/**
 * \brief The base class for signal receiver base classes.
 *
 * If defining a signal group, a sub class of this class must be defined,
 * in which only the abstract signal methods must be defined.
 *
 * \sa ::signal
 */
class Receiver: public object::Object
{
  friend class Emitter;

private:
   container::typed::List<Emitter> *emitters;

   void connectTo(Emitter *emitter);
   void unconnectFrom(Emitter *emitter);

public:
   Receiver();
   ~Receiver();

   void intoStringBuffer(misc::StringBuffer *sb);
};

/**
 * \brief An observed object has a signal emitter, which tells the
 *    receivers, when the object is deleted.
 */
class ObservedObject
{
public:
   class DeletionReceiver: public signal::Receiver
   {
   public:
      virtual void deleted (ObservedObject *object) = 0;
   };

private:
   class DeletionEmitter: public signal::Emitter
   {
   protected:
      bool emitToReceiver (signal::Receiver *receiver, int signalNo,
                           int argc, Object **argv);

   public:
      inline void connectDeletion (DeletionReceiver *receiver)
      { connect (receiver); }

      void emitDeletion (ObservedObject *obj);
   };

   DeletionEmitter deletionEmitter;

public:
   virtual ~ObservedObject();

   inline void connectDeletion (DeletionReceiver *receiver)
   { deletionEmitter.connectDeletion (receiver); }
};

} // namespace signal

} // namespace lout

#endif // __LOUT_SIGNALS_HH__
