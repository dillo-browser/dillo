/*
 * File: dicache.c
 *
 * Copyright 2000-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include <string.h>         /* for memset */
#include <stdlib.h>

#include "msg.h"
#include "image.hh"
#include "imgbuf.hh"
#include "web.hh"
#include "dicache.h"
#include "dpng.h"
#include "dgif.h"
#include "djpeg.h"


enum {
   DIC_Gif,
   DIC_Png,
   DIC_Jpeg
};


/*
 * List of DICacheEntry. May hold several versions of the same image,
 * although most of the time it holds just one.
 */
static Dlist *CachedIMGs = NULL;

static uint_t dicache_size_total; /* invariant: dicache_size_total is
                                   * the sum of the image sizes (3*w*h)
                                   * of all the images in the dicache. */

/*
 * Compare function for image entries
 */
static int Dicache_entry_cmp(const void *v1, const void *v2)
{
   const DICacheEntry *e1 = v1, *e2 = v2;

   int st = a_Url_cmp(e1->url, e2->url);
   if (st == 0) {
      if (e2->version == DIC_Last)
         st = (e1->Flags & DIF_Last ? 0 : -1);
      else
         st = (e1->version - e2->version);
   }
   return st;
}

/*
 * Initialize dicache data
 */
void a_Dicache_init(void)
{
   CachedIMGs = dList_new(256);
   dicache_size_total = 0;
}

/*
 * Create, and initialize a new, empty, dicache entry
 */
static DICacheEntry *Dicache_entry_new(void)
{
   DICacheEntry *entry = dNew(DICacheEntry, 1);

   entry->width = 0;
   entry->height = 0;
   entry->Flags = DIF_Valid;
   entry->SurvCleanup = 0;
   entry->type = DILLO_IMG_TYPE_NOTSET;
   entry->cmap = NULL;
   entry->v_imgbuf = NULL;
   entry->RefCount = 1;
   entry->TotalSize = 0;
   entry->ScanNumber = 0;
   entry->BitVec = NULL;
   entry->State = DIC_Empty;
   entry->version = 1;

   entry->Decoder = NULL;
   entry->DecoderData = NULL;
   entry->DecodedSize = 0;

   return entry;
}

/*
 * Add a new entry in the dicache
 * (a single URL may have several entries)
 */
static DICacheEntry *Dicache_add_entry(const DilloUrl *Url)
{
   DICacheEntry e, *entry, *last;

   entry = Dicache_entry_new();
   e.url = (DilloUrl*)Url;
   e.version = DIC_Last;
   last = dList_find_sorted(CachedIMGs, &e, Dicache_entry_cmp);
   if (last) {
      /* URL is already in CachedIMGs, make a new version */
      last->Flags &= ~DIF_Last;
      entry->version = last->version + 1;
   }
   entry->url = a_Url_dup(Url);
   entry->Flags |= DIF_Last;
   dList_insert_sorted(CachedIMGs, entry, Dicache_entry_cmp);

   return entry;
}

/*
 * Search a particular version of a URL in the Dicache.
 * Return value: a pointer to the entry if found; NULL otherwise.
 *
 * Notes: DIC_Last means last version of the image.
 *        version zero is not allowed.
 */
DICacheEntry *a_Dicache_get_entry(const DilloUrl *Url, int version)
{
   DICacheEntry e;
   DICacheEntry *entry = NULL;

   dReturn_val_if_fail(version != 0, NULL);
   e.url = (DilloUrl*)Url;
   e.version = version;
   entry = dList_find_sorted(CachedIMGs, &e, Dicache_entry_cmp);
   if (entry && !(entry->Flags & DIF_Valid) && version == DIC_Last)
      entry = NULL;
   return entry;
}

/*
 * Actually free a dicache entry, given the URL and the version number.
 */
static void Dicache_remove(const DilloUrl *Url, int version)
{
   DICacheEntry e, *entry;

   _MSG("Dicache_remove url=%s\n", URL_STR(Url));
   e.url = (DilloUrl*)Url;
   e.version = version;
   entry = dList_find_sorted(CachedIMGs, &e, Dicache_entry_cmp);
   dReturn_if (entry == NULL);

   _MSG("Dicache_remove Imgbuf=%p Decoder=%p DecoderData=%p\n",
       entry->v_imgbuf, entry->Decoder, entry->DecoderData);
   /* Eliminate this dicache entry */
   dList_remove(CachedIMGs, entry);
   dicache_size_total -= entry->TotalSize;

   /* entry cleanup */
   a_Url_free(entry->url);
   dFree(entry->cmap);
   a_Bitvec_free(entry->BitVec);
   a_Imgbuf_unref(entry->v_imgbuf);
   if (entry->Decoder) {
      entry->Decoder(CA_Abort, entry->DecoderData);
   }
   dFree(entry);
}

