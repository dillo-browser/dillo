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



#include "fltkcore.hh"
#include "../lout/msg.h"
#include "../lout/misc.hh"

#include <FL/fl_draw.H>

#define IMAGE_MAX_AREA (6000 * 6000)

namespace dw {
namespace fltk {

using namespace lout::container::typed;

FltkImgbuf::FltkImgbuf (Type type, int width, int height)
{
   _MSG("FltkImgbuf: new root %p\n", this);
   init (type, width, height, NULL);
}

FltkImgbuf::FltkImgbuf (Type type, int width, int height, FltkImgbuf *root)
{
   _MSG("FltkImgbuf: new scaled %p, root is %p\n", this, root);
   init (type, width, height, root);
}

void FltkImgbuf::init (Type type, int width, int height, FltkImgbuf *root)
{
   this->root = root;
   this->type = type;
   this->width = width;
   this->height = height;

   // TODO: Maybe this is only for root buffers
   switch (type) {
      case RGBA: bpp = 4; break;
      case RGB:  bpp = 3; break;
      default:   bpp = 1; break;
   }
   _MSG("FltkImgbuf::init width=%d height=%d bpp=%d\n", width, height, bpp);
   rawdata = new uchar[bpp * width * height];
   // Set light-gray as interim background color.
   memset(rawdata, 222, width*height*bpp);

   refCount = 1;
   deleteOnUnref = true;
   copiedRows = new lout::misc::BitSet (height);

   // The list is only used for root buffers.
   if (isRoot())
      scaledBuffers = new lout::container::typed::List <FltkImgbuf> (true);
   else
      scaledBuffers = NULL;

   if (!isRoot()) {
      // Scaling
      for (int row = 0; row < root->height; row++) {
         if (root->copiedRows->get (row))
            scaleRow (row, root->rawdata + row*root->width*root->bpp);
      }
   }
}

FltkImgbuf::~FltkImgbuf ()
{
   _MSG("~FltkImgbuf[%s %p] deleted\n", isRoot() ? "root":"scaled", this);

   if (!isRoot())
      root->detachScaledBuf (this);

   delete[] rawdata;
   delete copiedRows;

   if (scaledBuffers)
      delete scaledBuffers;
}

/**
 * \brief This method is called for the root buffer, when a scaled buffer
 *    removed.
 */
void FltkImgbuf::detachScaledBuf (FltkImgbuf *scaledBuf)
{
   scaledBuffers->detachRef (scaledBuf);

   _MSG("FltkImgbuf[root %p]: scaled buffer %p is detached, %d left\n",
        this, scaledBuf, scaledBuffers->size ());

   if (refCount == 0 && scaledBuffers->isEmpty () && deleteOnUnref)
      // If the root buffer is not used anymore, but this is the last scaled
      // buffer.
      // See also: FltkImgbuf::unref().
      delete this;
}

void FltkImgbuf::setCMap (int *colors, int num_colors)
{
}

inline void FltkImgbuf::scaleRow (int row, const core::byte *data)
{
   int sr1 = scaledY (row);
   int sr2 = scaledY (row + 1);

   for (int sr = sr1; sr < sr2; sr++) {
      // Avoid multiple passes.
      if (copiedRows->get(sr)) continue;

      copiedRows->set (sr, true);
      if (sr == sr1) {
         for (int px = 0; px < root->width; px++) {
            int px1 = px * width / root->width;
            int px2 = (px+1) * width / root->width;
            for (int sp = px1; sp < px2; sp++) {
               memcpy(rawdata + (sr*width + sp)*bpp, data + px*bpp, bpp);
            }
         }
      } else {
         memcpy(rawdata + sr*width*bpp, rawdata + sr1*width*bpp, width*bpp);
      }
   }
}

void FltkImgbuf::copyRow (int row, const core::byte *data)
{
   assert (isRoot());

   // Flag the row done and copy its data.
   copiedRows->set (row, true);
   memcpy(rawdata + row * width * bpp, data, width * bpp);

   // Update all the scaled buffers of this root image.
   for (Iterator <FltkImgbuf> it = scaledBuffers->iterator(); it.hasNext(); ) {
      FltkImgbuf *sb = it.getNext ();
      sb->scaleRow(row, data);
   }
}

void FltkImgbuf::newScan ()
{
   if (isRoot()) {
      for (Iterator<FltkImgbuf> it = scaledBuffers->iterator(); it.hasNext();){
         FltkImgbuf *sb = it.getNext ();
         sb->copiedRows->clear();
      }
   }
}

core::Imgbuf* FltkImgbuf::getScaledBuf (int width, int height)
{
   if (!isRoot())
      return root->getScaledBuf (width, height);

   if (width == this->width && height == this->height) {
      ref ();
      return this;
   }

   for (Iterator <FltkImgbuf> it = scaledBuffers->iterator(); it.hasNext(); ) {
      FltkImgbuf *sb = it.getNext ();
      if (sb->width == width && sb->height == height) {
         sb->ref ();
         return sb;
      }
   }

   /* Check for excessive image sizes which would cause crashes due to
    * too big allocations for the image buffer.
    * In this case we return a pointer to the unscaled image buffer.
    */
   if (width <= 0 || height <= 0 ||
       width > IMAGE_MAX_AREA / height) {
      MSG("FltkImgbuf::getScaledBuf: suspicious image size request %d x %d\n",
           width, height);
      ref ();
      return this;
   }

   /* This size is not yet used, so a new buffer has to be created. */
   FltkImgbuf *sb = new FltkImgbuf (type, width, height, this);
   scaledBuffers->append (sb);
   return sb;
}

void FltkImgbuf::getRowArea (int row, dw::core::Rectangle *area)
{
   // TODO: May have to be adjusted.

   if (isRoot()) {
      /* root buffer */
      area->x = 0;
      area->y = row;
      area->width = width;
      area->height = 1;
      _MSG("::getRowArea: area x=%d y=%d width=%d height=%d\n",
           area->x, area->y, area->width, area->height);
   } else {
      // scaled buffer
      int sr1 = scaledY (row);
      int sr2 = scaledY (row + 1);

      area->x = 0;
      area->y = sr1;
      area->width = width;
      area->height = sr2 - sr1;
      _MSG("::getRowArea: area x=%d y=%d width=%d height=%d\n",
           area->x, area->y, area->width, area->height);
   }
}

int  FltkImgbuf::getRootWidth ()
{
   return root ? root->width : width;
}

int  FltkImgbuf::getRootHeight ()
{
   return root ? root->height : height;
}

void FltkImgbuf::ref ()
{
   refCount++;

   //if (root)
   //   MSG("FltkImgbuf[scaled %p, root is %p]: ref() => %d\n",
   //        this, root, refCount);
   //else
   //   MSG("FltkImgbuf[root %p]: ref() => %d\n", this, refCount);
}

void FltkImgbuf::unref ()
{
   //if (root)
   //   MSG("FltkImgbuf[scaled %p, root is %p]: ref() => %d\n",
   //       this, root, refCount - 1);
   //else
   //   MSG("FltkImgbuf[root %p]: ref() => %d\n", this, refCount - 1);

   if (--refCount == 0) {
      if (isRoot ()) {
         // Root buffer, it must be ensured that no scaled buffers are left.
         // See also FltkImgbuf::detachScaledBuf().
         if (scaledBuffers->isEmpty () && deleteOnUnref) {
            delete this;
         } else {
            _MSG("FltkImgbuf[root %p]: not deleted. numScaled=%d\n",
                 this, scaledBuffers->size ());
         }
      } else
         // Scaled buffer buffer, simply delete it.
         delete this;
   }
}

bool FltkImgbuf::lastReference ()
{
   return refCount == 1 &&
      (scaledBuffers == NULL || scaledBuffers->isEmpty ());
}

void FltkImgbuf::setDeleteOnUnref (bool deleteOnUnref)
{
   assert (isRoot ());
   this->deleteOnUnref = deleteOnUnref;
}

bool FltkImgbuf::isReferred ()
{
   return refCount != 0 ||
      (scaledBuffers != NULL && !scaledBuffers->isEmpty ());
}


int FltkImgbuf::scaledY(int ySrc)
{
   // TODO: May have to be adjusted.
   assert (root != NULL);
   return ySrc * height / root->height;
}

void FltkImgbuf::draw (Fl_Widget *target, int xRoot, int yRoot,
                       int x, int y, int width, int height)
{
   // TODO: Clarify the question, whether "target" is the current widget
   //       (and so has not to be passed at all).

   _MSG("::draw: xRoot=%d x=%d yRoot=%d y=%d width=%d height=%d\n"
        "        this->width=%d this->height=%d\n",
        xRoot, x, yRoot, y, width, height, this->width, this->height);

   if (x > this->width || y > this->height) {
      return;
   }

   if (x + width > this->width) {
      width = this->width - x;
   }

   if (y + height > this->height) {
      height = this->height - y;
   }

   fl_draw_image(rawdata+bpp*(y*this->width + x), xRoot + x, yRoot + y, width,
                 height, bpp, this->width * bpp);

}

} // namespace dw
} // namespace fltk
