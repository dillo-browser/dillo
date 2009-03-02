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



#include "signal.hh"

namespace lout {
namespace signal {

using namespace container::typed;

// ------------
//    Emitter
// ------------

Emitter::Emitter ()
{
   receivers = new List <Receiver> (false);
}

Emitter::~Emitter ()
{
   for (Iterator<Receiver> it = receivers->iterator (); it.hasNext (); ) {
      Receiver *receiver = it.getNext ();
      receiver->unconnectFrom (this);
   }
   delete receivers;
}

void Emitter::intoStringBuffer(misc::StringBuffer *sb)
{
   sb->append ("<emitter: ");
   receivers->intoStringBuffer (sb);
   sb->append (">");
}

void Emitter::unconnect (Receiver *receiver)
{
   receivers->removeRef (receiver);
}

/**
 * \brief Connect a receiver to the emitter.
 *
 * This is protected, a sub class should define a wrapper, with the respective
 * receiver as an argument, to gain type safety.
 */
void Emitter::connect (Receiver *receiver)
{
   receivers->append (receiver);
   receiver->connectTo (this);
}

/**
 * \brief Emit a void signal.
 *
 * This method should be called by a wrapper (return value void), which
 * \em folds the signal, and delegates the emission to here.
 */
void Emitter::emitVoid (int signalNo, int argc, Object **argv)
{
   for (Iterator <Receiver> it = receivers->iterator (); it.hasNext (); ) {
      Receiver *receiver = it.getNext();
      emitToReceiver (receiver, signalNo, argc, argv);
   }
}

/**
 * \brief Emit a boolean signal.
 *
 * This method should be called by a wrapper, which \em folds the signal,
 * delegates the emission to here, and returns the same boolean value.
 */
bool Emitter::emitBool (int signalNo, int argc, Object **argv)
{
   bool b = false, bt;

   for (Iterator <Receiver> it = receivers->iterator (); it.hasNext (); ) {
      Receiver *receiver = it.getNext();
      // Note: All receivers are called, even if one returns true.
      // Therefore, something like
      //    b = b || emitToReceiver (receiver, signalNo, argc, argv);
      // does not work.
      bt = emitToReceiver (receiver, signalNo, argc, argv);
      b = b || bt;
   }

   return b;
}


// --------------
//    Receiver
// --------------

Receiver::Receiver()
{
   emitters = new List <Emitter> (false);
}

Receiver::~Receiver()
{
   for (Iterator<Emitter> it = emitters->iterator(); it.hasNext(); ) {
      Emitter *emitter = it.getNext();
      emitter->unconnect (this);
   }
   delete emitters;
}

void Receiver::intoStringBuffer(misc::StringBuffer *sb)
{
   // emitters are not listed, to prevent recursion
   sb->append ("<receiver>");
}

void Receiver::connectTo(Emitter *emitter)
{
   emitters->append (emitter);
}

void Receiver::unconnectFrom(Emitter *emitter)
{
   emitters->removeRef (emitter);
}

// ------------------------
//    ObservedObject
// ------------------------

bool ObservedObject::DeletionEmitter::emitToReceiver (Receiver *receiver,
                                                      int signalNo,
                                                      int argc, Object **argv)
{
   object::TypedPointer <ObservedObject> *p =
      (object::TypedPointer<ObservedObject>*)argv[0];
   ((DeletionReceiver*)receiver)->deleted (p->getTypedValue ());
   return false;
}

void ObservedObject::DeletionEmitter::emitDeletion (ObservedObject *obj)
{
   object::TypedPointer <ObservedObject> p(obj);
   object::Object *argv[1] = { &p };
   emitVoid (0, 1, argv);
}

ObservedObject::~ObservedObject()
{
   deletionEmitter.emitDeletion (this);
}

} // namespace signal
} // namespace lout