/*
 * Unrefs the counter of a dicache entry (it counts cache clients).
 * If there're no clients and no imgbuf, remove the entry.
 * Otherwise, let a_Dicache_cleanup() do the job later
 * (keeping it cached meanwhile for e.g. reload, repush, back/fwd).
 */
void a_Dicache_unref(const DilloUrl *Url, int version)
{
   DICacheEntry *entry;

   if ((entry = a_Dicache_get_entry(Url, version))) {
      _MSG("a_Dicache_unref: RefCount=%d State=%d ImgbufLastRef=%d\n",
          entry->RefCount, entry->State,
          entry->v_imgbuf ? a_Imgbuf_last_reference(entry->v_imgbuf) : -1);
      if (entry->RefCount > 0) --entry->RefCount;
      if (entry->RefCount == 0 && entry->v_imgbuf == NULL)
         Dicache_remove(Url, version);
   }
}

/*
 * Refs the counter of a dicache entry.
 */
DICacheEntry* a_Dicache_ref(const DilloUrl *Url, int version)
{
   DICacheEntry *entry;

   if ((entry = a_Dicache_get_entry(Url, version))) {
      ++entry->RefCount;
   }
   return entry;
}

/*
 * Invalidate this entry. This is used for the reloading mechanism.
 * Can't erase current versions, but a_Dicache_get_entry(url, DIC_Last)
 * must return NULL.
 */
void a_Dicache_invalidate_entry(const DilloUrl *Url)
{
   DICacheEntry *entry = a_Dicache_get_entry(Url, DIC_Last);
   if (entry)
      entry->Flags &= ~DIF_Valid;
}


/* ------------------------------------------------------------------------- */

/*
 * Set image's width, height & type
 * - 'width' and 'height' come from the image data.
 * - HTML width and height attrs are handled with setNonCssHint.
 * - CSS sizing is handled by the CSS engine.
 */
void a_Dicache_set_parms(DilloUrl *url, int version, DilloImage *Image,
                         uint_t width, uint_t height, DilloImgType type,
                         double gamma)
{
   DICacheEntry *DicEntry;

   _MSG("a_Dicache_set_parms (%s)\n", URL_STR(url));
   dReturn_if_fail ( Image != NULL && width && height );
   /* Find the DicEntry for this Image */
   DicEntry = a_Dicache_get_entry(url, version);
   dReturn_if_fail ( DicEntry != NULL );
   /* Parameters already set? Don't do it twice. */
   dReturn_if_fail ( DicEntry->State < DIC_SetParms );

   _MSG("  RefCount=%d version=%d\n", DicEntry->RefCount, DicEntry->version);

   /* BUG: there's just one image-type now */
   #define I_RGB 0
   DicEntry->v_imgbuf =
      a_Imgbuf_new(Image->layout, I_RGB, width, height, gamma);

   DicEntry->TotalSize = width * height * 3;
   DicEntry->width = width;
   DicEntry->height = height;
   DicEntry->type = type;
   DicEntry->BitVec = a_Bitvec_new((int)height);
   DicEntry->State = DIC_SetParms;

   dicache_size_total += DicEntry->TotalSize;
}

/*
 * Implement the set_cmap method for the Image
 */
void a_Dicache_set_cmap(DilloUrl *url, int version, int bg_color,
                        const uchar_t *cmap, uint_t num_colors,
                        int num_colors_max, int bg_index)
{
   DICacheEntry *DicEntry = a_Dicache_get_entry(url, version);

   _MSG("a_Dicache_set_cmap\n");
   dReturn_if_fail ( DicEntry != NULL );

   dFree(DicEntry->cmap);
   DicEntry->cmap = dNew0(uchar_t, 3 * num_colors_max);
   memcpy(DicEntry->cmap, cmap, 3 * num_colors);
   if (bg_index >= 0 && (uint_t)bg_index < num_colors) {
      DicEntry->cmap[bg_index * 3]     = (bg_color >> 16) & 0xff;
      DicEntry->cmap[bg_index * 3 + 1] = (bg_color >> 8) & 0xff;
      DicEntry->cmap[bg_index * 3 + 2] = (bg_color) & 0xff;
   }

   DicEntry->State = DIC_SetCmap;
}

/*
 * Reset for a new scan from a multiple-scan image.
 */
void a_Dicache_new_scan(const DilloUrl *url, int version)
{
   DICacheEntry *DicEntry;

   _MSG("a_Dicache_new_scan\n");
   dReturn_if_fail ( url != NULL );
   DicEntry = a_Dicache_get_entry(url, version);
   dReturn_if_fail ( DicEntry != NULL );
   if (DicEntry->State < DIC_SetParms) {
      MSG("a_Dicache_new_scan before DIC_SetParms\n");
      exit(1);
   }
   a_Bitvec_clear(DicEntry->BitVec);
   DicEntry->ScanNumber++;
   a_Imgbuf_new_scan(DicEntry->v_imgbuf);
}

