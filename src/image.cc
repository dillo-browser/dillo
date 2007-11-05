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
 * Local data
 */
static size_t linebuf_size = 0;
static uchar_t *linebuf = NULL;


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
   Image->ProcessedBytes = 0;
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
 * Decode 'buf' (an image line) into RGB format.
 */
static uchar_t *
 Image_line(DilloImage *Image, const uchar_t *buf, const uchar_t *cmap, int y)
{
   uint_t x;

   switch (Image->in_type) {
   case DILLO_IMG_TYPE_INDEXED:
      if (cmap) {
         for (x = 0; x < Image->width; x++)
            memcpy(linebuf + x * 3, cmap + buf[x] * 3, 3);
      } else {
         MSG("Gif:: WARNING, image lacks a color map\n");
      }
      break;
   case DILLO_IMG_TYPE_GRAY:
      for (x = 0; x < Image->width; x++)
         memset(linebuf + x * 3, buf[x], 3);
      break;
   case DILLO_IMG_TYPE_RGB:
      /* avoid a memcpy here!  --Jcid */
      return (uchar_t *)buf;
   case DILLO_IMG_TYPE_NOTSET:
      MSG_ERR("Image_line: type not set...\n");
      break;
   }
   return linebuf;
}

/*
 * Set initial parameters of the image
 */
void a_Image_set_parms(DilloImage *Image, void *v_imgbuf, DilloUrl *url,
                       int version, uint_t width, uint_t height,
                       DilloImgType type)
{
   _MSG("a_Image_set_parms: width=%d height=%d\n", width, height);

   OI(Image)->setBuffer((Imgbuf*)v_imgbuf);

   if (!Image->BitVec)
      Image->BitVec = a_Bitvec_new(height);
   Image->in_type = type;
   Image->width = width;
   Image->height = height;
   if (3 * width > linebuf_size) {
      linebuf_size = 3 * width;
      linebuf = (uchar_t*) dRealloc(linebuf, linebuf_size);
   }
   Image->State = IMG_SetParms;
}

/*
 * Reference the dicache entry color map
 */
void a_Image_set_cmap(DilloImage *Image, const uchar_t *cmap)
{
   Image->cmap = cmap;
   Image->State = IMG_SetCmap;
}

/*
 * Implement the write method
 */
void a_Image_write(DilloImage *Image, void *v_imgbuf,
                   const uchar_t *buf, uint_t y, int decode)
{
   uchar_t *newbuf;

   dReturn_if_fail ( y < Image->height );

   if (decode) {
      /* Decode 'buf' and copy it into the DicEntry buffer */
      newbuf = Image_line(Image, buf, Image->cmap, y);
      ((Imgbuf*)v_imgbuf)->copyRow(y, (byte *)newbuf);
   }
   a_Bitvec_set_bit(Image->BitVec, y);
   Image->State = IMG_Write;

   /* Update the row in DwImage */
   OI(Image)->drawRow(y);
}

/*
 * Implement the close method
 */
void a_Image_close(DilloImage *Image)
{
   a_Image_unref(Image);
}


// Wrappers for Imgbuf -------------------------------------------------------

/*
 * Increment reference count for an Imgbuf
 */
void a_Image_imgbuf_ref(void *v_imgbuf)
{
   ((Imgbuf*)v_imgbuf)->ref();
}

/*
 * Decrement reference count for an Imgbuf
 */
void a_Image_imgbuf_unref(void *v_imgbuf)
{
   ((Imgbuf*)v_imgbuf)->unref();
}

/*
 * Create a new Imgbuf
 */
void *a_Image_imgbuf_new(void *v_dw, int img_type, int width, int height)
{
   Layout *layout = ((Widget*)v_dw)->getLayout();
   if (!layout) {
      MSG_ERR("a_Image_imgbuf_new: layout is NULL.\n");
      exit(1);
   }
   return (void*)layout->createImgbuf(Imgbuf::RGB, width, height);
}

/*
 * Last reference for this Imgbuf?
 */
int a_Image_imgbuf_last_reference(void *v_imgbuf)
{
   return ((Imgbuf*)v_imgbuf)->lastReference () ? 1 : 0;
}

