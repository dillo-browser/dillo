/*
 * File: jpeg.c
 *
 * Copyright (C) 1997 Raph Levien <raph@acm.org>
 * Copyright (C) 2000-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

/*
 * The jpeg decoder for dillo. It is responsible for decoding JPEG data
 * and transferring it to the dicache. It uses libjpeg to do the actual
 * decoding.
 */

#include <config.h>
#ifdef ENABLE_JPEG

#include <stdio.h>
#include <setjmp.h>

/* avoid a redefinition of HAVE_STDLIB_H with old jpeglib.h */
#ifdef HAVE_STDLIB_H
#  undef HAVE_STDLIB_H
#endif
#include <jpeglib.h>
#ifdef HAVE_STDLIB_H
#  undef HAVE_STDLIB_H
#endif

#include "image.hh"
#include "web.hh"
#include "cache.h"
#include "dicache.h"
#include "capi.h"       /* get cache entry status */

#define DEBUG_LEVEL 6
#include "debug.h"

typedef enum {
   DILLO_JPEG_INIT,
   DILLO_JPEG_STARTING,
   DILLO_JPEG_READ_BEGIN_SCAN,
   DILLO_JPEG_READ_IN_SCAN,
   DILLO_JPEG_READ_END_SCAN,
   DILLO_JPEG_DONE,
   DILLO_JPEG_ERROR
} DilloJpegState;

/* An implementation of a suspending source manager */

typedef struct {
   struct jpeg_source_mgr pub;  /* public fields */
   struct DilloJpeg *jpeg;      /* a pointer back to the jpeg object */
} my_source_mgr;

struct my_error_mgr {
   struct jpeg_error_mgr pub;    /* "public" fields */
   jmp_buf setjmp_buffer;        /* for return to caller */
};
typedef struct my_error_mgr * my_error_ptr;

typedef struct DilloJpeg {
   DilloImage *Image;
   DilloUrl *url;
   int version;

   my_source_mgr Src;

   DilloJpegState state;
   size_t Start_Ofs, Skip, NewStart;
   char *Data;

   uint_t y;

   struct jpeg_decompress_struct cinfo;
   struct my_error_mgr jerr;
} DilloJpeg;

/*
 * Forward declarations
 */
static DilloJpeg *Jpeg_new(DilloImage *Image, DilloUrl *url, int version);
static void Jpeg_callback(int Op, CacheClient_t *Client);
static void Jpeg_write(DilloJpeg *jpeg, void *Buf, uint_t BufSize);
static void Jpeg_close(DilloJpeg *jpeg, CacheClient_t *Client);
METHODDEF(void) Jpeg_errorexit (j_common_ptr cinfo);

/* exported function */
void *a_Jpeg_image(const char *Type, void *P, CA_Callback_t *Call,
                   void **Data);


/* this is the routine called by libjpeg when it detects an error. */
METHODDEF(void) Jpeg_errorexit (j_common_ptr cinfo)
{
   /* display message and return to setjmp buffer */
   my_error_ptr myerr = (my_error_ptr) cinfo->err;
   (*cinfo->err->output_message) (cinfo);
   longjmp(myerr->setjmp_buffer, 1);
}

/*
 * MIME handler for "image/jpeg" type
 * (Sets Jpeg_callback or a_Dicache_callback as the cache-client)
 */
void *a_Jpeg_image(const char *Type, void *P, CA_Callback_t *Call,
                   void **Data)
{
   DilloWeb *web = P;
   DICacheEntry *DicEntry;

   if (!web->Image)
      web->Image = a_Image_new(0, 0, NULL, 0);

   /* Add an extra reference to the Image (for dicache usage) */
   a_Image_ref(web->Image);

   DicEntry = a_Dicache_get_entry(web->url);
   if (!DicEntry) {
      /* Let's create an entry for this image... */
      DicEntry = a_Dicache_add_entry(web->url);

      /* ... and let the decoder feed it! */
      *Data = Jpeg_new(web->Image, DicEntry->url, DicEntry->version);
      *Call = (CA_Callback_t) Jpeg_callback;
   } else {
      /* Let's feed our client from the dicache */
      a_Dicache_ref(DicEntry->url, DicEntry->version);
      *Data = web->Image;
      *Call = (CA_Callback_t) a_Dicache_callback;
   }
   return (web->Image->dw);
}

/*
 * Finish the decoding process
 */
static void Jpeg_close(DilloJpeg *jpeg, CacheClient_t *Client)
{
   a_Dicache_close(jpeg->url, jpeg->version, Client);
   jpeg_destroy_decompress(&(jpeg->cinfo));
   dFree(jpeg);
}

static void init_source(j_decompress_ptr cinfo)
{
}

