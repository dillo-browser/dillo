/*
 * The png decoder for Dillo. It is responsible for decoding PNG data
 * and transferring it to the dicache.
 *
 * Geoff Lane nov 1999 zzassgl@twirl.mcc.ac.uk
 * Luca Rota, Jorge Arellano Cid, Eric Gaudet 2000
 *
 * "PNG: The Definitive Guide" by Greg Roelofs, O'Reilly
 * ISBN 1-56592-542-4
 */

#include <config.h>
#ifdef ENABLE_PNG

#include <stdio.h>
#include <string.h>
#include <stdlib.h> /* For abort() */

#include <zlib.h>

#ifdef HAVE_LIBPNG_PNG_H
#include <libpng/png.h>
#else
#include <png.h>
#endif

#include "msg.h"
#include "image.hh"
#include "web.hh"
#include "cache.h"
#include "dicache.h"
#include "prefs.h"

#define DEBUG_LEVEL 6
#include "debug.h"

enum prog_state {
   IS_finished, IS_init, IS_nextdata
};

static char *prog_state_name[] =
{
   "IS_finished", "IS_init", "IS_nextdata"
};

/*
 * This holds the data that must be saved between calls to this module.
 * Each time it is called it is supplied with a vector of data bytes
 * obtained from the web server. The module can process any amount of the
 * supplied data.  The next time the module is called, the vector may be
 * extended with additional data bytes to be processed.  The module must
 * keep track of the current start and cursor position of the input data
 * vector.  As complete output rasters are determined they are sent out of
 * the module for additional processing.
 *
 * NOTE:  There is no external control of the splitting of the input data
 * vector (only this module understands PNG format data.) This means that
 * the complete state of a PNG image being processed must be held in the
 * structure below so that processing can be suspended or resumed at any
 * point within an input image.
 *
 * In the case of the libpng library, it maintains it's own state in
 * png_ptr and into_ptr so the FSM is very simple - much simpler than the
 * ones for XBM and PNM are.
 */

typedef
struct _DilloPng {
   DilloImage *Image;           /* Image meta data */
   DilloUrl *url;               /* Primary Key for the dicache */
   int version;                /* Secondary Key for the dicache */

   double display_exponent;     /* gamma correction */
   ulong_t width;                /* png image width */
   ulong_t height;               /* png image height */
   png_structp png_ptr;         /* libpng private data */
   png_infop info_ptr;          /* libpng private info */
   uchar_t *image_data;          /* decoded image data    */
   uchar_t **row_pointers;       /* pntr to row starts    */
   jmp_buf jmpbuf;              /* png error processing */
   int error;                  /* error flag */
   png_uint_32 previous_row;
   int rowbytes;               /* No. bytes in image row */
   short passes;
   short channels;              /* No. image channels */

/*
 * 0                                              last byte
 * +-------+-+-----------------------------------+-+
 * |       | |     -- data to be processed --    | |
 * +-------+-+-----------------------------------+-+
 * ^        ^                                     ^
 * ipbuf    ipbufstart                            ipbufsize
 */

   uchar_t *ipbuf;               /* image data in buffer */
   int ipbufstart;             /* first valid image byte */
   int ipbufsize;              /* size of valid data in */

   enum prog_state state;       /* FSM current state  */

   uchar_t *linebuf;             /* o/p raster data */

} DilloPng;

#define DATASIZE  (png->ipbufsize - png->ipbufstart)
#define BLACK     0
#define WHITE     255

/*
 * Forward declarations
 */
/* exported function */
void *a_Png_image(const char *Type, void *Ptr, CA_Callback_t *Call,
                  void **Data);


static
void Png_error_handling(png_structp png_ptr, png_const_charp msg)
{
   DilloPng *png;

   DEBUG_MSG(6, "Png_error_handling: %s\n", msg);
   png = png_get_error_ptr(png_ptr);

   png->error = 1;
   png->state = IS_finished;

   longjmp(png->jmpbuf, 1);
}