/*
 * Implement the write method
 * (Write a scan line into the Dicache entry)
 * buf: row buffer
 * Y  : row number
 */
void a_Dicache_write(DilloUrl *url, int version, const uchar_t *buf, uint_t Y)
{
   DICacheEntry *DicEntry;

   _MSG("a_Dicache_write\n");
   DicEntry = a_Dicache_get_entry(url, version);
   dReturn_if_fail ( DicEntry != NULL );
   dReturn_if_fail ( DicEntry->width > 0 && DicEntry->height > 0 );

   /* update the common buffer in the imgbuf */
   a_Imgbuf_update(DicEntry->v_imgbuf, buf, DicEntry->type,
                   DicEntry->cmap, DicEntry->width, DicEntry->height, Y);

   a_Bitvec_set_bit(DicEntry->BitVec, (int)Y);
   DicEntry->State = DIC_Write;
}

/*
 * Implement the close method of the decoding process
 */
void a_Dicache_close(DilloUrl *url, int version, CacheClient_t *Client)
{
   DilloWeb *Web = Client->Web;
   DICacheEntry *DicEntry = a_Dicache_get_entry(url, version);

   dReturn_if_fail ( DicEntry != NULL );

   /* a_Dicache_unref() may free DicEntry */
   _MSG("a_Dicache_close RefCount=%d\n", DicEntry->RefCount - 1);
   _MSG("a_Dicache_close DIC_Close=%d State=%d\n", DIC_Close, DicEntry->State);
   _MSG(" a_Dicache_close imgbuf=%p Decoder=%p DecoderData=%p\n",
        DicEntry->v_imgbuf, DicEntry->Decoder, DicEntry->DecoderData);

   if (DicEntry->State < DIC_Close) {
      DicEntry->State = DIC_Close;
      dFree(DicEntry->cmap);
      DicEntry->cmap = NULL;
      DicEntry->Decoder = NULL;
      DicEntry->DecoderData = NULL;
   }
   a_Dicache_unref(url, version);

   a_Bw_close_client(Web->bw, Client->Key);
}

/* ------------------------------------------------------------------------- */

/*
 * Generic MIME handler for GIF, JPEG and PNG.
 * Sets a_Dicache_callback as the cache-client,
 * and also sets the image decoder.
 *
 * Parameters:
 *   Type: MIME type
 *   Ptr:  points to a Web structure
 *   Call: Dillo calls this with more data/eod
 *   Data: Decoding data structure
 */
static void *Dicache_image(int ImgType, const char *MimeType, void *Ptr,
                           CA_Callback_t *Call, void **Data)
{
   DilloWeb *web = Ptr;
   DICacheEntry *DicEntry;

   dReturn_val_if_fail(MimeType && Ptr, NULL);

   if (!web->Image) {
      web->Image =
         a_Image_new_with_dw(web->bw->render_layout, NULL, web->bgColor);
      a_Image_ref(web->Image);
   }

   DicEntry = a_Dicache_get_entry(web->url, DIC_Last);
   if (!DicEntry) {
      /* Create an entry for this image... */
      DicEntry = Dicache_add_entry(web->url);
      /* Attach a decoder */
      if (ImgType == DIC_Jpeg) {
         DicEntry->Decoder = (CA_Callback_t)a_Jpeg_callback;
         DicEntry->DecoderData =
            a_Jpeg_new(web->Image, DicEntry->url, DicEntry->version);
      } else if (ImgType == DIC_Gif) {
         DicEntry->Decoder = (CA_Callback_t)a_Gif_callback;
         DicEntry->DecoderData =
            a_Gif_new(web->Image, DicEntry->url, DicEntry->version);
      } else if (ImgType == DIC_Png) {
         DicEntry->Decoder = (CA_Callback_t)a_Png_callback;
         DicEntry->DecoderData =
            a_Png_new(web->Image, DicEntry->url, DicEntry->version);
      }
   } else {
      /* Repeated image */
      a_Dicache_ref(DicEntry->url, DicEntry->version);
   }
   /* Survive three cleanup passes (set to zero = old behaviour). */
   DicEntry->SurvCleanup = 3;

   *Data = DicEntry->DecoderData;
   *Call = (CA_Callback_t) a_Dicache_callback;

   return (a_Image_get_dw (web->Image));
}

/*
 * PNG wrapper for Dicache_image()
 */
void *a_Dicache_png_image(const char *Type, void *Ptr, CA_Callback_t *Call,
                          void **Data)
{
   return Dicache_image(DIC_Png, Type, Ptr, Call, Data);
}

/*
 * GIF wrapper for Dicache_image()
 */
