/*
 * File: imgbuf.cc
 *
 * Copyright (C) 2008 Jorge Arellano Cid <jcid@dillo.org>,
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include "msg.h"
#include "imgbuf.hh"
#include "dw/core.hh"
#include "dw/image.hh"

using namespace dw::core;

/*
 * Local data
 */
static size_t linebuf_size = 0;
static uchar_t *linebuf = NULL;


/*
 * Decode 'buf' (an image line) into RGB format.
 */
static uchar_t *Imgbuf_rgb_line(const uchar_t *buf,
                                DilloImgType type, uchar_t *cmap,
                                uint_t width, uint_t y)
{
   uint_t x;

   switch (type) {
   case DILLO_IMG_TYPE_INDEXED:
      if (cmap) {
         for (x = 0; x < width; x++)
            memcpy(linebuf + x * 3, cmap + buf[x] * 3, 3);
      } else {
         MSG_WARN("Gif:: image lacks a color map\n");
      }
      break;
   case DILLO_IMG_TYPE_GRAY:
      for (x = 0; x < width; x++)
         memset(linebuf + x * 3, buf[x], 3);
      break;
   case DILLO_IMG_TYPE_CMYK_INV:
      /*
       * We treat CMYK as if it were "RGBW", and it works. Everyone who is
       * trying to handle CMYK jpegs is confused by this, and supposedly
       * the issue is that Adobe CMYK is "wrong" but ubiquitous.
       */
      for (x = 0; x < width; x++) {
         uint_t white = buf[x * 4 + 3];
         linebuf[x * 3] = buf[x * 4] * white / 0x100;
         linebuf[x * 3 + 1] = buf[x * 4 + 1] * white / 0x100;
         linebuf[x * 3 + 2] = buf[x * 4 + 2] * white / 0x100;
      }
      break;
   case DILLO_IMG_TYPE_RGB:
      /* avoid a memcpy here!  --Jcid */
      return (uchar_t *)buf;
   case DILLO_IMG_TYPE_NOTSET:
      MSG_ERR("Imgbuf_rgb_line: type not set...\n");
      break;
   }
   return linebuf;
}

// Wrappers for Imgbuf -------------------------------------------------------

/*
 * Increment reference count for an Imgbuf
 */
void a_Imgbuf_ref(void *v_imgbuf)
{
   ((Imgbuf*)v_imgbuf)->ref();
}

/*
 * Decrement reference count for an Imgbuf
 */
void a_Imgbuf_unref(void *v_imgbuf)
{
   if (v_imgbuf)
      ((Imgbuf*)v_imgbuf)->unref();
}

/*
 * Create a new Imgbuf
 */
void *a_Imgbuf_new(void *layout, int img_type, uint_t width, uint_t height,
                   double gamma)
{
   if (!layout) {
      MSG_ERR("a_Imgbuf_new: layout is NULL.\n");
      exit(1);
   }
   // Assert linebuf is wide enough.
   if (3 * width > linebuf_size) {
      linebuf_size = 3 * width;
      linebuf = (uchar_t*) dRealloc(linebuf, linebuf_size);
   }

   return (void*)((Layout*)layout)->createImgbuf(Imgbuf::RGB, width, height,
                                                 gamma);
}

/*
 * Last reference for this Imgbuf?
 */
int a_Imgbuf_last_reference(void *v_imgbuf)
{
   return ((Imgbuf*)v_imgbuf)->lastReference () ? 1 : 0;
}

/*
 * Update the root buffer of an imgbuf.
 */
void a_Imgbuf_update(void *v_imgbuf, const uchar_t *buf, DilloImgType type,
                     uchar_t *cmap, uint_t width, uint_t height, uint_t y)

{
   dReturn_if_fail ( y < height );

   /* Decode 'buf' and copy it into the imgbuf */
   uchar_t *newbuf = Imgbuf_rgb_line(buf, type, cmap, width, y);
   ((Imgbuf*)v_imgbuf)->copyRow(y, (byte *)newbuf);
}

/*
 * Reset for a new scan from a multiple-scan image.
 */
void a_Imgbuf_new_scan(void *v_imgbuf)
{
   ((Imgbuf*)v_imgbuf)->newScan();
}

