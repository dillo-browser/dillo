/*
 * Dillo Widget
 *
 * Copyright 2005-2007, 2012-2013 Sebastian Geerken <sgeerken@dillo.org>
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
#include <math.h>

#define IMAGE_MAX_AREA (6000 * 6000)

#define MAX_WIDTH 0x8000
#define MAX_HEIGHT 0x8000

namespace dw {
namespace fltk {

using namespace lout::container::typed;

const enum ScaleMode { SIMPLE, BEAUTIFUL, BEAUTIFUL_GAMMA }
   scaleMode = BEAUTIFUL_GAMMA;

Vector <FltkImgbuf::GammaCorrectionTable> *FltkImgbuf::gammaCorrectionTables
   = new Vector <FltkImgbuf::GammaCorrectionTable> (true, 2);

uchar *FltkImgbuf::findGammaCorrectionTable (double gamma)
{
   // Since the number of possible keys is low, a linear search is
   // sufficiently fast.

   for (int i = 0; i < gammaCorrectionTables->size(); i++) {
      GammaCorrectionTable *gct = gammaCorrectionTables->get(i);
      if (gct->gamma == gamma)
         return gct->map;
   }

   _MSG("Creating new table for gamma = %g\n", gamma);

   GammaCorrectionTable *gct = new GammaCorrectionTable();
   gct->gamma = gamma;

   for (int i = 0; i < 256; i++)
      gct->map[i] = 255 * pow((double)i / 255, gamma);

   gammaCorrectionTables->put (gct);
   return gct->map;
}

bool FltkImgbuf::excessiveImageDimensions (int width, int height)
{
   return width <= 0 || height <= 0 ||
      width > IMAGE_MAX_AREA / height;
}

void FltkImgbuf::freeall ()
{
   _MSG("Deleting gammaCorrectionTables\n");
   delete gammaCorrectionTables;
   gammaCorrectionTables = NULL;
}

FltkImgbuf::FltkImgbuf (Type type, int width, int height, double gamma)
{
   DBG_OBJ_CREATE ("dw::fltk::FltkImgbuf");

   _MSG ("FltkImgbuf::FltkImgbuf: new root %p\n", this);
   init (type, width, height, gamma, NULL);
}

FltkImgbuf::FltkImgbuf (Type type, int width, int height, double gamma,
                        FltkImgbuf *root)
{
   DBG_OBJ_CREATE ("dw::fltk::FltkImgbuf");

   _MSG ("FltkImgbuf::FltkImgbuf: new scaled %p, root is %p\n", this, root);
   init (type, width, height, gamma, root);
}

void FltkImgbuf::init (Type type, int width, int height, double gamma,
                       FltkImgbuf *root)
{
   if (excessiveImageDimensions (width, height)) {
      // Excessive image sizes which would cause crashes due to too
      // big allocations for the image buffer (for root buffers, when
      // the image was specially prepared). In this case we use a 1 x
      // 1 size.
      MSG("FltkImgbuf::init: suspicious image size request %d x %d\n",
          width, height);
      init (type, 1, 1, gamma, root);
   } else if (width > MAX_WIDTH) {
      // Too large dimensions cause dangerous overflow errors, so we
      // limit dimensions to harmless values.
      //
      // Example: 65535 * 65536 / 65536 (see scaling below) results in
      // the negative value -1.

      MSG("FltkImgbuf::init: cannot handle large width %d\n", width);
      init (type, MAX_WIDTH, height, gamma, root);
   } else if (height > MAX_HEIGHT) {
      MSG("FltkImgbuf::init: cannot handle large height %d\n", height);
      init (type, width, MAX_HEIGHT, gamma, root);
   } else if (gamma <= 0) {
      MSG("FltkImgbuf::init: non-positive gamma %g\n", gamma);
      init (type, width, height, 1, root);
   } else {
      this->root = root;
      this->type = type;
      this->width = width;
      this->height = height;
      this->gamma = gamma;

      DBG_OBJ_SET_NUM ("width", width);
      DBG_OBJ_SET_NUM ("height", height);

      // TODO: Maybe this is only for root buffers
      switch (type) {
      case RGBA: bpp = 4; break;
      case RGB:  bpp = 3; break;
      default:   bpp = 1; break;
      }
      _MSG("FltkImgbuf::init this=%p width=%d height=%d bpp=%d gamma=%g\n",
           this, width, height, bpp, gamma);
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
}

FltkImgbuf::~FltkImgbuf ()
{
   _MSG ("FltkImgbuf::~FltkImgbuf\n");

   if (!isRoot())
      root->detachScaledBuf (this);

   delete[] rawdata;
   delete copiedRows;

   if (scaledBuffers)
      delete scaledBuffers;

   DBG_OBJ_DELETE ();
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
   if (row < root->height) {
      if (scaleMode == SIMPLE)
         scaleRowSimple (row, data);
      else
         scaleRowBeautiful (row, data);
   }
}

inline void FltkImgbuf::scaleRowSimple (int row, const core::byte *data)
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

inline void FltkImgbuf::scaleRowBeautiful (int row, const core::byte *data)
{
   int sr1 = scaledY (row);
   int sr2 = scaledY (row + 1);
   bool allRootRows = false;

   // Don't rescale rows!
   if (copiedRows->get(sr1)) return;

   if (height > root->height) {
      scaleBuffer (data, root->width, 1,
                   rawdata + sr1 * width * bpp, width, sr2 - sr1,
                   bpp, gamma);
      // Mark scaled rows done
      for (int sr = sr1; sr < sr2 || sr == sr1; sr++)
         copiedRows->set (sr, true);
   } else {
      assert (sr1 == sr2 || sr1 + 1 == sr2);
      int row1 = backscaledY(sr1), row2 = backscaledY(sr1 + 1);

      // Check all the necessary root lines already arrived,
      // a larger area than a single row may be accessed here.
      for (int r=row1; (allRootRows=root->copiedRows->get(r)) && ++r < row2; );
      if (allRootRows) {
         scaleBuffer (root->rawdata + row1 * root->width * bpp,
                      root->width, row2 - row1,
                      rawdata + sr1 * width * bpp, width, 1,
                      bpp, gamma);
         // Mark scaled row done
         copiedRows->set (sr1, true);
      }
   }
}

/**
 * General method to scale an image buffer. Used to scale single lines
 * in scaleRowBeautiful.
 *
 * The algorithm is rather simple. If the scaled buffer is smaller
 * (both width and height) than the original buffer, each pixel in the
 * scaled buffer is assigned a rectangle of pixels in the original
 * buffer; the resulting pixel value (red, green, blue) is simply the
 * average of all pixel values. This is pretty fast and leads to
 * rather good results.
 *
 * Nothing special (like interpolation) is done when scaling up.
 *
 * If scaleMode is set to BEAUTIFUL_GAMMA, gamma correction is
 * considered, see <http://www.4p8.com/eric.brasseur/gamma.html>.
 *
 * TODO Could be optimized as in scaleRowSimple: when the destination
 * image is larger, calculate only one row/column, and copy it to the
 * other rows/columns.
 */
