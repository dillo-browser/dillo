/*
 * File: image.hh
 *
 * Copyright (C) 2005-2007 Jorge Arellano Cid <jcid@dillo.org>,
 *                         Sebastian Geerken  <sgeerken@dillo.org>
 * Copyright (C) 2024 Rodrigo Arias Mallo <rodarima@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef __IMAGE_HH__
#define __IMAGE_HH__

/** @file
 * The DilloImage data-structure and methods */


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "bitvec.h"
#include "url.h"

/*
 * Defines
 */

/** Arbitrary maximum for image size. To avoid image size-crafting attacks. */
#define IMAGE_MAX_AREA (6000 * 6000)

/*
 * Types
 */

typedef struct _DilloImage DilloImage;

typedef enum {
   DILLO_IMG_TYPE_INDEXED,
   DILLO_IMG_TYPE_RGB,
   DILLO_IMG_TYPE_GRAY,
   DILLO_IMG_TYPE_CMYK_INV,
   DILLO_IMG_TYPE_NOTSET    /* Initial value */
} DilloImgType;

/* These will reflect the Image's "state" */
typedef enum {
   IMG_Empty,      /**< Just created the entry */
   IMG_SetParms,   /**< Parameters set */
   IMG_SetCmap,    /**< Color map set */
   IMG_Write,      /**< Feeding the entry */
   IMG_Close,      /**< Whole image got! */
   IMG_Abort       /**< Image transfer aborted */
} ImageState;

struct _DilloImage {
   void *layout, *img_rndr;

   /* Parameters as told by image data */
   uint_t width;
   uint_t height;

   float dpi;               /**< Dots per inch */
   int32_t bg_color;        /**< Background color */
   int32_t fg_color;        /**< Foreground color */
   bitvec_t *BitVec;        /**< Bit vector for decoded rows */
   uint_t ScanNumber;       /**< Current decoding scan */
   ImageState State;        /**< Processing status */

   int RefCount;            /**< Reference counter */
};


/*
 * Function prototypes
 */
DilloImage *a_Image_new(void *layout, void *img_rndr,
                        int32_t bg_color, int32_t fg_color);
DilloImage *a_Image_new_with_dw(void *layout, const char *alt_text,
                                int32_t bg_color, int32_t fg_color);
void *a_Image_get_dw(DilloImage *Image);
void a_Image_ref(DilloImage *Image);
void a_Image_unref(DilloImage *Image);

void a_Image_set_parms(DilloImage *Image, void *v_imgbuf, DilloUrl *url,
                       int version, uint_t width, uint_t height,
                       DilloImgType type);
void a_Image_write(DilloImage *Image, uint_t y);
void a_Image_close(DilloImage *Image);
void a_Image_abort(DilloImage *Image);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __IMAGE_HH__ */

