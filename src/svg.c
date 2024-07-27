/*
 * File: svg.c
 *
 * Copyright (C) 2024 Rodrigo Arias Mallo <rodarima@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include <config.h>

#ifdef ENABLE_SVG

#include <stdlib.h> /* For abort() */

#include "msg.h"
#include "image.hh"
#include "cache.h"
#include "dicache.h"

#define NANOSVG_ALL_COLOR_KEYWORDS
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"

typedef struct {
   DilloImage *Image;           /* Image meta data */
   DilloUrl *url;               /* Primary Key for the dicache */
   int version;                 /* Secondary Key for the dicache */
   int bgcolor;                 /* Parent widget background color */
   int fgcolor;                 /* Parent widget foreground color */
} DilloSvg;

/*
 * Free up the resources for this image.
 */
static void Svg_free(DilloSvg *svg)
{
   _MSG("Svg_free: svg=%p\n", svg);

   dFree(svg);
}

/*
 * Finish the decoding process (and free the memory)
 */
static void Svg_close(DilloSvg *svg, CacheClient_t *Client)
{
   _MSG("Svg_close\n");
   /* Let dicache know decoding is over */
   a_Dicache_close(svg->url, svg->version, Client);
   Svg_free(svg);
}

/*
 * Receive and process new chunks of SVG image data
 */
static void Svg_write(DilloSvg *svg, void *Buf, uint_t BufSize)
{
   static NSVGrasterizer *rasterizer = NULL;

   if (!rasterizer)
      rasterizer = nsvgCreateRasterizer();

   if (Buf == NULL || BufSize <= 0)
      return;

   /* SVG image not finished yet */
   if (strstr(Buf, "</svg>") == NULL)
      return;

   /* Use foreground as the current color, but transform to
    * nanosvg color format (BGR). */
   unsigned fg_r = (svg->fgcolor >> 16) & 0xff;
   unsigned fg_g = (svg->fgcolor >>  8) & 0xff;
   unsigned fg_b = (svg->fgcolor >>  0) & 0xff;
   unsigned curcolor = NSVG_RGB(fg_r, fg_g, fg_b);

   /* NULL-terminate Buf */
   char *str = dStrndup(Buf, BufSize);
   NSVGimage *nimg = nsvgParse(str, "px", svg->Image->dpi, curcolor);
   dFree(str);

   if (nimg == NULL) {
      MSG_ERR("Svg_write: cannot parse SVG image\n");
      return;
   }

   /* The height and width values can be nonintegral */
   unsigned width = nimg->width;
   unsigned height = nimg->height;

   if (width == 0 || height == 0 || width > IMAGE_MAX_AREA / height) {
      MSG_WARN("Svg_write: suspicious image size request %u x %u\n", width, height);
      nsvgDelete(nimg);
      return;
   }

   DilloImgType type = DILLO_IMG_TYPE_RGB;
   unsigned stride = width * 4;

   a_Dicache_set_parms(svg->url, svg->version, svg->Image,
         width, height, type, 1 / 2.2);

   unsigned char *dest = dNew(unsigned char, height * stride);

   nsvgRasterizeXY(rasterizer, nimg, 0, 0, 1, 1, dest, width, height, stride);

   unsigned bg_blue  = (svg->bgcolor) & 0xFF;
   unsigned bg_green = (svg->bgcolor >> 8) & 0xFF;
   unsigned bg_red   = (svg->bgcolor >> 16) & 0xFF;

   unsigned char *line = dNew(unsigned char, width * 3);
   for (unsigned i = 0; i < height; i++) {
      for (unsigned j = 0; j < width; j++) {
         unsigned r = dest[i * stride + 4 * j];
         unsigned g = dest[i * stride + 4 * j + 1];
         unsigned b = dest[i * stride + 4 * j + 2];
         unsigned alpha = dest[i * stride + 4 * j + 3];

         line[3 * j + 0] = (r * alpha + (bg_red   * (0xFF - alpha))) / 0xFF;
         line[3 * j + 1] = (g * alpha + (bg_green * (0xFF - alpha))) / 0xFF;
         line[3 * j + 2] = (b * alpha + (bg_blue  * (0xFF - alpha))) / 0xFF;
      }
      a_Dicache_write(svg->url, svg->version, line, i);
   }
   dFree(dest);
   dFree(line);
   nsvgDelete(nimg);
}

void a_Svg_callback(int Op, void *data)
{
   if (Op == CA_Send) {
      CacheClient_t *Client = data;
      Svg_write(Client->CbData, Client->Buf, Client->BufSize);
   } else if (Op == CA_Close) {
      CacheClient_t *Client = data;
      Svg_close(Client->CbData, Client);
   } else if (Op == CA_Abort) {
      Svg_free(data);
   }
}

/*
 * Create the image state data that must be kept between calls
 */
void *a_Svg_new(DilloImage *Image, DilloUrl *url, int version)
{
   DilloSvg *svg = dNew0(DilloSvg, 1);
   _MSG("a_Svg_new: svg=%p\n", svg);

   svg->Image = Image;
   svg->url = url;
   svg->version = version;
   svg->bgcolor = Image->bg_color;
   svg->fgcolor = Image->fg_color;

   return svg;
}

#else /* ENABLE_SVG */

void *a_Svg_new() { return 0; }
void a_Svg_callback() { return; }

#endif /* ENABLE_SVG */
