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
#define OI(Image)  ((dw::Image*)(Image->dw))


/*
 * Create and initialize a new image structure.
 */
DilloImage *a_Image_new(int width,
                        int height,
                        const char *alt_text,
                        int32_t bg_color)
{
   DilloImage *Image;

   Image = dNew(DilloImage, 1);
   Image->dw = (void*) new dw::Image(alt_text);
   Image->width = 0;
   Image->height = 0;
   Image->cmap = NULL;
   Image->in_type = DILLO_IMG_TYPE_NOTSET;
   Image->bg_color = bg_color;
   Image->ScanNumber = 0;
   Image->BitVec = NULL;
   Image->State = IMG_Empty;

   Image->RefCount = 1;

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
 */
void a_Image_unref(DilloImage *Image)
{
   _MSG(" %d ", Image->RefCount);
   if (Image && --Image->RefCount == 0)
      Image_free(Image);
}

/*
 * Add a reference to an Image struct
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
   MSG("a_Image_set_parms: width=%d height=%d\n", width, height);

   bool resize = (Image->width != width || Image->height != height);
   OI(Image)->setBuffer((Imgbuf*)v_imgbuf, resize);

   if (!Image->BitVec)
      Image->BitVec = a_Bitvec_new(height);
   Image->in_type = type;
   Image->width = width;
   Image->height = height;
   Image->State = IMG_SetParms;
}

/*
 * Reference the dicache entry color map
 */
void a_Image_set_cmap(DilloImage *Image, const uchar_t *cmap)
{
   MSG("a_Image_set_cmap\n");
   Image->cmap = cmap;
   Image->State = IMG_SetCmap;
}

/*
 * Begin a new scan for a multiple-scan image
 */
void a_Image_new_scan(DilloImage *Image, void *v_imgbuf)
{
   MSG("a_Image_new_scan\n");
   a_Bitvec_clear(Image->BitVec);
   Image->ScanNumber++;
   ((Imgbuf*)v_imgbuf)->newScan();
}

/*
 * Implement the write method
 */
void a_Image_write(DilloImage *Image, uint_t y)
{
   _MSG("a_Image_write\n");
   dReturn_if_fail ( y < Image->height );

   /* Update the row in DwImage */
   OI(Image)->drawRow(y);
   a_Bitvec_set_bit(Image->BitVec, y);
   Image->State = IMG_Write;
}

/*
 * Implement the close method
 */
void a_Image_close(DilloImage *Image)
{
   MSG("a_Image_close\n");
   a_Image_unref(Image);
}