inline void FltkImgbuf::scaleBuffer (const core::byte *src, int srcWidth,
                                     int srcHeight, core::byte *dest,
                                     int destWidth, int destHeight, int bpp,
                                     double gamma)
{
   uchar *gammaMap1, *gammaMap2;

   if (scaleMode == BEAUTIFUL_GAMMA) {
      gammaMap1 = findGammaCorrectionTable (gamma);
      gammaMap2 = findGammaCorrectionTable (1 / gamma);
   }

   for(int x = 0; x < destWidth; x++)
      for(int y = 0; y < destHeight; y++) {
         int xo1 = x * srcWidth / destWidth;
         int xo2 = lout::misc::max ((x + 1) * srcWidth / destWidth, xo1 + 1);
         int yo1 = y * srcHeight / destHeight;
         int yo2 = lout::misc::max ((y + 1) * srcHeight / destHeight, yo1 + 1);
         int n = (xo2 - xo1) * (yo2 - yo1);

         int v[bpp];
         for(int i = 0; i < bpp; i++)
            v[i] = 0;

         for(int xo = xo1; xo < xo2; xo++)
            for(int yo = yo1; yo < yo2; yo++) {
               const core::byte *ps = src + bpp * (yo * srcWidth + xo);
               for(int i = 0; i < bpp; i++)
                  v[i] +=
                     (scaleMode == BEAUTIFUL_GAMMA ? gammaMap2[ps[i]] : ps[i]);
            }

         core::byte *pd = dest + bpp * (y * destWidth + x);
         for(int i = 0; i < bpp; i++)
            pd[i] =
               scaleMode == BEAUTIFUL_GAMMA ? gammaMap1[v[i] / n] : v[i] / n;
      }
}