static void
Png_datainfo_callback(png_structp png_ptr, png_infop info_ptr)
{
   DilloPng *png;
   int color_type;
   int bit_depth;
   int interlace_type;
   uint_t i;
   double gamma;

   DEBUG_MSG(5, "Png_datainfo_callback:\n");

   png = png_get_progressive_ptr(png_ptr);
   dReturn_if_fail (png != NULL);

   png_get_IHDR(png_ptr, info_ptr, &png->width, &png->height,
                &bit_depth, &color_type, &interlace_type, NULL, NULL);

   DEBUG_MSG(5, "Png_datainfo_callback: png->width  = %ld\n"
             "Png_datainfo_callback: png->height = %ld\n",
             png->width, png->height);

   /* we need RGB/RGBA in the end */
   if (color_type == PNG_COLOR_TYPE_PALETTE && bit_depth <= 8) {
      /* Convert indexed images to RGB */
      png_set_expand (png_ptr);
   } else if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
      /* Convert grayscale to RGB */
      png_set_expand (png_ptr);
   } else if (png_get_valid (png_ptr, info_ptr, PNG_INFO_tRNS)) {
      /* We have transparency header, convert it to alpha channel */
      png_set_expand(png_ptr);
   } else if (bit_depth < 8) {
      png_set_expand(png_ptr);
   }

   if (bit_depth == 16) {
      png_set_strip_16(png_ptr);
   }

   /* Get and set gamma information. Beware: gamma correction 2.2 will
      only work on PC's. todo: select screen gamma correction for other
      platforms. */
   if (png_get_gAMA(png_ptr, info_ptr, &gamma))
      png_set_gamma(png_ptr, 2.2, gamma);

   /* Convert gray scale to RGB */
   if (color_type == PNG_COLOR_TYPE_GRAY ||
       color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
      png_set_gray_to_rgb(png_ptr);
   }

   /* Interlaced */
   if (interlace_type != PNG_INTERLACE_NONE) {
      png->passes = png_set_interlace_handling(png_ptr);
   }

   /* get libpng to update it's state */
   png_read_update_info(png_ptr, info_ptr);

   png_get_IHDR(png_ptr, info_ptr, &png->width, &png->height,
                &bit_depth, &color_type, &interlace_type, NULL, NULL);

   png->rowbytes = png_get_rowbytes(png_ptr, info_ptr);
   png->channels = png_get_channels(png_ptr, info_ptr);

   /* init Dillo specifics */
   DEBUG_MSG(5, "Png_datainfo_callback: rowbytes = %d\n"
             "Png_datainfo_callback: width    = %ld\n"
             "Png_datainfo_callback: height   = %ld\n",
             png->rowbytes, png->width, png->height);

   png->image_data = (uchar_t *) dMalloc(png->rowbytes * png->height);
   png->row_pointers = (uchar_t **) dMalloc(png->height * sizeof(uchar_t *));

   for (i = 0; i < png->height; i++)
      png->row_pointers[i] = png->image_data + (i * png->rowbytes);

   png->linebuf = dMalloc(3 * png->width);

   /* Initialize the dicache-entry here */
   a_Dicache_set_parms(png->url, png->version, png->Image,
                       (uint_t)png->width, (uint_t)png->height,
                       DILLO_IMG_TYPE_RGB);
}

