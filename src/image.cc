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

#include <stdio.h>
#include <string.h>

#include "msg.h"

#include "image.hh"
#include "dw/core.hh"
#include "dw/image.hh"

using namespace dw::core;

// Image to Object-Image macro
#define I2DW(Image)  ((dw::Image*)(Image->dw))


/*
 * Create and initialize a new image structure.
 */
DilloImage *a_Image_new(const char *alt_text, int32_t bg_color)
{
   DilloImage *Image;

   Image = dNew(DilloImage, 1);
   Image->dw = (void*) new dw::Image(alt_text);
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
   _MSG("a_Image_set_parms: width=%d height=%d\n", width, height);

   bool resize = (Image->width != width || Image->height != height);
   I2DW(Image)->setBuffer((Imgbuf*)v_imgbuf, resize);

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
   I2DW(Image)->drawRow(y);
   a_Bitvec_set_bit(Image->BitVec, y);
   Image->State = IMG_Write;
}

/*
 * Implement the close method
 */
void a_Image_close(DilloImage *Image)
{
   _MSG("a_Image_close\n");
}