static boolean fill_input_buffer(j_decompress_ptr cinfo)
{
   DilloJpeg *jpeg = ((my_source_mgr *) cinfo->src)->jpeg;

   DEBUG_MSG(5, "fill_input_buffer\n");
#if 0
   if (!cinfo->src->bytes_in_buffer) {
      DEBUG_MSG(5, "fill_input_buffer: %ld bytes in buffer\n",
                (long)cinfo->src->bytes_in_buffer);

      jpeg->Start_Ofs = (ulong_t) jpeg->cinfo.src->next_input_byte -
         (ulong_t) jpeg->Data;
#endif
      if (jpeg->Skip) {
         jpeg->Start_Ofs = jpeg->NewStart + jpeg->Skip - 1;
         jpeg->Skip = 0;
      } else {
         jpeg->Start_Ofs = (ulong_t) jpeg->cinfo.src->next_input_byte -
            (ulong_t) jpeg->Data;
      }
      return FALSE;
#if 0
   }
   return TRUE;
#endif
}

static void skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
   DilloJpeg *jpeg;

   if (num_bytes < 1)
      return;
   jpeg = ((my_source_mgr *) cinfo->src)->jpeg;

   DEBUG_MSG(5, "skip_input_data: Start_Ofs = %lu, num_bytes = %ld,"
             " %ld bytes in buffer\n",
             (ulong_t)jpeg->Start_Ofs, num_bytes,
             (long)cinfo->src->bytes_in_buffer);

   cinfo->src->next_input_byte += num_bytes;
   if (num_bytes < (long)cinfo->src->bytes_in_buffer) {
      cinfo->src->bytes_in_buffer -= num_bytes;
   } else {
      jpeg->Skip += num_bytes - cinfo->src->bytes_in_buffer + 1;
      cinfo->src->bytes_in_buffer = 0;
   }
}

static void term_source(j_decompress_ptr cinfo)
{
}

static DilloJpeg *Jpeg_new(DilloImage *Image, DilloUrl *url, int version)
{
   my_source_mgr *src;
   DilloJpeg *jpeg = dMalloc(sizeof(*jpeg));

   jpeg->Image = Image;
   jpeg->url = url;
   jpeg->version = version;

   jpeg->state = DILLO_JPEG_INIT;
   jpeg->Start_Ofs = 0;
   jpeg->Skip = 0;

   /* decompression step 1 (see libjpeg.doc) */
   jpeg->cinfo.err = jpeg_std_error(&(jpeg->jerr.pub));
   jpeg->jerr.pub.error_exit = Jpeg_errorexit;

   jpeg_create_decompress(&(jpeg->cinfo));

   /* decompression step 2 (see libjpeg.doc) */
   jpeg->cinfo.src = &jpeg->Src.pub;
   src = &jpeg->Src;
   src->pub.init_source = init_source;
   src->pub.fill_input_buffer = fill_input_buffer;
   src->pub.skip_input_data = skip_input_data;
   src->pub.resync_to_restart = jpeg_resync_to_restart;/* use default method */
   src->pub.term_source = term_source;
   src->pub.bytes_in_buffer = 0;   /* forces fill_input_buffer on first read */
   src->pub.next_input_byte = NULL;/* until buffer loaded */

   src->jpeg = jpeg;

   /* decompression steps continue in write method */
   return jpeg;
}

static void Jpeg_callback(int Op, CacheClient_t *Client)
{
   if (Op)
      Jpeg_close(Client->CbData, Client);
   else
      Jpeg_write(Client->CbData, Client->Buf, Client->BufSize);
}

/*
 * Receive and process new chunks of JPEG image data
 */
