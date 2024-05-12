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
   uint_t i, j, width, height;
   static NSVGrasterizer *rasterizer = NULL;

   if (!rasterizer)
      rasterizer = nsvgCreateRasterizer();

   dReturn_if_fail ( Buf != NULL && BufSize > 0 );
   dReturn_if_fail (strstr(Buf, "</svg>"));

   char *str = dStrndup(Buf, BufSize); /* 1. nsvg is said to modify the string. 2. it needs it null-terminated, and I don't remember right now whether Buf happens to be */

   /* TODO: Use FLTK reported DPI value */
   NSVGimage *nimg = nsvgParse(str, "px", 96);
   dFree(str);

   /* guess what? The height and width values that come back can be nonintegral */
   width = ceil(nimg->width);
   height = ceil(nimg->height);

   if (width == 0 || height == 0 || width > IMAGE_MAX_AREA / height) {
      MSG_WARN("Svg_write: suspicious image size request %u x %u\n", width, height);
      nsvgDelete(nimg);
      return;
   }

   const DilloImgType type = DILLO_IMG_TYPE_RGB;
   const uint_t stride = width * 4;

   a_Dicache_set_parms(svg->url, svg->version, svg->Image, width, height,
                       type, 1 / 2.2);

   /* is there any sort of gamma to deal with at all? Not aware of any. */

   unsigned char *dest = dNew(unsigned char, height * stride);

   nsvgRasterizeXY(rasterizer, nimg, 0, 0, 1, 1, dest, width, height, stride);

       uint_t bg_blue  = (svg->bgcolor) & 0xFF;
       uint_t bg_green = (svg->bgcolor>>8) & 0xFF;
       uint_t bg_red   = (svg->bgcolor>>16) & 0xFF;

   unsigned char *line = dNew(unsigned char, width * 3);
   for (i = 0; i < height; i++) {
      for (j = 0; j < width; j++) {
         uchar_t r = dest[i * stride + 4 * j];
         uchar_t g = dest[i * stride + 4 * j + 1];
         uchar_t b = dest[i * stride + 4 * j + 2];
         uchar_t alpha = dest[i * stride + 4 * j + 3];

         if (i * stride + 4 * j + 3 >= height * stride) {
            MSG_ERR("height %f i %d width %f j %d stride %d size %d here %d\n", nimg->height, i, nimg->width, j, stride, height * stride, i * stride + 4 * j + 3);
         }

         line[3 * j] = (r * alpha + (bg_red * (0xFF - alpha))) / 0xFF;
         line[3 * j + 1] = (g * alpha + (bg_green * (0xFF - alpha))) / 0xFF;
         line[3 * j + 2] = (b * alpha + (bg_blue * (0xFF - alpha))) / 0xFF;
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

   return svg;
}

#else /* ENABLE_SVG */

void *a_Svg_new() { return 0; }
void a_Svg_callback() { return; }

#endif /* ENABLE_SVG */
