/*
 * File: image.cc
 *
 * Copyright (C) 2005-2007 Jorge Arellano Cid <jcid@dillo.org>,
 *                         Sebastian Geerken  <sgeerken@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

/*
 * This file implements image data transfer methods. It handles the transfer
 * of data from an Image to a DwImage widget.
 */

#include "msg.h"

#include "image.hh"
#include "dw/core.hh"
#include "dw/image.hh"

using namespace dw::core;

// Image to Object-ImgRenderer macro
#define I2IR(Image)  ((dw::core::ImgRenderer*)(Image->img_rndr))


/*
 * Create and initialize a new image structure.
 */
DilloImage *a_Image_new(void *layout, void *img_rndr, int32_t bg_color)
{
   DilloImage *Image;

   Image = dNew(DilloImage, 1);
   Image->layout = layout;
   Image->img_rndr = img_rndr;
   Image->width = 0;
   Image->height = 0;
   Image->bg_color = bg_color;
   Image->ScanNumber = 0;
   Image->BitVec = NULL;
   Image->State = IMG_Empty;

   Image->RefCount = 0;

   return Image;
}

/*
 * Create and initialize a new image structure with an image widget.
 */
DilloImage *a_Image_new_with_dw(void *layout, const char *alt_text,
                                int32_t bg_color)
{
   dw::Image *dw = new dw::Image(alt_text);
   return a_Image_new(layout, (void*)(dw::core::ImgRenderer*)dw, bg_color);
}

/*
 * Return the image renderer as a widget. This is somewhat tricky,
 * since simple casting leads to wrong (and hard to debug) results,
 * because of multiple inheritance. This function can be used from C
 * code, where only access to void* is possible.
 */
void *a_Image_get_dw(DilloImage *Image)
{
   return (dw::Image*)(dw::core::ImgRenderer*)Image->img_rndr;
}
/*
 * Deallocate an Image structure
 */
static void Image_free(DilloImage *Image)
{
   a_Bitvec_free(Image->BitVec);
   dFree(Image);
}

/*
 * Unref and free if necessary
 * Do nothing if the argument is NULL
 */
void a_Image_unref(DilloImage *Image)
{
   _MSG(" %d ", Image->RefCount);
   if (Image && --Image->RefCount == 0)
      Image_free(Image);
}

/*
 * Add a reference to an Image struct
 * Do nothing if the argument is NULL
 */
void a_Image_ref(DilloImage *Image)
{
   if (Image)
      ++Image->RefCount;
}

/*
 * Set initial parameters of the image
 */
void a_Image_set_parms(DilloImage *Image, void *v_imgbuf, DilloUrl *url,
                       int version, uint_t width, uint_t height,
                       DilloImgType type)
{
   _MSG("a_Image_set_parms: width=%d height=%d iw=%d ih=%d\n",
        width, height, Image->width, Image->height);

   /* Resize from 0,0 to width,height */
   bool resize = true;
   I2IR(Image)->setBuffer((Imgbuf*)v_imgbuf, resize);

   if (!Image->BitVec)
      Image->BitVec = a_Bitvec_new(height);
   Image->width = width;
   Image->height = height;
   Image->State = IMG_SetParms;
}

/*
 * Implement the write method
 */
void a_Image_write(DilloImage *Image, uint_t y)
{
   _MSG("a_Image_write\n");
   dReturn_if_fail ( y < Image->height );

   /* Update the row in DwImage */
   I2IR(Image)->drawRow(y);
   a_Bitvec_set_bit(Image->BitVec, y);
   Image->State = IMG_Write;
}

/*
 * Implement the close method
 */
void a_Image_close(DilloImage *Image)
{
   _MSG("a_Image_close\n");
   I2IR(Image)->finish();
}

/*
 * Implement the abort method
 */
void a_Image_abort(DilloImage *Image)
{
   _MSG("a_Image_abort\n");
   I2IR(Image)->fatal();
}

