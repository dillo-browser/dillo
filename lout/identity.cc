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



#include "identity.hh"

#include <stdio.h>

namespace lout {
namespace identity {

// ------------------------
//    IdentifiableObject
// ------------------------

using namespace object;
using namespace container::typed;

IdentifiableObject::Class::Class (IdentifiableObject::Class *parent, int id,
                                  const char *className)
{
   this->parent = parent;
   this->id = id;
   this->className = className;
}

HashTable <ConstString, IdentifiableObject::Class>
   *IdentifiableObject::classesByName =
      new HashTable<ConstString, IdentifiableObject::Class> (true, true);
Vector <IdentifiableObject::Class> *IdentifiableObject::classesById =
   new Vector <IdentifiableObject::Class> (16, false);
IdentifiableObject::Class *IdentifiableObject::currentlyConstructedClass;

IdentifiableObject::IdentifiableObject ()
{
   currentlyConstructedClass = NULL;
}

void IdentifiableObject::intoStringBuffer(misc::StringBuffer *sb)
{
   sb->append("<instance of ");
   sb->append(getClassName());
   sb->append(">");
}

/**
 * \brief This method must be called in the constructor for the sub class.
 *    See class comment for details.
 */
void IdentifiableObject::registerName (const char *className, int *classId)
{
   ConstString str (className);
   Class *klass = classesByName->get (&str);
   if (klass == NULL) {
      klass = new Class (currentlyConstructedClass, classesById->size (),
                         className);
      ConstString *key = new ConstString (className);
      classesByName->put (key, klass);
      classesById->put (klass);
      *classId = klass->id;
   }

   this->classId = klass->id;
   currentlyConstructedClass = klass;
}

/**
 * \brief Returns, whether this class is an instance of the class, given by
 *    \em otherClassId, or of a sub class of this class.
 */
bool IdentifiableObject::instanceOf (int otherClassId)
{
   if (otherClassId == -1)
      // Other class has not been registered yet, while it should have been,
      // if this class is an instance of it or of a sub-class.
      return false;

   Class *otherClass = classesById->get (otherClassId);

   if (otherClass == NULL) {
      fprintf (stderr,
               "WARNING: Something got wrong here, it seems that a "
               "CLASS_ID was not initialized properly.\n");
      return false;
   }

   for (Class *klass = classesById->get (classId); klass != NULL;
        klass = klass->parent) {
      if (klass == otherClass)
         return true;
   }

   return false;
}

} // namespace identity
} // namespace lout