static void
 Png_datarow_callback(png_structp png_ptr, png_bytep new_row,
                      png_uint_32 row_num, int pass)
{
   DilloPng *png;
   uint_t i;

   if (!new_row)                /* work to do? */
      return;

   DEBUG_MSG(5, "Png_datarow_callback: row_num = %ld\n", row_num);

   png = png_get_progressive_ptr(png_ptr);

   png_progressive_combine_row(png_ptr, png->row_pointers[row_num], new_row);

   if (row_num < png->previous_row) {
      a_Dicache_new_scan(png->Image, png->url, png->version);
   }
   png->previous_row = row_num;

   switch (png->channels) {
   case 3:
      a_Dicache_write(png->Image, png->url, png->version,
                      png->image_data + (row_num * png->rowbytes),
                      (uint_t)row_num);
      break;
   case 4:
     {
        /* todo: get the backgound color from the parent
         * of the image widget -- Livio.                 */
        int a, bg_red, bg_green, bg_blue;
        uchar_t *pl = png->linebuf;
        uchar_t *data = png->image_data + (row_num * png->rowbytes);

        /* todo: maybe change prefs.bg_color to `a_Dw_widget_get_bg_color`,
         * when background colors are correctly implementated */
        bg_blue  = (png->Image->bg_color) & 0xFF;
        bg_green = (png->Image->bg_color>>8) & 0xFF;
        bg_red   = (png->Image->bg_color>>16) & 0xFF;

        for (i = 0; i < png->width; i++) {
           a = *(data+3);

           if (a == 255) {
              *(pl++) = *(data++);
              *(pl++) = *(data++);
              *(pl++) = *(data++);
              data++;
           } else if (a == 0) {
              *(pl++) = bg_red;
              *(pl++) = bg_green;
              *(pl++) = bg_blue;
              data += 4;
           } else {
              png_composite(*(pl++), *(data++), a, bg_red);
              png_composite(*(pl++), *(data++), a, bg_green);
              png_composite(*(pl++), *(data++), a, bg_blue);
              data++;
           }
        }
        a_Dicache_write(png->Image, png->url, png->version,
                        png->linebuf, (uint_t)row_num);
        break;
     }
   default:
      MSG("Png_datarow_callback: unexpected number of channels = %d\n",
          png->channels);
      abort();
   }
}

static void
 Png_dataend_callback(png_structp png_ptr, png_infop info_ptr)
{
   DilloPng *png;

   DEBUG_MSG(5, "Png_dataend_callback:\n");

   png = png_get_progressive_ptr(png_ptr);
   png->state = IS_finished;
}


/*
 * Op:  Operation to perform.
 *   If (Op == 0)
 *      start or continue processing an image if image data exists.
 *   else
 *       terminate processing, cleanup any allocated memory,
 *       close down the decoding process.
 *
 * Client->CbData  : pointer to previously allocated DilloPng work area.
 *  This holds the current state of the image processing and is saved
 *  across calls to this routine.
 * Client->Buf     : Pointer to data start.
 * Client->BufSize : the size of the data buffer.
 *
 * You have to keep track of where you are in the image data and
 * how much has been processed.
 *
 * It's entirely possible that you will not see the end of the data.  The
 * user may terminate transfer via a Stop button or there may be a network
 * failure.  This means that you can't just wait for all the data to be
 * presented before starting conversion and display.
 */
