/*
 * The png decoder for Dillo. It is responsible for decoding PNG data
 * and transferring it to the dicache.
 *
 * Geoff Lane nov 1999 zzassgl@twirl.mcc.ac.uk
 * Luca Rota, Jorge Arellano Cid, Eric Gaudet 2000
 * Jorge Arellano Cid 2009
 *
 * "PNG: The Definitive Guide" by Greg Roelofs, O'Reilly
 * ISBN 1-56592-542-4
 */

#include <config.h>
#ifdef ENABLE_PNG

#include <stdlib.h> /* For abort() */

#ifdef HAVE_LIBPNG_PNG_H
#include <libpng/png.h>
#else
#include <png.h>
#endif

#include "msg.h"
#include "image.hh"
#include "cache.h"
#include "dicache.h"

enum prog_state {
   IS_finished, IS_init, IS_nextdata
};

#if 0
static char *prog_state_name[] =
{
   "IS_finished", "IS_init", "IS_nextdata"
};
#endif

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
 * In the case of the libpng library, it maintains its own state in
 * png_ptr and into_ptr so the FSM is very simple - much simpler than the
 * ones for XBM and PNM are.
 */

typedef struct {
   DilloImage *Image;           /* Image meta data */
   DilloUrl *url;               /* Primary Key for the dicache */
   int version;                 /* Secondary Key for the dicache */
   int bgcolor;                 /* Parent widget background color */

   png_uint_32 width;           /* png image width */
   png_uint_32 height;          /* png image height */
   png_structp png_ptr;         /* libpng private data */
   png_infop info_ptr;          /* libpng private info */
   uchar_t *image_data;         /* decoded image data    */
   uchar_t **row_pointers;      /* pntr to row starts    */
   jmp_buf jmpbuf;              /* png error processing */
   int error;                   /* error flag */
   png_uint_32 previous_row;
   int rowbytes;                /* No. bytes in image row */
   short channels;              /* No. image channels */

/*
 * 0                                              last byte
 * +-------+-+-----------------------------------+-+
 * |       | |     -- data to be processed --    | |
 * +-------+-+-----------------------------------+-+
 * ^        ^                                     ^
 * ipbuf    ipbufstart                            ipbufsize
 */

   uchar_t *ipbuf;              /* image data in buffer */
   int ipbufstart;              /* first valid image byte */
   int ipbufsize;               /* size of valid data in */

   enum prog_state state;       /* FSM current state  */

   uchar_t *linebuf;            /* o/p raster data */

} DilloPng;

#define DATASIZE  (png->ipbufsize - png->ipbufstart)


static
void Png_error_handling(png_structp png_ptr, png_const_charp msg)
{
   DilloPng *png;

   MSG("Png_error_handling: %s\n", msg);
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
   double file_gamma = 1 / 2.2;

   _MSG("Png_datainfo_callback:\n");

   png = png_get_progressive_ptr(png_ptr);
   dReturn_if_fail (png != NULL);

   png_get_IHDR(png_ptr, info_ptr, &png->width, &png->height,
                &bit_depth, &color_type, &interlace_type, NULL, NULL);

   /* check max image size */
   if (png->width == 0 || png->height == 0 ||
       png->width > IMAGE_MAX_AREA / png->height) {
      MSG("Png_datainfo_callback: suspicious image size request %lu x %lu\n",
          (ulong_t) png->width, (ulong_t) png->height);
      Png_error_handling(png_ptr, "Aborting...");
      return; /* not reached */
   }

   _MSG("Png_datainfo_callback: png->width  = %lu\n"
        "Png_datainfo_callback: png->height = %lu\n",
        (ulong_t) png->width, (ulong_t) png->height);

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
      only work on PC's. TODO: select screen gamma correction for other
      platforms. */
   if (png_get_gAMA(png_ptr, info_ptr, &file_gamma))
      png_set_gamma(png_ptr, 2.2, file_gamma);

   /* Convert gray scale to RGB */
   if (color_type == PNG_COLOR_TYPE_GRAY ||
       color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
      png_set_gray_to_rgb(png_ptr);
   }

   /* Interlaced */
   if (interlace_type != PNG_INTERLACE_NONE) {
      png_set_interlace_handling(png_ptr);
   }

   /* get libpng to update its state */
   png_read_update_info(png_ptr, info_ptr);

   png_get_IHDR(png_ptr, info_ptr, &png->width, &png->height,
                &bit_depth, &color_type, &interlace_type, NULL, NULL);

   png->rowbytes = png_get_rowbytes(png_ptr, info_ptr);
   png->channels = png_get_channels(png_ptr, info_ptr);

   /* init Dillo specifics */
   _MSG("Png_datainfo_callback: rowbytes = %d\n"
        "Png_datainfo_callback: width    = %lu\n"
        "Png_datainfo_callback: height   = %lu\n",
        png->rowbytes, (ulong_t) png->width, (ulong_t) png->height);

   png->image_data = (uchar_t *) dMalloc(png->rowbytes * png->height);
   png->row_pointers = (uchar_t **) dMalloc(png->height * sizeof(uchar_t *));

   for (i = 0; i < png->height; i++)
      png->row_pointers[i] = png->image_data + (i * png->rowbytes);

   png->linebuf = dMalloc(3 * png->width);

   /* Initialize the dicache-entry here */
   a_Dicache_set_parms(png->url, png->version, png->Image,
                       (uint_t)png->width, (uint_t)png->height,
                       DILLO_IMG_TYPE_RGB, file_gamma);
   png->Image = NULL; /* safeguard: hereafter it may be freed by its owner */
}