static void Jpeg_write(DilloJpeg *jpeg, void *Buf, uint_t BufSize)
{
   DilloImgType type;
   uchar_t *linebuf;
   JSAMPLE *array[1];
   int num_read;

   DEBUG_MSG(5, "Jpeg_write: (%p) Bytes in buff: %ld Ofs: %lu\n", jpeg,
             (long) BufSize, (ulong_t)jpeg->Start_Ofs);

   /* See if we are supposed to skip ahead. */
   if (BufSize <= jpeg->Start_Ofs)
      return;

   /* Concatenate with the partial input, if any. */
   jpeg->cinfo.src->next_input_byte = (uchar_t *)Buf + jpeg->Start_Ofs;
   jpeg->cinfo.src->bytes_in_buffer = BufSize - jpeg->Start_Ofs;
   jpeg->NewStart = BufSize;
   jpeg->Data = Buf;

   if (setjmp(jpeg->jerr.setjmp_buffer)) {
      /* If we get here, the JPEG code has signaled an error. */
      jpeg->state = DILLO_JPEG_ERROR;
   }

   /* Process the bytes in the input buffer. */
   if (jpeg->state == DILLO_JPEG_INIT) {

      /* decompression step 3 (see libjpeg.doc) */
      if (jpeg_read_header(&(jpeg->cinfo), TRUE) != JPEG_SUSPENDED) {
         type = DILLO_IMG_TYPE_GRAY;
         if (jpeg->cinfo.num_components == 1)
            type = DILLO_IMG_TYPE_GRAY;
         else if (jpeg->cinfo.num_components == 3)
            type = DILLO_IMG_TYPE_RGB;
         else
            DEBUG_MSG(5, "jpeg: can't handle %d component images\n",
                      jpeg->cinfo.num_components);

         /*
          * If a multiple-scan image is not completely in cache,
          * use progressive display, updating as it arrives.
          */
         if (jpeg_has_multiple_scans(&jpeg->cinfo) &&
             !(a_Capi_get_flags(jpeg->url) & CAPI_Completed))
            jpeg->cinfo.buffered_image = TRUE;

         a_Dicache_set_parms(jpeg->url, jpeg->version, jpeg->Image,
                             (uint_t)jpeg->cinfo.image_width,
                             (uint_t)jpeg->cinfo.image_height,
                             type);

         /* decompression step 4 (see libjpeg.doc) */
         jpeg->state = DILLO_JPEG_STARTING;
      }
   }
   if (jpeg->state == DILLO_JPEG_STARTING) {
      /* decompression step 5 (see libjpeg.doc) */
      if (jpeg_start_decompress(&(jpeg->cinfo))) {
         jpeg->y = 0;
         jpeg->state = jpeg->cinfo.buffered_image ?
                          DILLO_JPEG_READ_BEGIN_SCAN : DILLO_JPEG_READ_IN_SCAN;
      }
   }

   /*
    * A progressive jpeg contains multiple scans that can be used to display
    * an increasingly sharp image as it is being received. The reading of each
    * scan must be surrounded by jpeg_start_output()/jpeg_finish_output().
    */

   if (jpeg->state == DILLO_JPEG_READ_END_SCAN) {
      if (jpeg_finish_output(&jpeg->cinfo)) {
         if (jpeg_input_complete(&jpeg->cinfo)) {
            jpeg->state = DILLO_JPEG_DONE;
         } else {
            jpeg->state = DILLO_JPEG_READ_BEGIN_SCAN;
         }
      }
   }

   if (jpeg->state == DILLO_JPEG_READ_BEGIN_SCAN) {
      if (jpeg_start_output(&jpeg->cinfo, jpeg->cinfo.input_scan_number)) {
         a_Dicache_new_scan(jpeg->Image, jpeg->url, jpeg->version);
         jpeg->state = DILLO_JPEG_READ_IN_SCAN;
      }
   }

   if (jpeg->state == DILLO_JPEG_READ_IN_SCAN) {
      linebuf = dMalloc(jpeg->cinfo.image_width *
                         jpeg->cinfo.num_components);
      array[0] = linebuf;

      while (1) {
         num_read = jpeg_read_scanlines(&(jpeg->cinfo), array, 1);
         if (num_read == 0) {
            /* out of input */
            break;
         }
         a_Dicache_write(jpeg->Image, jpeg->url, jpeg->version,
                         linebuf, 0, jpeg->y);

         jpeg->y++;

         if (jpeg->y == jpeg->cinfo.image_height) {
            /* end of scan */
            if (!jpeg->cinfo.buffered_image) {
               /* single scan */
               jpeg->state = DILLO_JPEG_DONE;
               break;
            } else {
               jpeg->y = 0;
               if (jpeg_input_complete(&jpeg->cinfo)) {
                  if (jpeg->cinfo.input_scan_number ==
                      jpeg->cinfo.output_scan_number) {
                     jpeg->state = DILLO_JPEG_DONE;
                     break;
                  } else {
                       /* one final loop through the scanlines */
                       jpeg_finish_output(&jpeg->cinfo);
                       jpeg_start_output(&jpeg->cinfo,
                                         jpeg->cinfo.input_scan_number);
                       continue;
                  }
               }
               jpeg->state = DILLO_JPEG_READ_END_SCAN;
               if (!jpeg_finish_output(&jpeg->cinfo)) {
                  /* out of input */
                  break;
               } else {
                  if (jpeg_input_complete(&jpeg->cinfo)) {
                     jpeg->state = DILLO_JPEG_DONE;
                     break;
                  } else {
                     jpeg->state = DILLO_JPEG_READ_BEGIN_SCAN;
                  }
               }
               if (!jpeg_start_output(&jpeg->cinfo,
                                      jpeg->cinfo.input_scan_number)) {
                  /* out of input */
                  break;
               }
               a_Dicache_new_scan(jpeg->Image, jpeg->url, jpeg->version);
               jpeg->state = DILLO_JPEG_READ_IN_SCAN;
            }
         }
      }
      dFree(linebuf);
   }
}

#endif /* ENABLE_JPEG */