void *a_Dicache_gif_image(const char *Type, void *Ptr, CA_Callback_t *Call,
                          void **Data)
{
   return Dicache_image(DIC_Gif, Type, Ptr, Call, Data);
}

/*
 * JPEG wrapper for Dicache_image()
 */
void *a_Dicache_jpeg_image(const char *Type, void *Ptr, CA_Callback_t *Call,
                           void **Data)
{
   return Dicache_image(DIC_Jpeg, Type, Ptr, Call, Data);
}

/*
 * This function is a cache client; (but feeds its clients from dicache)
 */
void a_Dicache_callback(int Op, CacheClient_t *Client)
{
   uint_t i;
   DilloWeb *Web = Client->Web;
   DilloImage *Image = Web->Image;
   DICacheEntry *DicEntry = a_Dicache_get_entry(Web->url, DIC_Last);

   dReturn_if_fail ( DicEntry != NULL );

   /* Copy the version number in the Client */
   if (Client->Version == 0)
      Client->Version = DicEntry->version;

   /* Only call the decoder when necessary */
   if (Op == CA_Send && DicEntry->State < DIC_Close &&
       DicEntry->DecodedSize < Client->BufSize) {
      DicEntry->Decoder(Op, Client);
      DicEntry->DecodedSize = Client->BufSize;
   } else if (Op == CA_Close || Op == CA_Abort) {
      if (DicEntry->State < DIC_Close) {
         DicEntry->Decoder(Op, Client);
      } else {
         a_Dicache_close(DicEntry->url, DicEntry->version, Client);
      }
   }

   /* when the data stream is not an image 'v_imgbuf' remains NULL */
   if (Op == CA_Send && DicEntry->v_imgbuf) {
      if (Image->height == 0 && DicEntry->State >= DIC_SetParms) {
         /* Set parms */
         a_Image_set_parms(
            Image, DicEntry->v_imgbuf, DicEntry->url,
            DicEntry->version, DicEntry->width, DicEntry->height,
            DicEntry->type);
      }
      if (DicEntry->State == DIC_Write) {
         if (DicEntry->ScanNumber == Image->ScanNumber) {
            for (i = 0; i < DicEntry->height; ++i)
               if (a_Bitvec_get_bit(DicEntry->BitVec, (int)i) &&
                   !a_Bitvec_get_bit(Image->BitVec, (int)i) )
                  a_Image_write(Image, i);
         } else {
            for (i = 0; i < DicEntry->height; ++i) {
               if (a_Bitvec_get_bit(DicEntry->BitVec, (int)i) ||
                   !a_Bitvec_get_bit(Image->BitVec, (int)i)   ||
                   DicEntry->ScanNumber > Image->ScanNumber + 1) {
                  a_Image_write(Image, i);
               }
               if (!a_Bitvec_get_bit(DicEntry->BitVec, (int)i))
                  a_Bitvec_clear_bit(Image->BitVec, (int)i);
            }
            Image->ScanNumber = DicEntry->ScanNumber;
         }
      }
   } else if (Op == CA_Close) {
      a_Image_close(Image);
      a_Bw_close_client(Web->bw, Client->Key);
   } else if (Op == CA_Abort) {
      a_Image_abort(Image);
      a_Bw_close_client(Web->bw, Client->Key);
   }
}

/* ------------------------------------------------------------------------- */

/*
 * Free the imgbuf (RGB data) of unused entries.
 */
void a_Dicache_cleanup(void)
{
   int i;
   DICacheEntry *entry;

   for (i = 0; (entry = dList_nth_data(CachedIMGs, i)); ++i) {
      _MSG(" SurvCleanup = %d\n", entry->SurvCleanup);
      if (entry->RefCount == 0 &&
          (!entry->v_imgbuf || a_Imgbuf_last_reference(entry->v_imgbuf))) {
         if (--entry->SurvCleanup >= 0)
            continue;  /* keep the entry one more pass */

         /* free this unused entry */
         Dicache_remove(entry->url, entry->version);
         --i; /* adjust counter */
      }
   }
   MSG("a_Dicache_cleanup: length = %d\n", dList_length(CachedIMGs));
}

/* ------------------------------------------------------------------------- */

/*
 * Deallocate memory used by dicache module
 * (Call this one at exit time, with no cache clients queued)
 */
void a_Dicache_freeall(void)
{
   DICacheEntry *entry;

   /* Remove all the dicache entries */
   while ((entry = dList_nth_data(CachedIMGs, dList_length(CachedIMGs)-1))) {
      dList_remove_fast(CachedIMGs, entry);
      a_Url_free(entry->url);
      dFree(entry->cmap);
      a_Bitvec_free(entry->BitVec);
      a_Imgbuf_unref(entry->v_imgbuf);
      dicache_size_total -= entry->TotalSize;
      dFree(entry);
   }
   dList_free(CachedIMGs);
}
