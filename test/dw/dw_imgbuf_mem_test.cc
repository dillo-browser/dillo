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

#include "../dw/core.hh"
#include "../dw/fltkcore.hh"

using namespace lout::signal;
using namespace dw::core;
using namespace dw::fltk;

void solution1 ()
{
   FltkPlatform *platform = new FltkPlatform ();
   Layout *layout = new Layout (platform);

   Imgbuf *rootbuf = layout->createImgbuf (Imgbuf::RGB, 100, 100, 1);
   rootbuf->ref (); // Extra reference by the dicache.
   printf ("=== Can be deleted? %s.\n",
           rootbuf->lastReference () ? "Yes" : "No");
   Imgbuf *scaledbuf = rootbuf->getScaledBuf (50, 50);
   printf ("=== Can be deleted? %s.\n",
           rootbuf->lastReference () ? "Yes" : "No");
   rootbuf->unref ();
   printf ("=== Can be deleted? %s.\n",
           rootbuf->lastReference () ? "Yes" : "No");
   scaledbuf->unref ();
   printf ("=== Can be deleted? %s.\n",
           rootbuf->lastReference () ? "Yes" : "No");
   rootbuf->unref (); // Extra reference by the dicache.

   delete layout;
}

void solution2 ()
{
   FltkPlatform *platform = new FltkPlatform ();
   Layout *layout = new Layout (platform);

   Imgbuf *rootbuf = layout->createImgbuf (Imgbuf::RGB, 100, 100, 1);
   rootbuf->setDeleteOnUnref (false);
   printf ("=== Can be deleted? %s.\n",
           !rootbuf->isReferred () ? "Yes" : "No");
   Imgbuf *scaledbuf = rootbuf->getScaledBuf (50, 50);
   printf ("=== Can be deleted? %s.\n",
           !rootbuf->isReferred () ? "Yes" : "No");
   rootbuf->unref ();
   printf ("=== Can be deleted? %s.\n",
           !rootbuf->isReferred () ? "Yes" : "No");
   scaledbuf->unref ();
   printf ("=== Can be deleted? %s.\n",
           !rootbuf->isReferred () ? "Yes" : "No");
   delete rootbuf;

   delete layout;
}

class RootbufDeletionReceiver: public ObservedObject::DeletionReceiver
{
   void deleted (ObservedObject *object);
};

void RootbufDeletionReceiver::deleted (ObservedObject *object)
{
   printf ("=== Is deleted now.\n");
   delete this;
}

void solution3 ()
{
   FltkPlatform *platform = new FltkPlatform ();
   Layout *layout = new Layout (platform);

   Imgbuf *rootbuf = layout->createImgbuf (Imgbuf::RGB, 100, 100, 1);
   rootbuf->connectDeletion (new RootbufDeletionReceiver ());
   Imgbuf *scaledbuf = rootbuf->getScaledBuf (50, 50);
   rootbuf->unref ();
   scaledbuf->unref ();

   delete layout;
}

int main (int argc, char **argv)
{
   printf ("========== SOLUTION 1 ==========\n");
   solution1 ();
   printf ("========== SOLUTION 2 ==========\n");
   solution2 ();
   printf ("========== SOLUTION 3 ==========\n");
   solution3 ();

   return 0;
}