void FltkImgbuf::copyRow (int row, const core::byte *data)
{
   assert (isRoot());

   if (row < height) {
      // Flag the row done and copy its data.
      copiedRows->set (row, true);
      memcpy(rawdata + row * width * bpp, data, width * bpp);

      // Update all the scaled buffers of this root image.
      for (Iterator <FltkImgbuf> it = scaledBuffers->iterator();
           it.hasNext(); ) {
         FltkImgbuf *sb = it.getNext ();
         sb->scaleRow (row, data);
      }
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

   if (width > MAX_WIDTH) {
      // Similar to init.
      MSG("FltkImgbuf::getScaledBuf: cannot handle large width %d\n", width);
      return getScaledBuf (MAX_WIDTH, height);
   }
   if (height > MAX_HEIGHT) {
      MSG("FltkImgbuf::getScaledBuf: cannot handle large height %d\n", height);
      return getScaledBuf (width, MAX_HEIGHT);
   }

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

   // Check for excessive image sizes which would cause crashes due to
   // too big allocations for the image buffer. In this case we return
   // a pointer to the unscaled image buffer.
   if (excessiveImageDimensions (width, height)) {
      MSG("FltkImgbuf::getScaledBuf: suspicious image size request %d x %d\n",
           width, height);
      ref ();
      return this;
   }

   // This size is not yet used, so a new buffer has to be created.
   FltkImgbuf *sb = new FltkImgbuf (type, width, height, gamma, this);
   scaledBuffers->append (sb);
   DBG_OBJ_ASSOC_CHILD (sb);

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
      if (row > root->height)
         area->x = area->y = area->width = area->height = 0;
      else {
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
}

int FltkImgbuf::getRootWidth ()
{
   return root ? root->width : width;
}

int FltkImgbuf::getRootHeight ()
{
   return root ? root->height : height;
}

core::Imgbuf *FltkImgbuf::createSimilarBuf (int width, int height)
{
   return new FltkImgbuf (type, width, height, gamma);
}

void FltkImgbuf::copyTo (Imgbuf *dest, int xDestRoot, int yDestRoot,
                         int xSrc, int ySrc, int widthSrc, int heightSrc)
{
   FltkImgbuf *fDest = (FltkImgbuf*)dest;
   assert (bpp == fDest->bpp);

   int xSrc2 = lout::misc::min (xSrc + widthSrc, fDest->width - xDestRoot);
   int ySrc2 = lout::misc::min (ySrc + heightSrc, fDest->height - yDestRoot);

   //printf ("copying from (%d, %d), %d x %d to (%d, %d) (root) => "
   //        "xSrc2 = %d, ySrc2 = %d\n",
   //        xSrc, ySrc, widthSrc, heightSrc, xDestRoot, yDestRoot,
   //        xSrc2, ySrc2);

   for (int x = xSrc; x < xSrc2; x++)
      for (int y = ySrc; y < ySrc2; y++) {
         int iSrc = x + width * y;
         int iDest = xDestRoot + x + fDest->width * (yDestRoot + y);

         //printf ("   (%d, %d): %d -> %d\n", x, y, iSrc, iDest);

         for (int b = 0; b < bpp; b++)
            fDest->rawdata[bpp * iDest + b] = rawdata[bpp * iSrc + b];
      }
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

int FltkImgbuf::backscaledY(int yScaled)
{
   assert (root != NULL);

   // Notice that rounding errors because of integers do not play a
   // role. This method cannot be the exact inverse of scaledY, since
   // scaleY is not bijective, and so not invertible. Instead, both
   // values always return the smallest value.
   return yScaled * root->height / height;
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

} // namespace fltk
} // namespace dw