static void
 Png_datarow_callback(png_structp png_ptr, png_bytep new_row,
                      png_uint_32 row_num, int pass)
{
   DilloPng *png;
   uint_t i;

   if (!new_row)                /* work to do? */
      return;

   _MSG("Png_datarow_callback: row_num = %ld\n", row_num);

   png = png_get_progressive_ptr(png_ptr);

   png_progressive_combine_row(png_ptr, png->row_pointers[row_num], new_row);

   _MSG("png: row_num=%u previous_row=%u\n", row_num, png->previous_row);
   if (row_num < png->previous_row) {
      a_Dicache_new_scan(png->url, png->version);
   }
   png->previous_row = row_num;

   switch (png->channels) {
   case 3:
      a_Dicache_write(png->url, png->version,
                      png->image_data + (row_num * png->rowbytes),
                      (uint_t)row_num);
      break;
   case 4:
     {
        /* TODO: get the backgound color from the parent
         * of the image widget -- Livio.                 */
        int a, bg_red, bg_green, bg_blue;
        uchar_t *pl = png->linebuf;
        uchar_t *data = png->image_data + (row_num * png->rowbytes);

        /* TODO: maybe change prefs.bg_color to `a_Dw_widget_get_bg_color`,
         * when background colors are correctly implementated */
        bg_blue  = (png->bgcolor) & 0xFF;
        bg_green = (png->bgcolor>>8) & 0xFF;
        bg_red   = (png->bgcolor>>16) & 0xFF;

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
        a_Dicache_write(png->url, png->version, png->linebuf, (uint_t)row_num);
        break;
     }
   default:
      MSG("Png_datarow_callback: unexpected number of channels=%d pass=%d\n",
          png->channels, pass);
      abort();
   }
}

static void Png_dataend_callback(png_structp png_ptr, png_infop info_ptr)
{
   DilloPng *png;

   _MSG("Png_dataend_callback:\n");
   if (!info_ptr)
      MSG("Png_dataend_callback: info_ptr = NULL\n");

   png = png_get_progressive_ptr(png_ptr);
   png->state = IS_finished;
}

/*
 * Free up the resources for this image.
 */
static void Png_free(DilloPng *png)
{
   _MSG("Png_free: png=%p\n", png);

   dFree(png->image_data);
   dFree(png->row_pointers);
   dFree(png->linebuf);
   if (setjmp(png->jmpbuf))
      MSG_WARN("PNG: can't destroy read structure\n");
   else if (png->png_ptr)
      png_destroy_read_struct(&png->png_ptr, &png->info_ptr, NULL);
   dFree(png);
}

/*
 * Finish the decoding process (and free the memory)
 */
static void Png_close(DilloPng *png, CacheClient_t *Client)
{
   _MSG("Png_close\n");
   /* Let dicache know decoding is over */
   a_Dicache_close(png->url, png->version, Client);
   Png_free(png);
}

/*
 * Receive and process new chunks of PNG image data
 */
static void Png_write(DilloPng *png, void *Buf, uint_t BufSize)
{
   dReturn_if_fail ( Buf != NULL && BufSize > 0 );

   /* Keep local copies so we don't have to pass multiple args to
    * a number of functions. */
   png->ipbuf = Buf;
   png->ipbufsize = BufSize;

   /* start/resume the FSM here */
   while (png->state != IS_finished && DATASIZE) {
      _MSG("State = %s\n", prog_state_name[png->state]);

      switch (png->state) {
      case IS_init:
         if (DATASIZE < 8) {
            return;            /* need MORE data */
         }
         /* check the image signature - DON'T update ipbufstart! */
         if (png_sig_cmp(png->ipbuf, 0, DATASIZE)) {
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
 * Op:  Operation to perform.
 *   If (Op == 0)
 *      start or continue processing an image if image data exists.
 *   else
 *       terminate processing, cleanup any allocated memory,
 *       close down the decoding process.
 *
 * Client->CbData  : pointer to previously allocated DilloPng work area.
 *  This holds the current state of the image processing and is kept
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
void a_Png_callback(int Op, void *data)
{
   if (Op == CA_Send) {
      CacheClient_t *Client = data;
      Png_write(Client->CbData, Client->Buf, Client->BufSize);
   } else if (Op == CA_Close) {
      CacheClient_t *Client = data;
      Png_close(Client->CbData, Client);
   } else if (Op == CA_Abort) {
      Png_free(data);
   }
}

/*
 * Create the image state data that must be kept between calls
 */
void *a_Png_new(DilloImage *Image, DilloUrl *url, int version)
{
   DilloPng *png = dNew0(DilloPng, 1);
   _MSG("a_Png_new: png=%p\n", png);

   png->Image = Image;
   png->url = url;
   png->version = version;
   png->bgcolor = Image->bg_color;
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

#else /* ENABLE_PNG */

void *a_Png_new() { return 0; }
void a_Png_callback() { return; }

#endif /* ENABLE_PNG */