static void Png_callback(int Op, CacheClient_t *Client)
{
   DilloPng *png = Client->CbData;

   if (Op) {
      /* finished - free up the resources for this image */
      a_Dicache_close(png->url, png->version, Client);
      dFree(png->image_data);
      dFree(png->row_pointers);
      dFree(png->linebuf);

      if (setjmp(png->jmpbuf))
         MSG_WARN("PNG: can't destroy read structure\n");
      else if (png->png_ptr)
         png_destroy_read_struct(&png->png_ptr, &png->info_ptr, NULL);
      dFree(png);
      return;
   }

   /* Let's make some sound if we have been called with no data */
   dReturn_if_fail ( Client->Buf != NULL && Client->BufSize > 0 );

   DEBUG_MSG(5, "Png_callback BufSize = %d\n", Client->BufSize);

   /* Keep local copies so we don't have to pass multiple args to
    * a number of functions. */
   png->ipbuf = Client->Buf;
   png->ipbufsize = Client->BufSize;

   /* start/resume the FSM here */
   while (png->state != IS_finished && DATASIZE) {
      DEBUG_MSG(5, "State = %s\n", prog_state_name[png->state]);

      switch (png->state) {
      case IS_init:
         if (DATASIZE < 8) {
            return;            /* need MORE data */
         }
         /* check the image signature - DON'T update ipbufstart! */
         if (!png_check_sig(png->ipbuf, DATASIZE)) {
            /* you lied to me about it being a PNG image */
            MSG_WARN("\"%s\" is not a PNG file.\n", URL_STR(png->url));
            png->state = IS_finished;
            break;
         }
         /* OK, it looks like a PNG image, lets do some set up stuff */
         png->png_ptr = png_create_read_struct(
                           PNG_LIBPNG_VER_STRING,
                           png,
                           (png_error_ptr)Png_error_handling,
                           (png_error_ptr)Png_error_handling);
         dReturn_if_fail (png->png_ptr != NULL);
         png->info_ptr = png_create_info_struct(png->png_ptr);
         dReturn_if_fail (png->info_ptr != NULL);

         setjmp(png->jmpbuf);
         if (!png->error) {
            png_set_progressive_read_fn(
               png->png_ptr,
               png,
               Png_datainfo_callback,   /* performs local init functions */
               Png_datarow_callback,    /* performs per row action */
               Png_dataend_callback);   /* performs cleanup actions */
            png->state = IS_nextdata;
         }
         break;

      case IS_nextdata:
         if (setjmp(png->jmpbuf)) {
            png->state = IS_finished;
         } else if (!png->error) {
            png_process_data( png->png_ptr,
                              png->info_ptr,
                              png->ipbuf + png->ipbufstart,
                              (png_size_t)DATASIZE);

            png->ipbufstart += DATASIZE;
         }
         break;

      default:
         MSG_WARN("PNG decoder: bad state = %d\n", png->state);
         abort();
      }
   }
}

/*
 * Create the image state data that must be kept between calls
 */
static DilloPng *Png_new(DilloImage *Image, DilloUrl *url, int version)
{
   DilloPng *png = dNew0(DilloPng, 1);

   png->Image = Image;
   png->url = url;
   png->version = version;
   png->error = 0;
   png->ipbuf = NULL;
   png->ipbufstart = 0;
   png->ipbufsize = 0;
   png->state = IS_init;
   png->linebuf = NULL;
   png->image_data = NULL;
   png->row_pointers = NULL;
   png->previous_row = 0;

   return png;
}

/*
 * MIME handler for "image/png" type
 * (Sets Png_callback or a_Dicache_callback as the cache-client)
 */
void *a_Png_image(const char *Type, void *Ptr, CA_Callback_t *Call,
                  void **Data)
{
/*
 * Type: MIME type
 * Ptr:  points to a Web structure
 * Call: Dillo calls this with more data/eod
 * Data: raw image data
 */

   DilloWeb *web = Ptr;
   DICacheEntry *DicEntry;

   DEBUG_MSG(5, "a_Png_image: Type = %s\n"
             "a_Png_image: libpng - Compiled %s; using %s.\n"
             "a_Png_image: zlib   - Compiled %s; using %s.\n",
             Type, PNG_LIBPNG_VER_STRING, png_libpng_ver,
             ZLIB_VERSION, zlib_version);

   if (!web->Image)
      web->Image = a_Image_new(0, 0, NULL, prefs.bg_color);

   /* Add an extra reference to the Image (for dicache usage) */
   a_Image_ref(web->Image);

   DicEntry = a_Dicache_get_entry(web->url);
   if (!DicEntry) {
      /* Let's create an entry for this image... */
      DicEntry = a_Dicache_add_entry(web->url);

      /* ... and let the decoder feed it! */
      *Data = Png_new(web->Image, DicEntry->url, DicEntry->version);
      *Call = (CA_Callback_t) Png_callback;
   } else {
      /* Let's feed our client from the dicache */
      a_Dicache_ref(DicEntry->url, DicEntry->version);
      *Data = web->Image;
      *Call = (CA_Callback_t) a_Dicache_callback;
   }
   return (web->Image->dw);
}

#endif /* ENABLE_PNG */
